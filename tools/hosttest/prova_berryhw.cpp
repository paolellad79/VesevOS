// VesevOS - prova sul computer: la REGISTRAZIONE VERA delle funzioni Berry gpio_*, i2c_*, spi_transfer, uptime, delay_us (packages/vos_berry_hw.cpp)
// con un interprete Berry vero e il nucleo hardware finto. Prova: gruppi in use=, nome del driver in use=, scrittura solo per l'Admin
// ("permesso negato" a chi non puo, ma la funzione c'e sempre), errori del bus come eccezioni, script (Admin) senza limiti. Uso: prova_berryhw.sh
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include "berry.h"
#include "vos_berry_int.h"
#include "vos_hw.h"
#include "mini.h"

namespace vosberry {
volatile bool g_stop = false;
static std::string g_use;
bool useHas(const char* w) { std::string u = " " + g_use + " "; return u.find(" " + std::string(w) + " ") != std::string::npos; }
}

static int g_calls = 0, g_errore = 0;
static uint8_t g_ultimo[8];
int hwGpioMode(const char*, int, int) { g_calls++; return g_errore; }
int hwGpioRead(const char*, int idx) { g_calls++; return g_errore ? g_errore : (idx == 0 ? 1 : 0); }
int hwGpioWrite(const char*, int, int) { g_calls++; return g_errore; }
int hwI2cProbe(const char*) { g_calls++; return g_errore ? g_errore : 1; }
int hwI2cRead(const char*, int reg, uint8_t* b, int n) { g_calls++; if (g_errore) return g_errore; for (int i = 0; i < n; i++) b[i] = (uint8_t)(reg + i); return n; }
int hwI2cWrite(const char*, int, const uint8_t* b, int n) { g_calls++; if (g_errore) return g_errore; if (n) memcpy(g_ultimo, b, n > 8 ? 8 : n); return n; }
int hwSpiTransfer(const char*, const uint8_t* tx, uint8_t* rx, int n) { g_calls++; if (g_errore) return g_errore; for (int i = 0; i < n; i++) rx[i] = (uint8_t)(tx[i] + 1); return n; }
int hwDelayUs(int us) { return us > 10000 ? 10000 : us; }
uint32_t hwUptime() { return 42; }
void berryPutInt(bvm*, const char*, long) {}
extern "C" {
void* vos_berry_malloc(size_t n) { return malloc(n); }
void vos_berry_free(void* p) { free(p); }
void* vos_berry_realloc(void* p, size_t n) { return realloc(p, n); }
void vos_berry_write(const char* b, size_t l) { fwrite(b, 1, l, stdout); }
}

// Esegue il codice con le funzioni registrate come fa il firmware. Ritorna 0 = bene; in err il testo dell'eccezione.
static int esegui(const char* use, bool app, bool admin, const char* code, std::string& err) {
  vosberry::g_use = use;
  bvm* vm = be_vm_new();
  berryHwRegister(vm, app, admin);
  int r = be_loadstring(vm, code);
  if (!r) r = be_pcall(vm, 0);
  err.clear();
  if (r) { const char* m = be_tostring(vm, -2); const char* d = be_tostring(vm, -1); err = std::string(m ? m : "") + ": " + (d ? d : ""); }
  be_vm_delete(vm);
  return r;
}
static bool bene(const char* use, bool app, bool admin, const char* code) { std::string e; int r = esegui(use, app, admin, code, e); if (r) printf("  (%s -> %s)\n", code, e.c_str()); return r == 0; }
static bool male(const char* use, bool app, bool admin, const char* code, const char* atteso) {
  std::string e; int r = esegui(use, app, admin, code, e);
  bool ok = r != 0 && e.find(atteso) != std::string::npos;
  if (!ok) printf("  (%s -> r=%d %s; atteso: %s)\n", code, r, e.c_str(), atteso);
  return ok;
}

// Funzionalita tolta dall'utente (vos_func.cpp): qui un interruttore finto, la prova vera della logica e prova_prog.
static bool g_funcOff = false;
void berryNeedFunc(bvm* vm, const char* g) {
  if (!g_funcOff) return;
  char m[64]; snprintf(m, sizeof m, "funzionalita %s spenta (Sistema > Funzionalita)", g);
  be_raise(vm, "permission_error", m);
}

int main() {
  // app Operatore con i2c e il driver in use=
  CHECK(bene("i2c bmp", true, false, "var b = i2c_read('bmp', 0xD0, 3)\nif size(b) != 3 || b[0] != 0xD0 || b[2] != 0xD2 raise 'x' end"));
  CHECK(bene("i2c bmp", true, false, "if !i2c_probe('bmp') raise 'x' end"));
  CHECK(male("i2c bmp", true, false, "i2c_write('bmp', 0xF4, bytes('25'))", "permesso negato"));        // la funzione c'e, ma scrive solo l'Admin
  CHECK(male("i2c bmp", true, false, "i2c_read('altro', 0, 1)", "permesso mancante"));                  // il driver non e in use=
  CHECK(male("i2c bmp", true, false, "gpio_read('bmp', 0)", "gpio_read"));                              // il gruppo gpio non e in use=: la funzione non c'e
  CHECK(male("i2c bmp", true, false, "uptime()", "uptime"));
  CHECK(male("i2c bmp", true, false, "i2c_read(5, 0, 1)", "nome del driver"));
  CHECK(male("i2c bmp", true, false, "i2c_read('bmp', 'a', 1)", "intero"));
  CHECK(male("i2c bmp", true, false, "i2c_read('bmp')", "intero"));
  // app Admin con tutti i gruppi
  CHECK(bene("i2c spi gpio time bmp", true, true, "i2c_write('bmp', 0xF4, bytes('2F'))"));
  CHECK(g_ultimo[0] == 0x2F);
  CHECK(bene("i2c spi gpio time bmp", true, true, "var r = spi_transfer('bmp', bytes('010203'))\nif r[0] != 2 || r[2] != 4 raise 'x' end"));
  CHECK(bene("i2c spi gpio time bmp", true, true, "gpio_mode('bmp', 0, 3)\ngpio_write('bmp', 0, 1)\nif gpio_read('bmp', 0) != 1 raise 'x' end"));
  CHECK(bene("i2c spi gpio time bmp", true, true, "if uptime() != 42 || delay_us(99999) != 10000 raise 'x' end"));
  CHECK(male("spi bmp", true, false, "spi_transfer('bmp', bytes('00'))", "permesso negato"));
  CHECK(male("gpio bmp", true, false, "gpio_mode('bmp', 0, 1)", "permesso negato"));
  CHECK(male("gpio bmp", true, false, "gpio_write('bmp', 0, 1)", "permesso negato"));
  CHECK(male("i2c spi bmp", true, true, "spi_transfer('bmp', bytes('" "0001020304050607080910111213141516171819202122232425262728293031323334353637383940414243444546474849505152535455565758596061626364" "'))", "grande"));
  // funzionalita tolta dall'utente: le funzioni ci sono (un file che le nomina compila) e rispondono "spenta"
  g_funcOff = true;
  CHECK(male("i2c bmp", true, false, "i2c_probe('bmp')", "spenta"));
  CHECK(male("i2c spi gpio time bmp", true, true, "gpio_read('bmp', 0)", "spenta"));
  CHECK(male("i2c spi gpio time bmp", true, true, "uptime()", "spenta"));
  g_funcOff = false;
  CHECK(bene("i2c bmp", true, false, "if !i2c_probe('bmp') raise 'x' end"));
  // errori del bus come eccezioni
  g_errore = HW_E_IO;
  CHECK(male("i2c bmp", true, false, "i2c_read('bmp', 0, 1)", "errore del bus"));
  g_errore = HW_E_OCCUPATO;
  CHECK(male("i2c bmp", true, false, "i2c_probe('bmp')", "occupato"));
  g_errore = HW_E_NONTROVATO;
  CHECK(male("i2c bmp", true, false, "i2c_read('bmp', 0, 1)", "non ha questo bus"));
  g_errore = HW_E_ARG;
  CHECK(male("i2c bmp", true, false, "i2c_read('bmp', 0, 1)", "non valido"));
  g_errore = 0;
  // script: tutto registrato, nessun nome da mettere in use=
  CHECK(bene("", false, true, "i2c_write('qualunque', 1, bytes('01'))\nuptime()"));
  CHECK(male("", false, false, "i2c_write('qualunque', 1, bytes('01'))", "permesso negato"));           // script non lanciato dall'Admin
  return done("prova_berryhw");
}
