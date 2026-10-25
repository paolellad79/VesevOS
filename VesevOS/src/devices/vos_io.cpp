// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_io.cpp
#include "vos_io.h"
#include "vos_pins.h"
#include "vos_io_int.h"
#include "vos_dev.h"
#include "../core/vos_common.h"
#include "../core/vos_i18n.h"
#include "../core/vos_log.h"
#include "../core/vos_util.h"
#include "../core/vos_parse.h"
#include "../core/vos_config.h"
#include "../drivers/vos_drv_gpio.h"
#include "../drivers/vos_drv_i2c.h"
#include "vos_bmp280.h"
#include "../drivers/vos_drv_fs.h"
#if VOS_WITH_SD
#include "../drivers/vos_drv_sd.h"
#endif
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "../drivers/vos_drv_time.h"
#define IO_MIN_SWITCH_MS 1000UL
#define IO_LINE_MAX 192                              // riga di io.cfg: oltre si scarta
#define IO_SD_KHZ_MIN 400                            // frequenza SPI della microSD: da 400 kHz (prova iniziale, fili lunghi) a 20 MHz
#define IO_SD_KHZ_MAX 20000
#define IO_SD_KHZ_DEF 1000                           // di fabbrica 1 MHz: prudente per i fili volanti dei moduli economici
#if VOS_WITH_SD
#define IO_NCLS 7                                   // con la microSD la classe "sd" e la settima
#else
#define IO_NCLS 6
#endif

static IoDev g_io[IO_MAX];
static int g_n = 0;

// un solo blocco per tutte le modifiche e per il loop: web, terminale e app (Berry) sono task diversi
static SemaphoreHandle_t g_iomx = nullptr;
struct IoLock {
  bool held, ok;
  explicit IoLock(uint32_t ticks = portMAX_DELAY) { held = g_iomx && xSemaphoreTakeRecursive(g_iomx, ticks) == pdTRUE; ok = held || !g_iomx; }
  ~IoLock() { if (held) xSemaphoreGiveRecursive(g_iomx); }
};

[[maybe_unused]] static void ioKeys() { tr("Periferiche"); }

static const char* const CN[] = {"", "rele", "led", "pin", "ingresso", "analogico", "i2c", "sd"};
static const char* const PFX[] = {"", "rele", "led", "pin", "in", "adc", "bmp", "sd"};
#define DRV_BMP280 1

// un solo sensore I2C: misura nel loop (ioTick), la pagina e le app leggono solo l'ultimo valore
static struct I2cState { bool up, have, armed; uint8_t fails, err, chip; int32_t t100; uint32_t pPa; uint32_t nextAt; Bmp280Cal cal; } g_i2c;
static bool g_i2cBus = false;
static uint32_t g_i2cKey = 0;                      // SDA, SCL e velocita con cui il bus e acceso

bool ioParseCls(const char* s, uint8_t& c) {
  for (uint8_t i = 1; i <= IO_NCLS; i++) if (!strcmp(s, CN[i])) { c = i; return true; }
  if (!strcmp(s, "relay")) { c = IC_RELE; return true; }
  if (!strcmp(s, "in") || !strcmp(s, "input")) { c = IC_IN; return true; }
  if (!strcmp(s, "adc")) { c = IC_ADC; return true; }
  return false;
}
static bool isOut(uint8_t c) { return c == IC_RELE || c == IC_LED || c == IC_PIN; }
static int find(const char* n) { for (int i = 0; i < g_n; i++) if (!strcmp(n, g_io[i].name)) return i; return -1; }
static int findAux(const char* n, bool& aux) {           // "bmp1.p" = pressione del sensore bmp1
  aux = false; int i = find(n); if (i >= 0) return i;
  size_t len = strlen(n);
  if (len > 2 && len - 2 <= IO_NAME_MAX && !strcmp(n + len - 2, ".p")) {
    char b[IO_NAME_MAX + 1]; memcpy(b, n, len - 2); b[len - 2] = 0;
    i = find(b); if (i >= 0 && g_io[i].cls == IC_I2C) { aux = true; return i; }
  }
  return -1;
}
bool ioHas(const char* n) { bool a; return findAux(n, a) >= 0; }
int  ioCount() { return g_n; }
bool ioNameOk(const char* n, ErrBuf& err) { const char* why = ioNameCheck(n); if (why) err = why; return !why; }   // forma e parole riservate (vos_io_name.cpp)
const char* ioNameAt(int i) { return (i >= 0 && i < g_n) ? g_io[i].name : ""; }

// Scrittura sicura di io.cfg: vedi vos_io_cfg.cpp. skip = periferica da non scrivere, extra = una in piu.
static const char* save(int skip = -1, const IoDev* extra = nullptr) { return ioCfgSave(g_io, g_n, skip, extra); }

static bool levelOn(const IoDev& d, bool on) { return (d.flags & IOF_INV) ? !on : on; }   // livello fisico da stato logico
static void outWrite(IoDev& d, bool on) { d.on = on; drvGpioWrite(d.gpio, levelOn(d, on)); d.last = drvMillis(); }

static void hwSetup(IoDev& d) {
  if (isOut(d.cls)) { drvGpioWrite(d.gpio, levelOn(d, false)); drvGpioMode(d.gpio, DG_OUTPUT); d.on = false; }   // prima il livello, poi uscita: nessun impulso
  else if (d.cls == IC_I2C) { g_i2c = I2cState(); }                       // il bus lo avvia ioTick
  else if (d.cls == IC_SD) {}                                              // i pin li prende lo SPI quando si monta la scheda (vos_sd): qui non si toccano
  else if (d.cls == IC_IN) drvGpioMode(d.gpio, (d.flags & IOF_UP) ? DG_INPUT_PULLUP : (d.flags & IOF_DOWN) ? DG_INPUT_PULLDOWN : DG_INPUT);
  else drvGpioMode(d.gpio, DG_INPUT);
}

static bool hasI2c() { for (int i = 0; i < g_n; i++) if (g_io[i].cls == IC_I2C) return true; return false; }
static int sdMounted() {                                        // microSD: 1 = montata
#if VOS_WITH_SD
  return drvSdUp() ? 1 : 0;
#else
  return 0;
#endif
}
static bool hasSd() { for (int i = 0; i < g_n; i++) if (g_io[i].cls == IC_SD) return true; return false; }
static const char* gpioBlock(int gpio, uint8_t cls) {
  const char* why = pinTestBlock(gpio);
  if (why) return why;
  if (cls == IC_SD && VOS_SD_RESERVED(gpio)) return tr("Pin riservato dal sistema (avvio, USB, console, JTAG, I2C o memoria)");
  static_assert(VOS_ADC_FIRST == 1 && VOS_ADC_LAST == 10, "il testo qui sotto dice 1-10: aggiornarlo (e lang_src.json) per questa scheda");
  if (cls == IC_ADC && !(gpio >= VOS_ADC_FIRST && gpio <= VOS_ADC_LAST)) return tr("Analogico: solo i pin 1-10 (gli altri disturbano il Wi-Fi)");
  return nullptr;
}

static int devPins(const IoDev& d, uint8_t* p) {          // i pin della periferica: 1 (normale), 2 (i2c: SDA SCL), 4 (sd: CS MOSI SCK MISO)
  p[0] = (uint8_t)d.gpio;
  if (d.cls == IC_I2C) { p[1] = (uint8_t)d.gpio2; return 2; }
  if (d.cls == IC_SD) { p[1] = (uint8_t)d.gpio2; p[2] = d.addr; p[3] = d.drv; return 4; }
  return 1;
}
static bool claimDev(const IoDev& d) {
  uint8_t p[4]; int n = devPins(d, p);
  for (int i = 0; i < n; i++) if (!pinClaim(p[i], d.name, CN[d.cls])) { for (int k = 0; k < i; k++) pinRelease(p[k]); return false; }
  return true;
}
static void releaseDev(const IoDev& d) { uint8_t p[4]; int n = devPins(d, p); for (int i = 0; i < n; i++) pinRelease(p[i]); }

// Una riga di io.cfg in line (senza \n). false = fine del file. too = la riga non ci sta: si legge e si butta il resto fino al \n (mai la coda come riga nuova).
static bool readLine(VosFile& f, char* line, size_t cap, bool& too) {
  size_t k = 0; too = false;
  for (;;) {
    int ch = f.read();
    if (ch < 0) { line[k] = 0; return k > 0 || too; }       // fine del file: una riga senza \n finale conta
    if (ch == '\n') break;
    if (k + 1 < cap) line[k++] = (char)ch; else too = true;
  }
  line[k] = 0; return true;
}

void ioInit() {
  if (!g_iomx) g_iomx = xSemaphoreCreateRecursiveMutex();
  IoLock L;
  g_n = 0; ioCfgReset();
  const char* src = IO_FILE;
  bool hasMain = drvFsExists(IO_FILE);
  int st = 0;                                           // 1 = a 0 byte (le versioni vecchie lo lasciavano togliendo l'ultima periferica), 2 = non si apre
  if (hasMain) { VosFile c = drvFsOpen(IO_FILE, "r"); if (!c) st = 2; else if (c.size() == 0) st = 1; c.close(); }
  if (!hasMain || st) {
    if (drvFsExists(IO_BAK)) {
      src = IO_BAK; ioCfgPend(1);
      vlog("ERRORE: IO: %s %s: uso la copia %s", IO_FILE, hasMain ? "vuoto o illeggibile" : "manca", IO_BAK);
    } else {
      if (st == 2) { ioCfgPend(2); vlog("ERRORE: IO: %s illeggibile e senza copia: nessuna periferica", IO_FILE); }
      return;
    }
  }
  VosFile f = drvFsOpen(src, "r");
  if (!f) { ioCfgPend(2); vlog("ERRORE: IO: non riesco a leggere %s", src); return; }
  char line[IO_LINE_MAX];                               // una riga alla volta (quelle valide sono sotto 100 caratteri)
  bool tooLong;
  while (g_n < IO_MAX && readLine(f, line, sizeof line, tooLong)) {
    if (tooLong) { vlog("IO: riga scartata (troppo lunga)"); continue; }
    std::string_view tv = trimView(line);                // come String::trim(): spazi bianchi ai lati
    if (tv.empty()) continue;
    char* l = line + (tv.data() - line); l[tv.size()] = 0;
    // riga: <nome> <classe> <gpio> <opzioni> (lettura a mano, niente sscanf)
    char nm[16]; unsigned c = 0, fl = 0, ad = 0, dr = 0, kh = 1000; int g = -1, g2 = -1;
    {
      const char* p = l; size_t k = 0;
      while (*p && *p != ' ' && *p != '\t') { if (k + 1 < sizeof(nm)) nm[k++] = *p; p++; }
      nm[k] = 0;
      long x[7]; int got = 0; char* e;
      while (got < 7) { x[got] = strtol(p, &e, 10); if (e == p) break; p = e; got++; }
      if (!k || (got != 3 && got != 6 && got != 7) || x[0] < 0 || x[2] < 0) continue;
      c = (unsigned)x[0]; g = (int)x[1]; fl = (unsigned)x[2];
      if (got >= 6) { g2 = (int)x[3]; ad = (unsigned)x[4]; dr = (unsigned)x[5]; }
      if (got == 7) kh = (unsigned)x[6];
    }
    if (c < 1 || c > 7 || ioNameCheck(nm) || find(nm) >= 0) { vlog("IO: riga scartata (%s)", l); continue; }
    if (c > IO_NCLS) { vlog("IO: %s e una microSD ma questo firmware e senza microSD (la riga resta nel file)", nm); ioCfgKeepRow(l); continue; }
    if (c == IC_I2C && (g2 < 0 || g2 == g || ad < 0x08 || ad > 0x77 || dr != DRV_BMP280 || hasI2c())) { vlog("IO: riga scartata (%s)", l); continue; }
    if (c == IC_SD && (g2 < 0 || g < 0 || g == g2 || g == (int)ad || g == (int)dr || g2 == (int)ad || g2 == (int)dr || ad == dr || ad > 48 || dr > 48 || kh < IO_SD_KHZ_MIN || kh > IO_SD_KHZ_MAX || hasSd())) { vlog("IO: riga scartata (%s)", l); continue; }
    const char* why = gpioBlock(g, (uint8_t)c);
    if (!why && (c == IC_I2C || c == IC_SD)) why = gpioBlock(g2, (uint8_t)c);
    if (!why && c == IC_SD) { why = gpioBlock((int)ad, (uint8_t)c); if (!why) why = gpioBlock((int)dr, (uint8_t)c); }
    if (why) { vlog("IO: %s scartata, GPIO%d: %s (la riga resta nel file)", nm, g, why); ioCfgKeepRow(l); continue; }
    IoDev& d = g_io[g_n];
    memset(&d, 0, sizeof d);
    strncpy(d.name, nm, sizeof(d.name) - 1); d.cls = (uint8_t)c; d.gpio = (int8_t)g; d.flags = (uint8_t)fl;
    if (c == IC_I2C || c == IC_SD) { d.gpio2 = (int8_t)g2; d.addr = (uint8_t)ad; d.drv = (uint8_t)dr; if (c == IC_SD) d.khz = (uint16_t)kh; }
    if (!claimDev(d)) { vlog("IO: %s scartata, GPIO%d gia usato (la riga resta nel file)", nm, g); ioCfgKeepRow(l); continue; }
    hwSetup(d); g_n++;
  }
  f.close();
  if (g_n) vlog("IO: %d periferiche, uscite spente", g_n);
}

// Nome di una periferica nuova in name (12 byte): quello dato, ripulito, oppure automatico (rele1, led1...). nullptr = pronto, altrimenti il motivo.
static const char* pickName(const char* nameIn, uint8_t c, char* name) {
  const char* why = ioNameNorm(nameIn, name);
  if (why) return why;
  if (!name[0]) {
    for (int k = 1; k < 100 && !name[0]; k++) { char t[16]; snprintf(t, sizeof t, "%s%d", PFX[c], k); if (find(t) < 0 && !ioNameReserved(t)) strcpy(name, t); }
    if (!name[0]) return tr("Troppe periferiche (massimo 16)");
  }
  why = ioNameCheck(name);
  if (why) return why;
  return find(name) >= 0 ? tr("Nome gia usato") : nullptr;
}

bool ioAdd(const char* cls, int gpio, const char* nameIn, const char* opt, char* outName, ErrBuf& err) {
  IoLock L;
  uint8_t c;
  if (!ioParseCls(cls, c)) { err = tr("Classe non valida (rele, led, pin, ingresso, analogico, i2c, sd)"); return false; }
  if (c == IC_SD) {                                     // opzioni: mosi=N sck=N miso=N (il pin e CS)
    int v[4] = {-1, -1, -1, IO_SD_KHZ_DEF}; static const char* const K[4] = {"mosi=", "sck=", "miso=", "khz="}; char o[16] = "", eb[IO_ERR_MAX] = "";
    if (nameIn[0] && !ioNameOk(nameIn, err)) return false;           // il nome della microSD, se dato, deve essere gia valido (minuscole e numeri)
    for (int k = 0; k < 4; k++) { const char* q = strstr(opt, K[k]); if (q) v[k] = (int)strtol(q + strlen(K[k]), nullptr, 0); }
    if (!ioAddSd(gpio, v[0], v[1], v[2], v[3], nameIn, o, eb)) { err = eb; return false; }
    snprintf(outName, 16, "%s", o); return true;
  }
  if (c == IC_I2C) {                                    // opzioni: scl=N addr=0x76 drv=bmp280 fast
    int scl = gpio + 1, addr = 0x76; char drv[16] = "bmp280", o[16] = "", eb[IO_ERR_MAX] = ""; bool fast = strstr(opt, "fast") != nullptr;
    const char* q = strstr(opt, "scl="); if (q) scl = (int)strtol(q + 4, nullptr, 0);
    q = strstr(opt, "addr="); if (q) addr = (int)strtol(q + 5, nullptr, 0);
    q = strstr(opt, "drv=");
    if (q) { const char* a = q + 4; size_t k = 0; while (a[k] && a[k] != ',' && a[k] != ' ' && k < sizeof drv - 1) { drv[k] = a[k]; k++; } drv[k] = 0; }
    if (!ioAddI2c(drv, gpio, scl, addr, fast, nameIn, o, eb)) { err = eb; return false; }
    snprintf(outName, 16, "%s", o); return true;
  }
  if (g_n >= IO_MAX) { err = tr("Troppe periferiche (massimo 16)"); return false; }
  const char* why = gpioBlock(gpio, c);
  if (why) { err = why; return false; }
  char name[IO_NAME_MAX + 1];
  const char* bad = pickName(nameIn, c, name);
  if (bad) { err = bad; return false; }
  uint8_t fl = 0;
  if (strstr(opt, "inv") && (isOut(c) || c == IC_IN)) fl |= IOF_INV;
  if (c == IC_IN) { if (strstr(opt, "up")) fl |= IOF_UP; else if (strstr(opt, "down")) fl |= IOF_DOWN; }
  IoDev& d = g_io[g_n];
  memset(&d, 0, sizeof d);
  strncpy(d.name, name, sizeof(d.name) - 1); d.cls = c; d.gpio = (int8_t)gpio; d.flags = fl;
  if (!claimDev(d)) { err = tr("Gia usato dal sistema"); return false; }
  const char* se = save(-1, &d);
  if (se) { releaseDev(d); memset(&d, 0, sizeof d); err = se; return false; }
  hwSetup(d); g_n++;
  snprintf(outName, 16, "%s", name);
  vlog("IO: aggiunta %s (%s) su GPIO%d", d.name, CN[c], gpio);
  return true;
}

static bool i2cErr(char* err, const char* msg) { strncpy(err, msg, IO_ERR_MAX - 1); err[IO_ERR_MAX - 1] = 0; return false; }
bool ioAddI2c(const char* drv, int sda, int scl, int addr, bool fast, const char* nameIn, char* outName, char* err) {
  IoLock L;
  if (strcmp(drv, "bmp280")) return i2cErr(err, tr("Driver I2C sconosciuto (ammesso: bmp280)"));
  if (hasI2c()) return i2cErr(err, tr("C'e gia un sensore I2C (un solo bus)"));
  if (g_n >= IO_MAX) return i2cErr(err, tr("Troppe periferiche (massimo 16)"));
  if (addr != 0x76 && addr != 0x77) return i2cErr(err, tr("Indirizzo I2C del BMP280: 0x76 oppure 0x77"));
  if (sda == scl) return i2cErr(err, tr("SDA e SCL devono essere due pin diversi"));
  const char* why = gpioBlock(sda, IC_I2C);
  if (!why) why = gpioBlock(scl, IC_I2C);
  if (why) return i2cErr(err, why);
  char name[IO_NAME_MAX + 1];
  const char* bad = pickName(nameIn, IC_I2C, name);
  if (bad) return i2cErr(err, bad);
  IoDev& d = g_io[g_n];
  memset(&d, 0, sizeof d);
  strncpy(d.name, name, sizeof(d.name) - 1); d.cls = IC_I2C; d.gpio = (int8_t)sda; d.gpio2 = (int8_t)scl; d.addr = (uint8_t)addr; d.drv = DRV_BMP280;
  if (fast) d.flags |= IOF_FAST;
  if (!claimDev(d)) return i2cErr(err, tr("Gia usato dal sistema"));
  const char* se = save(-1, &d);
  if (se) { releaseDev(d); memset(&d, 0, sizeof d); return i2cErr(err, se); }
  hwSetup(d); g_n++;
  strncpy(outName, d.name, 15); outName[15] = 0;
  vlog("IO: aggiunto %s (bmp280 0x%02X) su SDA GPIO%d, SCL GPIO%d", d.name, addr, sda, scl);
  return true;
}

static bool nameTaken(const char* n) { for (int i = 0; i < g_n; i++) if (!strcmp(g_io[i].name, n)) return true; return false; }
bool ioAddSd(int cs, int mosi, int sck, int miso, int khz, const char* nameIn, char* outName, char* err) {
#if VOS_WITH_SD
  IoLock L;
  if (hasSd()) return i2cErr(err, tr("C'e gia una microSD (un solo lettore)"));
  if (g_n >= IO_MAX) return i2cErr(err, tr("Troppe periferiche (massimo 16)"));
  const int P[4] = {cs, mosi, sck, miso};
  for (int i = 0; i < 4; i++) for (int k = i + 1; k < 4; k++) if (P[i] == P[k]) return i2cErr(err, tr("I quattro pin della microSD devono essere diversi"));
  for (int i = 0; i < 4; i++) { if (P[i] < 0 || P[i] > 48) return i2cErr(err, tr("Indica tutti e quattro i pin: CS, MOSI, SCK, MISO")); const char* w = gpioBlock(P[i], IC_SD); if (w) return i2cErr(err, w); }
  if (khz < IO_SD_KHZ_MIN || khz > IO_SD_KHZ_MAX) return i2cErr(err, tr("Frequenza SPI della microSD: da 400 a 20000 kHz (di solito 1000; prova 400 se i fili sono lunghi)"));
  char nm[sizeof g_io[0].name] = "";                   // nome: quello dato (gia controllato da ioAdd) oppure sd1, sd2... (senza String)
  if (nameIn && nameIn[0]) { strncpy(nm, nameIn, sizeof nm - 1); if (nameTaken(nm)) return i2cErr(err, tr("Nome gia usato")); }
  else for (int k = 1; k < 100; k++) { snprintf(nm, sizeof nm, "sd%d", k); if (!nameTaken(nm)) break; }
  IoDev& d = g_io[g_n];
  memset(&d, 0, sizeof d);
  strncpy(d.name, nm, sizeof(d.name) - 1); d.cls = IC_SD; d.gpio = (int8_t)cs; d.gpio2 = (int8_t)mosi; d.addr = (uint8_t)sck; d.drv = (uint8_t)miso; d.khz = (uint16_t)khz;
  if (!claimDev(d)) return i2cErr(err, tr("Gia usato dal sistema"));
  const char* se = save(-1, &d);
  if (se) { releaseDev(d); memset(&d, 0, sizeof d); return i2cErr(err, se); }
  hwSetup(d); g_n++;
  strncpy(outName, d.name, 15); outName[15] = 0;
  vlog("IO: aggiunta %s (microSD) su CS GPIO%d, MOSI GPIO%d, SCK GPIO%d, MISO GPIO%d", d.name, cs, mosi, sck, miso);
  return true;
#else
  return i2cErr(err, tr("Questo firmware e senza microSD"));
#endif
}
bool ioSdPins(int* p) {                                  // p[5]: CS, MOSI, SCK, MISO e kHz della microSD configurata; false = nessuna microSD in io.cfg
  IoLock L;
  for (int i = 0; i < g_n; i++) if (g_io[i].cls == IC_SD) { p[0] = g_io[i].gpio; p[1] = g_io[i].gpio2; p[2] = g_io[i].addr; p[3] = g_io[i].drv; p[4] = g_io[i].khz; return true; }
  return false;
}

// Un nome in ingresso puo puntare dentro g_io (che si sposta quando si toglie una voce): si copia sotto il blocco, prima di toccare la tabella.
static bool copyName(const char* in, char* out) {
  size_t n = strlen(in);
  if (n > IO_NAME_MAX) return false;                     // nessuna periferica ha un nome cosi lungo
  memcpy(out, in, n + 1);
  return true;
}

bool ioRemove(const char* nameIn, ErrBuf& err) {
  IoLock L;
  char name[IO_NAME_MAX + 1];
  int i = copyName(nameIn, name) ? find(name) : -1;
  if (i < 0) { err = tr("Periferica sconosciuta"); return false; }
  const char* se = save(i);                              // prima il file: se non si scrive, la periferica resta com'e
  if (se) { err = se; return false; }
  IoDev d = g_io[i];
  if (isOut(d.cls)) outWrite(g_io[i], false);
#if VOS_WITH_SD
  if (d.cls == IC_SD) drvSdEnd();                          // prima si smonta la scheda, poi si liberano i pin
#endif
  if (d.cls != IC_I2C && d.cls != IC_SD) drvGpioMode(d.gpio, DG_INPUT);       // i pin del bus li lascia liberi ioTick quando ferma Wire
  releaseDev(d);
  for (int k = i; k < g_n - 1; k++) g_io[k] = g_io[k + 1];
  g_n--;
  // i nomi dei pin nel registro puntano ai buffer della tabella: dopo lo spostamento vanno richiesti da capo
  for (int k = 0; k < g_n; k++) { releaseDev(g_io[k]); claimDev(g_io[k]); }
  vlog("IO: tolta %s", name);
  return true;
}

bool ioRename(const char* fromIn, const char* toIn, ErrBuf& err) {
  IoLock L;
  char from[IO_NAME_MAX + 1], to[IO_NAME_MAX + 1];
  int i = copyName(fromIn, from) ? find(from) : -1;
  if (i < 0) { err = tr("Periferica sconosciuta"); return false; }
  const char* why = ioNameNorm(toIn, to);
  if (!why && !strcmp(to, from)) return true;
  if (!why) why = ioNameCheck(to);
  if (!why && find(to) >= 0) why = tr("Nome gia usato");
  if (why) { err = why; return false; }
  char old[sizeof g_io[i].name]; memcpy(old, g_io[i].name, sizeof old);
  memset(g_io[i].name, 0, sizeof g_io[i].name); strncpy(g_io[i].name, to, sizeof(g_io[i].name) - 1);
  const char* se = save();
  if (se) { memcpy(g_io[i].name, old, sizeof old); err = se; return false; }
  vlog("IO: %s ora si chiama %s", from, to);
  return true;
}

bool ioSet(const char* name, int val, int role, ErrBuf& err) {
  int i = find(name);
  if (i < 0) { err = tr("Periferica sconosciuta"); return false; }
  IoDev& d = g_io[i];
  if (!isOut(d.cls)) { err = tr("Questa periferica non e un'uscita"); return false; }
  if (d.cls == IC_RELE && role < ROLE_ADMIN) { err = tr("Il rele lo comanda solo l'Amministratore"); return false; }
  if (role < ROLE_OPER) { err = tr("Il tuo ruolo non permette questo comando"); return false; }
  bool on = val != 0;
  d.offAt = 0; d.blinkEnd = 0;                                  // un comando a mano annulla impulso e lampeggio
  if (on == d.on) return true;
  if (on && d.cls == IC_RELE && d.last && drvMillis() - d.last < IO_MIN_SWITCH_MS) { err = tr("Troppo presto: il rele si riaccende al massimo una volta al secondo"); return false; }
  outWrite(d, on);
  vlog("IO: %s %s", d.name, on ? "acceso" : "spento");
  return true;
}

// Valore della periferica i (aux = la pressione del sensore I2C). nullptr = fatto, altrimenti il motivo.
static const char* getIdx(int i, bool aux, int& val) {
  const IoDev& d = g_io[i];
  if (d.cls == IC_I2C) {
    if (!g_i2c.have) return tr("Il sensore non risponde");
    val = aux ? (int)g_i2c.pPa : (int)g_i2c.t100; return nullptr;
  }
  if (d.cls == IC_SD) { val = sdMounted(); return nullptr; }
  if (isOut(d.cls)) val = d.on ? 1 : 0;
  else if (d.cls == IC_IN) { bool lv = drvGpioRead(d.gpio); val = ((d.flags & IOF_INV) ? !lv : lv) ? 1 : 0; }
  else val = drvGpioAnalog(d.gpio);
  return nullptr;
}
bool ioGet(const char* name, int& val, ErrBuf& err) {
  bool aux; int i = findAux(name, aux);
  if (i < 0) { err = tr("Periferica sconosciuta"); return false; }
  const char* why = getIdx(i, aux, val);
  if (why) err = why;
  return !why;
}
bool ioIsOut(const char* n) { int i = find(n); return i >= 0 && isOut(g_io[i].cls); }
bool ioCanSwitch(const char* n) { return ioIsOut(n); }
const char* ioClassName(const char* n) { bool a; int i = findAux(n, a); return i >= 0 ? CN[g_io[i].cls] : ""; }
int  ioState(const char* n) { int i = find(n); if (i < 0) return 0; if (g_io[i].cls == IC_I2C) return g_i2c.have ? 1 : 0; if (g_io[i].cls == IC_SD) return sdMounted(); return isOut(g_io[i].cls) ? (g_io[i].on ? 1 : 0) : 1; }

void ioAllOff() { for (int i = 0; i < g_n; i++) if (isOut(g_io[i].cls)) { g_io[i].offAt = 0; g_io[i].blinkEnd = 0; if (g_io[i].on) outWrite(g_io[i], false); } }

// Lettura della tabella per vos_io_text.cpp (elenco JSON e testo della shell): sempre dentro il blocco di vos_io
bool ioLockTake(uint32_t ticks) { return g_iomx && xSemaphoreTakeRecursive(g_iomx, ticks) == pdTRUE; }
void ioLockGive() { if (g_iomx) xSemaphoreGiveRecursive(g_iomx); }
const IoDev* ioDevAt(int i) { return (i >= 0 && i < g_n) ? &g_io[i] : nullptr; }
const char* ioClsName(uint8_t cls) { return CN[cls]; }
const char* ioValueAt(int i, int& v) { return getIdx(i, false, v); }
void ioI2cRead(bool& have, int32_t& t100, uint32_t& pPa) { have = g_i2c.have; t100 = g_i2c.t100; pPa = g_i2c.pPa; }

// pin consigliati, in ordine: prima quelli comodi del bordo, per ultimi quelli meno usati. Salta vietati, gia usati e quelli che l'utente ha segnato nell'inventario.
int ioSuggest(uint8_t cls, int n, int* out) {
  static constexpr int8_t C[] = VOS_GPIO_SUGGEST;
  static constexpr int8_t CI[] = VOS_GPIO_SUGGEST_I2C;   // I2C: prima SDA e SCL (i pin di fabbrica di Wire)
  static_assert(sizeof(C) == sizeof(CI), "le due liste dei pin consigliati devono avere la stessa lunghezza");
  static_assert(CI[0] == VOS_I2C_SDA && CI[1] == VOS_I2C_SCL, "la lista I2C deve cominciare con SDA e SCL della scheda");
  const int8_t* L = cls == IC_I2C ? CI : C;
  int k = 0;
  for (size_t i = 0; i < sizeof(C) && k < n; i++) {
    int g = L[i];
    if (gpioBlock(g, cls)) continue;
    if (cfg.pinNote[g].length()) continue;
    out[k++] = g;
  }
  return k;
}

// prove dalla pagina: impulso (si spegne da solo) e lampeggio (solo led e pin: il rele non si fa lampeggiare, si consuma)
bool ioPulse(const char* name, uint32_t ms, int role, ErrBuf& err) {
  int i = find(name);
  if (i < 0) { err = tr("Periferica sconosciuta"); return false; }
  if (ms < 100 || ms > 5000) { err = tr("Valori non validi"); return false; }
  if (!ioSet(name, 1, role, err)) return false;
  g_io[i].offAt = drvMillis() + ms; if (!g_io[i].offAt) g_io[i].offAt = 1;
  return true;
}
bool ioBlink(const char* name, uint32_t sec, int role, ErrBuf& err) {
  int i = find(name);
  if (i < 0) { err = tr("Periferica sconosciuta"); return false; }
  if (g_io[i].cls == IC_RELE) { err = tr("Il rele non si fa lampeggiare"); return false; }
  if (!isOut(g_io[i].cls)) { err = tr("Questa periferica non e un'uscita"); return false; }
  if (role < ROLE_OPER) { err = tr("Il tuo ruolo non permette questo comando"); return false; }
  if (sec < 1 || sec > 10) { err = tr("Valori non validi"); return false; }
  IoDev& d = g_io[i]; d.offAt = 0;
  d.blinkEnd = drvMillis() + sec * 1000UL; if (!d.blinkEnd) d.blinkEnd = 1; d.blinkNext = drvMillis();
  return true;
}
static void i2cFail(uint32_t now, int r) {
  g_i2c.up = false; g_i2c.err = (uint8_t)r; if (g_i2c.fails < 250) g_i2c.fails++;
  if (g_i2c.fails >= 3) g_i2c.have = false;
  uint32_t wait = 2000UL << (g_i2c.fails < 4 ? g_i2c.fails : 4);                  // 4, 8, 16, 32 s: un sensore staccato non occupa il loop
  g_i2c.nextAt = now + (wait > 30000UL ? 30000UL : wait); g_i2c.armed = true;
}
static void i2cTick(uint32_t now) {
  IoDev* d = nullptr;
  for (int i = 0; i < g_n; i++) if (g_io[i].cls == IC_I2C) { d = &g_io[i]; break; }
  if (!d) { if (g_i2cBus) { drvI2cEnd(); g_i2cBus = false; } return; }
  if (g_i2c.armed && (int32_t)(now - g_i2c.nextAt) < 0) return;
  if (!g_i2c.up) {
    uint32_t key = (uint32_t)(uint8_t)d->gpio | ((uint32_t)(uint8_t)d->gpio2 << 8) | ((d->flags & IOF_FAST) ? 0x10000UL : 0);
    if (g_i2cBus && key != g_i2cKey) { drvI2cEnd(); g_i2cBus = false; }            // sensore tolto e rimesso su altri pin
    if (!g_i2cBus) {
      if (!drvI2cBegin(d->gpio, d->gpio2, (d->flags & IOF_FAST) ? 400000UL : 100000UL)) { i2cFail(now, BMP_BUS); return; }
      g_i2cBus = true; g_i2cKey = key;
    }
    int r = bmp280Begin(d->addr, g_i2c.cal, g_i2c.chip);
    if (r) { i2cFail(now, r); return; }
    g_i2c.up = true; g_i2c.fails = 0; g_i2c.err = 0;
    g_i2c.nextAt = now + 300; g_i2c.armed = true;                                // la prima misura e pronta dopo qualche ms
    return;
  }
  int32_t t; uint32_t p;
  int r = bmp280Sample(d->addr, g_i2c.cal, t, p);
  if (r == BMP_NOT_READY) { g_i2c.nextAt = now + 300; g_i2c.armed = true; return; }
  if (r) { i2cFail(now, r); return; }
  g_i2c.t100 = t; g_i2c.pPa = p; g_i2c.have = true; g_i2c.fails = 0; g_i2c.err = 0;
  g_i2c.nextAt = now + 2000; g_i2c.armed = true;
}
void ioTick() {                                    // da chiamare spesso (nel loop)
  ioCfgPendFlush();                                // avvisi dell'avvio: l'audit parte dopo ioInit
  IoLock L(0);                                     // se una modifica sta scrivendo, questo giro si salta
  if (!L.ok) return;
  uint32_t now = drvMillis();
  i2cTick(now);
  for (int i = 0; i < g_n; i++) {
    IoDev& d = g_io[i];
    if (d.offAt && (int32_t)(now - d.offAt) >= 0) { d.offAt = 0; if (d.on) outWrite(d, false); }
    if (d.blinkEnd) {
      if ((int32_t)(now - d.blinkEnd) >= 0) { d.blinkEnd = 0; if (d.on) outWrite(d, false); }
      else if ((int32_t)(now - d.blinkNext) >= 0) { d.blinkNext = now + 400; outWrite(d, !d.on); }
    }
  }
}
