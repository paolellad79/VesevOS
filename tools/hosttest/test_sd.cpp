// VesevOS - prove sul computer: periferica microSD (devices/vos_sd.cpp, vos_sd_io.cpp, vos_sd_path.cpp) con il driver finto (fake_sd.cpp)
// I comandi e la gestione della scheda non sono nel firmware: stanno nel driver microSD (docs/esempi/driver/microsd.vdb).
// La microSD e una periferica di io.cfg (vos_io.cpp, vero, con il file system finto): i 4 pin li occupa la periferica per tutta la sua vita,
// non il montaggio; senza periferica non c'e nessun pin occupato e nessuna azione.
#include <Arduino.h>
#include "vos_board.h"
#include "vos_pins.h"
#include "vos_sd.h"
#include "vos_io.h"
#include "vos_drv_fs.h"
#include "vos_dev.h"
#include "vos_config.h"
#include "vos_common.h"
#include "vos_drv_gpio.h"
#include "fake_sd.h"
#include <FS.h>
#include "mini.h"
#include <string.h>
// ioListJson/ioText scrivono in un HeapBuf: qui si rileggono come String per usare indexOf
static String ioJ() { HeapBuf h(IO_JSON_MAX); CHECK(ioListJson(h)); return String(h.c_str()); }
static String ioT() { HeapBuf h(IO_JSON_MAX); CHECK(ioText(h)); return String(h.c_str()); }

// ---- finti pezzi (il resto e vero: vos_sd.cpp e vos_pins.cpp) ----
EspClass ESP;
FakeFsFaults g_ff;
namespace fs { std::map<std::string, std::shared_ptr<FakeNode>> g_fakeFiles; }
void drvGpioMode(int, DrvGpioMode) {}
void drvGpioWrite(int, bool) {}
bool drvGpioRead(int) { return false; }
int  drvGpioAnalog(int) { return 0; }
const DevOps* devFind(const char*) { return nullptr; }

static const int P4[4] = {10, 11, 12, 13};
static bool pinsFree() { for (int i = 0; i < 4; i++) if (pinIsUsed(P4[i])) return false; return true; }
static bool pinsSd() {                         // i 4 pin sono della periferica "sd1" (classe sd)
  for (int k = 0; k < 4; k++) { bool f = false; for (int i = 0; i < pinCount(); i++) if (pinAt(i)->gpio == P4[k] && !strcmp(pinAt(i)->owner, "sd1") && !strcmp(pinAt(i)->note, "sd")) f = true; if (!f) return false; }
  return true;
}
static char err[SD_ERR_MAX];
static bool has(const char* h, const char* n) { return strstr(h, n) != nullptr; }
static bool pathOk(const char* p) { err[0] = 0; return sdPathOk(p, err, sizeof err); }
static bool mount() { err[0] = 0; return sdMount(err, sizeof err); }
static bool unmount() { err[0] = 0; return sdUnmount(err, sizeof err); }
static void reinit() { pinsInit(); ioInit(); }          // come all avvio: registro dei pin vuoto, poi si legge io.cfg
static bool hst(int state) { SdVolume v; return sdVolume(v) && v.state == state; }      // stato del volume (lo stesso numero che il codice del driver legge con sd_status)
static char txt[SD_ERR_MAX];
static bool tx(const char* n) { sdText(txt, sizeof txt); return has(txt, n); }

int main() {
  pinsInit(); fakeSdReset();

  // ---- percorsi ----
  CHECK(pathOk("/") && pathOk("/a.txt") && pathOk("/dati/log 1(2).txt") && pathOk("/A_b-c.d"));
  const char* bad[] = {"", "a", "a.txt", "/../x", "/a/../b", "/a//b", "/a/", "/a\n", "/a\tb", "/a:b", "/a*b", "/a?b", "/a\\b", "/\xc3\xa4", "/a;b", "/a\"b", "/a,b", "/a ", "/a.", "/ a", "/a /b", "/a./b", "/./x", "/x/."};   // spazio o punto in fondo a un nome, spazio all'inizio, "." (FatFs li taglia o li confonde)
  for (const char* b : bad) CHECK(!pathOk(b) && err[0]);
  { char lg[130] = "/"; memset(lg + 1, 'x', 100); lg[101] = 0; CHECK(!pathOk(lg)); lg[100] = 0; CHECK(pathOk(lg)); }
  CHECK(!sdPathOk(nullptr, err, sizeof err));
  { char tiny[6]; CHECK(!sdPathOk("a", tiny, sizeof tiny) && tiny[5] == 0 && strlen(tiny) == 5); CHECK(!sdPathOk("a", nullptr, 0)); }   // errore in un buffer minuscolo: nessun overflow

  // ---- senza periferica "sd" in io.cfg: nulla si vede, nulla si occupa, nessuna azione ----
  CHECK(!sdConfigured() && pinsFree());
  CHECK(!mount() && has(err, "Nessuna microSD") && g_sd.begins == 0);
  CHECK(tx("non configurata"));
  { SdVolume v; CHECK(!sdVolume(v)); }   // senza periferica: nessun volume

  // ---- aggiungere la microSD come una periferica: controlli di frequenza e pin, come per le altre ----
  String nm, e;
  CHECK(!ioAdd("sd", 10, "", "mosi=11 sck=12 miso=13 khz=399", nm, e) && e.indexOf("400") >= 0);
  CHECK(!ioAdd("sd", 10, "", "mosi=11 sck=12 miso=13 khz=20001", nm, e) && !ioAdd("sd", 10, "", "mosi=11 sck=12 miso=13 khz=0", nm, e) && !ioAdd("sd", 10, "", "mosi=11 sck=12 miso=13 khz=-5", nm, e));
  CHECK(!ioAdd("sd", 10, "", "mosi=10 sck=12 miso=13 khz=1000", nm, e) && e.indexOf("diversi") >= 0);
  CHECK(!ioAdd("sd", 10, "", "mosi=11 sck=12 khz=1000", nm, e) && e.indexOf("quattro") >= 0);          // manca MISO
  { const int bad[][4] = {{19,11,12,13},{10,20,12,13},{10,11,0,13},{10,11,12,3},{10,11,12,43},{10,11,44,13},{10,8,12,13},{10,11,9,13},{10,11,12,48},{10,11,12,26},{10,11,12,49},{10,11,12,100}};
    for (const auto& b : bad) { char o[64]; snprintf(o, sizeof o, "mosi=%d sck=%d miso=%d khz=1000", b[1], b[2], b[3]); CHECK(!ioAdd("sd", b[0], "", o, nm, e) && e.length()); } }   // USB, BOOT, avvio, console, I2C, LED, memoria, inesistenti
  CHECK(!ioAdd("sd", 10, "led", "mosi=11 sck=12 miso=13 khz=1000", nm, e) && !ioAdd("sd", 10, "a b", "mosi=11 sck=12 miso=13 khz=1000", nm, e));   // nomi non validi
  CHECK(!sdConfigured() && pinsFree() && ioCount() == 0 && g_sd.begins == 0);                       // nessun tentativo ha lasciato pin occupati o righe
  CHECK(pinClaim(12, "rele1", "rele"));
  CHECK(!ioAdd("sd", 10, "", "mosi=11 sck=12 miso=13 khz=1000", nm, e) && !pinIsUsed(10) && !pinIsUsed(11) && !pinIsUsed(13) && ioCount() == 0);   // un pin gia usato da altri: nessun pin preso
  pinRelease(12);
  CHECK(ioAdd("sd", 10, "", "mosi=11 sck=12 miso=13 khz=1000", nm, e) && nm == "sd1" && sdConfigured() && ioCount() == 1);
  CHECK(pinsSd() && g_sd.begins == 0 && !sdUp());                  // i 4 pin sono della periferica anche da non montata; il driver non e toccato
  CHECK(!ioAdd("sd", 14, "", "mosi=15 sck=16 miso=17 khz=1000", nm, e) && e.indexOf("gia una microSD") >= 0);   // una sola
  CHECK(!ioAdd("rele", 11, "", "", nm, e));                        // un pin della microSD non e piu libero per altro
  { String j = ioJ(), t = ioT();
    CHECK(j.indexOf("\"c\":\"sd\"") > 0 && j.indexOf("\"g\":10") > 0 && j.indexOf("\"g2\":11,\"g3\":12,\"g4\":13,\"k\":1000") > 0 && j.indexOf("\"sdp\":[10,11,12,13]") > 0 && j.indexOf("\"out\":0") > 0);
    CHECK(t.indexOf("CS GPIO10") >= 0 && t.indexOf("MISO GPIO13") >= 0 && t.indexOf("1000 kHz") >= 0 && t.indexOf("non montata") >= 0); }
  { File f = drvFsOpen("/io.cfg", "r"); String c = f.readString(); f.close(); CHECK(c.indexOf("sd1 7 10 0 11 12 13 1000") >= 0); }   // salvata in io.cfg
  reinit(); CHECK(sdConfigured() && ioCount() == 1 && pinsSd());                                    // e ricaricata
  { String bak = "x"; File f = drvFsOpen("/io.cfg", "w"); f.print("sd1 7 10 0 11 12 13\n"); f.close();   // riga senza frequenza (nessuna di fabbrica la scrive): vale 1000
    reinit(); CHECK(sdConfigured() && ioJ().indexOf("\"k\":1000") > 0);
    f = drvFsOpen("/io.cfg", "w"); f.print("sd1 7 10 0 11 12 13 100\nsd2 7 14 0 15 16 17 99999\nsd3 7 10 0 10 12 13 1000\nsd4 7 10 0 11 12 20 1000\n"); f.close();   // frequenza fuori limite, pin uguali, pin USB: scartate
    reinit(); CHECK(!sdConfigured() && ioCount() == 0);
    f = drvFsOpen("/io.cfg", "w"); f.print("sd1 7 10 0 11 12 13 400\nsd2 7 14 0 15 16 17 1000\n"); f.close();   // una sola microSD: la seconda e scartata
    reinit(); CHECK(sdConfigured() && ioCount() == 1 && ioJ().indexOf("\"k\":400") > 0);
    f = drvFsOpen("/io.cfg", "w"); f.print("sd1 7 10 0 11 12 13 1000\n"); f.close(); reinit(); CHECK(sdConfigured() && ioCount() == 1 && pinsSd()); }

  // ---- stato di partenza: nulla montato, il driver non e stato nemmeno toccato ----
  CHECK(!sdUp() && pinsSd() && g_sd.begins == 0);
  CHECK(tx("non montata"));

  // ---- manca la scheda: errore chiaro, pin liberati, niente blocchi ----
  g_sd.card = false;
  CHECK(!mount() && err[0] && g_sd.begins == 1 && !sdUp() && pinsSd());
  CHECK(tx("(") && tx("non montata"));
  CHECK(!mount() && pinsSd());
  CHECK(unmount() && pinsSd());                             // smontare quando non c'e nulla non e un errore

  // ---- c'e la scheda ----
  g_sd.card = true;
  CHECK(mount() && sdUp() && pinsSd());
  CHECK(g_sd.cs == 10 && g_sd.mosi == 11 && g_sd.sck == 12 && g_sd.miso == 13 && g_sd.hz == 1000000);   // pin e frequenza arrivano dalla periferica
  { int b = g_sd.begins; CHECK(mount() && g_sd.begins == b); }   // montare due volte non rompe nulla ne tocca il driver
  CHECK(hst(1) && tx("montata") && tx("SDHC") && tx("FAT32"));
  { SdVolume v; CHECK(sdVolume(v) && v.state == 1 && !strcmp(v.fs, "FAT32") && v.khz == 1000); }   // dati letti dal driver (sd_status)

  // ---- buffer troppo piccoli: niente scritture oltre la fine, testo sempre chiuso ----
  { char b[24]; memset(b, 'Z', sizeof b); sdText(b, 5); CHECK(strlen(b) <= 4); sdText(b, 0); CHECK(true); }

  // ---- prova di scrittura e lettura (nel driver): il file di prova non resta ----
  { char o[512], e[SD_ERR_MAX]; CHECK(sdProve(o, sizeof o, e, sizeof e) && g_sd.proves == 1 && g_sd.files.empty());
    g_sd.failWrite = true; CHECK(!sdProve(o, sizeof o, e, sizeof e) && has(e, "scrivere")); g_sd.failWrite = false;
    g_sd.corrupt = true;   CHECK(!sdProve(o, sizeof o, e, sizeof e) && has(e, "coincidono")); g_sd.corrupt = false; }

  // ---- primitive dei file (le usa Berry: sd_read, sd_write, sd_rm, sd_mkdir) ----
  { uint8_t buf[16]; char e[SD_ERR_MAX];
    CHECK(sdWriteBytes("/a.txt", (const uint8_t*)"ciao", 4, false, e, sizeof e) && g_sd.files["/a.txt"] == "ciao");
    CHECK(sdWriteBytes("/a.txt", (const uint8_t*)"!", 1, true, e, sizeof e) && g_sd.files["/a.txt"] == "ciao!");
    CHECK(sdReadAt("/a.txt", 0, buf, sizeof buf, e, sizeof e) == 5 && !memcmp(buf, "ciao!", 5));
    CHECK(sdReadAt("/a.txt", 5, buf, sizeof buf, e, sizeof e) == 0);                   // oltre la fine: 0 byte
    CHECK(sdReadAt("/nonesiste.txt", 0, buf, sizeof buf, e, sizeof e) < 0);
    CHECK(!sdWriteBytes("/../x", (const uint8_t*)"1", 1, false, e, sizeof e) && !sdWriteBytes("x", (const uint8_t*)"1", 1, false, e, sizeof e));   // percorsi non validi
    CHECK(sdMakeDir("/d", e, sizeof e) && g_sd.dirs.count("/d"));
    CHECK(sdRemove("/a.txt", e, sizeof e) && !g_sd.files.count("/a.txt"));
    CHECK(!sdRemove("/a.txt", e, sizeof e)); }


  // ---- il driver non e raggiungibile con la scheda smontata ----
  CHECK(unmount() && !sdUp() && pinsSd() && g_sd.ends >= 1);

  // ---- scheda tolta mentre e montata: errori chiari, si smonta senza problemi, e si rimonta ----
  CHECK(mount()); g_sd.gone = true;
  CHECK(unmount() && pinsSd());
  g_sd.gone = false; CHECK(mount() && pinsSd() && unmount() && pinsSd());

  // ---- scheda tolta mentre e montata: lo stato la vede, la smonta da sola e dice "non presente" (il widget della Home resta, sbarrato) ----
  CHECK(mount() && hst(1)); g_sd.gone = true;
  { int en = g_sd.ends; CHECK(hst(3) && !sdUp() && g_sd.ends == en + 1 && tx("tolta") && tx("non montata") && pinsSd()); }
  { SdVolume v; CHECK(sdVolume(v) && v.state == 3 && v.totalKb == 0 && !v.fs[0]); }   // dati azzerati
  g_sd.gone = false; CHECK(mount() && hst(1) && !tx("tolta") && unmount() && hst(2));
  // ---- exFAT o formato non FAT: rifiutato con "serve FAT32", mai formattato ----
  g_sd.exfat = true; CHECK(!mount() && has(err, "exFAT") && has(err, "FAT32") && hst(4) && !sdUp() && tx("exFAT"));
  { SdVolume v; CHECK(sdVolume(v) && v.state == 4); }
  g_sd.exfat = false; g_sd.badfs = true; CHECK(!mount() && has(err, "FAT/FAT32") && hst(4)); g_sd.badfs = false;
  g_sd.card = false; CHECK(!mount() && has(err, "inserita") && hst(3)); g_sd.card = true;      // senza scheda: "non presente"
  CHECK(mount() && hst(1) && !tx("(") && unmount());                                          // un montaggio riuscito cancella l'errore di prima

  // ---- si toglie la periferica con la scheda montata: si smonta da sola, i pin tornano liberi, nessuna azione resta possibile ----
  CHECK(mount() && sdUp() && pinsSd());
  { String er; int en = g_sd.ends; CHECK(ioRemove("sd1", er) && !sdUp() && g_sd.ends == en + 1 && pinsFree() && !sdConfigured() && ioCount() == 0); }
  CHECK(!mount() && has(err, "Nessuna microSD"));
  { File f = drvFsOpen("/io.cfg", "r"); CHECK(!f || f.size() == 0 || f.readString().indexOf("sd1") < 0); }   // e sparita anche da io.cfg
  // ---- altri pin e altra frequenza: il driver li riceve cosi come sono ----
  CHECK(ioAdd("sd", 4, "mia", "mosi=5 sck=6 miso=7 khz=400", nm, e) && nm == "mia" && sdConfigured());
  CHECK(mount() && g_sd.cs == 4 && g_sd.mosi == 5 && g_sd.sck == 6 && g_sd.miso == 7 && g_sd.hz == 400000 && unmount());
  CHECK(ioRename("mia", "scheda", e) && sdConfigured() && ioRemove("scheda", e) && !sdConfigured());

  // ---- i pin proposti dal file di scheda sono coerenti (4 diversi, mai riservati) ----
  { const int D[4] = {VOS_SD_DEF_CS, VOS_SD_DEF_MOSI, VOS_SD_DEF_SCK, VOS_SD_DEF_MISO};
    for (int i = 0; i < 4; i++) { CHECK(VOS_GPIO_EXISTS(D[i]) && !VOS_SD_RESERVED(D[i]) && D[i] == P4[i]); for (int k = i + 1; k < 4; k++) CHECK(D[i] != D[k]); } }
  return done("sd");
}
