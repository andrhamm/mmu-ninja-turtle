# AFC-Pro port (Phase 6) — hardware mapping

Board: Isik's Tech AFC-Pro, STM32H723 ([repo](https://github.com/xbst/AFC-Pro), [manual](https://docs.isiks.tech/box-turtle/afc-pro/manual/)).
Pin source: Armored Turtle `AFC-Klipper-Add-On/config/mcu/AFC_Pro.cfg` (fetched 2026-10-23). Clock, flash/RAM size and
bootloader are **unverified**.

## MMU3 model -> Box Turtle hardware

| MMU3 concept (unchanged logic) | AFC-Pro hardware |
|---|---|
| Pulley axis | STEP/DIR/EN of the **active lane** stepper (lane = idler's engaged slot); one pulse generator, output routed per lane |
| Selector axis | virtual: no motor; moves complete at once, homing synthesized (as in `tools/vmmu`) |
| Idler engage/disengage | enable / disable the active lane's TMC2209 |
| FINDA | `HUB` switch (PD0) |
| (new) lane prep / lane exit sensors | `TRGn` / `EXTn` switches — usable for lane-empty errors and auto-load |
| (new) rewinders | `MOTn_RWD/FWD/EN` DC drivers — rewind while the lane retracts |
| (new) TurtleNeck buffer | `TN_ADV` (PB3), `TN_TRL` (PG15) — candidate for lane-motor assist during prints |
| TMC2130 over SPI (MMU3) | TMC2209 single-wire UART, **one pin per driver** (`Mn_UART`) |
| Cut (`K`) | rejected (`HasCutter()` = false) |
| Printer link UART (115200 8N1) | **open**: needs a free USART-capable pin pair on a connector (I2C/CAN/expansion header) — check AFC-Pro schematic |

## Lane pins (lanes 1–5 used)

| Lane | STEP | DIR | EN | UART | TRG (prep) | EXT (exit) | Rewinder RWD/FWD/EN |
|---|---|---|---|---|---|---|---|
| 1 | PE7 | PG1 | PE10 | PE8 | PD1 | PD7 | PE9 / PE11 / PE15 |
| 2 | PB2 | PA3 | PG0 | PF15 | PD4 | PG10 | PE13 / PE14 / PD9 |
| 3 | PA0 | PC1 | PA2 | PA1 | PD6 | PG12 | PB10 / PB11 / PB13 |
| 4 | PF9 | PF8 | PC0 | PF10 | PG9 | PG13 | PB14 / PB15 / PD11 |
| 5 | PF5 | PF4 | PF7 | PF6 | PG11 | PB7 | PD12 / PD13 / PG3 |

HUB = PD0. Lanes 6–8 exist on the board (unused at 5 lanes).

## HAL work (new `src/hal/stm32/`)
- USART (printer link), GPIO, timer ISR step generation behind `pulse_gen` (replaces AVR timer1), per-lane output routing.
- TMC2209 UART driver (replaces `hal/tmc2130` + `hal/shr16`), StallGuard not required (no homing axes).
- DC rewinder PWM, flash-emulated EEPROM, watchdog, USB CDC for logs (`hal/usb_cdc`).
- Guard AVR-only code (`progmem`, `__attribute__((section))`, `<avr/*>`).
