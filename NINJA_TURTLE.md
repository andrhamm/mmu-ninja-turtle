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
- `tests/unit/modules/protocol/test_protocol.cpp`: test helper read past the end of a `string_view` (caught by GCC 16 assertions).

## vmmu (virtual MMU)
Unmodified MMU application + logic on the host, speaking MMU3 protocol on a serial device or a pty;
motors/sensors/EEPROM simulated (`tools/vmmu/physics.cpp`: stall-guard homing, per-slot filament tips, FINDA).

    build/linux/tools/vmmu/vmmu --port pty --log frames.jsonl    # prints the pty path; also --port <device> | tcp:host:port
    python3 -I tools/vmmu/smoke.py /dev/ttysNNN [load]

Motion runs at planned feed rates (`tools/vmmu/motion_sim.cpp`, constant velocity): L1 incl. first homing ≈ 9 s.
Faults: `--fault finda-stuck-off|finda-stuck-on|idler-home-fail|selector-home-fail[:ms]` (`:ms` = auto-clear).

## vprinter (virtual printer)
Prusa Buddy's unmodified printer-side protocol state machine (`third_party/buddy`, v6.10.1) driving an MMU over
pty/serial/TCP, with a simulated printer filament sensor. Reports reply latency (the CORE One extension board allows 10 ms).

    tools/vprinter/loop_test.sh build/linux/tools/vmmu/vmmu build/linux/tools/vprinter/vprinter --scenario tc:5 --gate-ms 10
    # scenarios: lineup | idle:SECS | tc:N | tc-retry (with VMMU_ARGS="--fault finda-stuck-off:6000")

Not yet modelled: the extension board's MODBUS hop (late replies dropped -> retried) and register-write whitelist.
