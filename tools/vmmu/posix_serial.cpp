// modules::serial implemented on a POSIX fd, plus JSONL frame logging with reply latency.
#include "posix_serial.h"
#include "physics.h"
#include "serial.h"

#include <deque>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#ifdef __APPLE__
#include <util.h> // openpty
#else
#include <pty.h> // openpty (link -lutil)
#endif
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

namespace {
int fd = -1;
FILE *logf = nullptr;
std::deque<uint8_t> rx;
std::string rxLine, txLine;
uint64_t lastRxEndUs = 0;

void LogLine(const char *dir, const std::string &line, int64_t latencyUs) {
    if (!logf)
        return;
    std::string esc;
    for (char c : line)
        if (c != '\n')
            esc += (c == '"' || c == '\\') ? std::string("\\") + c : std::string(1, c);
    if (latencyUs >= 0)
        fprintf(logf, "{\"t_us\":%llu,\"dir\":\"%s\",\"msg\":\"%s\",\"latency_us\":%lld}\n", (unsigned long long)vmmu::NowUs(), dir, esc.c_str(), (long long)latencyUs);
    else
        fprintf(logf, "{\"t_us\":%llu,\"dir\":\"%s\",\"msg\":\"%s\"}\n", (unsigned long long)vmmu::NowUs(), dir, esc.c_str());
    fflush(logf);
}

bool SetRaw(int f) {
    termios t {};
    if (tcgetattr(f, &t) != 0)
        return false;
    cfmakeraw(&t);
    cfsetispeed(&t, B115200);
    cfsetospeed(&t, B115200);
    t.c_cflag |= CLOCAL | CREAD;
    t.c_cflag &= ~(CSTOPB | PARENB | CRTSCTS);
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
    return tcsetattr(f, TCSANOW, &t) == 0;
}
} // namespace

namespace vmmu {

uint64_t NowUs() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return uint64_t(ts.tv_sec) * 1000000u + ts.tv_nsec / 1000;
}

void SetLog(FILE *f) { logf = f; }

int OpenPort(const std::string &path, std::string &openedName) {
    if (path.rfind("tcp:", 0) == 0) { // tcp:host:port, e.g. a socat bridge to the Mac's USB serial device
        std::string hp = path.substr(4);
        size_t colon = hp.rfind(':');
        if (colon == std::string::npos)
            return -1;
        addrinfo hints {}, *res = nullptr;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(hp.substr(0, colon).c_str(), hp.substr(colon + 1).c_str(), &hints, &res) != 0)
            return -1;
        fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (fd < 0 || connect(fd, res->ai_addr, res->ai_addrlen) != 0) {
            freeaddrinfo(res);
            return -1;
        }
        freeaddrinfo(res);
        int one = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)); // no Nagle: replies must leave within ms
        openedName = path;
        return fd;
    } else if (path == "pty") {
        int master, slave;
        char name[128];
        if (openpty(&master, &slave, name, nullptr, nullptr) != 0)
            return -1;
        SetRaw(slave); // the peer opens `name`; keep slave open so the pty stays alive
        fd = master;
        openedName = name;
    } else {
        fd = open(path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (fd < 0 || !SetRaw(fd))
            return -1;
        openedName = path;
    }
    SetRaw(fd);
    return fd;
}

bool PumpRx(int timeoutMs) {
    pollfd p { fd, POLLIN, 0 };
    if (poll(&p, 1, timeoutMs) <= 0)
        return true;
    uint8_t buf[256];
    ssize_t n = read(fd, buf, sizeof(buf));
    if (n <= 0)
        return n < 0; // 0 = peer closed
    for (ssize_t i = 0; i < n; ++i) {
        rx.push_back(buf[i]);
        rxLine += char(buf[i]);
        if (buf[i] == '\n') {
            lastRxEndUs = NowUs();
            LogLine("rx", rxLine, -1);
            rxLine.clear();
        }
    }
    return true;
}

} // namespace vmmu

namespace modules {
namespace serial {

bool WriteToUSART(const uint8_t *src, uint8_t len) {
    if (vmmu::physics::faults.replyDelayMs)
        usleep(vmmu::physics::faults.replyDelayMs * 1000u);
    // shortcut: blocking write is fine at these sizes (<=14 B); revisit if the fd ever backs up
    for (uint8_t written = 0; written < len;) {
        ssize_t n = write(fd, src + written, len - written);
        if (n < 0)
            return false;
        written += n;
    }
    for (uint8_t i = 0; i < len; ++i) {
        txLine += char(src[i]);
        if (src[i] == '\n') {
            LogLine("tx", txLine, lastRxEndUs ? int64_t(vmmu::NowUs() - lastRxEndUs) : -1);
            txLine.clear();
        }
    }
    return true;
}

bool Available() { return !rx.empty(); }

uint8_t ConsumeByte() {
    if (rx.empty())
        return 0xff;
    uint8_t c = rx.front();
    rx.pop_front();
    return c;
}

} // namespace serial
} // namespace modules
