/// @file stm32/gpio.h
/// hal::gpio API (see ../gpio.h) implemented on STM32 GPIO registers (Ninja Turtle AFC-Pro port).
#pragma once
#include <inttypes.h>
#include "stm32h7xx.h"
#undef CRC // CMSIS peripheral macro clashes with modules::protocol CRC() (Buddy does the same)

#define ATOMIC_BLOCK(x) // single 32-bit BSRR writes are atomic on STM32

namespace hal {
namespace gpio {

using GPIO_TypeDef = ::GPIO_TypeDef;

enum class Mode : uint8_t {
    input = 0,
    output,
};

enum class Pull : uint8_t {
    none = 0,
    up,
    down,
};

enum class Level : uint8_t {
    low = 0,
    high,
};

struct GPIO_InitTypeDef {
    Mode mode;
    Pull pull;
    Level level;
    inline GPIO_InitTypeDef(Mode mode, Pull pull)
        : mode(mode)
        , pull(pull) {};
    inline GPIO_InitTypeDef(Mode mode, Level level)
        : mode(mode)
        , level(level) {};
};

struct GPIO_pin {
    GPIO_TypeDef *const port; ///< nullptr = not connected (virtual axes); writes ignored, reads low
    const uint16_t pin_mask;
};

/// Sentinel port for the pulley STEP pin: the MMU3 pulley drives whichever Box Turtle lane is active, so writes to this
/// pseudo-port are routed to *nt::pulleyStep (set by nt::SelectLane).
#define NT_PULLEY_MUX_PORT ((hal::gpio::GPIO_TypeDef *)uintptr_t(1))

} // namespace gpio
} // namespace hal
namespace nt {
extern const hal::gpio::GPIO_pin *volatile pulleyStep;
}
namespace hal {
namespace gpio {

__attribute__((always_inline)) inline const GPIO_pin &Route(const GPIO_pin &p) {
    return p.port == NT_PULLEY_MUX_PORT ? *nt::pulleyStep : p;
}

__attribute__((always_inline)) inline void WritePin(const GPIO_pin pin, Level level) {
    const GPIO_pin &p = Route(pin);
    if (p.port)
        p.port->BSRR = level == Level::high ? uint32_t(p.pin_mask) : uint32_t(p.pin_mask) << 16;
}

__attribute__((always_inline)) inline Level ReadPin(const GPIO_pin pin) {
    const GPIO_pin &p = Route(pin);
    return (Level)(p.port && (p.port->IDR & p.pin_mask) != 0);
}

__attribute__((always_inline)) inline void TogglePin(const GPIO_pin pin) {
    const GPIO_pin &p = Route(pin);
    if (p.port)
        WritePin(p, (Level)((p.port->ODR & p.pin_mask) == 0));
}

__attribute__((always_inline)) inline void Init(const GPIO_pin p, GPIO_InitTypeDef init) {
    if (!p.port || p.port == NT_PULLEY_MUX_PORT)
        return;
    const uint32_t n = __builtin_ctz(p.pin_mask);
    if (init.mode == Mode::output) {
        WritePin(p, init.level);
        p.port->PUPDR &= ~(3u << (2 * n));
    } else {
        uint32_t pull = init.pull == Pull::up ? 1u : init.pull == Pull::down ? 2u : 0u;
        p.port->PUPDR = (p.port->PUPDR & ~(3u << (2 * n))) | (pull << (2 * n));
    }
    p.port->MODER = (p.port->MODER & ~(3u << (2 * n))) | ((init.mode == Mode::output ? 1u : 0u) << (2 * n));
}

} // namespace gpio
} // namespace hal
