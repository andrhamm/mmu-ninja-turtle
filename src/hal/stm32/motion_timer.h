/// @file stm32/motion_timer.h
/// Step-generation timer for the upstream motion module (Ninja Turtle STM32 port). The timer ISR calls
/// modules::motion::motion.Step() and reprograms itself with the returned interval (same contract as AVR TIMER1).
#pragma once
#include <stdint.h>

namespace hal {
namespace stm32 {
void StepTimerInit();
void StepTimerIrqEnable(bool on);
/// Disable the IRQ; @returns previous enabled state
bool StepTimerIrqDisable();
} // namespace stm32
} // namespace hal
