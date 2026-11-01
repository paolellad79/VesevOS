/* VesevOS - prove sul computer: esegue un file Berry (con le funzioni finte che il file stesso definisce). Uso: berry_run <file.be>
   Stampa l'errore di Berry e torna 1 se qualcosa va storto. Le 4 funzioni vos_berry_* sono quelle che l'interprete chiede al firmware. */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "berry.h"
int main(int c, char** v) {
  if (c < 2) return 2;
  FILE* f = fopen(v[1], "rb"); if (!f) return 2;
  fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f);
  char* b = calloc(n + 1, 1); if (fread(b, 1, n, f) != (size_t)n) return 2;
  bvm* vm = be_vm_new();
  int r = be_loadstring(vm, b);
  if (!r) r = be_pcall(vm, 0);
  if (r) { be_dumpexcept(vm); return 1; }
  return 0;
}
void* vos_berry_malloc(size_t n) { return malloc(n); }
void vos_berry_free(void* p) { free(p); }
void* vos_berry_realloc(void* p, size_t n) { return realloc(p, n); }
void vos_berry_write(const char* b, size_t l) { fwrite(b, 1, l, stdout); }
