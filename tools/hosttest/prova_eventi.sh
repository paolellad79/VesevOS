#!/bin/bash
# Prova sul computer del registro degli Eventi di sicurezza (senza Arduino): accorpamento, giro del buffer, testo JSON, nomi con virgolette.
# Uso: tools/hosttest/prova_eventi.sh
set -e
R=$(cd "$(dirname "$0")/../.." && pwd); O=$(mktemp -d)
mkdir -p "$O/inc/freertos"
cat > "$O/inc/esp_timer.h" <<'H'
#pragma once
#include <stdint.h>
extern unsigned long g_ms;
inline int64_t esp_timer_get_time() { return (int64_t)g_ms * 1000; }
H
touch "$O/inc/esp_rom_sys.h"
cat > "$O/inc/freertos/task.h" <<'H'
#pragma once
#define pdMS_TO_TICKS(x) (x)
#define taskYIELD()
inline void vTaskDelay(int) {}
inline void esp_rom_delay_us(unsigned) {}
H
cat > "$O/inc/freertos/FreeRTOS.h" <<'H'
#pragma once
#define portMAX_DELAY 0
H
cat > "$O/inc/freertos/semphr.h" <<'H'
#pragma once
typedef void* SemaphoreHandle_t;
static int g_finto;
inline SemaphoreHandle_t xSemaphoreCreateRecursiveMutex() { return &g_finto; }
inline int xSemaphoreTakeRecursive(SemaphoreHandle_t, int) { return 1; }
inline int xSemaphoreGiveRecursive(SemaphoreHandle_t) { return 1; }
H
cat > "$O/prova.cpp" <<'CPP'
#include <stdio.h>
#include <string.h>
#include <string>
#include <stdint.h>
#include <time.h>
#include "core/vos_heapbuf.h"
#include "core/vos_eventi.h"

static int falliti = 0;
static void vero(bool c, const char* nome) { if (!c) { printf("KO %s\n", nome); falliti++; } }
static uint32_t conta(const std::string& s, const char* x) { uint32_t n = 0; size_t p = 0; while ((p = s.find(x, p)) != std::string::npos) { n++; p += strlen(x); } return n; }
extern unsigned long g_ms; extern bool g_ora;

static std::string testo() { HeapBuf j(16384); vero(eventiJson(j), "json"); return std::string(j.c_str()); }

int main() {
  vero(conta(testo(), "\"k\":") == 0, "vuoto");
  for (int i = 0; i < 5; i++) { g_ms += 1000; eventoNota(EV_PASSWORD, EVC_WEB, 0xC0A80132u, "admin"); }
  std::string t = testo();
  vero(conta(t, "\"k\":") == 1, "voci uguali unite");
  vero(t.find("\"n\":5") != std::string::npos, "contatore");
  vero(t.find("192.168.1.50") != std::string::npos, "indirizzo");
  vero(t.find("\"ult24\":5") != std::string::npos, "ultime 24 ore");
  g_ms += 700000;
  eventoNota(EV_PASSWORD, EVC_WEB, 0xC0A80132u, "admin");
  vero(conta(testo(), "\"k\":") == 2, "dopo 10 minuti voce nuova");
  eventoNota(EV_PASSWORD, EVC_WEB, 0xC0A80132u, "");
  eventoNota(EV_MFA, EVC_WEB, 0xC0A80132u, "admin");
  eventoNota(EV_PASSWORD, EVC_SERIALE, 0, "admin");
  vero(conta(testo(), "\"k\":") == 5, "tipo, canale e nome distinguono");
  vero(testo().find("\"i\":\"\"") != std::string::npos, "seriale senza indirizzo");
  eventoNota(EV_MFA, EVC_WEB, 1, "a\"b\\c");
  vero(testo().find("a\\\"b\\\\c") != std::string::npos, "nome protetto nel JSON");
  vero(eventiBlocchiNuovi() == 0, "nessun blocco");
  eventoNota(EV_BLOCCO, EVC_WEB, 2); eventoNota(EV_BLOCCO, EVC_WEB, 2);
  vero(eventiBlocchiNuovi() == 2, "blocchi contati");
  eventiBlocchiVisti();
  vero(eventiBlocchiNuovi() == 0, "blocchi visti");
  for (uint32_t i = 0; i < 300; i++) eventoNota(EV_FILTRO, EVC_WEB, 1000 + i);
  t = testo();
  vero(conta(t, "\"k\":") == EVENTI_VOCI, "buffer pieno senza crescere");
  vero(t.find("\"tot\":[8,2,2,300,0,0]") != std::string::npos, "totali oltre il buffer");
  for (uint32_t i = 0; i < 70000; i++) eventoNota(EV_RICHIESTE, EVC_WEB, 5);
  vero(testo().find("\"n\":65535") != std::string::npos, "contatore fermo al massimo");
  eventoNota((EventoTipo)99, EVC_WEB, 5);
  g_ora = false;
  eventiSvuota();
  eventoNota(EV_PASSWORD, EVC_WEB, 5);
  t = testo();
  vero(conta(t, "\"k\":") == 1 && t.find("\"q\":0") != std::string::npos, "senza ora la voce resta");
  vero(t.find("\"ora\":false") != std::string::npos, "ora non valida");
  char lungo[64]; memset(lungo, 'x', 63); lungo[63] = 0;
  eventoNota(EV_MFA, EVC_WEB, 6, lungo);
  vero(testo().find(std::string(20, 'x') + "\"") != std::string::npos, "nome lungo tagliato a 20");
  eventiSvuota();
  vero(testo().find("\"tot\":[0,0,0,0,0,0]") != std::string::npos, "svuotato");
  if (falliti) { printf("falliti: %d\n", falliti); return 1; }
  printf("ok\n");
  return 0;
}
CPP
cat > "$O/finti.cpp" <<'CPP'
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>
unsigned long g_ms = 1000; bool g_ora = true;
extern "C" time_t time(time_t* t) { time_t v = g_ora ? 1800000000 : 0; if (t) *t = v; return v; }   // vos_eventi legge l'ora direttamente
CPP
c++ -std=gnu++17 -w -I"$O/inc" -I"$R/VesevOS/src" "$O/prova.cpp" "$O/finti.cpp" "$R/VesevOS/src/core/vos_eventi.cpp" -o "$O/prova" 2>&1 | head -20
"$O/prova"
rm -rf "$O"
