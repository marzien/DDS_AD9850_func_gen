# AD9850 DDS Function Generator (STM32F103)

Rotary-encoder controlled signal generator built from an AD9850 DDS module,
a 20x4 HD44780 LCD with PCF8574 I2C backpack and an STM32F103RB.

## Features
- 0 Hz – 40 MHz, 1 Hz resolution (125 MHz reference clock)
- STM32 runs at 72 MHz (8 MHz crystal × 9, APB1 36 MHz, 48 MHz USB clock ready)
- Phase offset in 11.25° steps
- Output on/off (AD9850 power-down)
- Linear frequency sweep (up or down), period 0.1 s – 60 s
- Burst / gated output: on for X ms, off for Y ms (1 ms – 60 s each), also during a sweep
- Settings are saved to flash and restored at power-up
- Text command interface over the Blue Pill micro-USB port (USB virtual COM port)
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
Items: Output, Phase, Burst, On time, Off time, Sweep, Sw start, Sw stop, Sw time, Exit
- Rotate: select item / change value while editing (marker changes to `*`)
- Short press: edit / confirm (on frequency items it moves the digit cursor)
- Press and turn while editing Sw start / Sw stop: change the step
- Long press: confirm edit, or leave the menu

**Burst:** set Burst ON, then On time / Off time. The output is switched with the
AD9850 power-down bit, timed from SysTick (1 ms resolution). Each cycle starts with
the on time. Times change in 1 ms steps below 100 ms, 10 ms below 1 s, 100 ms below
10 s and 1 s above. The main screen shows `Burst on/off ms` on the last line (`Out:BST`
while sweeping). Gating is not synchronised to the waveform, so a burst does not start
or end at a zero crossing.

## Saved settings
All settings (frequency, step, phase, output, sweep start/stop/time, sweep on/off)
are saved to flash automatically 3 s after the last knob or button action, and
restored at power-up. Hold the encoder button while powering up to restore the
defaults (the splash screen shows "Defaults restored").

Storage uses the last 1 KB page of the first 64 KB (0x0800FC00), split into
32 slots, so the page is erased only once per 32 saves. For that reason the
linker script limits program flash to 63 KB.

LCD size is set by `LCD_COLS` / `LCD_ROWS` in `Core/Inc/I2C_LCD.h` (layout is designed for 20x4).

## USB command interface
Plug the Blue Pill micro-USB into a PC: it appears as a virtual COM port
("AD9850 DDS Function Generator", VID 0483 / PID 5740; Windows 10/11 needs no driver).
Open it with any terminal (PuTTY, Arduino serial monitor, Python `pyserial`); the baud rate
does not matter. One command per line, case-insensitive; every command answers `OK` or `ERR ...`.

| Command | Meaning |
|---|---|
| `F 1000`, `F 12.5k`, `F 7.1M` | set frequency (0 … 40 MHz), stops a running sweep |
| `STEP 1k` | encoder step, power of ten 1 … 10M |
| `PH 90` | phase in degrees (rounded to 11.25° steps) |
| `OUT ON` / `OUT OFF` | output on/off |
| `SWEEP 100 10k 2.5` | sweep 100 Hz → 10 kHz in 2.5 s and start it |
| `SWEEP ON` / `SWEEP OFF` | start/stop the sweep with the stored settings |
| `BURST 10 90` | burst: 10 ms on, 90 ms off, and start it |
| `BURST ON` / `BURST OFF` | burst mode on/off |
| `SAVE` | write settings to flash now (otherwise saved 3 s after the last change) |
| `?` or `STATUS` | print all settings |
| `*IDN?` | identification string |
| `HELP` | list the commands |

Remote changes update the LCD and are autosaved like changes made with the knob.
Notes:
- Some Blue Pills have a 10 kΩ resistor (R10) on USB D+ instead of 1.5 kΩ. If the PC does
  not detect the board, add 1.8 kΩ between PA12 and 3.3 V (or replace R10 with 1.5 kΩ).
- The firmware pulls D+ low for 20 ms at start-up so the PC re-detects the board after a reset.
- With the carrier PCB, do not connect the PC USB and the 5 V DC jack at the same time
  (the PC would supply the whole board through the Blue Pill 5V pin).

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

## Hardware
A KiCad carrier PCB (Blue Pill + AD9850 module sockets, level shifter, LM7171 sine amplifier,
square buffer, sync output, BNC connectors) is in [`Hardware/`](Hardware/README.md).

## Source layout
- `Core/Src/AD9850.c` – DDS serial driver
- `Core/Src/I2C_LCD.c` – LCD driver
- `Core/Src/Rotary_Sw.c` – encoder + button, polled from SysTick (1 ms)
- `Core/Src/FuncGen.c` – user interface and sweep logic
- `Core/Src/Flash_Store.c` – wear-levelled settings storage in flash
- `Core/Src/UsbCmd.c` – USB command parser
- `USB_DEVICE/` – USB CDC device glue (descriptors, low-level driver, virtual COM port)
- `Middlewares/ST/STM32_USB_Device_Library/` – ST USB device library v2.11.6 (core + CDC)
