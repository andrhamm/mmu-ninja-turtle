// Minimal world model so the unmodified MMU logic can run without hardware:
// - homing: stall-guard from src/nt/virtual_axes.cpp (same travel as upstream tests/unit/logic/stubs/homing.cpp)
// - filament: each slot's tip moves with the pulley while the idler presses that slot; FINDA trips past findaTripMm
#include "physics.h"
#include "idler.h"
#include "motion.h"
#include "selector.h"
#include "stub_motion.h"
#include "../../src/nt/virtual_axes.h"
#include "../../src/pins.h"
#include "../../src/hal/gpio.h"

#include <stdlib.h>
#include <string.h>

namespace vmmu {
namespace physics {

Faults faults;
Changer changer = Changer::MMU3;

bool SetChanger(const char *n) {
    if (!strcmp(n, "mmu3"))
        changer = Changer::MMU3;
    else if (!strcmp(n, "boxturtle"))
        changer = Changer::BoxTurtle;
    else
        return false;
    return true;
}

bool SetFault(const char *arg) {
    char n[64];
    strncpy(n, arg, sizeof(n) - 1);
    n[sizeof(n) - 1] = 0;
    if (char *c = strchr(n, ':')) { // name:ms -> fault clears after ms (lets tests exercise error -> retry -> success)
        *c = 0;
        faults.untilMs = atoi(c + 1);
    }
    if (!strncmp(n, "reply-delay=", 12))
        return (faults.replyDelayMs = atoi(n + 12)) > 0;
    if (!strcmp(n, "finda-stuck-off"))
        return faults.findaStuckOff = true;
    if (!strcmp(n, "finda-stuck-on"))
        return faults.findaStuckOn = true;
    if (!strcmp(n, "idler-home-fail"))
        return faults.idlerHomeFail = true;
    if (!strcmp(n, "selector-home-fail"))
        return faults.selectorHomeFail = true;
    return false;
}

namespace {

bool stallPending[mm::NUM_AXIS] = {};
mm::pos_t lastPulley = 0;
bool pulleyInit = false;
long double tipSteps[config::toolCount] = {};

/// Homing end-stops come from the same synthesizer the AFC-Pro firmware uses (src/nt/virtual_axes.cpp).
void SimHoming(mm::Axis axis, bool broken) {
    if (stallPending[axis]) {
        mm::motion.StallGuardReset(axis);
        stallPending[axis] = false;
    }
    if (nt::VirtualAxisStall(axis) && !broken) {
        mm::TriggerStallGuard(axis);
        stallPending[axis] = true;
    }
}

} // namespace

void Step() {
    static uint32_t simMs = 0;
    if (faults.untilMs && ++simMs >= faults.untilMs)
        faults = Faults {};
    SimHoming(mm::Idler, faults.idlerHomeFail);
    SimHoming(mm::Selector, faults.selectorHomeFail);

    mm::pos_t p = mm::axes[mm::Pulley].pos;
    if (!pulleyInit) {
        lastPulley = p;
        pulleyInit = true;
    }
    uint8_t s = mi::idler.Slot();
    if (mi::idler.Engaged() && s < config::toolCount)
        tipSteps[s] += p - lastPulley;
    lastPulley = p;

    uint8_t sel = ms::selector.Slot();
    bool finda = sel < config::toolCount && tipSteps[sel] >= findaTripMm * config::pulley.stepsPerUnit;
    finda = (finda || faults.findaStuckOn) && !faults.findaStuckOff;
    hal::gpio::WritePin(FINDA_PIN, finda ? hal::gpio::Level::high : hal::gpio::Level::low);
}

long double TipMm(uint8_t slot) {
    return slot < config::toolCount ? tipSteps[slot] / config::pulley.stepsPerUnit : 0;
}

} // namespace physics
} // namespace vmmu

/// Box Turtle has no cutter at the changer: Cut requests are rejected (overrides the weak default in src/application.cpp).
bool HasCutter() { return vmmu::physics::changer != vmmu::physics::Changer::BoxTurtle; }
