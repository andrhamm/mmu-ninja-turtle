/// @file nt/virtual_axes.cpp
#include "virtual_axes.h"
#include "../modules/idler.h"
#include "../modules/motion.h"
#include "../modules/selector.h"

namespace nt {
namespace {
struct Track {
    uint8_t lastState = 0xff;
    mm::pos_t start = 0;
    bool fired = false;
};
Track tracks[mm::NUM_AXIS];
int32_t Abs(int32_t v) { return v < 0 ? -v : v; }
} // namespace

bool VirtualAxisStall(uint8_t axis) {
    uint8_t state;
    uint32_t fwd, back;
    if (axis == mm::Idler) {
        state = mi::idler.State();
        fwd = mm::unitToSteps<mm::I_pos_t>(config::idlerLimits.lenght - 5.0_deg);
        back = mm::unitToSteps<mm::I_pos_t>(config::idlerLimits.lenght);
    } else if (axis == mm::Selector) {
        state = ms::selector.State();
        fwd = 5;
        back = mm::unitToSteps<mm::S_pos_t>(config::selectorLimits.lenght) + 1;
    } else {
        return false;
    }
    Track &t = tracks[axis];
    mm::pos_t pos = mm::motion.CurPosition((mm::Axis)axis);
    if (state != t.lastState) {
        t.lastState = state;
        t.start = pos;
        t.fired = false;
    }
    uint32_t need = state == mm::MovableBase::HomeForward ? fwd : state == mm::MovableBase::HomeBack ? back : 0;
    if (need && !t.fired && uint32_t(Abs(pos - t.start)) >= need) {
        t.fired = true;
        return true;
    }
    return false;
}
} // namespace nt
