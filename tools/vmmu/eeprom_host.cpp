// In-memory EEPROM for vmmu (same layout as tests/unit/modules/stubs/stub_eeprom.cpp, without Catch2).
#include "../../src/hal/eeprom.h"
#include "stub_eeprom.h"

#include <algorithm>
#include <array>
#include <assert.h>
#include <string.h>

namespace hal {
namespace eeprom {

EEPROM eeprom;
static std::array<uint8_t, 2048> EE;

void ClearEEPROM() { std::fill(EE.begin(), EE.end(), 0xff); }

void EEPROM::WriteByte(addr_t offset, uint8_t value) {
    assert(offset < EE.size());
    EE[offset] = value;
}
void EEPROM::UpdateByte(addr_t offset, uint8_t value) { WriteByte(offset, value); }
uint8_t EEPROM::ReadByte(addr_t offset) {
    assert(offset < EE.size());
    return EE[offset];
}
uint8_t EEPROM::ReadByte(addr_t offset, uint8_t defaultValue) {
    uint8_t v = ReadByte(offset);
    return v == 0xff ? defaultValue : v;
}
void EEPROM::WriteWord(addr_t offset, uint16_t value) {
    assert(offset + 1u < EE.size());
    memcpy(&EE[offset], &value, 2);
}
void EEPROM::UpdateWord(addr_t offset, uint16_t value) { WriteWord(offset, value); }
uint16_t EEPROM::ReadWord(addr_t offset) {
    assert(offset + 1u < EE.size());
    uint16_t v;
    memcpy(&v, &EE[offset], 2);
    return v;
}

} // namespace eeprom
} // namespace hal
