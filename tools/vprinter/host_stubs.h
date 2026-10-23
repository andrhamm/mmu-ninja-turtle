#pragma once
#include "mmu2_fsensor.h"
#include <algorithm>
#include <stdint.h>
#include <string>
#include <vector>

namespace vprinter {
/// Reply latency (request written -> first reply byte), the quantity the CORE One extension board limits to 10 ms.
struct Latency {
    std::vector<uint32_t> us;
    void Record(uint64_t v) { us.push_back(uint32_t(v)); }
    uint32_t Percentile(double p) const {
        if (us.empty())
            return 0;
        std::vector<uint32_t> s = us;
        std::sort(s.begin(), s.end());
        return s[std::min(s.size() - 1, size_t(p * s.size()))];
    }
    uint32_t Max() const { return us.empty() ? 0 : *std::max_element(us.begin(), us.end()); }
};
extern Latency latency;
extern MMU2::FilamentState fsensor; ///< the printer's own filament sensor, simulated by the scenario driver
extern bool verbose;
/// CORE One extension-board emulation (0 = off): a reply whose next byte arrives more than gateMs after the last UART
/// activity is abandoned (Buddy xbuddy_extension/mmu.cpp: linkLayerTimeout = 10 ms -> GatewayPathUnavailable), and RX is
/// flushed before every transmit, so late bytes are lost and the printer has to retry.
extern uint32_t gateMs;
extern uint32_t gateDrops;
uint64_t NowUs();
bool Open(const std::string &path); ///< serial device, pty slave path, or tcp:host:port
void WaitInput(int timeoutMs);
} // namespace vprinter
