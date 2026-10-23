// Ninja Turtle — AFC-Pro (STM32H723) implementations of the upstream HAL interfaces.
// Motion math keeps the AVR contract: logical F_CPU 16 MHz, step timer ticking at 2 MHz (config::stepTimerFrequencyDivider).
#include "stm32h7xx.h"
#undef CRC // CMSIS peripheral macro clashes with modules::protocol CRC() (Buddy does the same)

#include "../../src/pins.h"
#include "../../src/hal/adc.h"
#include "../../src/hal/cpu.h"
#include "../../src/hal/eeprom.h"
#include "../../src/hal/shr16.h"
#include "../../src/hal/stm32/motion_timer.h"
#include "../../src/hal/tmc2130.h"
#include "../../src/hal/usart.h"
#include "../../src/hal/watchdog.h"
#include "../../src/modules/motion.h"
#include "../../src/modules/timebase.h"
#include "../../src/nt/virtual_axes.h"
#include "lanes.h"
#include "tmc_uart.h"
#include "eeprom_flush.h"
#include "../../src/nt/tmc2209.h"

#include <string.h>

// ---------------- lanes (pulley axis -> active Box Turtle lane) ----------------
namespace nt {
const hal::gpio::GPIO_pin *volatile pulleyStep = &NC;
static uint8_t activeLane = noLane;
static bool pulleyEnabled = false;

static void LaneEnable(uint8_t lane, bool on) {
    if (lane < laneCount) // TMC2209 EN is active low
        hal::gpio::WritePin(LANE_EN[lane], on ? hal::gpio::Level::low : hal::gpio::Level::high);
}

void SelectLane(uint8_t lane) {
    if (lane == activeLane)
        return;
    LaneEnable(activeLane, false);
    activeLane = lane < laneCount ? lane : noLane;
    pulleyStep = activeLane == noLane ? &NC : &LANE_STEP[activeLane];
    LaneEnable(activeLane, pulleyEnabled);
}

uint8_t ActiveLane() { return activeLane; }

static void LanesInit() {
    for (uint8_t i = 0; i < laneCount; ++i) {
        hal::gpio::Init(LANE_EN[i], hal::gpio::GPIO_InitTypeDef(hal::gpio::Mode::output, hal::gpio::Level::high)); // disabled
        hal::gpio::Init(LANE_STEP[i], hal::gpio::GPIO_InitTypeDef(hal::gpio::Mode::output, hal::gpio::Level::low));
        hal::gpio::Init(LANE_DIR[i], hal::gpio::GPIO_InitTypeDef(hal::gpio::Mode::output, hal::gpio::Level::low));
        hal::gpio::Init(LANE_PREP[i], hal::gpio::GPIO_InitTypeDef(hal::gpio::Mode::input, hal::gpio::Pull::up));
        hal::gpio::Init(LANE_EXIT[i], hal::gpio::GPIO_InitTypeDef(hal::gpio::Mode::input, hal::gpio::Pull::up));
    }
    hal::gpio::Init(FINDA_PIN, hal::gpio::GPIO_InitTypeDef(hal::gpio::Mode::input, hal::gpio::Pull::up));
}
} // namespace nt

// ---------------- shift register (MMU3 LEDs + TMC dir/enable) -> lanes ----------------
namespace hal {
namespace shr16 {
SHR16 shr16;
void SHR16::Init() {
    hal::gpio::Init(nt::STATUS_LED, hal::gpio::GPIO_InitTypeDef(hal::gpio::Mode::output, hal::gpio::Level::low));
}
// MMU3 has 5 green/red LED pairs; the AFC-Pro has one status LED: lit whenever any MMU LED is on (blinks follow the
// MMU's own blink patterns). shortcut: RGB headers (per-lane colour) not used yet.
void SHR16::SetLED(uint16_t led) {
    hal::gpio::WritePin(nt::STATUS_LED, led ? hal::gpio::Level::high : hal::gpio::Level::low);
}
void SHR16::SetTMCEnabled(uint8_t index, bool ena) {
    if (index == mm::Pulley) {
        nt::pulleyEnabled = ena;
        nt::LaneEnable(nt::activeLane, ena);
    }
}
void SHR16::SetTMCDir(uint8_t index, bool dir) {
    if (index == mm::Pulley && nt::activeLane < nt::laneCount)
        hal::gpio::WritePin(nt::LANE_DIR[nt::activeLane], dir ? hal::gpio::Level::high : hal::gpio::Level::low);
}
void SHR16::Write(uint16_t) {}
} // namespace shr16

// ---------------- stepper drivers ----------------
namespace tmc2130 {
namespace t9 = nt::tmc2209;
// Only the pulley (= Box Turtle lanes) has real drivers; selector/idler are virtual (no UART traffic).
static bool ConfigureLane(uint8_t lane, const MotorParams &params, const MotorCurrents &c, MotorMode mode) {
    uint32_t before = 0, after = 0;
    bool ok = nt::TmcRead(lane, t9::IFCNT, before); // IFCNT counts successful writes: proves the UART link
    nt::TmcWrite(lane, t9::GCONF, t9::Gconf(mode == Normal));
    nt::TmcWrite(lane, t9::CHOPCONF, t9::Chopconf(uint8_t(params.mRes), c.vSense));
    nt::TmcWrite(lane, t9::IHOLD_IRUN, t9::IholdIrun(c.iHold, c.iRun));
    ok = ok && nt::TmcRead(lane, t9::IFCNT, after) && uint8_t(after - before) == 3;
    return ok;
}

bool TMC2130::Init(const MotorParams &params, const MotorCurrents &currents, MotorMode mode) {
    sg_filter_threshold = 1;
    sg_filter_counter = 1;
    initialized = true;
    if (params.axis != mm::Pulley)
        return true;
    nt::TmcUartInit();
    bool ok = true;
    for (uint8_t lane = 0; lane < nt::laneCount; ++lane)
        ok &= ConfigureLane(lane, params, currents, mode);
    return ok; // false -> upstream reports a TMC init error for the pulley
}
void TMC2130::SetMode(const MotorParams &params, MotorMode mode) {
    if (params.axis == mm::Pulley)
        for (uint8_t lane = 0; lane < nt::laneCount; ++lane)
            nt::TmcWrite(lane, t9::GCONF, t9::Gconf(mode == Normal));
}
void TMC2130::SetBridgeOutput(const MotorParams &, bool) {}
void TMC2130::SetCurrents(const MotorParams &params, const MotorCurrents &c) {
    if (params.axis == mm::Pulley)
        for (uint8_t lane = 0; lane < nt::laneCount; ++lane)
            nt::TmcWrite(lane, t9::IHOLD_IRUN, t9::IholdIrun(c.iHold, c.iRun));
}
void TMC2130::SetSGTHRS(const MotorParams &) {}
void TMC2130::SetEnabled(const MotorParams &params, bool en) {
    hal::shr16::shr16.SetTMCEnabled(params.idx, en);
    enabled = en;
}
bool TMC2130::CheckForErrors(const MotorParams &) { return true; } // TODO: DRV_STATUS over UART (ot/otpw/s2g)
uint32_t TMC2130::ReadRegister(const MotorParams &, Registers) { return 0; }
void TMC2130::WriteRegister(const MotorParams &, Registers, uint32_t) {}
void TMC2130::Isr(const MotorParams &params) {
    if (nt::VirtualAxisStall(params.axis)) // selector/idler: synthesized end-stops
        sg_filter_counter = 0;
}
} // namespace tmc2130

// ---------------- printer link USART ----------------
namespace usart {
USART usart1;
// USART1 on PB6 (TX) / PB7 (RX), AF7. Kernel clock = PCLK2 (64 MHz after reset; TODO update when clocks are configured).
static constexpr uint32_t usartClock = 64000000;

static void PinAF7(GPIO_TypeDef *port, uint32_t n) {
    port->MODER = (port->MODER & ~(3u << (2 * n))) | (2u << (2 * n));
    port->OSPEEDR |= 2u << (2 * n);
    port->PUPDR = (port->PUPDR & ~(3u << (2 * n))) | (1u << (2 * n)); // pull-up keeps an unconnected RX idle
    volatile uint32_t &afr = port->AFR[n >> 3];
    afr = (afr & ~(0xFu << (4 * (n & 7)))) | (7u << (4 * (n & 7)));
}

uint8_t USART::Read() {
    uint8_t c = 0;
    NVIC_DisableIRQ(USART1_IRQn);
    rx_buf.pop(c);
    NVIC_EnableIRQ(USART1_IRQn);
    return c;
}
void USART::Write(uint8_t c) {
    for (;;) {
        NVIC_DisableIRQ(USART1_IRQn);
        bool ok = tx_buf.push(c);
        NVIC_EnableIRQ(USART1_IRQn);
        if (ok)
            break;
    }
    USART1->CR1 |= USART_CR1_TXEIE_TXFNFIE;
}
void USART::WriteS(const char *s) {
    while (*s)
        Write(uint8_t(*s++));
}
void USART::puts(const char *s) {
    WriteS(s);
    Write('\n');
}
void USART::Flush() {
    while (!tx_buf.empty() || !(USART1->ISR & USART_ISR_TC)) {
    }
}
void USART::rx_enable() { USART1->CR1 |= USART_CR1_RE | USART_CR1_RXNEIE_RXFNEIE; }
void USART::Init(USART_InitTypeDef *const conf) {
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    (void)RCC->APB2ENR;
    PinAF7(GPIOB, 6);
    PinAF7(GPIOB, 7);
    USART1->CR1 = 0;
    USART1->BRR = (usartClock + conf->baudrate / 2) / conf->baudrate;
    USART1->CR1 = USART_CR1_TE | USART_CR1_UE; // RX enabled later by rx_enable(), like the AVR code
    NVIC_EnableIRQ(USART1_IRQn);
}
void USART::Isr() {
    uint32_t isr = USART1->ISR;
    if (isr & USART_ISR_ORE)
        USART1->ICR = USART_ICR_ORECF;
    if (isr & USART_ISR_FE)
        USART1->ICR = USART_ICR_FECF;
    if (isr & USART_ISR_RXNE_RXFNE)
        rx_buf.push(uint8_t(USART1->RDR));
    if ((USART1->CR1 & USART_CR1_TXEIE_TXFNFIE) && (isr & USART_ISR_TXE_TXFNF)) {
        uint8_t c;
        if (tx_buf.pop(c))
            USART1->TDR = c;
        else
            USART1->CR1 &= ~USART_CR1_TXEIE_TXFNFIE;
    }
}
} // namespace usart

// ---------------- misc ----------------
namespace cpu {
bool resetPending = false;
void Init() {
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOAEN | RCC_AHB4ENR_GPIOBEN | RCC_AHB4ENR_GPIOCEN | RCC_AHB4ENR_GPIODEN
        | RCC_AHB4ENR_GPIOEEN | RCC_AHB4ENR_GPIOFEN | RCC_AHB4ENR_GPIOGEN;
    (void)RCC->AHB4ENR;
    nt::LanesInit();
}
void Reset() { NVIC_SystemReset(); }
void Step() {}
} // namespace cpu

namespace watchdog { // IWDG1 on the 32 kHz LSI: prescaler /256 -> 125 Hz; ~8 s like the MMU3
void Enable(const configuration &) {
    IWDG1->KR = 0xCCCC; // start
    IWDG1->KR = 0x5555; // unlock PR/RLR
    IWDG1->PR = 6; // /256
    IWDG1->RLR = 1000; // 1000 / 125 Hz = 8 s
    while (IWDG1->SR) {
    }
    IWDG1->KR = 0xAAAA;
}
void Disable() {} // IWDG cannot be stopped once started
void Reset() { IWDG1->KR = 0xAAAA; }
} // namespace watchdog

namespace adc { // Box Turtle has no MMU3 buttons: report "nothing pressed"
void Init() {}
uint16_t ReadADC(uint8_t) { return 1023; }
} // namespace adc

namespace eeprom {
// 2 KB EEPROM image in RAM, persisted to flash sector 3 (0x08060000, excluded from the linker FLASH region).
// Erased flash reads 0xFF == erased EEPROM, so a blank sector is a valid "factory" image.
// shortcut: a sector erase stalls the CPU (code runs from the same bank) for ~1-2 s; flush only when the MMU is idle and
// the printer link is quiet, upgrade to RAM-resident flash routines if the stall causes link timeouts on hardware.
EEPROM eeprom;
static uint8_t EE[EEPROM::End() + 1];
static bool loaded = false, dirty = false;
static uint16_t lastWriteMs = 0;
static constexpr uint32_t sectorAddr = 0x08060000;
static constexpr uint32_t sectorIndex = 3;
static uint8_t *Mem() {
    if (!loaded) {
        memcpy(EE, (const void *)sectorAddr, sizeof(EE));
        loaded = true;
    }
    return EE;
}
static void Touch() {
    dirty = true;
    lastWriteMs = mt::timebase.Millis();
}
void EEPROM::WriteByte(addr_t a, uint8_t v) {
    if (Mem()[a] != v) {
        Mem()[a] = v;
        Touch();
    }
}
void EEPROM::UpdateByte(addr_t a, uint8_t v) { WriteByte(a, v); }
uint8_t EEPROM::ReadByte(addr_t a) { return Mem()[a]; }
uint8_t EEPROM::ReadByte(addr_t a, uint8_t d) { return Mem()[a] == 0xff ? d : Mem()[a]; }
void EEPROM::WriteWord(addr_t a, uint16_t v) {
    if (memcmp(Mem() + a, &v, 2)) {
        memcpy(Mem() + a, &v, 2);
        Touch();
    }
}
void EEPROM::UpdateWord(addr_t a, uint16_t v) { WriteWord(a, v); }
uint16_t EEPROM::ReadWord(addr_t a) {
    uint16_t v;
    memcpy(&v, Mem() + a, 2);
    return v;
}

static void WaitFlash() {
    while (FLASH->SR1 & (FLASH_SR_QW | FLASH_SR_BSY)) {
    }
}

bool FlushIfDue(bool idle) {
    if (!dirty || !idle || !mt::timebase.Elapsed(lastWriteMs, 2000))
        return false;
    uint32_t prim = __get_PRIMASK();
    __disable_irq();
    FLASH->KEYR1 = 0x45670123;
    FLASH->KEYR1 = 0xCDEF89AB;
    WaitFlash();
    FLASH->CCR1 = 0xFFFFFFFF; // clear error flags
    FLASH->CR1 = (FLASH->CR1 & ~FLASH_CR_SNB) | FLASH_CR_SER | (sectorIndex << FLASH_CR_SNB_Pos);
    FLASH->CR1 |= FLASH_CR_START;
    WaitFlash();
    FLASH->CR1 &= ~FLASH_CR_SER;
    FLASH->CR1 |= FLASH_CR_PG;
    volatile uint32_t *dst = (volatile uint32_t *)sectorAddr;
    const uint32_t *src = (const uint32_t *)EE;
    for (uint32_t i = 0; i < sizeof(EE) / 4; i += 8) { // 256-bit flash words
        for (uint32_t w = 0; w < 8; ++w)
            dst[i + w] = src[i + w];
        __DSB();
        WaitFlash();
    }
    FLASH->CR1 &= ~FLASH_CR_PG;
    FLASH->CR1 |= FLASH_CR_LOCK;
    SCB_InvalidateDCache_by_Addr((void *)sectorAddr, sizeof(EE));
    __set_PRIMASK(prim);
    dirty = false;
    return true;
}
} // namespace eeprom

// ---------------- step timer: TIM2 @ 2 MHz, reprogrammed from motion.Step() ----------------
namespace stm32 {
void StepTimerInit() {
    RCC->APB1LENR |= RCC_APB1LENR_TIM2EN;
    (void)RCC->APB1LENR;
    TIM2->PSC = SystemCoreClock / 2000000u - 1; // TODO(phase 6b): use the real APB1 timer clock once clocks are configured
    TIM2->ARR = 0x4000; // first interrupt ~8 ms from now (same as AVR)
    TIM2->CNT = 0;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0;
    TIM2->DIER = TIM_DIER_UIE;
    TIM2->CR1 = TIM_CR1_CEN;
}
void StepTimerIrqEnable(bool on) {
    if (on)
        NVIC_EnableIRQ(TIM2_IRQn);
    else
        NVIC_DisableIRQ(TIM2_IRQn);
}
bool StepTimerIrqDisable() {
    bool was = NVIC_GetEnableIRQ(TIM2_IRQn);
    NVIC_DisableIRQ(TIM2_IRQn);
    return was;
}
} // namespace stm32
} // namespace hal

extern "C" void USART1_IRQHandler() { hal::usart::usart1.Isr(); }

extern "C" void TIM2_IRQHandler() {
    TIM2->SR = 0;
    mm::st_timer_t next = mm::motion.Step();
    TIM2->ARR = next ? next : 0x4000; // idle: next look in ~8 ms
}

// ---------------- 1 ms timebase on SysTick ----------------
namespace modules {
namespace time {
Timebase timebase;
void Timebase::Init() { SysTick_Config(SystemCoreClock / 1000); }
void Timebase::Isr() { ms++; }
uint16_t Timebase::Millis() const { return ms; }
bool Timebase::Elapsed(uint16_t start, uint16_t timeout) const {
    uint16_t d = Millis();
    d -= start;
    return d > timeout;
}
} // namespace time
} // namespace modules

extern "C" void SysTick_Handler() { modules::time::timebase.Isr(); }
