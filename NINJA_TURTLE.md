# Ninja Turtle

Unofficial fork of [Prusa-Firmware-MMU](https://github.com/prusa3d/Prusa-Firmware-MMU) v3.0.4 (GPL-3.0, see `LICENSE.md`).
Goal: make a 5-lane Box Turtle filament changer present itself to a stock-firmware Prusa CORE One L+ as an MMU3.
Not affiliated with or supported by Prusa Research. Project notes: `~/dev/3dfarm/Builds/Ninja Turtle.md`.

## Build & test (dev container — primary, same as CI)
Ubuntu 24.04 image (`docker/Dockerfile`): GCC, CMake, Ninja, Python, arm-none-eabi-gcc, socat.

    scripts/dev ./scripts/ci          # build + all tests (upstream + vmmu smoke), identical to GitHub Actions
    scripts/dev                       # interactive shell in the container
    scripts/dev cmake --build --preset linux

VS Code: "Reopen in Container" (`.devcontainer/devcontainer.json`). CI: `.github/workflows/ci.yml`.

## Native macOS build (fallback)
Apple's Command Line Tools C++ headers are broken on this macOS build and libc++ clashes with `src/modules/math.h`,
so native builds need Homebrew GCC (`brew install gcc`): `cmake --preset macos && cmake --build --preset macos`.

## Talking to real hardware from the container
Docker Desktop on macOS can't hand USB serial devices to containers, so the Mac owns the USB bridge and exposes it over TCP:

    # on the Mac (brew install socat)
    socat TCP-LISTEN:5555,reuseaddr,nodelay FILE:/dev/cu.usbmodemXXXX,raw,echo=0,b115200
    # in the container
    build/linux/tools/vmmu/vmmu --port tcp:host.docker.internal:5555 --log frames.jsonl

Flashing MCUs (later phases) also runs on the Mac; builds stay in the container.

## Changes from upstream
- `src/hal/gpio.h`, `src/hal/usart.h`, `src/pins.h`: `#ifdef NT_STM32` dispatch to `src/hal/stm32/*` / `src/pins_afcpro.h` (AVR/host paths unchanged).
- `src/modules/motion.cpp`: `NT_STM32` hooks for the step-timer IRQ (enable/disable/init).
- New: `src/nt/tmc2209.*` (TMC2209 UART datagrams/register values, unit-tested against Klipper's algorithm).
- New: `src/nt/virtual_axes.*` (synthesized selector/idler end-stops for virtual axes; used by the AFC-Pro firmware and vmmu).
- `src/modules/permanent_storage.cpp`: `EEOFFSET` casts via `uintptr_t` (32-bit MCUs).
- `src/application.cpp`: Cut (`K`) requests are rejected when `HasCutter()` is false (weak default `true`; Box Turtle builds/sim override).
- `tests/unit/modules/protocol/test_protocol.cpp`: test helper read past the end of a `string_view` (caught by GCC 16 assertions).

## vmmu (virtual MMU)
Unmodified MMU application + logic on the host, speaking MMU3 protocol on a serial device or a pty;
motors/sensors/EEPROM simulated (`tools/vmmu/physics.cpp`: stall-guard homing, per-slot filament tips, FINDA).

    build/linux/tools/vmmu/vmmu --port pty --log frames.jsonl    # prints the pty path; also --port <device> | tcp:host:port
    python3 -I tools/vmmu/smoke.py /dev/ttysNNN [load]

Motion runs at planned feed rates (`tools/vmmu/motion_sim.cpp`, constant velocity): L1 incl. first homing ≈ 9 s.
Changer model: `--changer mmu3` (default) or `--changer boxturtle` — no selector/idler mechanics (virtual axes, 20x speed so
homing still sees its end-stop), the idler slot picks which lane motor the pulley axis drives, FINDA = Box Turtle hub sensor.
Faults: `--fault finda-stuck-off|finda-stuck-on|idler-home-fail|selector-home-fail[:ms]` (`:ms` = auto-clear).

## vprinter (virtual printer)
Prusa Buddy's unmodified printer-side protocol state machine (`third_party/buddy`, v6.10.1) driving an MMU over
pty/serial/TCP, with a simulated printer filament sensor. Reports reply latency (the CORE One extension board allows 10 ms).

    tools/vprinter/loop_test.sh build/linux/tools/vmmu/vmmu build/linux/tools/vprinter/vprinter --scenario tc:5 --gate-ms 10
    # scenarios: lineup | idle:SECS | tc:N | tc-retry (with VMMU_ARGS="--fault finda-stuck-off:6000")

`--gate-ms 10` emulates the CORE One extension board: a reply byte later than 10 ms after the last UART activity abandons
that reply and RX is flushed before each transmit (Buddy `xbuddy_extension/mmu.cpp`). `vmmu --fault reply-delay=15` proves it
(negative test `loop::gate_rejects_slow_mmu`). Register-write whitelist not modelled (Buddy only writes whitelisted registers).
Soak tests (excluded from `scripts/ci`): `ctest --preset linux -L soak` (100 tool changes, 1 h idle).

## AFC-Pro port (Phase 6)
`ports/afcpro/` — standalone Cortex-M7 build (arm-none-eabi in the dev container):
- `nt_afcpro.elf/.bin`: upstream application/logic/motion on Box Turtle hardware — 22.8 KB flash, 9.6 KB RAM.
  HAL in `hal_afcpro.cpp` + `src/hal/stm32/`: GPIO, SysTick timebase, TIM2 step timer (2 MHz, same contract as AVR TIMER1),
  pulley axis routed to the active lane (`nt::SelectLane`, lane = idler slot), virtual selector/idler with synthesized end-stops,
  HUB = FINDA, EEPROM image in RAM persisted to flash sector 3 when idle, IWDG (~8 s). CMSIS from `third_party/cmsis`.
- TMC2209 per-lane config over bit-banged single-wire UART (`tmc_uart.cpp`, 19200 baud): GCONF/CHOPCONF (with dedge)/IHOLD_IRUN,
  link verified via IFCNT.
- Not done (needs hardware/info): printer-link USART pins, Box Turtle steps/mm calibration, clocks (reset HSI 64 MHz),
  LEDs, bootloader offset. Untested on hardware; flash erase stalls the CPU ~1-2 s (see hal_afcpro.cpp).
Hardware mapping: `docs/afc-pro-port.md`.
