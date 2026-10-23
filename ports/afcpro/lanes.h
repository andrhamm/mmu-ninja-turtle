#pragma once
#include <stdint.h>
namespace nt {
constexpr uint8_t noLane = 0xff;
/// Lane the pulley should drive now: the idler's slot while engaged or partially disengaged, else noLane.
uint8_t DesiredLane();
/// Route the MMU3 pulley axis (STEP/DIR/EN) to DesiredLane() (main-loop safe; masks the step ISR briefly).
void UpdateLane();
uint8_t ActiveLane();
} // namespace nt
