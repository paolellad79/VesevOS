// VesevOS - prova sul computer: salvataggio sicuro di /io.cfg (1.7.62a; analisi del 13/10/2026 in docs/progetto/analisi-iocfg-2026-10-13.md)
// NON fa parte di run.sh (il nome non comincia con test_). Si lancia con tools/hosttest/prova_iocfg.sh.
// Prima della 1.7.62a le prove C, D, E scoprivano: file a 0 byte con "ok" all'utente, righe scartate cancellate dal salvataggio successivo.
// Ora devono dire "nessuna perdita". Il file system e finto (tools/hosttest/iocfg/FS.h): "disco pieno" = la scrittura non entra ma close() conferma.
// Non si prova qui: la corsa tra task (serve la scheda) e il taglio vero della corrente (littlefs lo gestisce da solo).
#include <Arduino.h>
#include "vos_io.h"
#include "vos_io_int.h"
#include "vos_pins.h"
#include "vos_dev.h"
#include "vos_drv_gpio.h"
#include "vos_drv_fs.h"
#include "vos_common.h"
#include "vos_audit.h"
#include "mini.h"
#include "fake_i2c.h"

FakeFsFaults g_ff;
namespace fs { std::map<std::string, std::shared_ptr<FakeNode>> g_fakeFiles; }

static int lvl[50]; static int mode[50]; static int claimed[50]; static const char* owner[50];
void drvGpioMode(int g, DrvGpioMode m) { mode[g] = m; }
void drvGpioWrite(int g, bool h) { lvl[g] = h; }
bool drvGpioRead(int g) { return lvl[g] != 0; }
int  drvGpioAnalog(int g) { return 1234 + g; }
const char* pinTestBlock(int g) {
  if (g < 0 || g > 48 || (g >= 22 && g <= 32)) return "inesistente";
  if (g == 19 || g == 20) return "USB"; if (g == 48) return "LED RGB"; if (g == 0) return "BOOT"; if (g == 3 || g == 45 || g == 46) return "avvio";
  if (claimed[g]) return "gia usato";
  return nullptr;
}
bool pinClaim(uint8_t g, const char* o, const char*, bool) { if (claimed[g] && owner[g] && strcmp(owner[g], o)) return false; claimed[g] = 1; owner[g] = o; return true; }
void pinRelease(uint8_t g) { claimed[g] = 0; owner[g] = nullptr; }
const DevOps* devFind(const char* id) { static DevOps d; return (!strcmp(id, "mqtt") || !strcmp(id, "boot")) ? &d : nullptr; }

// allarmi (audit) registrati dalla prova
static int audN = 0, audClr = 0; static std::string audKey;
void auditEvent(AuditLevel, const char* key, const char*) { audN++; audKey = key; }
void auditClear(const String& key) { if (key == "iocfg") audClr++; }

static bool has(const char* p) { return fs::g_fakeFiles.count(p) > 0; }
static std::string txt(const char* p) { auto it = fs::g_fakeFiles.find(p); return it == fs::g_fakeFiles.end() ? std::string("<manca>") : it->second->data; }
static size_t sz(const char* p) { auto it = fs::g_fakeFiles.find(p); return it == fs::g_fakeFiles.end() ? (size_t)-1 : it->second->data.size(); }
static void put(const char* p, const char* d) { fs::g_fakeFiles[p] = std::make_shared<fs::FakeNode>(); fs::g_fakeFiles[p]->data = d; }
static void putS(const char* p, const std::string& d) { fs::g_fakeFiles[p] = std::make_shared<fs::FakeNode>(); fs::g_fakeFiles[p]->data = d; }
static bool inCfg(const char* w) { return txt("/io.cfg").find(w) != std::string::npos; }
static bool listed(const char* n) { for (int i = 0; i < ioCount(); i++) if (ioNameAt(i) == n) return true; return false; }
static void reboot() { memset(claimed, 0, sizeof claimed); memset(owner, 0, sizeof owner); fakeI2cReset(); ioInit(); }
static void wipe() {
  g_ff = FakeFsFaults();
  while (ioCount()) { String e; if (!ioRemove(ioNameAt(0).c_str(), e)) break; }
  fs::g_fakeFiles.clear(); memset(claimed, 0, sizeof claimed); memset(owner, 0, sizeof owner);
  ioInit(); ioTick(); audN = 0; audClr = 0; audKey.clear();
}

int main() {
  String err, nm; char o[16], eb[IO_ERR_MAX]; int v_dummy = 0;
  wipe();

  // A) giro completo: formato nuovo a 6 numeri (i2c) e vecchio a 3 numeri, salva e ricarica; niente file temporaneo che resta
  CHECK(ioAddI2c("bmp280", 8, 9, 0x76, false, "bmp1", o, eb));
  CHECK(ioAdd("rele", 5, "", "inv", nm, err) && ioAdd("led", 7, "", "", nm, err));
  CHECK(inCfg("bmp1 6 8 0 9 118 1\n") && inCfg("rele1 1 5 1\n") && inCfg("led1 2 7 0\n"));
  CHECK(!has("/io.cfg.tmp~"));
  CHECK(has("/io.cfg.bak"));                              // la versione di prima (2 periferiche) e nella copia
  CHECK(txt("/io.cfg.bak").find("led1") == std::string::npos && txt("/io.cfg.bak").find("rele1") != std::string::npos);
  CHECK(ioCount() == 3);
  reboot();
  CHECK(ioCount() == 3 && ioNameAt(0) == "bmp1");
  printf("A) 3 periferiche scritte e rilette, copia .bak = versione precedente, nessun .tmp~\n");

  // B) togliere l'ULTIMA periferica = il file si TOGLIE (mai 0 byte) e la copia con lui; dopo il riavvio nessun errore
  while (ioCount() > 1) { String e; CHECK(ioRemove(ioNameAt(ioCount() - 1).c_str(), e)); }
  CHECK(sz("/io.cfg") > 0 && has("/io.cfg.bak"));
  CHECK(ioRemove("bmp1", err));
  CHECK(ioCount() == 0 && !has("/io.cfg") && !has("/io.cfg.bak") && !has("/io.cfg.tmp~"));
  reboot(); ioTick();
  CHECK(ioCount() == 0 && audN == 0);
  printf("B) tolta l'ultima: nessun file (non 0 byte), riavvio pulito, nessun allarme\n");
  wipe();

  // C) disco pieno (la scrittura non entra): ioAdd risponde ERRORE, l'elenco e il file restano com'erano
  CHECK(ioAdd("rele", 5, "", "", nm, err));
  std::string before = txt("/io.cfg");
  CHECK(before.size() > 0);
  g_ff.full = true;
  CHECK(!ioAdd("led", 7, "", "", nm, err));
  CHECK(err == "Spazio esaurito");
  g_ff.full = false;
  CHECK(ioCount() == 1 && txt("/io.cfg") == before && !has("/io.cfg.tmp~"));
  CHECK(claimed[7] == 0);                                // il pin non resta prenotato
  CHECK(audN == 1 && audKey == "iocfg");                  // allarme nell'audit
  reboot();
  CHECK(ioCount() == 1 && listed("rele1"));
  CHECK(ioAdd("led", 7, "", "", nm, err) && audClr == 1);   // riuscito: l'allarme si spegne
  printf("C) disco pieno: errore \"%s\", rele1 ancora nel file e dopo il riavvio; poi aggiunta riuscita\n", "Spazio esaurito");
  wipe();

  // C2) poco spazio libero (margine di 8 KB come fsWriteText): errore senza nemmeno provare a scrivere
  CHECK(ioAdd("rele", 5, "", "", nm, err));
  int op = g_ff.opensW;
  g_ff.used = 640 * 1024 - 4096; drvFsUsedRefresh(1000);      // lo spazio usato e in cache (fscap): qui si ricalcola come fa il monitor
  CHECK(!ioAdd("led", 7, "", "", nm, err) && err == "Spazio esaurito" && g_ff.opensW == op);
  CHECK(ioCount() == 1);
  g_ff.used = 0; drvFsUsedRefresh(2000);
  wipe();

  // C3) togliere con disco pieno: errore e la periferica resta; togliere l'ultima non ha bisogno di spazio
  CHECK(ioAdd("rele", 5, "", "", nm, err) && ioAdd("led", 7, "", "", nm, err));
  before = txt("/io.cfg");
  g_ff.full = true;
  CHECK(!ioRemove("led1", err) && err == "Spazio esaurito");
  CHECK(listed("led1") && ioCount() == 2 && txt("/io.cfg") == before);
  CHECK(claimed[7] == 1);
  g_ff.full = false;
  CHECK(ioRemove("led1", err));
  g_ff.full = true;
  CHECK(ioRemove("rele1", err) && ioCount() == 0 && !has("/io.cfg"));
  g_ff.full = false;
  printf("C3) tolta con disco pieno: errore e nulla cambia; l'ultima si toglie anche senza spazio\n");
  wipe();

  // C4) rinominare con disco pieno: errore e il nome vecchio resta (anche nel registro dei pin)
  CHECK(ioAdd("rele", 5, "", "", nm, err));
  g_ff.full = true;
  CHECK(!ioRename("rele1", "caldaia", err));
  g_ff.full = false;
  CHECK(listed("rele1") && !listed("caldaia") && inCfg("rele1 1 5 0") && owner[5] && !strcmp(owner[5], "rele1"));
  CHECK(ioRename("rele1", "caldaia", err) && inCfg("caldaia 1 5 0") && !inCfg("rele1"));
  wipe();

  // C5) il file temporaneo non si puo rinominare: errore, il file di prima torna al suo posto
  CHECK(ioAdd("rele", 5, "", "", nm, err) && ioAdd("pin", 10, "", "", nm, err));
  before = txt("/io.cfg");
  g_ff.failRenameTo = "/io.cfg";
  CHECK(!ioAdd("led", 7, "", "", nm, err) && err == "Impossibile scrivere");
  g_ff.failRenameTo.clear();
  CHECK(txt("/io.cfg") == before && !has("/io.cfg.tmp~") && ioCount() == 2);
  reboot();
  CHECK(ioCount() == 2);
  // e se non si riesce nemmeno ad aprire il temporaneo
  g_ff.noOpenW = true;
  CHECK(!ioAdd("led", 7, "", "", nm, err) && err == "Impossibile scrivere" && ioCount() == 2);
  g_ff.noOpenW = false;
  printf("C5) rinomina o apertura fallita: errore, file di prima intatto\n");
  wipe();

  // D) scrittura che non arriva al file (corrente tagliata prima di close): ioAdd dice errore, il file vecchio resta intero
  CHECK(ioAdd("rele", 5, "", "", nm, err));
  before = txt("/io.cfg");
  g_ff.cut = true;
  CHECK(!ioAdd("led", 7, "", "", nm, err));
  g_ff.cut = false;
  CHECK(txt("/io.cfg") == before && !has("/io.cfg.tmp~"));
  reboot();
  CHECK(ioCount() == 1);
  printf("D) scrittura persa: errore, dopo il riavvio %d periferica (file vecchio intero)\n", ioCount());
  wipe();

  // D2) interruzione tra i due scambi (io.cfg manca, la copia c'e): si usa la copia e l'audit lo dice
  put("/io.cfg.bak", "rele1 1 5 0\nled1 2 7 0\n");
  put("/io.cfg.tmp~", "mezza riga");
  reboot();
  CHECK(ioCount() == 2 && listed("rele1") && listed("led1"));
  ioTick();
  CHECK(audN == 1 && audKey == "iocfgbad");
  CHECK(ioAdd("pin", 10, "", "", nm, err) && ioCount() == 3 && inCfg("pin1 3 10 0") && inCfg("rele1"));   // il primo salvataggio riscrive io.cfg
  printf("D2) io.cfg mancante: lette %d periferiche dalla copia, allarme dato\n", 2);
  wipe();

  // D3) io.cfg a 0 byte (guasto) con la copia: si usa la copia. Senza copia (le versioni vecchie lo lasciavano togliendo l'ultima): elenco vuoto, nessun allarme
  put("/io.cfg", ""); put("/io.cfg.bak", "rele1 1 5 0\n");
  reboot(); ioTick();
  CHECK(ioCount() == 1 && audN == 1 && audKey == "iocfgbad");
  wipe();
  put("/io.cfg", "");
  reboot(); ioTick();
  CHECK(ioCount() == 0 && audN == 0);
  CHECK(ioAdd("rele", 5, "", "", nm, err) && ioCount() == 1 && sz("/io.cfg") > 0 && !has("/io.cfg.bak"));   // e il primo salvataggio lo ripara
  printf("D3) 0 byte: con la copia si usa la copia; senza copia elenco vuoto e silenzio\n");
  wipe();

  // E) ioInit scarta una riga (pin occupato all'avvio): la riga RESTA nel file anche dopo altri salvataggi, e torna quando il pin si libera
  put("/io.cfg", "rele1 1 5 0\nbmp1 6 8 0 9 118 1\nled1 2 7 0\n");
  claimed[8] = 1; owner[8] = "altro";
  ioInit(); ioTick();
  CHECK(ioCount() == 2);
  CHECK(audN == 1 && audKey == "iocfgrow");
  CHECK(ioAdd("pin", 10, "", "", nm, err));
  CHECK(inCfg("bmp1 6 8 0 9 118 1\n"));
  CHECK(ioRemove("rele1", err) && inCfg("bmp1 6 8 0 9 118 1\n"));
  CHECK(ioRename("led1", "luce", err) && inCfg("bmp1 6 8 0 9 118 1\n"));
  printf("E) dopo aggiunta, tolta e rinomina il file contiene ancora:\n%s", txt("/io.cfg").c_str());
  claimed[8] = 0; owner[8] = nullptr;
  reboot();
  CHECK(ioCount() == 3 && listed("bmp1") && listed("luce") && listed("pin1"));
  printf("E) pin liberato e riavvio: bmp1 e tornata (%d periferiche)\n", ioCount());
  wipe();

  // E2) limite delle righe tenute: oltre 512 byte non si tengono (nessun errore, il file non cresce senza fine)
  { std::string big; for (int i = 0; i < 40; i++) { char l[40]; snprintf(l, sizeof l, "pa%02d 2 8 0\n", i); big += l; }
    claimed[8] = 1; owner[8] = "altro"; put("/io.cfg", big.c_str());
    ioInit();
    CHECK(ioCount() == 0);
    CHECK(ioAdd("rele", 5, "", "", nm, err));
    CHECK(sz("/io.cfg") > 0 && sz("/io.cfg") <= 512 + 40);
    printf("E2) 40 righe scartate (%zu byte): dopo il salvataggio il file e di %zu byte\n", big.size(), sz("/io.cfg")); }
  wipe();

  // F) riga rotta in mezzo: le altre si caricano, la riga cattiva NON si tiene
  put("/io.cfg", "rele1 1 5 0\nxxxx\nled1 2 7 0\n");
  ioInit();
  CHECK(ioCount() == 2);
  CHECK(ioAdd("pin", 10, "", "", nm, err) && !inCfg("xxxx") && inCfg("led1") && inCfg("pin1"));
  printf("F) riga rotta: %d periferiche buone caricate, la riga rotta non resta\n", 2);
  wipe();

  // G) formato vecchio a 3 numeri: le righe normali si leggono e si riscrivono; una riga i2c a 3 numeri e scartata (non tenuta)
  put("/io.cfg", "bmp1 6 8 0\nrele1 1 5 0\n");
  ioInit();
  CHECK(ioCount() == 1 && listed("rele1"));
  CHECK(ioAdd("pin", 10, "", "", nm, err) && !inCfg("bmp1") && inCfg("rele1 1 5 0\n"));
  printf("G) formato vecchio: rele1 riletto, riga i2c a 3 numeri scartata\n");
  wipe();

  // H) limite a un solo sensore i2c: il secondo e rifiutato e non cambia niente
  CHECK(ioAddI2c("bmp280", 8, 9, 0x76, false, "bmp1", o, eb));
  before = txt("/io.cfg");
  CHECK(!ioAddI2c("bmp280", 11, 12, 0x77, false, "bmp2", o, eb));
  CHECK(ioCount() == 1 && txt("/io.cfg") == before && claimed[11] == 0 && claimed[12] == 0);
  // e il salvataggio fallito per un i2c lascia i pin liberi
  g_ff.full = true;
  CHECK(!ioAdd("rele", 5, "", "", nm, err));
  g_ff.full = false;
  wipe();
  g_ff.full = true;
  CHECK(!ioAddI2c("bmp280", 8, 9, 0x76, false, "bmp1", o, eb) && ioCount() == 0 && claimed[8] == 0 && claimed[9] == 0 && !has("/io.cfg"));
  g_ff.full = false;
  CHECK(ioAddI2c("bmp280", 8, 9, 0x76, false, "bmp1", o, eb) && ioCount() == 1);
  printf("H) secondo sensore rifiutato; i2c con disco pieno: errore e pin liberi\n");
  wipe();

  // I) il sensore che non risponde (nessun sensore sul bus) NON toglie la periferica dall'elenco
  CHECK(ioAddI2c("bmp280", 8, 9, 0x76, false, "bmp1", o, eb));
  fakeI2cReset();
  for (int i = 0; i < 40; i++) { ioTick(); }
  CHECK(ioCount() == 1);
  printf("I) 40 giri di ioTick senza sensore: ioCount=%d (nessuna rimozione automatica)\n", ioCount());
  wipe();

  // J) nessun file vecchio: ioInit non scrive mai (nemmeno con la copia)
  put("/io.cfg.bak", "rele1 1 5 0\n");
  int w0 = g_ff.opensW;
  reboot();
  CHECK(g_ff.opensW == w0 && !has("/io.cfg"));
  wipe();

  // K) righe di io.cfg: CRLF, spazi e tab ai lati, riga vuota, riga senza \n finale, riga troppo lunga (oltre 191 caratteri: scartata INTERA, la coda non diventa una riga nuova),
  //    riga che inizia con uno zero (non carica niente) e riga con uno zero in mezzo (vale la parte prima dello zero, come prima)
  { std::string f = "  rele1 1 5 0  \r\n\tled1 2 7 0\t\r\n\r\n";
    f += std::string(191, ' ') + "pin9 3 13 0\n";
    f += std::string("\0junk 3 14 0\n", sizeof("\0junk 3 14 0\n") - 1);
    f += std::string("pin3 3 15 0\0xyz\n", sizeof("pin3 3 15 0\0xyz\n") - 1);
    f += "adc1 5 4 0";                                          // ultima riga senza \n
    putS("/io.cfg", f);
    ioInit();
    CHECK(ioCount() == 4 && listed("rele1") && listed("led1") && listed("pin3") && listed("adc1"));
    CHECK(!listed("pin9") && !listed("junk") && claimed[13] == 0 && claimed[14] == 0 && claimed[5] == 1 && claimed[7] == 1 && claimed[15] == 1 && claimed[4] == 1);
    printf("K) righe di io.cfg: CRLF, spazi, riga lunga scartata intera, zeri, ultima riga senza a capo: %d periferiche\n", ioCount()); }
  wipe();
  wipe();

  // L) nomi: pulizia (spazi e maiuscole), lunghezza controllata prima di copiare, puntatori dentro la tabella (rimozione e rinomina)
  CHECK(ioAdd("pin", 10, "  LuCe9  ", "", nm, err) && nm == "luce9");
  CHECK(!ioAdd("pin", 11, "abcdefghijkl", "", nm, err) && err.indexOf("da 2 a 11") >= 0);                       // 12 caratteri
  CHECK(!ioAdd("pin", 11, "   abcdefghijklmnopqrstuvwxyz   ", "", nm, err) && err.indexOf("da 2 a 11") >= 0);   // molto lungo: stesso errore, nessun overflow
  CHECK(!ioAdd("pin", 11, "lu ce", "", nm, err) && err.indexOf("solo lettere") >= 0);
  CHECK(!ioAdd("pin", 11, "1luce", "", nm, err) && !ioAdd("pin", 11, "x", "", nm, err) && !ioAdd("pin", 11, "mqtt", "", nm, err));
  CHECK(ioAdd("pin", 11, "abcdefghijk", "", nm, err) && nm == "abcdefghijk" && ioHas("abcdefghijk"));            // 11 caratteri: il massimo
  CHECK(ioAdd("rele", 12, "", "", nm, err) && nm == "rele1" && ioAdd("rele", 13, "", "", nm, err) && nm == "rele2");
  CHECK(!ioHas("abcdefghijklmnop") && !ioHas("abcdefghijklmnopqrst.p") && !ioHas("pin1.p") && !strcmp(ioClassName("zzz"), ""));
  CHECK(!ioGet("luce9.p", v_dummy, err) && !ioRemove("abcdefghijklm", err) && err.indexOf("sconosciuta") >= 0);
  CHECK(!ioRename("abcdefghijklm", "zeta", err) && !ioRename("luce9", "abcdefghijkl", err) && err.indexOf("da 2 a 11") >= 0);
  CHECK(ioRename("luce9", "LUCE9", err));                                                                        // stesso nome (dopo la pulizia): niente da fare
  CHECK(!ioRename("luce9", "rele1", err) && err.indexOf("gia usato") >= 0);
  CHECK(ioRename("luce9", " NUOVO ", err) && ioHas("nuovo") && !ioHas("luce9"));
  { int n0 = ioCount();
    CHECK(!ioRename(ioDevAt(0)->name, ioDevAt(1)->name, err) && err.indexOf("gia usato") >= 0 && ioCount() == n0);   // from e to dentro la tabella
    char first[16]; strcpy(first, ioDevAt(0)->name);
    CHECK(ioRemove(ioDevAt(0)->name, err) && !ioHas(first) && ioCount() == n0 - 1);                                  // il nome punta dentro la tabella che si sposta
    CHECK(ioRename(ioDevAt(0)->name, "zeta", err) && ioHas("zeta") && ioCount() == n0 - 1); }
  printf("L) nomi: pulizia, 11 caratteri al massimo, puntatori dentro la tabella\n");
  wipe();

  return done("prova_iocfg");
}
