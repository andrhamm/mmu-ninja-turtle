#pragma once
// ARM has a unified address space: PROGMEM data lives in flash and is read with normal loads.
#define PROGMEM /* */
#define PSTR(s) (s)
#define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#define pgm_read_word(addr) (*(const unsigned short *)(addr))
#define pgm_read_ptr(addr) (*(void *const *)(addr))
#define strncmp_P strncmp
#define strlen_P strlen
#define memcpy_P memcpy
