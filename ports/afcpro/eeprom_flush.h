#pragma once
namespace hal {
namespace eeprom {
/// Persist the RAM EEPROM image to flash if it changed >2 s ago and @p idle; @returns true if it wrote.
bool FlushIfDue(bool idle);
} // namespace eeprom
} // namespace hal
