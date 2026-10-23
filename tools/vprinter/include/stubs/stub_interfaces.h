#pragma once
// Minimal replacement for Buddy's tests/unit/lib/Marlin/MMU2/stubs/stub_interfaces.h (no Catch2).
#include <stdio.h>
struct MarlinLogSim {
    void AppendLine(const char *s) { fprintf(stderr, "[printer] %s\n", s); }
};
extern MarlinLogSim marlinLogSim;
