#pragma once
// STUB di esp_private/panic_internal.h (solo il tipo che serve al gancio del panico)
typedef struct { int core; int exception; const char* reason; const char* description; const void* addr; const void* frame; } panic_info_t;
// Come l'header vero (IDF 5.5.5, esp_private/panic_internal.h:21-22 e 31-40)
#ifdef __cplusplus
extern "C" {
#endif
extern bool g_panic_abort;
extern char *g_panic_abort_details;
#ifdef __cplusplus
}
#endif
typedef enum { PANIC_EXCEPTION_DEBUG, PANIC_EXCEPTION_IWDT, PANIC_EXCEPTION_TWDT, PANIC_EXCEPTION_ABORT, PANIC_EXCEPTION_FAULT } panic_exception_t;
