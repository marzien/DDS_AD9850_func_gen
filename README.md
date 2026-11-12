# AD9850 DDS Function Generator (STM32F103)

Rotary-encoder controlled signal generator built from an AD9850 DDS module,
a 20x4 HD44780 LCD with PCF8574 I2C backpack and an STM32F103RB.

## Features
- 0 Hz – 40 MHz, 1 Hz resolution (125 MHz reference clock)
- Phase offset in 11.25° steps
- Output on/off (AD9850 power-down)
- Linear frequency sweep (up or down), period 0.1 s – 60 s
- Outputs of the AD9850 module: **sine** on ZOUT1/ZOUT2, **square** on QOUT1/QOUT2
  (square duty cycle is set by the blue trimmer on the module). The AD9850 cannot
  generate triangle/sawtooth.

## Wiring
| STM32 | Function | Connect to |
|------|----------|------------|
| PA0  | W_CLK    | AD9850 W_CLK |
| PA1  | FQ_UD    | AD9850 FQ_UD |
| PA2  | Data     | AD9850 D7 (serial data) |
| –    | RESET    | AD9850 RESET → GND |
| PA6  | BTN_A    | Encoder A / CLK |
| PA4  | BTN_B    | Encoder B / DT |
| PA5  | BTN_PUSH | Encoder SW (to GND when pressed; internal pull-up + optional 10k to 3V3) |
| PB10 | I2C2 SCL | LCD backpack SCL |
| PB11 | I2C2 SDA | LCD backpack SDA |

Power the encoder from 3V3 (PA4/PA5/PA6 are not 5 V tolerant).
LCD address 0x27 or 0x3F is auto-detected (`SLAVE_ADDRESS_LCD` in `I2C_LCD.h`).

## Controls
**Main screen**
```
Freq:      1,000 Hz     <- cursor under the digit being changed
Step: 1 kHz
Phase: 0.00°
Output: ON
```
- Rotate: change frequency by the digit under the cursor
- Press and turn: change the step – clockwise = bigger, counter-clockwise = smaller
  (1 Hz … 10 MHz, shown on the "Step:" line and by the cursor)
- Short press: move cursor one digit left (1 Hz … 10 MHz, wraps)
- Long press: open menu
- While sweeping the screen shows live frequency, start, stop and period;
  short press stops the sweep

**Menu** (3 items visible, scrolls)
```
MENU      hold=exit
>Output           ON
 Phase         0.00°
 Sweep           OFF
```
Items: Output, Phase, Sweep, Sw start, Sw stop, Sw time, Exit
- Rotate: select item / change value while editing (marker changes to `*`)
- Short press: edit / confirm (on frequency items it moves the digit cursor)
- Press and turn while editing Sw start / Sw stop: change the step
- Long press: confirm edit, or leave the menu

LCD size is set by `LCD_COLS` / `LCD_ROWS` in `Core/Inc/I2C_LCD.h` (layout is designed for 20x4).

## Tuning
In `Core/Inc/Rotary_Sw.h`:
- `RS_STEPS_PER_DETENT` – 4 for KY-040 style encoders, 2 if one click counts double
- `RS_REVERSE` – flip rotation direction
- `RS_BTN_ACTIVE_LEVEL` – 0 (default) for a switch to GND, 1 for a switch to 3V3

## Building (STM32CubeIDE)
- File > Import > General > Existing Projects into Workspace, select this folder
  (project name `DDS_AD9850_FULL`), then build with the hammer icon.
- Startup code: `Core/Startup/startup_stm32f103rbtx.s`, linker script: `STM32F103RBTX_FLASH.ld`.
- Pin configuration lives in `DDS_AD9850_FULL.ioc` (toolchain: STM32CubeIDE). Application code is
  inside `USER CODE` blocks and in separate files, so it survives code regeneration.

## Source layout
- `Core/Src/AD9850.c` – DDS serial driver
- `Core/Src/I2C_LCD.c` – LCD driver
- `Core/Src/Rotary_Sw.c` – encoder + button, polled from SysTick (1 ms)
- `Core/Src/FuncGen.c` – user interface and sweep logic
