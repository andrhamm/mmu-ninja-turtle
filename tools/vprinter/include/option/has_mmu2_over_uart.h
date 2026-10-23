#pragma once
// vprinter talks MMU3 protocol directly over a byte stream (MK4-style UART path); the CORE One's MODBUS extension-board
// hop is modelled separately by vprinter's --gate-ms check.
#define HAS_MMU2_OVER_UART() 1
#ifdef __cplusplus
namespace option {
inline constexpr bool has_mmu2_over_uart = 1;
}
#endif
