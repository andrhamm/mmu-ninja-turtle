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
void SHR16::Init() {}
void SHR16::SetLED(uint16_t) {} // LEDs: TODO map to AFC-Pro RGB headers
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
// OPEN: printer-link pins unknown (pins_afcpro.h). Buffers work; the peripheral is wired up once pins are chosen.
uint8_t USART::Read() {
    uint8_t c = 0;
    rx_buf.pop(c);
    return c;
}
void USART::Write(uint8_t c) {
    while (!tx_buf.push(c)) {
    }
}
void USART::WriteS(const char *s) {
    while (*s)
        Write(uint8_t(*s++));
}
void USART::puts(const char *s) {
    WriteS(s);
    Write('\n');
}
void USART::Flush() {}
void USART::rx_enable() {}
void USART::Init(USART_InitTypeDef *const) {}
void USART::Isr() {}
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

namespace watchdog { // TODO(phase 6b): IWDG
void Enable(const configuration &) {}
void Disable() {}
void Reset() {}
} // namespace watchdog

namespace adc { // Box Turtle has no MMU3 buttons: report "nothing pressed"
void Init() {}
uint16_t ReadADC(uint8_t) { return 1023; }
} // namespace adc

namespace eeprom { // TODO(phase 6b): persist in a flash sector
EEPROM eeprom;
static uint8_t EE[EEPROM::End() + 1];
static bool eeInit = false;
static uint8_t *Mem() {
    if (!eeInit) {
        memset(EE, 0xff, sizeof(EE));
        eeInit = true;
    }
    return EE;
}
void EEPROM::WriteByte(addr_t a, uint8_t v) { Mem()[a] = v; }
void EEPROM::UpdateByte(addr_t a, uint8_t v) { Mem()[a] = v; }
uint8_t EEPROM::ReadByte(addr_t a) { return Mem()[a]; }
uint8_t EEPROM::ReadByte(addr_t a, uint8_t d) { return Mem()[a] == 0xff ? d : Mem()[a]; }
void EEPROM::WriteWord(addr_t a, uint16_t v) { memcpy(Mem() + a, &v, 2); }
void EEPROM::UpdateWord(addr_t a, uint16_t v) { WriteWord(a, v); }
uint16_t EEPROM::ReadWord(addr_t a) {
    uint16_t v;
    memcpy(&v, Mem() + a, 2);
    return v;
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
