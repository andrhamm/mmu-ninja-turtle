// Minimal world model so the unmodified MMU logic can run without hardware:
// - homing: stall-guard fires after the same travel the upstream unit tests use (tests/unit/logic/stubs/homing.cpp)
// - filament: each slot's tip moves with the pulley while the idler presses that slot; FINDA trips past findaTripMm
#include "physics.h"
#include "idler.h"
#include "motion.h"
#include "selector.h"
#include "stub_motion.h"
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

struct HomeSim {
    uint8_t lastState = 0xff;
    mm::pos_t start = 0;
    bool fired = false;
    bool resetPending = false;
};

HomeSim idlerHome, selectorHome;
mm::pos_t lastPulley = 0;
bool pulleyInit = false;
long double tipSteps[config::toolCount] = {};

int64_t Abs(int64_t v) { return v < 0 ? -v : v; }

void SimHoming(HomeSim &h, mm::Axis axis, uint8_t state, uint32_t fwdSteps, uint32_t backSteps, bool broken) {
    if (h.resetPending) {
        mm::motion.StallGuardReset(axis);
        h.resetPending = false;
    }
    if (state != h.lastState) {
        h.lastState = state;
        h.start = mm::axes[axis].pos;
        h.fired = false;
    }
    uint32_t need = state == mm::MovableBase::HomeForward ? fwdSteps : state == mm::MovableBase::HomeBack ? backSteps : 0;
    if (need && !broken && !h.fired && Abs(int64_t(mm::axes[axis].pos) - h.start) >= need) {
        mm::TriggerStallGuard(axis);
        h.fired = true;
        h.resetPending = true;
    }
}

} // namespace

void Step() {
    static uint32_t simMs = 0;
    if (faults.untilMs && ++simMs >= faults.untilMs)
        faults = Faults {};
    SimHoming(idlerHome, mm::Idler, mi::idler.State(),
        mm::unitToSteps<mm::I_pos_t>(config::idlerLimits.lenght - 5.0_deg),
        mm::unitToSteps<mm::I_pos_t>(config::idlerLimits.lenght), faults.idlerHomeFail);
    SimHoming(selectorHome, mm::Selector, ms::selector.State(),
        5,
        mm::unitToSteps<mm::S_pos_t>(config::selectorLimits.lenght) + 1, faults.selectorHomeFail);

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
