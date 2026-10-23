#pragma once
#include <stdint.h>

namespace vmmu {
namespace physics {
/// Distance past the pulley at which the simulated FINDA trips.
constexpr long double findaTripMm = 20.0;
/// Injectable faults (vmmu --fault <name>), for exercising the printer's error/recovery paths.
struct Faults {
    bool findaStuckOff = false; ///< FINDA never triggers -> load/tool-change errors
    bool findaStuckOn = false; ///< FINDA never releases -> unload errors
    bool idlerHomeFail = false; ///< idler stall-guard never fires
    bool selectorHomeFail = false; ///< selector stall-guard never fires
    uint32_t untilMs = 0; ///< faults auto-clear at this simulated time (0 = never), e.g. --fault finda-stuck-off:8000
};
extern Faults faults;
/// Parse one --fault name[:ms]; returns false if unknown.
bool SetFault(const char *name);

/// Advance the simulated world by one millisecond: stall-guard on homing ends, filament tips, FINDA pin.
void Step();
/// Filament tip position of a slot in mm relative to the pulley (for logs/tests).
long double TipMm(uint8_t slot);
} // namespace physics
} // namespace vmmu
