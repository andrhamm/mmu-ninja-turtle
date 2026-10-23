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

int main() {
    hal::cpu::Init();
    hal::shr16::shr16.Init();
    mt::timebase.Init();
    mg::globals.Init();
    hu::USART::USART_InitTypeDef usart_conf = { .rx_pin = USART_RX, .tx_pin = USART_TX, .baudrate = 115200 };
    hu::usart1.Init(&usart_conf);
    mm::Init();
    hwd::Enable(hwd::configuration::compute(8000));
    __enable_irq();

    mf::finda.BlockingInit();
    logic::startUp.Reset(0);
    mi::idler.Init();
    ms::selector.Init();
    hu::usart1.rx_enable();

    for (;;) {
        // the pulley drives whichever lane the (virtual) idler has engaged
        nt::SelectLane(mi::idler.Engaged() ? mi::idler.Slot() : nt::noLane);
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
