// Bit-banged half-duplex UART to each lane's TMC2209 (AFC-Pro wires one PDN_UART pin per driver, nt::LANE_UART).
// 19200 baud (TMC2209 auto-bauds; software timing via the DWT cycle counter). Blocking, IRQs masked per byte: only used
// while motors are idle (driver init / mode / current changes).
#include "stm32h7xx.h"
#undef CRC
#include "../../src/pins.h"
#include "../../src/nt/tmc2209.h"
#include "tmc_uart.h"

namespace nt {
namespace {
constexpr uint32_t baud = 19200;

void DwtInit() {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}
inline uint32_t BitCycles() { return SystemCoreClock / baud; }
inline void WaitUntil(uint32_t t) {
    while (int32_t(DWT->CYCCNT - t) < 0) {
    }
}

void SetOutput(const hal::gpio::GPIO_pin &p) { hal::gpio::Init(p, hal::gpio::GPIO_InitTypeDef(hal::gpio::Mode::output, hal::gpio::Level::high)); }
void SetInput(const hal::gpio::GPIO_pin &p) { hal::gpio::Init(p, hal::gpio::GPIO_InitTypeDef(hal::gpio::Mode::input, hal::gpio::Pull::up)); }

void SendByte(const hal::gpio::GPIO_pin &p, uint8_t b) {
    uint32_t prim = __get_PRIMASK();
    __disable_irq();
    uint32_t t = DWT->CYCCNT;
    uint16_t frame = uint16_t(b) << 1 | 0x200; // start 0, 8 data LSB first, stop 1
    for (uint8_t i = 0; i < 10; ++i) {
        hal::gpio::WritePin(p, (frame >> i) & 1 ? hal::gpio::Level::high : hal::gpio::Level::low);
        t += BitCycles();
        WaitUntil(t);
    }
    __set_PRIMASK(prim);
}

bool RecvByte(const hal::gpio::GPIO_pin &p, uint8_t &b, uint32_t timeoutCycles) {
    uint32_t start = DWT->CYCCNT;
    while (hal::gpio::ReadPin(p) == hal::gpio::Level::high) // wait for start bit
        if (DWT->CYCCNT - start > timeoutCycles)
            return false;
    uint32_t prim = __get_PRIMASK();
    __disable_irq();
    uint32_t t = DWT->CYCCNT + BitCycles() + BitCycles() / 2; // middle of bit 0
    b = 0;
    for (uint8_t i = 0; i < 8; ++i) {
        WaitUntil(t);
        b |= uint8_t(hal::gpio::ReadPin(p) == hal::gpio::Level::high) << i;
        t += BitCycles();
    }
    WaitUntil(t); // stop bit
    __set_PRIMASK(prim);
    return true;
}
} // namespace

void TmcUartInit() { DwtInit(); }

void TmcWrite(uint8_t lane, uint8_t reg, uint32_t value) {
    if (lane >= laneCount)
        return;
    uint8_t d[8];
    tmc2209::EncodeWrite(0, reg, value, d);
    SetOutput(LANE_UART[lane]);
    for (uint8_t b : d)
        SendByte(LANE_UART[lane], b);
    SetInput(LANE_UART[lane]);
}

bool TmcRead(uint8_t lane, uint8_t reg, uint32_t &value) {
    if (lane >= laneCount)
        return false;
    uint8_t rq[4], rep[8];
    tmc2209::EncodeRead(0, reg, rq);
    SetOutput(LANE_UART[lane]);
    for (uint8_t b : rq)
        SendByte(LANE_UART[lane], b);
    SetInput(LANE_UART[lane]);
    for (uint8_t &b : rep) // driver answers after a short turnaround; allow ~20 bit times per byte
        if (!RecvByte(LANE_UART[lane], b, BitCycles() * 20))
            return false;
    return tmc2209::DecodeReply(reg, rep, value);
}
} // namespace nt
