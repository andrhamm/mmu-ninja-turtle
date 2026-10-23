// Host implementations of what Buddy's ProtocolLogic needs from the printer: clock, MMU serial port, printer filament
// sensor, logging. The serial port measures reply latency (time from request sent to first reply byte).
#include <avr/pgmspace.h> // PROGMEM for mmu2_log.h
#include <errno.h>
#include "host_stubs.h"
#include "mmu2_fsensor.h"
#include "mmu2_log.h"
#include "mmu2_serial.h"
#include "stubs/stub_interfaces.h"

#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

MarlinLogSim marlinLogSim;

namespace vprinter {
MMU2::FilamentState fsensor = MMU2::FilamentState::NOT_PRESENT;
Latency latency;
bool verbose = false;
uint32_t gateMs = 0;
uint32_t gateDrops = 0;
namespace {
int fd = -1;
uint64_t lastTxUs = 0;
bool awaitingFirstByte = false;
uint64_t lastActivityUs = 0;
bool gateClosed = false; ///< extension board gave up on the current reply
} // namespace

uint64_t NowUs() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return uint64_t(ts.tv_sec) * 1000000u + ts.tv_nsec / 1000;
}

bool Open(const std::string &path) {
    if (path.rfind("tcp:", 0) == 0) {
        std::string hp = path.substr(4);
        size_t colon = hp.rfind(':');
        addrinfo hints {}, *res = nullptr;
        hints.ai_socktype = SOCK_STREAM;
        if (colon == std::string::npos || getaddrinfo(hp.substr(0, colon).c_str(), hp.substr(colon + 1).c_str(), &hints, &res) != 0)
            return false;
        fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        bool ok = fd >= 0 && connect(fd, res->ai_addr, res->ai_addrlen) == 0;
        freeaddrinfo(res);
        int one = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
        fcntl(fd, F_SETFL, O_NONBLOCK);
        return ok;
    }
    fd = open(path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0)
        return false;
    termios t {};
    tcgetattr(fd, &t);
    cfmakeraw(&t);
    cfsetispeed(&t, B115200);
    cfsetospeed(&t, B115200);
    t.c_cflag |= CLOCAL | CREAD;
    tcsetattr(fd, TCSANOW, &t);
    return true;
}

void WaitInput(int timeoutMs) {
    pollfd p { fd, POLLIN, 0 };
    poll(&p, 1, timeoutMs);
}

} // namespace vprinter

extern "C" uint32_t millis(void) {
    static const uint64_t t0 = vprinter::NowUs();
    return uint32_t((vprinter::NowUs() - t0) / 1000);
}

namespace MMU2 {

MMU2Serial mmu2Serial;

void MMU2Serial::begin() {}
void MMU2Serial::close() {}
void MMU2Serial::flush() {}
void MMU2Serial::check_recovery() {}

int MMU2Serial::read() {
    using namespace vprinter;
    uint8_t c;
    if (::read(fd, &c, 1) != 1) {
        if (gateMs && !gateClosed && awaitingFirstByte && NowUs() - lastActivityUs > gateMs * 1000ull) {
            gateClosed = true; // nothing arrived in time
            ++gateDrops;
        }
        return -1;
    }
    if (gateMs) {
        uint64_t now = NowUs();
        if (!gateClosed && now - lastActivityUs > gateMs * 1000ull) {
            gateClosed = true; // byte came too late (first byte or a gap inside the reply)
            ++gateDrops;
        }
        lastActivityUs = now;
        if (gateClosed) {
            if (awaitingFirstByte) {
                latency.Record(now - lastTxUs);
                awaitingFirstByte = false;
            }
            return -1; // extension board no longer listening for this reply
        }
    }
    if (vprinter::awaitingFirstByte) {
        vprinter::latency.Record(vprinter::NowUs() - vprinter::lastTxUs);
        vprinter::awaitingFirstByte = false;
    }
    return c;
}

size_t MMU2Serial::write(const uint8_t *buffer, size_t size) {
    if (vprinter::gateMs) { // extension board flushes RX before each transmit
        uint8_t junk[64];
        while (::read(vprinter::fd, junk, sizeof(junk)) > 0) {
        }
        vprinter::gateClosed = false;
    }
    size_t done = 0;
    while (done < size) {
        ssize_t n = ::write(vprinter::fd, buffer + done, size - done);
        if (n > 0)
            done += n;
        else if (n < 0 && errno != EAGAIN)
            break;
    }
    vprinter::lastTxUs = vprinter::lastActivityUs = vprinter::NowUs();
    vprinter::awaitingFirstByte = true;
    return done;
}

FilamentState WhereIsFilament() { return vprinter::fsensor; }
FSensorBlockRunout::FSensorBlockRunout() {}
FSensorBlockRunout::~FSensorBlockRunout() {}

void LogErrorEvent_P(const char *msg) { fprintf(stderr, "[printer] error: %s\n", msg); }
void LogEchoEvent_P(const char *msg) {
    if (vprinter::verbose)
        fprintf(stderr, "[printer] %s\n", msg);
}
void LogRequestMsg(const char *msg) {
    if (vprinter::verbose)
        fprintf(stderr, "[printer] >MMU %s\n", msg);
}
void LogResponseMsg(const char *msg) {
    if (vprinter::verbose)
        fprintf(stderr, "[printer] <MMU %s\n", msg);
}

} // namespace MMU2
