#include "motion.h"
#include "idler.h"
#include "stub_motion.h"
#include "physics.h"

#include <deque>

// vmmu copy of tests/unit/logic/stubs/stub_motion.cpp: identical API, but Step() is called once per simulated
// millisecond and advances each axis at its planned feed rate (constant velocity, no acceleration ramps),
// so homing/feeds take roughly real-world time. The upstream stub (1 step per call) stays untouched for unit tests.
namespace {
std::deque<modules::motion::steps_t> feeds[3]; // parallel to AxisSim::plannedMoves
long double frac[3] = {}; // sub-step carry
} // namespace

namespace modules {
namespace motion {

Motion motion;

// Intentionally inited with strange values
// Need to call ReinitMotion() each time we start some unit test
AxisSim axes[3] = {
    { -32767, false, false, false, config::pulley.sg_thrs, {} }, // pulley
    { -32767, false, false, false, config::selector.sg_thrs, {} }, // selector //@@TODO proper selector positions once defined
    { -32767, false, false, false, config::idler.sg_thrs, {} }, // idler
};

bool Motion::InitAxis(Axis axis) {
    SetEnabled(axis, true);
    return true;
}

bool Motion::InitAxis(Axis axis, MotorCurrents /*mc*/) {
    SetEnabled(axis, true);
    return true;
}

void Motion::SetEnabled(Axis axis, bool enabled) {
    axisData[axis].enabled = axes[axis].enabled = enabled;
}

bool Motion::StallGuard(Axis axis) {
    return axes[axis].stallGuard;
}

void Motion::StallGuardReset(Axis axis) {
    axes[axis].stallGuard = false;
}

void TriggerStallGuard(Axis axis) {
    axes[axis].stallGuard = true;
}

void Motion::PlanStallGuardThreshold(Axis axis, int8_t sg_thrs) {
    // do nothing for now
    axes[axis].sg_thrs = sg_thrs;
}

void Motion::PlanMoveTo(Axis axis, pos_t pos, steps_t feed_rate, steps_t end_rate) {
    axes[axis].plannedMoves.push_back(pos);
    feeds[axis].push_back(feed_rate ? feed_rate : 1);
    if (!axisData[axis].enabled)
        SetEnabled(axis, true);
}

pos_t Motion::Position(Axis axis) const {
    return axes[axis].pos;
}

pos_t Motion::CurPosition(Axis axis) const {
    return axes[axis].pos;
}

void Motion::SetPosition(Axis axis, pos_t x) {
    axes[axis].pos = x;
    axisData[axis].ctrl.SetPosition(axes[axis].pos);
}

void Motion::SetMode(Axis axis, hal::tmc2130::MotorMode mode) {
}

void Motion::SetMode(MotorMode mode) {
}

st_timer_t Motion::Step() {
    for (uint8_t i = 0; i < 3; ++i) {
        bool virtualAxis = vmmu::physics::changer == vmmu::physics::Changer::BoxTurtle && i != Pulley; // no selector/idler hardware
        frac[i] += axes[i].plannedMoves.empty() ? 0 : feeds[i].front() * (virtualAxis ? 20 : 1) / 1000.0L; // steps this ms; virtual axes 20x (instant would skip the homing stall)
        while (!axes[i].plannedMoves.empty()) {
            pos_t target = axes[i].plannedMoves.front();
            if (axes[i].pos == target) {
                axes[i].plannedMoves.pop_front(); // one move completed, continue with the next one
                feeds[i].pop_front();
                continue;
            }
            if (frac[i] < 1)
                break;
            pos_t n = (pos_t)frac[i];
            pos_t dist = target > axes[i].pos ? target - axes[i].pos : axes[i].pos - target;
            if (n > dist)
                n = dist;
            axes[i].pos += target > axes[i].pos ? n : -n;
            frac[i] -= n;
            axisData[i].ctrl.SetPosition(axes[i].pos);
            if (axes[i].stallGuard) // homing end-stop hit: physics only arms this at the right distance
                break;
        }
        if (axes[i].plannedMoves.empty())
            frac[i] = 0;
    }
    return 0;
}

bool Motion::QueueEmpty() const {
    for (uint8_t i = 0; i < 3; ++i) {
        if (!axes[i].plannedMoves.empty())
            return false;
    }
    return true;
}

bool Motion::QueueEmpty(Axis axis) const {
    return axes[axis].plannedMoves.empty();
}

uint8_t Motion::PlannedMoves(Axis axis) const {
    return axes[axis].plannedMoves.size();
}

void Motion::AbortPlannedMoves(bool halt) {
    for (uint8_t i = 0; i < 3; ++i) {
        AbortPlannedMoves((config::Axis)i, halt); // leave the axis where it was at the time of abort
    }
}

void Motion::AbortPlannedMoves(config::Axis i, bool) {
    axes[i].plannedMoves.clear(); // leave the axis where it was at the time of abort
    feeds[i].clear();
    frac[i] = 0;
    axisData[i].ctrl.SetPosition(axes[i].pos);
}

void ReinitMotion() {
    for (auto &f : feeds)
        f.clear();
    for (auto &f : frac)
        f = 0;
    // reset the simulation data to defaults
    axes[0] = AxisSim({ 0, false, false, false, {} }); // pulley
    axes[1] = AxisSim({ unitToSteps<S_pos_t>(config::selectorSlotPositions[0]),
        false, false, false, {} }); // selector
    axes[2] = AxisSim({ unitToSteps<I_pos_t>(config::idlerSlotPositions[mi::Idler::IdleSlotIndex()]),
        false, false, false, {} }); // idler
}

bool PulleyEnabled() {
    return axes[0].enabled;
}

pos_t AxisNearestTargetPos(Axis axis) {
    if (axes[axis].plannedMoves.empty()) {
        return axes[axis].pos;
    } else {
        return axes[axis].plannedMoves.front();
    }
}

/// probably higher-level operations knowing the semantic meaning of axes

} // namespace motion
} // namespace modules
