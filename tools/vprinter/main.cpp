// vprinter — Prusa Buddy's unmodified printer-side MMU protocol state machine (ProtocolLogic, v6.10.1) driving an MMU
// over a serial device / pty / TCP, with a simulated printer filament sensor.
// Usage: vprinter --port <dev|pty-path|tcp:host:port> [--scenario lineup|idle:SECS|tc:N|tc-retry] [--gate-ms 10] [--sensor-ms 300] [-v]
#include "host_stubs.h"
#include "mmu2_supported_version.h"
#include "protocol_logic.h"
#include <wiring_time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

using namespace MMU2;

namespace {
ProtocolLogic *pl;
const char *StatusName(StepStatus s) {
    static const char *n[] = { "Processing", "MessageReady", "Finished", "Interrupted", "CommunicationTimeout", "ProtocolError",
        "CommandRejected", "CommandError", "VersionMismatch", "PrinterError", "CommunicationRecovered", "ButtonPushed", "PrematureFinish" };
    return s < sizeof(n) / sizeof(n[0]) ? n[s] : "?";
}
bool IsLinkFault(StepStatus s) { return s == CommunicationTimeout || s == ProtocolError || s == VersionMismatch; }
int linkFaults = 0;

StepStatus Step() {
    vprinter::WaitInput(1);
    StepStatus s = pl->Step();
    if (IsLinkFault(s)) {
        ++linkFaults;
        fprintf(stderr, "link fault: %s\n", StatusName(s));
    }
    return s;
}

bool Lineup() {
    uint32_t t0 = millis();
    while (millis() - t0 < 5000) {
        Step();
        if (pl->Running() && pl->MmuFwVersionMajor() == mmuVersionMajor && pl->MmuFwVersionMinor() == mmuVersionMinor && pl->MmuFwVersionRevision() == mmuVersionPatch) {
            printf("lineup ok: MMU %d.%d.%d after %u ms\n", pl->MmuFwVersionMajor(), pl->MmuFwVersionMinor(), pl->MmuFwVersionRevision(), millis() - t0);
            return true;
        }
    }
    printf("lineup FAILED (running=%d, version %d.%d.%d)\n", pl->Running(), pl->MmuFwVersionMajor(), pl->MmuFwVersionMinor(), pl->MmuFwVersionRevision());
    return false;
}

bool Idle(uint32_t secs) {
    int before = linkFaults;
    for (uint32_t t0 = millis(); millis() - t0 < secs * 1000;)
        Step();
    printf("idle %us: %d link faults\n", secs, linkFaults - before);
    return linkFaults == before;
}

/// One tool change with a simulated printer: the printer's filament sensor clears shortly after the MMU starts pulling
/// filament back, and triggers sensorMs into the MMU's slow feed towards the sensor.
bool ToolChange(uint8_t slot, uint32_t sensorMs, int retryButtons = 0) {
    pl->ToolChange(slot);
    uint32_t t0 = millis(), phaseStart = t0;
    ProgressCode lastPc = ProgressCode::OK;
    for (;;) {
        StepStatus s = Step();
        ProgressCode pc = pl->Progress();
        if (pc != lastPc) {
            phaseStart = millis();
            lastPc = pc;
            if (vprinter::verbose)
                fprintf(stderr, "  T%d progress P%d\n", slot, int(pc));
        }
        uint32_t inPhase = millis() - phaseStart;
        if ((pc == ProgressCode::UnloadingToFinda || pc == ProgressCode::UnloadingToPulley) && inPhase > 200)
            vprinter::fsensor = FilamentState::NOT_PRESENT;
        // MMU fast-feeds the bowden (P6 FeedingToBondtech), then slow-feeds (P28 FeedingToFSensor) while the printer
        // spins its extruder; the tip reaches the printer's sensor shortly into the slow phase.
        if (pc == ProgressCode::FeedingToFSensor && inPhase > sensorMs && vprinter::fsensor == FilamentState::NOT_PRESENT)
            vprinter::fsensor = FilamentState::AT_FSENSOR;
        if (s == Finished) {
            vprinter::fsensor = FilamentState::IN_NOZZLE; // printer finishes loading into the nozzle itself
            printf("T%d ok in %u ms\n", slot, millis() - t0);
            return true;
        }
        if (s == CommandError && retryButtons > 0) { // user presses the MMU's middle button (Retry) on the printer screen
            printf("T%d error 0x%04x -> Retry\n", slot, unsigned(pl->Error()));
            --retryButtons;
            pl->Button(uint8_t(Buttons::Middle));
            continue;
        }
        if (s == CommandError || s == CommandRejected || s == Interrupted || millis() - t0 > 120000) {
            printf("T%d FAILED: %s, error 0x%04x, progress P%d\n", slot, StatusName(s), unsigned(pl->Error()), int(pc));
            return false;
        }
    }
}
} // namespace

int main(int argc, char **argv) {
    std::string port, scenario = "lineup";
    uint32_t &gateMs = vprinter::gateMs, sensorMs = 300;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--port") && i + 1 < argc)
            port = argv[++i];
        else if (!strcmp(argv[i], "--scenario") && i + 1 < argc)
            scenario = argv[++i];
        else if (!strcmp(argv[i], "--gate-ms") && i + 1 < argc)
            gateMs = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--sensor-ms") && i + 1 < argc)
            sensorMs = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-v"))
            vprinter::verbose = true;
        else {
            fprintf(stderr, "usage: %s --port <dev|tcp:host:port> [--scenario lineup|idle:SECS|tc:N|tc-retry] [--gate-ms N] [--sensor-ms N] [-v]\n", argv[0]);
            return 2;
        }
    }
    if (port.empty() || !vprinter::Open(port)) {
        fprintf(stderr, "cannot open port '%s'\n", port.c_str());
        return 1;
    }
    ProtocolLogic logic(&mmu2Serial, 30, 20); // extra load distance [mm], pulley slow feedrate [mm/s]
    pl = &logic;
    logic.Start();

    bool ok = Lineup();
    if (ok && scenario.rfind("idle:", 0) == 0)
        ok = Idle(atoi(scenario.c_str() + 5));
    else if (ok && scenario == "tc-retry") // expects one error (injected fault), recovered by pressing Retry
        ok = ToolChange(1, sensorMs, 1) && pl->Error() == ErrorCode::OK;
    else if (ok && scenario.rfind("tc:", 0) == 0) {
        int n = atoi(scenario.c_str() + 3);

        for (int i = 0; ok && i < n; ++i)
            ok = ToolChange(uint8_t((i + 1) % 5), sensorMs);
    }

    auto &L = vprinter::latency;
    size_t late = gateMs ? std::count_if(L.us.begin(), L.us.end(), [&](uint32_t v) { return v > gateMs * 1000; }) : 0;
    printf("replies %zu  latency p50 %.2f ms  p99 %.2f ms  max %.2f ms  over-gate %zu  gate-drops %u  link-faults %d\n",
        L.us.size(), L.Percentile(0.5) / 1000.0, L.Percentile(0.99) / 1000.0, L.Max() / 1000.0, late, vprinter::gateDrops, linkFaults);
    // A late reply on the CORE One is answered GatewayPathUnavailable by the extension board and retried by ProtocolLogic,
    // so isolated misses are tolerated; what must hold is no link faults and a p99 inside the gate.
    ok = ok && linkFaults == 0 && (!gateMs || L.Percentile(0.99) <= gateMs * 1000);
    printf("%s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
