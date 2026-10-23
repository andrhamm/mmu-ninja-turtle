#pragma once
#include <stdint.h>
namespace nt {
void TmcUartInit();
void TmcWrite(uint8_t lane, uint8_t reg, uint32_t value);
bool TmcRead(uint8_t lane, uint8_t reg, uint32_t &value);
} // namespace nt
