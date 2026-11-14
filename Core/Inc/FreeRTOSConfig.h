#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H
/*
 * FreeRTOS V11.2.0 configuration for the AD9850 function generator
 * (STM32F103C8 / RB, 72 MHz, 20 KB RAM).
 *
 * All kernel objects are statically allocated (no FreeRTOS heap), so RAM use
 * is fixed at link time and visible in the .map file. See Core/Src/App.c for
 * the task list and stack sizes.
 */
#include <stdint.h>

extern uint32_t SystemCoreClock;

#define configUSE_PREEMPTION                     1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION  1
#define configCPU_CLOCK_HZ                       (SystemCoreClock)
#define configTICK_RATE_HZ                       ((TickType_t)1000)
#define configMAX_PRIORITIES                     5
#define configMINIMAL_STACK_SIZE                 ((uint16_t)96)   /* words, idle task */
#define configMAX_TASK_NAME_LEN                  6
#define configTICK_TYPE_WIDTH_IN_BITS            TICK_TYPE_WIDTH_32_BITS
#define configIDLE_SHOULD_YIELD                  1
#define configUSE_TASK_NOTIFICATIONS             1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES    1
#define configUSE_MUTEXES                        1
#define configUSE_RECURSIVE_MUTEXES              0
#define configUSE_COUNTING_SEMAPHORES            0
#define configQUEUE_REGISTRY_SIZE                0
#define configUSE_TIME_SLICING                   1
#define configUSE_NEWLIB_REENTRANT               0
#define configENABLE_BACKWARD_COMPATIBILITY      0

/* Memory: static only */
#define configSUPPORT_STATIC_ALLOCATION          1
#define configSUPPORT_DYNAMIC_ALLOCATION         0
#define configKERNEL_PROVIDED_STATIC_MEMORY      1   /* kernel supplies the idle task memory */

/* Hooks and checks */
#define configUSE_IDLE_HOOK                      0
#define configUSE_TICK_HOOK                      0
#define configCHECK_FOR_STACK_OVERFLOW           2
#define configUSE_MALLOC_FAILED_HOOK             0
#define configUSE_TRACE_FACILITY                 0
#define configGENERATE_RUN_TIME_STATS            0

/* Not used */
#define configUSE_CO_ROUTINES                    0
#define configUSE_TIMERS                         0

/* API functions */
#define INCLUDE_vTaskPrioritySet                 0
#define INCLUDE_uxTaskPriorityGet                0
#define INCLUDE_vTaskDelete                      0
#define INCLUDE_vTaskSuspend                     1   /* needed for portMAX_DELAY blocking */
#define INCLUDE_xTaskDelayUntil                  1
#define INCLUDE_vTaskDelay                       1
#define INCLUDE_xTaskGetSchedulerState           1
#define INCLUDE_uxTaskGetStackHighWaterMark      1
#define INCLUDE_xTaskGetCurrentTaskHandle        1
#define INCLUDE_xTaskGetIdleTaskHandle           1

/* Cortex-M interrupt priorities (STM32F1 has 4 priority bits).
 * Interrupts that call FreeRTOS "FromISR" functions must use a priority
 * number >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY (USB uses 5). */
#define configPRIO_BITS                               4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY       15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY  5
#define configKERNEL_INTERRUPT_PRIORITY      (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

void App_AssertFailed(void);
#define configASSERT(x)  do { if ((x) == 0) { App_AssertFailed(); } } while (0)

/* Map the port's handlers onto the CMSIS vector names. SysTick_Handler stays in
 * stm32f1xx_it.c and calls xPortSysTickHandler() once the scheduler runs. */
#define vPortSVCHandler      SVC_Handler
#define xPortPendSVHandler   PendSV_Handler

#endif /* FREERTOS_CONFIG_H */
