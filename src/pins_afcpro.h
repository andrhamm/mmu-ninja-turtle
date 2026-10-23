/// @file pins_afcpro.h
/// Ninja Turtle pin map: Box Turtle on Isik's Tech AFC-Pro (STM32H723). Source: AFC-Klipper-Add-On config/mcu/AFC_Pro.cfg.
/// MMU3 names are kept so the upstream modules compile unchanged; see docs/afc-pro-port.md for the mapping.
#pragma once
#include "hal/gpio.h"

namespace nt {
static const hal::gpio::GPIO_pin NC = { nullptr, 0 }; ///< not connected
}

// --- printer link (MMU3 protocol, 115200 8N1, 3.3 V logic) on USART1 (AF7) via two limit-switch ports ---
// TX = PB6 (port SW15 / EXT6), RX = PB7 (port SW14 / EXT5). Switch ports wire straight to the MCU (AFC-Pro RC3 schematic p.2);
// pin 1 of those XH-3A ports is 3V3 — never connect it to the printer. AF mapping per Klipper src/stm32/serial.c
// (USART1 alt PB7/PB6); verify against the STM32H723 datasheet before wiring.
static const hal::gpio::GPIO_pin USART_TX = { GPIOB, 1u << 6 };
static const hal::gpio::GPIO_pin USART_RX = { GPIOB, 1u << 7 };

// --- MMU3 hardware that does not exist on a Box Turtle ---
static const hal::gpio::GPIO_pin TMC2130_SPI_MISO_PIN = nt::NC;
static const hal::gpio::GPIO_pin TMC2130_SPI_MOSI_PIN = nt::NC;
static const hal::gpio::GPIO_pin TMC2130_SPI_SCK_PIN = nt::NC;
static const hal::gpio::GPIO_pin TMC2130_SPI_SS_PIN = nt::NC;
static const hal::gpio::GPIO_pin SHR16_DATA = nt::NC;
static const hal::gpio::GPIO_pin SHR16_LATCH = nt::NC;
static const hal::gpio::GPIO_pin SHR16_CLOCK = nt::NC;
static const hal::gpio::GPIO_pin SELECTOR_CS_PIN = nt::NC; // selector is virtual
static const hal::gpio::GPIO_pin SELECTOR_SG_PIN = nt::NC;
static const hal::gpio::GPIO_pin SELECTOR_STEP_PIN = nt::NC;
static const hal::gpio::GPIO_pin IDLER_CS_PIN = nt::NC; // idler is virtual (engage = lane driver enable)
static const hal::gpio::GPIO_pin IDLER_SG_PIN = nt::NC;
static const hal::gpio::GPIO_pin IDLER_STEP_PIN = nt::NC;

// --- pulley = active lane (routed per lane by the TMC lane driver) ---
static const hal::gpio::GPIO_pin PULLEY_CS_PIN = nt::NC;
static const hal::gpio::GPIO_pin PULLEY_SG_PIN = nt::NC;
static const hal::gpio::GPIO_pin PULLEY_STEP_PIN = { NT_PULLEY_MUX_PORT, 0 }; // routed to the active lane

// --- FINDA = Box Turtle hub sensor ---
static const hal::gpio::GPIO_pin FINDA_PIN = { GPIOD, 1u << 0 }; // HUB / SW1

// --- lanes 1..5 (index 0..4) ---
namespace nt {
constexpr uint8_t laneCount = 5;
static const hal::gpio::GPIO_pin LANE_STEP[laneCount] = { { GPIOE, 1u << 7 }, { GPIOB, 1u << 2 }, { GPIOA, 1u << 0 }, { GPIOF, 1u << 9 }, { GPIOF, 1u << 5 } };
static const hal::gpio::GPIO_pin LANE_DIR[laneCount] = { { GPIOG, 1u << 1 }, { GPIOA, 1u << 3 }, { GPIOC, 1u << 1 }, { GPIOF, 1u << 8 }, { GPIOF, 1u << 4 } };
static const hal::gpio::GPIO_pin LANE_EN[laneCount] = { { GPIOE, 1u << 10 }, { GPIOG, 1u << 0 }, { GPIOA, 1u << 2 }, { GPIOC, 1u << 0 }, { GPIOF, 1u << 7 } };
static const hal::gpio::GPIO_pin LANE_UART[laneCount] = { { GPIOE, 1u << 8 }, { GPIOF, 1u << 15 }, { GPIOA, 1u << 1 }, { GPIOF, 1u << 10 }, { GPIOF, 1u << 6 } };
static const hal::gpio::GPIO_pin LANE_PREP[laneCount] = { { GPIOD, 1u << 1 }, { GPIOD, 1u << 4 }, { GPIOD, 1u << 6 }, { GPIOG, 1u << 9 }, { GPIOG, 1u << 11 } }; // TRGn
static const hal::gpio::GPIO_pin LANE_EXIT[laneCount] = { { GPIOD, 1u << 7 }, { GPIOG, 1u << 10 }, { GPIOG, 1u << 12 }, { GPIOG, 1u << 13 }, { GPIOB, 1u << 5 } }; // EXT1-4, lane 5 on EXT7 (EXT5 = USART RX)
static const hal::gpio::GPIO_pin REWIND_RWD[laneCount] = { { GPIOE, 1u << 9 }, { GPIOE, 1u << 13 }, { GPIOB, 1u << 10 }, { GPIOB, 1u << 14 }, { GPIOD, 1u << 12 } };
static const hal::gpio::GPIO_pin REWIND_FWD[laneCount] = { { GPIOE, 1u << 11 }, { GPIOE, 1u << 14 }, { GPIOB, 1u << 11 }, { GPIOB, 1u << 15 }, { GPIOD, 1u << 13 } };
static const hal::gpio::GPIO_pin REWIND_EN[laneCount] = { { GPIOE, 1u << 15 }, { GPIOD, 1u << 9 }, { GPIOB, 1u << 13 }, { GPIOD, 1u << 11 }, { GPIOG, 1u << 3 } };
static const hal::gpio::GPIO_pin TN_ADVANCE = { GPIOB, 1u << 3 }; // TurtleNeck buffer
static const hal::gpio::GPIO_pin TN_TRAILING = { GPIOG, 1u << 15 };
} // namespace nt
