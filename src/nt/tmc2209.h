/// @file nt/tmc2209.h
/// TMC2209 single-wire UART datagrams and register values (Ninja Turtle: Box Turtle lane drivers).
/// Format and CRC follow Klipper klippy/extras/tmc_uart.py; bit fields follow klippy/extras/tmc2208.py.
#pragma once
#include <stdint.h>

namespace nt {
namespace tmc2209 {

enum Reg : uint8_t {
    GCONF = 0x00,
    GSTAT = 0x01,
    IFCNT = 0x02,
    IHOLD_IRUN = 0x10,
    TPOWERDOWN = 0x11,
    TPWMTHRS = 0x13,
    CHOPCONF = 0x6C,
    DRV_STATUS = 0x6F,
    PWMCONF = 0x70,
};

constexpr uint8_t sync = 0x05;
constexpr uint8_t masterAddr = 0xFF;

uint8_t Crc8(const uint8_t *data, uint8_t len);
/// 8-byte write datagram (register address gets the write bit 0x80)
void EncodeWrite(uint8_t addr, uint8_t reg, uint32_t value, uint8_t (&out)[8]);
/// 4-byte read request
void EncodeRead(uint8_t addr, uint8_t reg, uint8_t (&out)[4]);
/// Validate an 8-byte read reply for @p reg; @returns false on bad sync/address/register/CRC
bool DecodeReply(uint8_t reg, const uint8_t (&in)[8], uint32_t &value);

/// GCONF for UART-controlled operation: pdn_disable (UART owns PDN), mstep_reg_select (MRES from CHOPCONF),
/// multistep_filt; en_spreadcycle selects SpreadCycle instead of StealthChop.
constexpr uint32_t Gconf(bool spreadCycle) {
    return (spreadCycle ? 1u << 2 : 0u) | (1u << 6) | (1u << 7) | (1u << 8);
}

/// CHOPCONF: TMC defaults (toff 3, hstrt 5, hend 0, tbl 2, intpol) + mres + vsense + dedge.
/// dedge is required: the MMU step code toggles STEP once per microstep (tmc2130.h Step "assumes DEDGE").
/// @param mres 0 = 256 microsteps ... 8 = full step (same encoding as config::MRes)
constexpr uint32_t Chopconf(uint8_t mres, bool vsense) {
    return 3u | (5u << 4) | (0u << 7) | (2u << 15) | (vsense ? 1u << 17 : 0u) | (uint32_t(mres & 0xF) << 24) | (1u << 28) | (1u << 29);
}

constexpr uint32_t IholdIrun(uint8_t ihold, uint8_t irun, uint8_t iholdDelay = 8) {
    return (ihold & 0x1Fu) | (uint32_t(irun & 0x1F) << 8) | (uint32_t(iholdDelay & 0xF) << 16);
}

} // namespace tmc2209
} // namespace nt
