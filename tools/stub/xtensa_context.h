#pragma once
// STUB di xtensa_context.h (solo i campi del frame di eccezione che servono al gancio del panico)
#include <stdint.h>
typedef struct { uint32_t pc; uint32_t a0; uint32_t a1; } XtExcFrame;
