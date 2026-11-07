// Sostituisce (con la stessa firma) la finta xTaskCreatePinnedToCore dello stub, che e "inline" e non permette di vedere le chiamate.
#include <stdint.h>
void fakeCreateRecord(void (*fn)(void*), const char* name, uint32_t stack);
int xTaskCreatePinnedToCore(void (*fn)(void*), const char* name, uint32_t stack, void*, unsigned, void**, int) {
  fakeCreateRecord(fn, name, stack);
  return 1;
}
