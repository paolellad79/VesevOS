// VesevOS - prove sul computer: periferica microSD (devices/vos_sd.cpp) con il driver finto (fake_sd.cpp)
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
static char out[SD_OUT_MAX], err[SD_ERR_MAX];
static bool act(const char* a, const char* arg, int role) { out[0] = 0; err[0] = 0; return sdAct(a, arg, role, out, sizeof out, err, sizeof err); }
static bool has(const char* h, const char* n) { return strstr(h, n) != nullptr; }
static bool pathOk(const char* p) { err[0] = 0; return sdPathOk(p, err, sizeof err); }
static bool mount() { err[0] = 0; return sdMount(err, sizeof err); }
static bool unmount() { err[0] = 0; return sdUnmount(err, sizeof err); }
static void reinit() { pinsInit(); ioInit(); }          // come all avvio: registro dei pin vuoto, poi si legge io.cfg
static char status[SD_OUT_MAX];
static bool st(const char* n) { sdStatusJson(status, sizeof status); return has(status, n); }
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
  CHECK(!act("state", "", ROLE_ADMIN) && has(err, "Nessuna microSD") && !act("ls", "/", ROLE_OPER) && !act("test", "", ROLE_ADMIN) && g_sd.begins == 0 && g_sd.writes == 0);
  CHECK(st("\"cfg\":0") && tx("non configurata") && !st("\"Pin\""));
  { char hj[SD_HOME_MAX]; sdHomeJson(hj, sizeof hj); CHECK(!hj[0]); char t1[1]; sdHomeJson(t1, 1); sdHomeJson(t1, 0); CHECK(!t1[0]); }   // senza periferica: nessun widget nella Home

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
  CHECK(st("\"cfg\":1") && st("CS 10, MOSI 11, SCK 12, MISO 13") && st("1000 kHz"));
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
  CHECK(st("\"up\":0") && tx("non montata") && !st("Ultimo errore"));

  // ---- manca la scheda: errore chiaro, pin liberati, niente blocchi ----
  g_sd.card = false;
  CHECK(!mount() && err[0] && g_sd.begins == 1 && !sdUp() && pinsSd());
  CHECK(st("Ultimo errore") && tx("(") && tx("non montata"));
  CHECK(!mount() && pinsSd());
  CHECK(unmount() && pinsSd());                             // smontare quando non c'e nulla non e un errore

  // ---- c'e la scheda ----
  g_sd.card = true;
  CHECK(mount() && sdUp() && pinsSd() && !st("Ultimo errore"));
  CHECK(g_sd.cs == 10 && g_sd.mosi == 11 && g_sd.sck == 12 && g_sd.miso == 13 && g_sd.hz == 1000000);   // pin e frequenza arrivano dalla periferica
  { int b = g_sd.begins; CHECK(mount() && g_sd.begins == b); }   // montare due volte non rompe nulla ne tocca il driver
  CHECK(st("SDHC") && st("\"up\":1") && st("\"s\":1") && st("\"fs\":\"FAT32\"") && st("Formato") && st("8192 MB") && st("\"cap\":8388608") && st("CS 10, MOSI 11, SCK 12, MISO 13") && st("1000 kHz") && tx("montata") && tx("SDHC") && tx("FAT32"));
  { char hj[SD_HOME_MAX]; sdHomeJson(hj, sizeof hj); CHECK(has(hj, "\"s\":1") && has(hj, "\"fs\":\"FAT32\"") && has(hj, "\"cap\":8388608") && has(hj, "\"khz\":1000") && hj[strlen(hj) - 1] == '}'); }   // dati del widget della Home
  { sdStatusJson(status, sizeof status); CHECK(status[strlen(status) - 1] == '}' && status[0] == '{'); }

  // ---- buffer troppo piccoli: niente scritture oltre la fine, testo sempre chiuso ----
  { char b[24]; memset(b, 'Z', sizeof b); sdStatusJson(b, 10); CHECK(b[9] == 0 && b[10] == 'Z'); sdText(b, 5); CHECK(strlen(b) <= 4);
    sdStatusJson(b, 0); sdText(b, 0); CHECK(true); }

  // ---- ruoli ----
  CHECK(!act("state", "", ROLE_GUEST) && err[0]);
  CHECK(act("state", "", ROLE_OPER) && has(out, "\"rows\""));
  CHECK(act("ls", "", ROLE_OPER));
  CHECK(!act("mount", "", ROLE_OPER) && !act("unmount", "", ROLE_OPER) && !act("test", "", ROLE_OPER));
  CHECK(!act("read", "/a.txt", ROLE_OPER) && !act("write", "/a.txt,x", ROLE_OPER) && !act("rm", "/a.txt", ROLE_OPER) && !act("mkdir", "/d", ROLE_OPER) && !act("append", "/a.txt,x", ROLE_GUEST));
  CHECK(sdUp() && g_sd.writes == 0 && g_sd.reads == 0);       // nessuna azione vietata ha toccato il driver

  // ---- prova di scrittura e lettura (nel driver): il file di prova non resta ----
  CHECK(act("test", "", ROLE_ADMIN) && has(out, "\"ok\":true") && has(out, "\"bytes\":4096") && has(out, "\"wms\":3") && has(out, "\"rms\":2") && g_sd.files.empty() && g_sd.proves == 1);
  { char o[SD_OUT_MAX], e[SD_ERR_MAX]; CHECK(sdProve(o, sizeof o, e, sizeof e) && g_sd.proves == 2); }
  g_sd.failWrite = true; CHECK(!act("test", "", ROLE_ADMIN) && has(err, "scrivere")); g_sd.failWrite = false;
  g_sd.corrupt = true;   CHECK(!act("test", "", ROLE_ADMIN) && has(err, "coincidono")); g_sd.corrupt = false;

  // ---- file piccoli ----
  CHECK(act("write", "/a.txt,ciao mondo", ROLE_ADMIN) && g_sd.files["/a.txt"] == "ciao mondo" && has(out, "\"n\":10"));
  CHECK(act("append", "/a.txt,!", ROLE_ADMIN) && g_sd.files["/a.txt"] == "ciao mondo!");
  CHECK(act("write", "/virg.txt,uno,due", ROLE_ADMIN) && g_sd.files["/virg.txt"] == "uno,due");   // la virgola nel testo resta
  CHECK(act("read", "/a.txt", ROLE_ADMIN) && has(out, "\"text\":\"ciao mondo!\"") && has(out, "\"n\":11"));
  g_sd.files["/b.bin"] = std::string("A\x01\xff" "B", 4);
  CHECK(act("read", "/b.bin", ROLE_ADMIN) && has(out, "\"text\":\"A..B\""));                      // il binario diventa punti
  g_sd.files["/q.txt"] = "dice \"ciao\"\\\n";
  CHECK(act("read", "/q.txt", ROLE_ADMIN) && has(out, "\"text\":\"dice \\\"ciao\\\"\\\\\\n\""));   // virgolette, barra e a capo protetti
  CHECK(!act("read", "/nonesiste.txt", ROLE_ADMIN) && err[0]);
  g_sd.files["/grande.txt"] = std::string(5000, 'x');
  CHECK(act("read", "/grande.txt", ROLE_ADMIN) && has(out, "\"n\":1024"));                          // al massimo 1024 byte
  { char lg[600] = "/l.txt,"; memset(lg + 7, 'x', 513); lg[7 + 513] = 0; CHECK(!act("write", lg, ROLE_ADMIN) && !g_sd.files.count("/l.txt") && has(err, "512")); }
  { char lg[600] = "/m.txt,"; memset(lg + 7, 'x', 512); lg[7 + 512] = 0; CHECK(act("write", lg, ROLE_ADMIN) && g_sd.files["/m.txt"].size() == 512); }
  { int w = g_sd.writes, r = g_sd.reads; CHECK(!act("write", "/../x,1", ROLE_ADMIN) && !act("write", "x,1", ROLE_ADMIN) && !act("write", "", ROLE_ADMIN) && !act("read", "/a/../b", ROLE_ADMIN) && !act("rm", "/a//b", ROLE_ADMIN) && g_sd.writes == w && g_sd.reads == r); }
  { char lg[200]; memset(lg, 'a', 150); strcpy(lg + 150, ",x"); lg[0] = '/'; int w = g_sd.writes; CHECK(!act("write", lg, ROLE_ADMIN) && g_sd.writes == w); }   // percorso troppo lungo prima della virgola
  CHECK(act("mkdir", "/d", ROLE_ADMIN) && g_sd.dirs.count("/d"));
  CHECK(act("write", "/d/x.txt,1", ROLE_ADMIN));
  CHECK(act("rm", "/b.bin", ROLE_ADMIN) && !g_sd.files.count("/b.bin"));
  CHECK(!act("rm", "/b.bin", ROLE_ADMIN) && err[0]);
  CHECK(!act("cosa", "", ROLE_ADMIN) && has(err, "non valida") && !act(nullptr, nullptr, ROLE_ADMIN));

  // ---- elenco ----
  g_sd.files["/caff\xc3\xa8.txt"] = "x"; g_sd.files["/" + std::string(60, 'n') + ".txt"] = "x";
  CHECK(act("ls", "", ROLE_OPER) && has(out, "caff??.txt\",\"d\":0,\"s\":1,\"x\":1}") && has(out, "nnnn\",\"d\":0,\"s\":1,\"x\":1}") && has(out, "\"n\":\"a.txt\",\"d\":0,\"s\":11,\"x\":0}"));   // accenti e nomi troppo lunghi: si vedono ma segnati "x" (non apribili da qui)
  g_sd.files.erase("/caff\xc3\xa8.txt"); g_sd.files.erase("/" + std::string(60, 'n') + ".txt");
  CHECK(act("ls", "", ROLE_OPER) && has(out, "\"dir\":\"/\"") && has(out, "\"n\":\"a.txt\"") && has(out, "\"n\":\"d\",\"d\":1") && !has(out, "x.txt\""));
  CHECK(act("ls", "/d", ROLE_OPER) && has(out, "\"n\":\"x.txt\"") && has(out, "\"s\":1,\"x\":0}"));
  CHECK(!act("ls", "/nonesiste", ROLE_OPER) && !act("ls", "/a/../", ROLE_OPER));
  for (int i = 0; i < 40; i++) g_sd.files["/f" + std::to_string(100 + i) + ".t"] = "x";
  CHECK(act("ls", "/", ROLE_OPER) && has(out, "\"more\":1"));
  { char small[120]; err[0] = 0; CHECK(!sdAct("ls", "/", ROLE_OPER, small, sizeof small, err, sizeof err) && err[0] && strlen(small) < sizeof small); }   // risposta troppo lunga per il buffer: errore, mai overflow

  // ---- il driver non e raggiungibile con la scheda smontata ----
  CHECK(act("unmount", "", ROLE_ADMIN) && !sdUp() && pinsSd() && g_sd.ends >= 1 && has(out, "\"up\":0"));
  { int w = g_sd.writes, r = g_sd.reads; CHECK(!act("read", "/a.txt", ROLE_ADMIN) && !act("write", "/a.txt,x", ROLE_ADMIN) && !act("ls", "/", ROLE_OPER) && !act("test", "", ROLE_ADMIN) && has(err, "non e montata") && g_sd.writes == w && g_sd.reads == r); }

  // ---- scheda tolta mentre e montata: errori chiari, si smonta senza problemi, e si rimonta ----
  CHECK(mount()); g_sd.gone = true;
  CHECK(!act("read", "/a.txt", ROLE_ADMIN) && !act("ls", "/", ROLE_OPER) && !act("test", "", ROLE_ADMIN) && !act("write", "/z.txt,1", ROLE_ADMIN));
  CHECK(unmount() && pinsSd());
  g_sd.gone = false; CHECK(mount() && pinsSd() && unmount() && pinsSd());

  // ---- scheda tolta mentre e montata: lo stato la vede, la smonta da sola e dice "non presente" (il widget della Home resta, sbarrato) ----
  CHECK(mount() && st("\"s\":1")); g_sd.gone = true;
  { int en = g_sd.ends; CHECK(st("\"s\":3") && !sdUp() && g_sd.ends == en + 1 && st("tolta") && st("\"up\":0") && tx("non montata") && pinsSd()); }
  { char hj[SD_HOME_MAX]; sdHomeJson(hj, sizeof hj); CHECK(has(hj, "\"s\":3") && has(hj, "\"cap\":0") && has(hj, "\"fs\":\"\"")); }   // dati azzerati
  g_sd.gone = false; CHECK(mount() && st("\"s\":1") && !st("tolta") && unmount() && st("\"s\":2"));
  // ---- exFAT o formato non FAT: rifiutato con "serve FAT32", mai formattato ----
  g_sd.exfat = true; CHECK(!mount() && has(err, "exFAT") && has(err, "FAT32") && st("\"s\":4") && st("\"up\":0") && !sdUp() && tx("exFAT") && g_sd.writes == 0 + g_sd.writes);
  { char hj[SD_HOME_MAX]; sdHomeJson(hj, sizeof hj); CHECK(has(hj, "\"s\":4")); }
  g_sd.exfat = false; g_sd.badfs = true; CHECK(!mount() && has(err, "FAT/FAT32") && st("\"s\":4")); g_sd.badfs = false;
  g_sd.card = false; CHECK(!mount() && has(err, "inserita") && st("\"s\":3")); g_sd.card = true;      // senza scheda: "non presente"
  CHECK(mount() && st("\"s\":1") && !st("Ultimo errore") && unmount());                                // un montaggio riuscito cancella l'errore di prima

  // ---- si toglie la periferica con la scheda montata: si smonta da sola, i pin tornano liberi, nessuna azione resta possibile ----
  CHECK(mount() && sdUp() && pinsSd());
  { String er; int en = g_sd.ends; CHECK(ioRemove("sd1", er) && !sdUp() && g_sd.ends == en + 1 && pinsFree() && !sdConfigured() && ioCount() == 0); }
  CHECK(!mount() && has(err, "Nessuna microSD") && st("\"cfg\":0") && !act("state", "", ROLE_ADMIN));
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
