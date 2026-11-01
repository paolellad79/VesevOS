/* VesevOS - prova sul computer: una funzione nativa che restituisce una mappa/lista deve poter essere letta dallo script (m['k'], l[0]).
   Senza berryWrap la mappa e "grezza" e Berry dice "value 'map' does not support subscriptable". Uso: prova_wrap.sh */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "berry.h"
static void put(bvm* vm, const char* k, int v) { be_pushstring(vm, k); be_pushint(vm, v); be_data_insert(vm, -3); be_pop(vm, 2); }
static void wrap(bvm* vm, const char* cls) {   /* stesso codice di berryWrap in vos_berry_files.cpp */
  be_getbuiltin(vm, cls); be_pushvalue(vm, -2); be_call(vm, 1); be_moveto(vm, -2, -3); be_pop(vm, 2);
}
static int nativoGrezzo(bvm* vm) { be_newmap(vm); put(vm, "state", 1); be_return(vm); }
static int nativoVero(bvm* vm) {
  be_newmap(vm);
  be_pushstring(vm, "items"); be_newlist(vm);
  be_newmap(vm); put(vm, "size", 7); wrap(vm, "map"); be_data_push(vm, -2); be_pop(vm, 1);
  wrap(vm, "list"); be_data_insert(vm, -3); be_pop(vm, 2);
  put(vm, "next", 3); wrap(vm, "map"); be_return(vm);
}
static int prova(bvm* vm, const char* src) {
  if (be_loadstring(vm, src) || be_pcall(vm, 0)) { be_dumpexcept(vm); return 1; }
  return 0;
}
int main(void) {
  bvm* vm = be_vm_new();
  be_regfunc(vm, "grezzo", nativoGrezzo); be_regfunc(vm, "vero", nativoVero);
  int r = prova(vm, "var m = grezzo()\nprint(m['state'])");
  printf(r ? "grezzo: errore, come previsto\n" : "grezzo: ERRORE, doveva fallire\n");
  if (!r) return 1;
  if (prova(vm, "var m = vero()\nif m['next'] != 3 || m['items'][0]['size'] != 7 || size(m['items']) != 1 raise 'x' end\nprint('vero: ok')")) return 1;
  return 0;
}
void* vos_berry_malloc(size_t n) { return malloc(n); }
void vos_berry_free(void* p) { free(p); }
void* vos_berry_realloc(void* p, size_t n) { return realloc(p, n); }
void vos_berry_write(const char* b, size_t l) { fwrite(b, 1, l, stdout); }
