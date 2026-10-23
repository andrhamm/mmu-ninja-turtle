#pragma once
#include <stdint.h>
#include <string>

namespace vmmu {
/// Open a serial device at 115200 8N1 raw, "pty" (create a pty pair) or "tcp:host:port"; returns fd or -1.
int OpenPort(const std::string &path, std::string &openedName);
/// Move available bytes from the fd into the firmware's RX queue; returns false if the port closed.
bool PumpRx(int timeoutMs);
/// JSONL log of every frame (rx/tx) with timestamps and reply latency; nullptr disables.
void SetLog(FILE *f);
uint64_t NowUs();
} // namespace vmmu
