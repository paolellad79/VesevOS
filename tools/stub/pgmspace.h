#pragma once
#define pgm_read_byte(a) (*(const uint8_t*)(a))
#define pgm_read_word(a) (*(const uint16_t*)(a))
#define pgm_read_dword(a) (*(const uint32_t*)(a))
#define pgm_read_float(a) (*(const float*)(a))
#define pgm_read_ptr(a) (*(void* const*)(a))
#define PGM_P const char*
#define PSTR(x) x
#define strlen_P strlen
#define strncmp_P strncmp
#define strcmp_P strcmp
#define memcpy_P memcpy
#define strcpy_P strcpy
#define PROGMEM
