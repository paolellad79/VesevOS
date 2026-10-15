// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_io.cpp
#include "vos_io.h"
#include "vos_pins.h"
#include "vos_dev.h"
#include "../core/vos_common.h"
#include "../core/vos_i18n.h"
#include "../core/vos_log.h"
#include "../core/vos_util.h"
#include "../core/vos_config.h"
#include "../drivers/vos_drv_gpio.h"
#include "../drivers/vos_drv_i2c.h"
#include "vos_bmp280.h"
#include "../drivers/vos_drv_fs.h"
#include "../core/vos_audit.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#define IO_FILE "/io.cfg"
#define IO_BAK  "/io.cfg.bak"                       // l'ultima versione buona di io.cfg
#define IO_TMP  "/io.cfg.tmp~"                      // nome riservato: i file .tmp~ non si toccano da fuori (vedi fsWriteText)
#define IO_JUNK_MAX 512                             // righe di io.cfg che questo avvio non ha caricato (pin occupato): si riscrivono uguali
#define IO_MIN_SWITCH_MS 1000UL

struct IoDev { char name[12]; uint8_t cls; int8_t gpio; uint8_t flags; bool on; uint32_t last; uint32_t offAt; uint32_t blinkEnd, blinkNext; int8_t gpio2; uint8_t addr, drv; };   // offAt: fine dell'impulso; blinkEnd: fine della prova lampeggio (0 = nessuna); i2c: gpio = SDA, gpio2 = SCL, drv 1 = bmp280
static IoDev g_io[IO_MAX];
static int g_n = 0;
static char g_junk[IO_JUNK_MAX + 1];
static size_t g_junkLen = 0;
static uint8_t g_pend = 0;                          // avvisi dell'avvio da mandare all'audit appena e pronto (ioTick): 1 copia usata, 2 illeggibile, 4 righe tenute
static bool g_saveBad = false;                      // l'ultimo salvataggio e fallito: l'allarme "iocfg" e acceso

// un solo blocco per tutte le modifiche e per il loop: web, terminale e app (Berry) sono task diversi
static SemaphoreHandle_t g_iomx = nullptr;
struct IoLock {
  bool held, ok;
  explicit IoLock(uint32_t ticks = portMAX_DELAY) { held = g_iomx && xSemaphoreTakeRecursive(g_iomx, ticks) == pdTRUE; ok = held || !g_iomx; }
  ~IoLock() { if (held) xSemaphoreGiveRecursive(g_iomx); }
};

[[maybe_unused]] static void ioKeys() { tr("Periferiche"); }

static const char* const CN[] = {"", "rele", "led", "pin", "ingresso", "analogico", "i2c"};
static const char* const PFX[] = {"", "rele", "led", "pin", "in", "adc", "bmp"};
#define DRV_BMP280 1

// un solo sensore I2C: misura nel loop (ioTick), la pagina e le app leggono solo l'ultimo valore
static struct I2cState { bool up, have, armed; uint8_t fails, err, chip; int32_t t100; uint32_t pPa; uint32_t nextAt; Bmp280Cal cal; } g_i2c;
static bool g_i2cBus = false;
static uint32_t g_i2cKey = 0;                      // SDA, SCL e velocita con cui il bus e acceso

bool ioParseCls(const String& s, uint8_t& c) {
  for (uint8_t i = 1; i <= 6; i++) if (s == CN[i]) { c = i; return true; }
  if (s == "relay") { c = IC_RELE; return true; }
  if (s == "in" || s == "input") { c = IC_IN; return true; }
  if (s == "adc") { c = IC_ADC; return true; }
  return false;
}
static bool isOut(uint8_t c) { return c == IC_RELE || c == IC_LED || c == IC_PIN; }
static int find(const String& n) { for (int i = 0; i < g_n; i++) if (n == g_io[i].name) return i; return -1; }
static int findAux(const String& n, bool& aux) {         // "bmp1.p" = pressione del sensore bmp1
  aux = false; int i = find(n); if (i >= 0) return i;
  if (n.length() > 2 && n.endsWith(".p")) { i = find(n.substring(0, n.length() - 2)); if (i >= 0 && g_io[i].cls == IC_I2C) { aux = true; return i; } }
  return -1;
}
bool ioHas(const String& n) { bool a; return findAux(n, a) >= 0; }
int  ioCount() { return g_n; }
String ioNameAt(int i) { return (i >= 0 && i < g_n) ? String(g_io[i].name) : String(); }

// parole che hanno gia un altro significato (periferiche di sistema, permessi delle app, comandi)
static bool reserved(const String& n) {
  static const char* const R[] = {"led", "time", "temp", "log", "act", "btn", "logread", "mqtt", "mesh", "ble", "mfa", "stat", "pin", "boot", "usb", "mem", "cpu", "rad", "gpio", "all", "tutto", "tutti"};
  for (size_t i = 0; i < sizeof(R) / sizeof(R[0]); i++) if (n == R[i]) return true;
  return devFind(n) != nullptr;
}
bool ioNameOk(const String& n, String& err) {
  if (n.length() < 2 || n.length() > 11) { err = tr("Nome: da 2 a 11 caratteri"); return false; }
  for (size_t i = 0; i < n.length(); i++) {
    char c = n[i];
    bool ok = (c >= 'a' && c <= 'z') || (i > 0 && c >= '0' && c <= '9');
    if (!ok) { err = tr("Nome: solo lettere minuscole senza accento e numeri, e deve iniziare con una lettera"); return false; }
  }
  if (reserved(n)) { err = tr("Nome riservato: scegline un altro"); return false; }
  return true;
}

// Scrittura sicura: file temporaneo controllato (byte scritti, close, dimensione), copia dell'ultima versione buona in .bak, poi lo scambio.
// Mai un io.cfg a 0 byte: lista vuota = file tolto. Se qualcosa fallisce il file di prima resta com'e. skip = periferica da non scrivere, extra = una in piu.
static size_t lineOf(char* out, size_t room, const IoDev& d) {
  int n;
  if (d.cls == IC_I2C) n = snprintf(out, room, "%s %u %d %u %d %u %u\n", d.name, (unsigned)d.cls, (int)d.gpio, (unsigned)d.flags, (int)d.gpio2, (unsigned)d.addr, (unsigned)d.drv);
  else n = snprintf(out, room, "%s %u %d %u\n", d.name, (unsigned)d.cls, (int)d.gpio, (unsigned)d.flags);
  return (n > 0 && (size_t)n < room) ? (size_t)n : 0;
}
static const char* writeCfg(int skip, const IoDev* extra) {
  static char buf[IO_MAX * 48 + IO_JUNK_MAX + 8];       // sotto il blocco: nessuna corsa
  size_t len = 0;
  for (int i = 0; i <= g_n; i++) {
    const IoDev* d = (i < g_n) ? (i == skip ? nullptr : &g_io[i]) : extra;
    if (d) len += lineOf(buf + len, sizeof buf - len, *d);
  }
  memcpy(buf + len, g_junk, g_junkLen); len += g_junkLen; buf[len] = 0;
  if (!len) {                                           // niente da tenere: si toglie il file (prima la copia, cosi un .bak vecchio non torna mai)
    drvFsRemove(IO_BAK);
    if (drvFsExists(IO_FILE) && !drvFsRemove(IO_FILE)) return tr("Impossibile scrivere");
    return nullptr;
  }
  size_t used = drvFsUsed(), total = drvFsTotal();
  if (len + 8192 > (used < total ? total - used : 0)) return tr("Spazio esaurito");
  File f = drvFsOpen(IO_TMP, "w");
  if (!f) return tr("Impossibile scrivere");
  size_t w = f.write((const uint8_t*)buf, len);
  f.close();
  bool good = (w == len);
  if (good) { File c = drvFsOpen(IO_TMP, "r"); good = c && (size_t)c.size() == len; c.close(); }
  if (!good) { drvFsRemove(IO_TMP); return tr("Spazio esaurito"); }
  bool moved = false;
  if (drvFsExists(IO_FILE)) {
    File m = drvFsOpen(IO_FILE, "r"); size_t ms = m ? (size_t)m.size() : 0; m.close();
    if (ms > 0) {
      drvFsRemove(IO_BAK);
      if (!drvFsRename(IO_FILE, IO_BAK)) { drvFsRemove(IO_TMP); return tr("Impossibile scrivere"); }
      moved = true;
    } else if (!drvFsRemove(IO_FILE)) { drvFsRemove(IO_TMP); return tr("Impossibile scrivere"); }
  }
  if (!drvFsRename(IO_TMP, IO_FILE)) {
    if (moved) drvFsRename(IO_BAK, IO_FILE);
    drvFsRemove(IO_TMP);
    return tr("Impossibile scrivere");
  }
  return nullptr;
}
static const char* save(int skip = -1, const IoDev* extra = nullptr) {
  const char* e = writeCfg(skip, extra);
  if (e) {
    vlog("ERRORE: IO: non riesco a salvare %s (%s): l'elenco non cambia", IO_FILE, e);
    auditEvent(AUD_YELLOW, "iocfg", tr("Impossibile salvare le periferiche (io.cfg): nessuna modifica fatta. Controlla lo spazio libero"));
    g_saveBad = true;
  } else if (g_saveBad) { auditClear("iocfg"); g_saveBad = false; }
  return e;
}
static void keepRow(const char* row) {                // riga valida ma non caricata (pin occupato): resta nel file
  size_t k = strlen(row);
  if (k > 100 || g_junkLen + k + 1 > IO_JUNK_MAX) { vlog("ERRORE: IO: riga non tenuta (troppo lunga o troppe righe): %s", row); return; }
  memcpy(g_junk + g_junkLen, row, k); g_junkLen += k; g_junk[g_junkLen++] = '\n'; g_junk[g_junkLen] = 0;
  g_pend |= 4;
}

static bool levelOn(const IoDev& d, bool on) { return (d.flags & IOF_INV) ? !on : on; }   // livello fisico da stato logico
static void outWrite(IoDev& d, bool on) { d.on = on; drvGpioWrite(d.gpio, levelOn(d, on)); d.last = millis(); }

static void hwSetup(IoDev& d) {
  if (isOut(d.cls)) { drvGpioWrite(d.gpio, levelOn(d, false)); drvGpioMode(d.gpio, DG_OUTPUT); d.on = false; }   // prima il livello, poi uscita: nessun impulso
  else if (d.cls == IC_I2C) { g_i2c = I2cState(); }                       // il bus lo avvia ioTick
  else if (d.cls == IC_IN) drvGpioMode(d.gpio, (d.flags & IOF_UP) ? DG_INPUT_PULLUP : (d.flags & IOF_DOWN) ? DG_INPUT_PULLDOWN : DG_INPUT);
  else drvGpioMode(d.gpio, DG_INPUT);
}

static bool hasI2c() { for (int i = 0; i < g_n; i++) if (g_io[i].cls == IC_I2C) return true; return false; }
static const char* gpioBlock(int gpio, uint8_t cls) {
  const char* why = pinTestBlock(gpio);
  if (why) return why;
  static_assert(VOS_ADC_FIRST == 1 && VOS_ADC_LAST == 10, "il testo qui sotto dice 1-10: aggiornarlo (e lang_src.json) per questa scheda");
  if (cls == IC_ADC && !(gpio >= VOS_ADC_FIRST && gpio <= VOS_ADC_LAST)) return tr("Analogico: solo i pin 1-10 (gli altri disturbano il Wi-Fi)");
  return nullptr;
}

static bool claimDev(const IoDev& d) {
  if (!pinClaim((uint8_t)d.gpio, d.name, CN[d.cls])) return false;
  if (d.cls == IC_I2C && !pinClaim((uint8_t)d.gpio2, d.name, CN[d.cls])) { pinRelease((uint8_t)d.gpio); return false; }
  return true;
}
static void releaseDev(const IoDev& d) { pinRelease((uint8_t)d.gpio); if (d.cls == IC_I2C) pinRelease((uint8_t)d.gpio2); }

void ioInit() {
  if (!g_iomx) g_iomx = xSemaphoreCreateRecursiveMutex();
  IoLock L;
  g_n = 0; g_junkLen = 0; g_junk[0] = 0; g_pend = 0;
  const char* src = IO_FILE;
  bool hasMain = drvFsExists(IO_FILE);
  int st = 0;                                           // 1 = a 0 byte (le versioni vecchie lo lasciavano togliendo l'ultima periferica), 2 = non si apre
  if (hasMain) { File c = drvFsOpen(IO_FILE, "r"); if (!c) st = 2; else if (c.size() == 0) st = 1; c.close(); }
  if (!hasMain || st) {
    if (drvFsExists(IO_BAK)) {
      src = IO_BAK; g_pend |= 1;
      vlog("ERRORE: IO: %s %s: uso la copia %s", IO_FILE, hasMain ? "vuoto o illeggibile" : "manca", IO_BAK);
    } else {
      if (st == 2) { g_pend |= 2; vlog("ERRORE: IO: %s illeggibile e senza copia: nessuna periferica", IO_FILE); }
      return;
    }
  }
  File f = drvFsOpen(src, "r");
  if (!f) { g_pend |= 2; vlog("ERRORE: IO: non riesco a leggere %s", src); return; }
  while (f.available() && g_n < IO_MAX) {
    String l = f.readStringUntil('\n'); l.trim();
    if (!l.length()) continue;
    // riga: <nome> <classe> <gpio> <opzioni> (lettura a mano, niente sscanf)
    char nm[16]; unsigned c = 0, fl = 0, ad = 0, dr = 0; int g = -1, g2 = -1;
    {
      const char* p = l.c_str(); size_t k = 0;
      while (*p && *p != ' ' && *p != '\t') { if (k + 1 < sizeof(nm)) nm[k++] = *p; p++; }
      nm[k] = 0;
      long x[6]; int got = 0; char* e;
      while (got < 6) { x[got] = strtol(p, &e, 10); if (e == p) break; p = e; got++; }
      if (!k || (got != 3 && got != 6) || x[0] < 0 || x[2] < 0) continue;
      c = (unsigned)x[0]; g = (int)x[1]; fl = (unsigned)x[2];
      if (got == 6) { g2 = (int)x[3]; ad = (unsigned)x[4]; dr = (unsigned)x[5]; }
    }
    String e;
    if (c < 1 || c > 6 || !ioNameOk(String(nm), e) || find(nm) >= 0) { vlog("IO: riga scartata (%s)", l.c_str()); continue; }
    if (c == IC_I2C && (g2 < 0 || g2 == g || ad < 0x08 || ad > 0x77 || dr != DRV_BMP280 || hasI2c())) { vlog("IO: riga scartata (%s)", l.c_str()); continue; }
    const char* why = gpioBlock(g, (uint8_t)c);
    if (!why && c == IC_I2C) why = gpioBlock(g2, (uint8_t)c);
    if (why) { vlog("IO: %s scartata, GPIO%d: %s (la riga resta nel file)", nm, g, why); keepRow(l.c_str()); continue; }
    IoDev& d = g_io[g_n];
    memset(&d, 0, sizeof d);
    strncpy(d.name, nm, sizeof(d.name) - 1); d.cls = (uint8_t)c; d.gpio = (int8_t)g; d.flags = (uint8_t)fl;
    if (c == IC_I2C) { d.gpio2 = (int8_t)g2; d.addr = (uint8_t)ad; d.drv = (uint8_t)dr; }
    if (!claimDev(d)) { vlog("IO: %s scartata, GPIO%d gia usato (la riga resta nel file)", nm, g); keepRow(l.c_str()); continue; }
    hwSetup(d); g_n++;
  }
  f.close();
  if (g_n) vlog("IO: %d periferiche, uscite spente", g_n);
}

static bool pickName(const String& nameIn, uint8_t c, String& name, String& err) {
  name = nameIn; name.trim(); name.toLowerCase();
  if (!name.length()) {
    for (int k = 1; k < 100 && !name.length(); k++) { String t = String(PFX[c]) + String(k); if (find(t) < 0 && !reserved(t)) name = t; }
    if (!name.length()) { err = tr("Troppe periferiche (massimo 16)"); return false; }
  }
  if (!ioNameOk(name, err)) return false;
  if (find(name) >= 0) { err = tr("Nome gia usato"); return false; }
  return true;
}

bool ioAdd(const String& cls, int gpio, const String& nameIn, const String& opt, String& outName, String& err) {
  IoLock L;
  uint8_t c;
  if (!ioParseCls(cls, c)) { err = tr("Classe non valida (rele, led, pin, ingresso, analogico, i2c)"); return false; }
  if (c == IC_I2C) {                                    // opzioni: scl=N addr=0x76 drv=bmp280 fast
    int scl = gpio + 1, addr = 0x76; char drv[16] = "bmp280", o[16] = "", eb[IO_ERR_MAX] = ""; bool fast = opt.indexOf("fast") >= 0;
    int q = opt.indexOf("scl="); if (q >= 0) scl = (int)strtol(opt.c_str() + q + 4, nullptr, 0);
    q = opt.indexOf("addr="); if (q >= 0) addr = (int)strtol(opt.c_str() + q + 5, nullptr, 0);
    q = opt.indexOf("drv=");
    if (q >= 0) { const char* a = opt.c_str() + q + 4; size_t k = 0; while (a[k] && a[k] != ',' && a[k] != ' ' && k < sizeof drv - 1) { drv[k] = a[k]; k++; } drv[k] = 0; }
    if (!ioAddI2c(drv, gpio, scl, addr, fast, nameIn.c_str(), o, eb)) { err = eb; return false; }
    outName = o; return true;
  }
  if (g_n >= IO_MAX) { err = tr("Troppe periferiche (massimo 16)"); return false; }
  const char* why = gpioBlock(gpio, c);
  if (why) { err = why; return false; }
  String name;
  if (!pickName(nameIn, c, name, err)) return false;
  uint8_t fl = 0;
  if (opt.indexOf("inv") >= 0 && (isOut(c) || c == IC_IN)) fl |= IOF_INV;
  if (c == IC_IN) { if (opt.indexOf("up") >= 0) fl |= IOF_UP; else if (opt.indexOf("down") >= 0) fl |= IOF_DOWN; }
  IoDev& d = g_io[g_n];
  memset(&d, 0, sizeof d);
  strncpy(d.name, name.c_str(), sizeof(d.name) - 1); d.cls = c; d.gpio = (int8_t)gpio; d.flags = fl;
  if (!claimDev(d)) { err = tr("Gia usato dal sistema"); return false; }
  const char* se = save(-1, &d);
  if (se) { releaseDev(d); memset(&d, 0, sizeof d); err = se; return false; }
  hwSetup(d); g_n++;
  outName = name;
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
  String name, e;
  if (!pickName(nameIn, IC_I2C, name, e)) return i2cErr(err, e.c_str());
  IoDev& d = g_io[g_n];
  memset(&d, 0, sizeof d);
  strncpy(d.name, name.c_str(), sizeof(d.name) - 1); d.cls = IC_I2C; d.gpio = (int8_t)sda; d.gpio2 = (int8_t)scl; d.addr = (uint8_t)addr; d.drv = DRV_BMP280;
  if (fast) d.flags |= IOF_FAST;
  if (!claimDev(d)) return i2cErr(err, tr("Gia usato dal sistema"));
  const char* se = save(-1, &d);
  if (se) { releaseDev(d); memset(&d, 0, sizeof d); return i2cErr(err, se); }
  hwSetup(d); g_n++;
  strncpy(outName, d.name, 15); outName[15] = 0;
  vlog("IO: aggiunto %s (bmp280 0x%02X) su SDA GPIO%d, SCL GPIO%d", d.name, addr, sda, scl);
  return true;
}

bool ioRemove(const String& name, String& err) {
  IoLock L;
  int i = find(name);
  if (i < 0) { err = tr("Periferica sconosciuta"); return false; }
  const char* se = save(i);                              // prima il file: se non si scrive, la periferica resta com'e
  if (se) { err = se; return false; }
  IoDev d = g_io[i];
  if (isOut(d.cls)) outWrite(g_io[i], false);
  if (d.cls != IC_I2C) drvGpioMode(d.gpio, DG_INPUT);       // i pin del bus li lascia liberi ioTick quando ferma Wire
  releaseDev(d);
  for (int k = i; k < g_n - 1; k++) g_io[k] = g_io[k + 1];
  g_n--;
  // i nomi dei pin nel registro puntano ai buffer della tabella: dopo lo spostamento vanno richiesti da capo
  for (int k = 0; k < g_n; k++) { releaseDev(g_io[k]); claimDev(g_io[k]); }
  vlog("IO: tolta %s", name.c_str());
  return true;
}

bool ioRename(const String& from, const String& toIn, String& err) {
  IoLock L;
  int i = find(from);
  if (i < 0) { err = tr("Periferica sconosciuta"); return false; }
  String to = toIn; to.trim(); to.toLowerCase();
  if (to == from) return true;
  if (!ioNameOk(to, err)) return false;
  if (find(to) >= 0) { err = tr("Nome gia usato"); return false; }
  char old[sizeof g_io[i].name]; memcpy(old, g_io[i].name, sizeof old);
  memset(g_io[i].name, 0, sizeof g_io[i].name); strncpy(g_io[i].name, to.c_str(), sizeof(g_io[i].name) - 1);
  const char* se = save();
  if (se) { memcpy(g_io[i].name, old, sizeof old); err = se; return false; }
  vlog("IO: %s ora si chiama %s", from.c_str(), to.c_str());
  return true;
}

bool ioSet(const String& name, int val, int role, String& err) {
  int i = find(name);
  if (i < 0) { err = tr("Periferica sconosciuta"); return false; }
  IoDev& d = g_io[i];
  if (!isOut(d.cls)) { err = tr("Questa periferica non e un'uscita"); return false; }
  if (d.cls == IC_RELE && role < ROLE_ADMIN) { err = tr("Il rele lo comanda solo l'Amministratore"); return false; }
  if (role < ROLE_OPER) { err = tr("Il tuo ruolo non permette questo comando"); return false; }
  bool on = val != 0;
  d.offAt = 0; d.blinkEnd = 0;                                  // un comando a mano annulla impulso e lampeggio
  if (on == d.on) return true;
  if (on && d.cls == IC_RELE && d.last && millis() - d.last < IO_MIN_SWITCH_MS) { err = tr("Troppo presto: il rele si riaccende al massimo una volta al secondo"); return false; }
  outWrite(d, on);
  vlog("IO: %s %s", d.name, on ? "acceso" : "spento");
  return true;
}

bool ioGet(const String& name, int& val, String& err) {
  bool aux; int i = findAux(name, aux);
  if (i < 0) { err = tr("Periferica sconosciuta"); return false; }
  IoDev& d = g_io[i];
  if (d.cls == IC_I2C) {
    if (!g_i2c.have) { err = tr("Il sensore non risponde"); return false; }
    val = aux ? (int)g_i2c.pPa : (int)g_i2c.t100; return true;
  }
  if (isOut(d.cls)) val = d.on ? 1 : 0;
  else if (d.cls == IC_IN) { bool lv = drvGpioRead(d.gpio); val = ((d.flags & IOF_INV) ? !lv : lv) ? 1 : 0; }
  else val = drvGpioAnalog(d.gpio);
  return true;
}
bool ioIsOut(const String& n) { int i = find(n); return i >= 0 && isOut(g_io[i].cls); }
bool ioCanSwitch(const String& n) { return ioIsOut(n); }
const char* ioClassName(const String& n) { bool a; int i = findAux(n, a); return i >= 0 ? CN[g_io[i].cls] : ""; }
int  ioState(const String& n) { int i = find(n); if (i < 0) return 0; if (g_io[i].cls == IC_I2C) return g_i2c.have ? 1 : 0; return isOut(g_io[i].cls) ? (g_io[i].on ? 1 : 0) : 1; }

void ioAllOff() { for (int i = 0; i < g_n; i++) if (isOut(g_io[i].cls)) { g_io[i].offAt = 0; g_io[i].blinkEnd = 0; if (g_io[i].on) outWrite(g_io[i], false); } }

String ioListJson() {
  IoLock L(pdMS_TO_TICKS(300));
  String j = "{\"max\":" + String(IO_MAX) + ",\"io\":[";
  for (int i = 0; i < g_n; i++) {
    int v = 0; String e; ioGet(g_io[i].name, v, e);
    if (i) j += ",";
    char h[96]; snprintf(h, sizeof h, "{\"n\":\"%s\",\"c\":\"%s\",\"g\":%d,\"f\":%d,\"v\":%d,\"out\":%d", g_io[i].name, CN[g_io[i].cls], (int)g_io[i].gpio, (int)g_io[i].flags, v, isOut(g_io[i].cls) ? 1 : 0);
    j += h;
    if (g_io[i].cls == IC_I2C) {
      char x[96]; snprintf(x, sizeof x, ",\"g2\":%d,\"a\":%d,\"d\":\"bmp280\",\"p\":%u,\"ok\":%d", (int)g_io[i].gpio2, (int)g_io[i].addr, (unsigned)g_i2c.pPa, g_i2c.have ? 1 : 0);
      j += x;
    }
    j += "}";
  }
  return j + "]}";
}

String ioText() {
  IoLock L(pdMS_TO_TICKS(300));
  if (!g_n) return String(tr("Nessuna periferica. Aggiungi: io add <classe> <pin> [nome] [inv|up|down]")) + "\n";
  String t;
  for (int i = 0; i < g_n; i++) {
    int v = 0; String e; ioGet(g_io[i].name, v, e);
    String n = g_io[i].name; while (n.length() < 12) n += ' ';
    String c = CN[g_io[i].cls]; while (c.length() < 10) c += ' ';
    if (g_io[i].cls == IC_I2C) {
      char x[96];
      if (g_i2c.have) snprintf(x, sizeof x, "SDA GPIO%d SCL GPIO%d  0x%x  %.1f C  %.1f hPa\n", (int)g_io[i].gpio, (int)g_io[i].gpio2, (unsigned)g_io[i].addr, g_i2c.t100 / 100.0f, g_i2c.pPa / 100.0f);
      else snprintf(x, sizeof x, "SDA GPIO%d SCL GPIO%d  0x%x  %s\n", (int)g_io[i].gpio, (int)g_io[i].gpio2, (unsigned)g_io[i].addr, tr("non risponde"));
      t += n + c + x;
      continue;
    }
    t += n + c + "GPIO" + String((int)g_io[i].gpio) + "  " + (g_io[i].cls == IC_ADC ? String(v) : String(v ? tr("acceso") : tr("spento"))) +
         ((g_io[i].flags & IOF_INV) ? "  inv" : "") + ((g_io[i].flags & IOF_UP) ? "  up" : "") + ((g_io[i].flags & IOF_DOWN) ? "  down" : "") + "\n";
  }
  return t;
}

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
bool ioPulse(const String& name, uint32_t ms, int role, String& err) {
  int i = find(name);
  if (i < 0) { err = tr("Periferica sconosciuta"); return false; }
  if (ms < 100 || ms > 5000) { err = tr("Valori non validi"); return false; }
  if (!ioSet(name, 1, role, err)) return false;
  g_io[i].offAt = millis() + ms; if (!g_io[i].offAt) g_io[i].offAt = 1;
  return true;
}
bool ioBlink(const String& name, uint32_t sec, int role, String& err) {
  int i = find(name);
  if (i < 0) { err = tr("Periferica sconosciuta"); return false; }
  if (g_io[i].cls == IC_RELE) { err = tr("Il rele non si fa lampeggiare"); return false; }
  if (!isOut(g_io[i].cls)) { err = tr("Questa periferica non e un'uscita"); return false; }
  if (role < ROLE_OPER) { err = tr("Il tuo ruolo non permette questo comando"); return false; }
  if (sec < 1 || sec > 10) { err = tr("Valori non validi"); return false; }
  IoDev& d = g_io[i]; d.offAt = 0;
  d.blinkEnd = millis() + sec * 1000UL; if (!d.blinkEnd) d.blinkEnd = 1; d.blinkNext = millis();
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
  if (g_pend) {                                    // avvisi dell'avvio: l'audit parte dopo ioInit
    uint8_t p = g_pend; g_pend = 0;
    if (p & 1) auditEvent(AUD_YELLOW, "iocfgbad", tr("io.cfg era vuoto o illeggibile: uso la copia di sicurezza (io.cfg.bak)"));
    if (p & 2) auditEvent(AUD_YELLOW, "iocfgbad", tr("io.cfg illeggibile e senza copia di sicurezza: nessuna periferica caricata"));
    if (p & 4) auditEvent(AUD_YELLOW, "iocfgrow", tr("Alcune periferiche di io.cfg non sono state caricate (pin occupato): restano nel file"));
  }
  IoLock L(0);                                     // se una modifica sta scrivendo, questo giro si salta
  if (!L.ok) return;
  uint32_t now = millis();
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
