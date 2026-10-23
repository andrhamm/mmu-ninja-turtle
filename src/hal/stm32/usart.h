/// @file stm32/usart.h
/// hal::usart API (see ../usart.h) on an STM32 USART with interrupt-driven ring buffers (Ninja Turtle port).
#pragma once
#include <inttypes.h>
#include "../gpio.h"
#include "../circular_buffer.h"

namespace hal {
namespace usart {

class USART {
public:
    struct USART_InitTypeDef {
        hal::gpio::GPIO_pin rx_pin;
        hal::gpio::GPIO_pin tx_pin;
        uint32_t baudrate;
    };

    uint8_t Peek() const { return rx_buf.front(); }
    bool ReadEmpty() const { return rx_buf.empty(); }
    uint8_t Read();
    void Write(uint8_t c);
    void WriteS(const char *str);
    void WriteS_P(const char *str_P) { WriteS(str_P); }
    void puts(const char *str);
    void puts_P(const char *str_P) { puts(str_P); }
    bool CanWrite() const { return !tx_buf.full(); }
    void Flush();
    void rx_enable();
    void Init(USART_InitTypeDef *const conf);

    /// called from the USART IRQ handler
    void Isr();

private:
    CircularBuffer<uint8_t, uint_fast8_t, 32> tx_buf;
    CircularBuffer<uint8_t, uint_fast8_t, 32> rx_buf;
};

extern USART usart1;

} // namespace usart
} // namespace hal

namespace hu = hal::usart;
