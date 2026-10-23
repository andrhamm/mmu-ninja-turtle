// vmmu — the unmodified MMU application + logic on the host, speaking MMU3 protocol on a serial port or pty.
// Usage: vmmu [--port <device> | pty | tcp:host:port] [--log frames.jsonl] [--changer mmu3|boxturtle] [--fault <name>]...
#include "physics.h"
#include "posix_serial.h"

#include "application.h"
#include "buttons.h"
#include "finda.h"
#include "fsensor.h"
#include "globals.h"
#include "idler.h"
#include "leds.h"
#include "motion.h"
#include "pulley.h"
#include "selector.h"
#include "user_input.h"
#include "../../src/logic/start_up.h"
#include "../../src/pins.h"
#include "../../src/hal/gpio.h"
#include "stub_adc.h"
#include "stub_eeprom.h"
#include "stub_motion.h"
#include "stub_timebase.h"

#include <stdio.h>
#include <string.h>
#include <string>

static void StepModules() {
    vmmu::physics::Step();
    mb::buttons.Step();
    ml::leds.Step();
    mf::finda.Step();
    mfs::fsensor.Step();
    mi::idler.Step();
    ms::selector.Step();
    mpu::pulley.Step();
    mm::motion.Step();
    mui::userInput.Step();
    mt::IncMillis();
}

int main(int argc, char **argv) {
    std::string port = "pty", logPath;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--port") && i + 1 < argc)
            port = argv[++i];
        else if (!strcmp(argv[i], "--log") && i + 1 < argc)
            logPath = argv[++i];
        else if (!strcmp(argv[i], "--fault") && i + 1 < argc && vmmu::physics::SetFault(argv[i + 1]))
            ++i;
        else if (!strcmp(argv[i], "--changer") && i + 1 < argc && vmmu::physics::SetChanger(argv[i + 1]))
            ++i;
        else {
            fprintf(stderr, "usage: %s [--port <device>|pty|tcp:host:port] [--log <file.jsonl>] [--changer mmu3|boxturtle]\n"
                            "          [--fault finda-stuck-off|finda-stuck-on|idler-home-fail|selector-home-fail|reply-delay=MS[:ms]]...\n", argv[0]);
            return 2;
        }
    }
    std::string opened;
    if (vmmu::OpenPort(port, opened) < 0) {
        perror("open port");
        return 1;
    }
    FILE *logf = logPath.empty() ? nullptr : fopen(logPath.c_str(), "a");
    vmmu::SetLog(logf);
    printf("vmmu %d.%d.%d (%s) on %s\n", PROJECT_VERSION_MAJOR, PROJECT_VERSION_MINOR, PROJECT_VERSION_REV,
        vmmu::physics::changer == vmmu::physics::Changer::BoxTurtle ? "boxturtle" : "mmu3", opened.c_str());
    fflush(stdout);

    // "power on": same order as setup()/setup2() in src/main.cpp, with stubs for hardware
    hal::eeprom::ClearEEPROM();
    hal::adc::ReinitADC(config::buttonsADCIndex, hal::adc::TADCData({ 1023 }), 1); // no buttons pressed
    hal::gpio::WritePin(FINDA_PIN, hal::gpio::Level::low);
    mt::ReinitTimebase();
    mm::ReinitMotion();
    mg::globals.Init();
    logic::startUp.Reset(0);
    mi::idler.Init();
    ms::selector.Init();

    const uint64_t t0 = vmmu::NowUs();
    uint64_t simMs = 0;
    for (;;) {
        if (!vmmu::PumpRx(1)) // wakes immediately on RX, else every 1 ms
            break;
        application.Step();
        uint64_t realMs = (vmmu::NowUs() - t0) / 1000;
        if (realMs - simMs > 1000) // host was suspended: don't replay a backlog of time
            simMs = realMs - 1;
        while (simMs < realMs) {
            StepModules();
            application.Step();
            ++simMs;
        }
    }
    fprintf(stderr, "port closed\n");
    return 0;
}
