/// @file nt/tmc2209.cpp
#include "tmc2209.h"

namespace nt {
namespace tmc2209 {

uint8_t Crc8(const uint8_t *data, uint8_t len) { // CRC8-ATM, LSB-first per byte (TMC datasheet / Klipper)
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; ++i) {
        uint8_t b = data[i];
        for (uint8_t j = 0; j < 8; ++j) {
            crc = ((crc >> 7) ^ (b & 1)) ? uint8_t((crc << 1) ^ 0x07) : uint8_t(crc << 1);
            b >>= 1;
        }
    }
    return crc;
}

void EncodeWrite(uint8_t addr, uint8_t reg, uint32_t v, uint8_t (&o)[8]) {
    o[0] = sync;
    o[1] = addr;
    o[2] = reg | 0x80;
    o[3] = v >> 24;
    o[4] = v >> 16;
    o[5] = v >> 8;
    o[6] = v;
    o[7] = Crc8(o, 7);
}

void EncodeRead(uint8_t addr, uint8_t reg, uint8_t (&o)[4]) {
    o[0] = sync;
    o[1] = addr;
    o[2] = reg & 0x7F;
    o[3] = Crc8(o, 3);
}

bool DecodeReply(uint8_t reg, const uint8_t (&in)[8], uint32_t &value) {
    if (in[0] != sync || in[1] != masterAddr || in[2] != (reg & 0x7F) || in[7] != Crc8(in, 7))
        return false;
    value = (uint32_t(in[3]) << 24) | (uint32_t(in[4]) << 16) | (uint32_t(in[5]) << 8) | in[6];
    return true;
}

} // namespace tmc2209
} // namespace nt
