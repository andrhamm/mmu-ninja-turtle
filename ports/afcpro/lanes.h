#pragma once
#include <stdint.h>
namespace nt {
constexpr uint8_t noLane = 0xff;
/// Route the MMU3 pulley axis (STEP/DIR/EN) to Box Turtle lane `lane` (noLane = none).
void SelectLane(uint8_t lane);
uint8_t ActiveLane();
} // namespace nt
