// Ninja Turtle — AFC-Pro firmware entry: upstream MMU application/logic on Box Turtle hardware.
// Mirrors setup()/setup2()/loop() of src/main.cpp minus MMU3-only hardware (shift register LEDs, TMC2130 SPI, buttons).
#include "stm32h7xx.h"
#undef CRC // CMSIS peripheral macro clashes with modules::protocol CRC() (Buddy does the same)

#include "../../src/application.h"
#include "../../src/hal/cpu.h"
#include "../../src/hal/shr16.h"
#include "../../src/hal/usart.h"
#include "../../src/logic/start_up.h"
#include "../../src/modules/buttons.h"
#include "../../src/modules/finda.h"
#include "../../src/modules/fsensor.h"
#include "../../src/modules/globals.h"
#include "../../src/modules/idler.h"
#include "../../src/modules/leds.h"
#include "../../src/modules/motion.h"
#include "../../src/modules/permanent_storage.h"
#include "../../src/modules/pulley.h"
#include "../../src/modules/selector.h"
#include "../../src/modules/timebase.h"
#include "../../src/modules/user_input.h"
#include "../../src/panic.h"
#include "../../src/pins.h"
#include "lanes.h"
#include "eeprom_flush.h"
#include "../../src/hal/watchdog.h"

void Panic(ErrorCode ec) { application.Panic(ec); }

/// Keep the 5 V TX buffer disabled until the printer's TX line has idled high for 100 ms (printer powered), so we never
/// drive an unpowered printer input. RX is pulled down meanwhile so a floating line does not look "high".
static void WaitForPrinterLink() {
    hal::gpio::Init(nt::LINK_TX_OE, hal::gpio::GPIO_InitTypeDef(hal::gpio::Mode::output, hal::gpio::Level::high));
    hal::gpio::Init(USART_RX, hal::gpio::GPIO_InitTypeDef(hal::gpio::Mode::input, hal::gpio::Pull::down));
    uint16_t highSince = 0;
    bool high = false;
    for (;;) {
        if (hal::gpio::ReadPin(USART_RX) == hal::gpio::Level::high) {
            if (!high) {
                high = true;
                highSince = mt::timebase.Millis();
            } else if (mt::timebase.Elapsed(highSince, 100)) {
                return;
            }
        } else {
            high = false;
        }
        hal::gpio::WritePin(nt::STATUS_LED, (mt::timebase.Millis() / 500) & 1 ? hal::gpio::Level::high : hal::gpio::Level::low);
    }
}

int main() {
    hal::cpu::Init();
    hal::shr16::shr16.Init();
    mt::timebase.Init();
    mg::globals.Init();
    mm::Init();
    __enable_irq(); // SysTick needed by WaitForPrinterLink
    WaitForPrinterLink();
    hu::USART::USART_InitTypeDef usart_conf = { .rx_pin = USART_RX, .tx_pin = USART_TX, .baudrate = 115200 };
    hu::usart1.Init(&usart_conf);
    hal::gpio::WritePin(nt::LINK_TX_OE, hal::gpio::Level::low); // printer powered: enable TX buffer
    hwd::Enable(hwd::configuration::compute(8000));

    mf::finda.BlockingInit();
    logic::startUp.Reset(0);
    mi::idler.Init();
    ms::selector.Init();
    hu::usart1.rx_enable();

    for (;;) {
        // the pulley drives whichever lane the (virtual) idler has engaged; moves also re-select at block start (SetTMCDir)
        nt::UpdateLane();
        mb::buttons.Step();
        ml::leds.Step();
        mf::finda.Step();
        mfs::fsensor.Step();
        mi::idler.Step();
        ms::selector.Step();
        mpu::pulley.Step();
        mui::userInput.Step();
        application.Step();
        // persist settings only while nothing is running (flash erase stalls the CPU, see hal_afcpro.cpp)
        hal::eeprom::FlushIfDue(application.CurrentProgressCode() == uint8_t(ProgressCode::OK) && mm::motion.QueueEmpty());
        hwd::Reset();
    }
}
