/// @file nt/virtual_axes.h
/// Ninja Turtle: a Box Turtle has no selector or idler mechanics. Their axes stay in the upstream motion/logic code as
/// "virtual" axes; this synthesizes the StallGuard end-stops homing expects, at the travel distances a real MMU3 has
/// (same distances as the upstream unit-test homing simulation), so MovableBase homing completes normally.
#pragma once
#include <stdint.h>

namespace nt {
/// Call once per step-ISR pass for the selector and idler axes; @returns true when that axis should report a stall now.
bool VirtualAxisStall(uint8_t axis);
} // namespace nt
