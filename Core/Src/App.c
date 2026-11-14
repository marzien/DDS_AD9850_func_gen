#include "App.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "Dds.h"
#include "FuncGen.h"
#include "Rotary_Sw.h"
#include "UsbCmd.h"

/* Stack sizes in words (4 bytes). Checked at run time: see App_MemReport()
 * (USB command MEM) and vApplicationStackOverflowHook(). */
#define STACK_DDS    160U
#define STACK_INPUT  128U
#define STACK_UI     384U
#define STACK_USB    384U

#define PRIO_DDS     4U
#define PRIO_INPUT   3U
#define PRIO_UI      2U
#define PRIO_USB     1U

#define MSP_FILL     0xA5A5A5A5UL

static StackType_t  s_stack_dds[STACK_DDS];
static StackType_t  s_stack_input[STACK_INPUT];
static StackType_t  s_stack_ui[STACK_UI];
static StackType_t  s_stack_usb[STACK_USB];
static StaticTask_t s_tcb_dds, s_tcb_input, s_tcb_ui, s_tcb_usb;

static TaskHandle_t s_task_dds, s_task_input, s_task_ui, s_task_usb;

static StaticSemaphore_t s_lock_mem;
static SemaphoreHandle_t s_lock;

/* Linker script symbols */
extern uint32_t _sdata, _edata, _sbss, _ebss, _sidata, _end, _estack;
extern uint32_t _flash_size, _ram_size;   /* absolute symbols: the address is the value */

/* ------------------------------------------------------------------ tasks */

/* Encoder and button: sampled every 1 ms, the UI task is woken on activity */
static void input_task(void *arg)
{
	TickType_t last = xTaskGetTickCount();

	(void)arg;
	for (;;)
	{
		(void)xTaskDelayUntil(&last, pdMS_TO_TICKS(1));
		RS_Poll_1ms();
		if (RS_Pending())
		{
			App_NotifyUi(APP_EV_INPUT);
		}
	}
}

void App_Start(void)
{
	/* Paint the unused part of the main stack so its peak use can be measured.
	 * After the scheduler starts, the main stack is used by interrupts only. */
	{
		uint32_t *p = &_end;
		uint32_t *top = (uint32_t *)(__get_MSP() - 64U);
		while (p < top)
		{
			*p++ = MSP_FILL;
		}
	}

	DDS_Init();
	RS_Init();
	FG_LoadSettings();                     /* also sends the settings to the DDS task */

	s_lock = xSemaphoreCreateMutexStatic(&s_lock_mem);

	s_task_dds = xTaskCreateStatic(DDS_Task, "DDS", STACK_DDS, NULL, PRIO_DDS, s_stack_dds, &s_tcb_dds);
	s_task_input = xTaskCreateStatic(input_task, "INPUT", STACK_INPUT, NULL, PRIO_INPUT, s_stack_input, &s_tcb_input);
	s_task_ui = xTaskCreateStatic(FG_UiTask, "UI", STACK_UI, NULL, PRIO_UI, s_stack_ui, &s_tcb_ui);
	s_task_usb = xTaskCreateStatic(UC_Task, "USB", STACK_USB, NULL, PRIO_USB, s_stack_usb, &s_tcb_usb);

	vTaskStartScheduler();
	for (;;)
	{
	}
}

/* ------------------------------------------------------------ primitives */

void App_Lock(void)
{
	(void)xSemaphoreTake(s_lock, portMAX_DELAY);
}

void App_Unlock(void)
{
	(void)xSemaphoreGive(s_lock);
}

void App_NotifyUi(uint32_t events)
{
	if (s_task_ui != NULL)
	{
		(void)xTaskNotify(s_task_ui, events, eSetBits);
	}
}

void App_NotifyUsbFromISR(void)
{
	BaseType_t woken = pdFALSE;

	if (s_task_usb != NULL && xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
	{
		vTaskNotifyGiveFromISR(s_task_usb, &woken);
		portYIELD_FROM_ISR(woken);
	}
}

/* ------------------------------------------------------------ memory use */

void App_MemReport(void (*print)(const char *fmt, ...))
{
	static const char *const names[] = {"DDS", "INPUT", "UI", "USB", "IDLE"};
	const TaskHandle_t handles[] = {s_task_dds, s_task_input, s_task_ui, s_task_usb, xTaskGetIdleTaskHandle()};
	const uint16_t sizes[] = {STACK_DDS, STACK_INPUT, STACK_UI, STACK_USB, configMINIMAL_STACK_SIZE};
	uint32_t flash_used = ((uint32_t)&_sidata - FLASH_BASE) + ((uint32_t)&_edata - (uint32_t)&_sdata);
	uint32_t ram_static = (uint32_t)&_ebss - (uint32_t)&_sdata;
	uint32_t msp_size = (uint32_t)&_estack - (uint32_t)&_end;
	uint32_t msp_free = 0;
	const uint32_t *p = &_end;

	while (p < &_estack && *p == MSP_FILL)
	{
		p++;
		msp_free += 4U;
	}
	print("FLASH %lu / %lu B\r\n", (unsigned long)flash_used, (unsigned long)(uint32_t)&_flash_size);
	print("RAM static %lu / %lu B\r\n", (unsigned long)ram_static, (unsigned long)(uint32_t)&_ram_size);
	print("STACK ISR size %lu min free %lu B\r\n", (unsigned long)msp_size, (unsigned long)msp_free);
	for (uint32_t i = 0; i < (sizeof(handles) / sizeof(handles[0])); i++)
	{
		print("STACK %s size %u min free %u B\r\n", names[i], (unsigned)(sizes[i] * 4U),
		      (unsigned)(uxTaskGetStackHighWaterMark(handles[i]) * 4U));
	}
}

/* ------------------------------------------------------------ RTOS hooks */

/* Fatal errors: stop with interrupts off (attach the debugger to see where) */
void App_AssertFailed(void)
{
	taskDISABLE_INTERRUPTS();
	for (;;)
	{
	}
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
	(void)xTask;
	(void)pcTaskName;   /* name of the task that overflowed its stack */
	taskDISABLE_INTERRUPTS();
	for (;;)
	{
	}
}

/* HAL_Delay() (LCD driver, HAL internals): sleep instead of spinning once the
 * scheduler runs, so lower priority tasks get the CPU. */
void HAL_Delay(uint32_t Delay)
{
	if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING && xPortIsInsideInterrupt() == pdFALSE)
	{
		vTaskDelay(pdMS_TO_TICKS(Delay) + 1U);
	}
	else
	{
		uint32_t t0 = HAL_GetTick();
		while ((HAL_GetTick() - t0) < (Delay + 1U))
		{
		}
	}
}
