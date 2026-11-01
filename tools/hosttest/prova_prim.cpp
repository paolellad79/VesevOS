// VesevOS - prova sul computer del REGISTRO DELLE PRIMITIVE (packages/vos_prim*.cpp), gruppi sd e dev. Senza Arduino ne scheda.
// Compila i file veri del firmware (vos_prim, vos_prim_sd, vos_prim_dev, vos_pathres, vos_paths, vos_sd_path) con -DVOS_WITH_ELF=1 e metta
// al posto dell'hardware un finto: microSD in RAM (sd*), volume (volList), periferiche (io*). La lancia prova_prim.sh.
// Prova: nomi uguali a Berry, permessi per ruolo / use= / api, rifiuti (percorsi con "..", fuori dalla microSD, segreti, file di sistema,
// blocchi oltre 1024, buffer piccoli, argomenti nulli), microSD assente, periferiche non concesse, contesto di esecuzione.
// NON prova: il collegamento ELF vero (risolutore + caricatore), che serve la scheda.
#include <stdio.h>
#include <string.h>
#include <string>
#include <map>
#include "vos_prim.h"
#include "vos_hw.h"
#include "vos_sd.h"
#include "vos_vol.h"
#include "vos_io.h"
#include "vos_files.h"
#include "vos_layout.h"
#include "vos_paths.h"
#include "vos_prog_data.h"
#include "mini.h"

// ---- finti ----
// dati delle app (vos_prog_data): registrano chi li ha chiamati e con quali argomenti
static std::string g_dApp, g_dFile;
static int g_dKind = -1, g_dN = -1, g_dOff = -1, g_dAppend = -1, g_dChiamate = 0;
int progWhere(uint8_t, const char*) { return PW_FLASH; }
bool progDataDir(uint8_t kind, const char* name, char* out, size_t cap, ErrBuf&) { g_dChiamate++; g_dKind = kind; g_dApp = name; snprintf(out, cap, "/var/lib/%s", name); return true; }
int progDataWrite(uint8_t kind, const char* name, const char* file, const uint8_t*, size_t n, bool append) { g_dChiamate++; g_dKind = kind; g_dApp = name; g_dFile = file; g_dN = (int)n; g_dAppend = append; return (int)n; }
int progDataRead(uint8_t kind, const char* name, const char* file, uint32_t off, uint8_t*, size_t n) { g_dChiamate++; g_dKind = kind; g_dApp = name; g_dFile = file; g_dOff = (int)off; g_dN = (int)n; return 2; }
int progDataRemove(uint8_t kind, const char* name, const char* file) { g_dChiamate++; g_dKind = kind; g_dApp = name; g_dFile = file; return 0; }
static bool g_up = true, g_cfg = true, g_inuse = false;
static std::map<std::string, std::string> g_files;     // percorso sulla scheda -> contenuto
static std::map<std::string, bool> g_dirs;
static int g_nmount = 0, g_nunmount = 0;
const char* tr(const char* it) { return it; }
void vlog(const char*, ...) {}
int layoutState() { return 0; }
extern "C++" int64_t esp_timer_get_time() { return 0; }
const char* layoutPath(LayoutItem) { return "/bin"; }
const char* layoutPathNew(LayoutItem) { return "/bin"; }
const char* layoutPathOld(LayoutItem) { return "/apps"; }
bool fsCleanTo(std::string_view in, char* out, size_t cap) {      // le stesse regole di vos_files.cpp, senza i collegamenti
  if (cap < FS_PATH_MAX + 1 || in.empty() || in.size() > FS_PATH_MAX) return false;
  std::string s(in);
  if (s[0] != '/') s = "/" + s;
  for (size_t i = 0; i < s.size(); i++) {
    unsigned char c = s[i];
    if (c < 32 || c >= 127 || c == '\\') return false;
    if (i + 1 < s.size() && ((c == '.' && s[i + 1] == '.') || (c == '/' && s[i + 1] == '/'))) return false;
  }
  if (s.size() > 1 && s.back() == '/') s.pop_back();
  strcpy(out, s.c_str());
  return pathSdShortcut(out, cap, FS_PATH_MAX);
}
bool sdUp() { return g_up; }
bool sdConfigured() { return g_cfg; }
bool sdMount(char* err, size_t) { g_nmount++; if (!g_cfg) { strcpy(err, "no"); return false; } g_up = true; return true; }
bool sdUnmount(char*, size_t) { g_nunmount++; g_up = false; return true; }
bool sdVolume(SdVolume& v) { if (!g_cfg) return false; memset(&v, 0, sizeof v); v.state = g_up ? 1 : 3; if (g_up) strcpy(v.fs, "FAT32"); v.totalKb = 1000; v.usedKb = 10; v.khz = 1000; return true; }
bool sdInUse() { return g_inuse; }
static int g_nformat = 0;
bool sdFormat(char*, size_t) { g_nformat++; return true; }
bool sdFormatStart(char*, size_t) { g_nformat++; return true; }          // la formattazione parte in un task a parte: la primitiva torna subito
bool sdFormatting() { return false; }
void sdFormatInfo(SdFmtInfo& f) { memset(&f, 0, sizeof f); f.running = true; f.secs = 42; }
int sdReadAt(const char* p, uint32_t off, uint8_t* buf, size_t n, char*, size_t) {
  auto it = g_files.find(p); if (it == g_files.end()) return -1;
  if (off >= it->second.size()) return 0;
  size_t k = it->second.size() - off; if (k > n) k = n;
  memcpy(buf, it->second.data() + off, k); return (int)k;
}
bool sdWriteBytes(const char* p, const uint8_t* b, size_t n, bool app, char*, size_t) { std::string d((const char*)b, n); if (app) g_files[p] += d; else g_files[p] = d; return true; }
bool sdRemove(const char* p, char*, size_t) { return strcmp(p, "/") != 0 && g_files.erase(p) > 0; }
bool sdMakeDir(const char* p, char*, size_t) { g_dirs[p] = true; return true; }
VolErr volList(const char* path, int role, int start, VolEntry* out, int max, int& count, int& consumed, bool& more) {
  count = consumed = 0; more = false;
  if (!g_up) return VOL_NO_MEDIA;
  if (strcmp(path, "/media/sd") != 0) return VOL_NOT_FOUND;
  const char* nomi[] = { "a.txt", "chiave.pem", "b.txt", "c.txt", "d.txt", "e.txt", "f.txt", "g.txt", "h.txt", "i.txt", "j.txt" };   // 11 voci, 1 segreta
  for (int i = start; i < 11 && count < max; i++) {
    consumed++;
    if (role < ROLE_ADMIN && !strcmp(nomi[i], "chiave.pem")) continue;       // nascosta come nel firmware
    strcpy(out[count].name, nomi[i]); out[count].dir = false; out[count].size = 10 + i; out[count].mtime = 0; count++;
  }
  more = start + consumed < 11;
  return VOL_OK;
}
VolErr volStat(const char*, int, VolEntry&) { return VOL_NOT_FOUND; }
// periferiche: rele1 (uscita), bmp1 (sensore), adc1 (ingresso)
static int g_rele = 0;
int ioCount() { return 3; }
const char* ioNameAt(int i) { static const char* n[] = { "rele1", "bmp1", "adc1" }; return i >= 0 && i < 3 ? n[i] : ""; }
bool ioHas(const char* n) { return !strcmp(n, "rele1") || !strcmp(n, "bmp1") || !strcmp(n, "bmp1.p") || !strcmp(n, "adc1"); }
const char* ioClassName(const char* n) { return !strcmp(n, "rele1") ? "rele" : !strcmp(n, "bmp1") ? "i2c" : "analogico"; }
bool ioGet(const char* n, int& v, ErrBuf&) { v = !strcmp(n, "rele1") ? g_rele : !strcmp(n, "bmp1") ? 2150 : !strcmp(n, "bmp1.p") ? 101325 : 1234; return true; }
bool ioSet(const char* n, int v, int role, ErrBuf& e) { if (strcmp(n, "rele1") || role != ROLE_ADMIN) { e = "no"; return false; } g_rele = v; return true; }

// ---- finti del nucleo hardware (vos_hw): registrano l'ultima chiamata; la logica vera di vos_hw.cpp la prova prova_hw ----
static char g_hwDrv[32]; static int g_hwIdx = -9, g_hwVal = -9, g_hwCalls = 0;
static int hwRec(const char* d, int i, int v) { snprintf(g_hwDrv, sizeof g_hwDrv, "%s", d); g_hwIdx = i; g_hwVal = v; g_hwCalls++; return 0; }
int hwGpioMode(const char* d, int i, int m) { return hwRec(d, i, m); }
int hwGpioRead(const char* d, int i) { hwRec(d, i, 0); return 1; }
int hwGpioWrite(const char* d, int i, int l) { return hwRec(d, i, l); }
int hwI2cProbe(const char* d) { hwRec(d, 0, 0); return 1; }
int hwI2cRead(const char* d, int reg, uint8_t* b, int n) { hwRec(d, reg, n); memset(b, 0x5a, (size_t)n); return n; }
int hwI2cWrite(const char* d, int reg, const uint8_t*, int n) { hwRec(d, reg, n); return n; }
int hwSpiTransfer(const char* d, const uint8_t*, uint8_t*, int n) { hwRec(d, 0, n); return n; }
int hwDelayUs(int us) { return us > 10000 ? 10000 : us; }
uint32_t hwUptime() { return 77; }
static void* g_tid = (void*)1;                           // il "task" corrente per la prova dei moduli insieme
static void* chiFinto() { return g_tid; }

// ---- prove ----
typedef int (*F_status)(int32_t*, int, char*, int);
typedef int (*F_read)(const char*, int, void*, int);
typedef int (*F_write)(const char*, const void*, int, int);
typedef int (*F_path)(const char*);
typedef int (*F_void)(void);
typedef int (*F_ls)(const char*, int, char*, int, int*);
typedef int (*F_dget)(const char*, int*);
typedef int (*F_dset)(const char*, int);
typedef int (*F_dlist)(char*, int);

template <class T> static T P(const char* nome, const char* gruppo, int ruolo_atteso) {
  const char* g = nullptr;
  const vos_prim_t* p = vos_prim_trova(nome, &g);
  CHECK(p != nullptr);
  if (!p) return nullptr;
  CHECK(g && !strcmp(g, gruppo));
  CHECK(p->ruolo_min == ruolo_atteso);
  CHECK(!strcmp(p->nome, nome));
  CHECK(vos_prim_trova_elf((std::string("vos_") + nome).c_str(), nullptr) == p);
  return (T)p->fn;
}

int main() {
  vos_prim_gruppi_registra();
  CHECK(vos_prim_conta() == 3 + 2 + 9 + 3 + 3 + 3 + 1 + 2 + 4);    // base (con print), log, sd, dev, gpio, i2c, spi, time, files
  CHECK(vos_prim_registra("x", nullptr, 1) < 0);

  // nomi, gruppi e ruoli minimi come in Berry (lettura: Operatore; scrittura: Admin)
  F_status sd_status = P<F_status>("sd_status", "sd", ROLE_OPER);
  F_read sd_read = P<F_read>("sd_read", "sd", ROLE_OPER);
  F_ls listdir = P<F_ls>("listdir", "sd", ROLE_OPER);
  F_write sd_write = P<F_write>("sd_write", "sd", ROLE_ADMIN);
  F_void sd_mount = P<F_void>("sd_mount", "sd", ROLE_ADMIN);
  F_void sd_eject = P<F_void>("sd_eject", "sd", ROLE_ADMIN);
  F_path sd_format = P<F_path>("sd_format", "sd", ROLE_ADMIN);
  F_path sd_rm = P<F_path>("sd_rm", "sd", ROLE_ADMIN);
  F_path sd_mkdir = P<F_path>("sd_mkdir", "sd", ROLE_ADMIN);
  F_dget dev_get = P<F_dget>("dev_get", "dev", ROLE_OPER);
  F_dlist dev_list = P<F_dlist>("dev_list", "dev", ROLE_OPER);
  F_dset dev_set = P<F_dset>("dev_set", "dev", ROLE_ADMIN);
  CHECK(sd_status && sd_read && listdir && sd_write && sd_mount && sd_eject && sd_format && sd_rm && sd_mkdir && dev_get && dev_list && dev_set);
  if (g_fail) return done("prova_prim");

  // permessi del risolutore: ruolo, use=, api
  const char* g; const vos_prim_t* w = vos_prim_trova("sd_write", &g);
  CHECK(vos_prim_permessa(g, w, ROLE_ADMIN, "log sd", 2) == VOS_PRIM_OK);
  CHECK(vos_prim_permessa(g, w, ROLE_OPER, "log sd", 2) == VOS_PRIM_NO_RUOLO);
  CHECK(vos_prim_permessa(g, w, ROLE_GUEST, "log sd", 2) == VOS_PRIM_NO_RUOLO);
  CHECK(vos_prim_permessa(g, w, ROLE_ADMIN, "log", 2) == VOS_PRIM_NO_USE);
  CHECK(vos_prim_permessa(g, w, ROLE_ADMIN, nullptr, 2) == VOS_PRIM_NO_USE);
  CHECK(vos_prim_permessa(g, w, ROLE_ADMIN, "log sd", 1) == VOS_PRIM_NO_API);        // un modulo dell'API 1 non vede sd
  CHECK(vos_prim_permessa(g, w, ROLE_ADMIN, "sdx", 2) == VOS_PRIM_NO_USE);           // la parola intera, non un pezzo
  const vos_prim_t* rd = vos_prim_trova("sd_read", &g);
  CHECK(vos_prim_permessa(g, rd, ROLE_OPER, "sd", 2) == VOS_PRIM_OK);
  const vos_prim_t* dg = vos_prim_trova("dev_set", &g);
  CHECK(vos_prim_permessa(g, dg, ROLE_OPER, "dev", 2) == VOS_PRIM_NO_RUOLO);
  CHECK(vos_prim_permessa(g, dg, ROLE_ADMIN, "dev,rele1", 2) == VOS_PRIM_OK);        // virgola o spazio

  // sd_status
  int32_t v[4]; char fs[8];
  CHECK(sd_status(v, 4, fs, sizeof fs) == 4 && v[0] == 1 && v[1] == 1000 && !strcmp(fs, "FAT32"));
  CHECK(sd_status(v, 3, fs, sizeof fs) == VOS_PRIM_E_ARG && sd_status(nullptr, 4, nullptr, 0) == VOS_PRIM_E_ARG);
  CHECK(sd_status(v, 4, nullptr, 0) == 4);
  g_cfg = false; CHECK(sd_status(v, 4, fs, sizeof fs) == 4 && v[0] == 0); g_cfg = true;

  // scrivi e rileggi
  CHECK(sd_write("/sd/prova.txt", "ciao", 4, 0) == 0);
  CHECK(sd_write("/media/sd/prova.txt", "!!", 2, 1) == 0);
  char b[64]; memset(b, 'x', sizeof b);
  CHECK(sd_read("/sd/prova.txt", 0, b, 64) == 6 && !memcmp(b, "ciao!!", 6));
  CHECK(sd_read("/sd/prova.txt", 4, b, 64) == 2);
  CHECK(sd_read("/sd/prova.txt", 99, b, 64) == 0);                                  // oltre la fine
  CHECK(sd_read("/sd/manca.txt", 0, b, 64) == VOS_PRIM_E_NONTROVATO);
  CHECK(sd_read("/sd/prova.txt", 0, b, 0) == 0);

  // rifiuti: argomenti
  CHECK(sd_read(nullptr, 0, b, 8) == VOS_PRIM_E_PERCORSO && sd_read("/sd/a", -1, b, 8) == VOS_PRIM_E_ARG);
  CHECK(sd_read("/sd/prova.txt", 0, nullptr, 8) == VOS_PRIM_E_ARG && sd_read("/sd/prova.txt", 0, b, -1) == VOS_PRIM_E_ARG);
  static char grande[2048];
  CHECK(sd_read("/sd/prova.txt", 0, grande, 1025) == VOS_PRIM_E_GRANDE && sd_write("/sd/g.txt", grande, 1025, 0) == VOS_PRIM_E_GRANDE);
  CHECK(sd_write("/sd/g.txt", grande, 1024, 0) == 0);
  CHECK(sd_write("/sd/g.txt", nullptr, 4, 0) == VOS_PRIM_E_ARG && sd_write("/sd/g.txt", "a", -1, 0) == VOS_PRIM_E_ARG);
  // rifiuti: percorsi
  CHECK(sd_read("/etc/passwd", 0, b, 8) == VOS_PRIM_E_PERCORSO);                    // fuori dalla microSD
  CHECK(sd_read("/vesevos.conf", 0, b, 8) == VOS_PRIM_E_PERCORSO);
  CHECK(sd_read("/sd/../vesevos.conf", 0, b, 8) == VOS_PRIM_E_PERCORSO);            // ".." risolto: esce dalla scheda
  CHECK(sd_read("/sd/a/../../x", 0, b, 8) == VOS_PRIM_E_PERCORSO);
  CHECK(sd_write("/sd/../x.txt", "a", 1, 0) == VOS_PRIM_E_PERCORSO);
  CHECK(sd_write("/sd/a\\b", "a", 1, 0) == VOS_PRIM_E_PERCORSO);
  CHECK(sd_write("/sd/a\x01", "a", 1, 0) == VOS_PRIM_E_PERCORSO);
  std::string lungo = "/sd/" + std::string(200, 'a');
  CHECK(sd_write(lungo.c_str(), "a", 1, 0) == VOS_PRIM_E_PERCORSO);                 // oltre il tetto del testo
  CHECK(sd_mkdir("/sd") == 0);   // la radice: la rifiuta devices (qui il finto accetta); il modulo non la tocca comunque
  CHECK(sd_rm("/sd") != 0);                                                         // mai la radice
  // segreti e file di sistema: mai, nemmeno per l'Admin
  CHECK(sd_write("/sd/chiave.pem", "k", 1, 0) == VOS_PRIM_E_VIETATO);
  CHECK(sd_write("/sd/x.KEY", "k", 1, 0) == VOS_PRIM_E_VIETATO);
  CHECK(sd_write("/sd/copia.vbk", "k", 1, 0) == VOS_PRIM_E_VIETATO);
  g_files["/chiave.pem"] = "SEGRETO";
  CHECK(sd_read("/sd/chiave.pem", 0, b, 8) == VOS_PRIM_E_NONTROVATO);               // come se non esistesse
  CHECK(sd_rm("/sd/chiave.pem") == VOS_PRIM_E_VIETATO && sd_mkdir("/sd/x.crt") == VOS_PRIM_E_VIETATO);
  CHECK(g_files.count("/chiave.pem") == 1);
  CHECK(sd_write("/sd/vesevos.conf", "k", 1, 0) == VOS_PRIM_E_VIETATO);   // file di sistema anche sulla scheda
  // microSD assente
  g_up = false;
  CHECK(sd_read("/sd/prova.txt", 0, b, 8) == VOS_PRIM_E_NOSD && sd_write("/sd/p.txt", "a", 1, 0) == VOS_PRIM_E_NOSD);
  CHECK(sd_rm("/sd/p.txt") == VOS_PRIM_E_NOSD && sd_mkdir("/sd/d") == VOS_PRIM_E_NOSD);
  CHECK(sd_status(v, 4, nullptr, 0) == 4 && v[0] == 3);
  char out[1024]; int next = 0;
  CHECK(listdir("/sd", 0, out, sizeof out, &next) == VOS_PRIM_E_NOSD);
  CHECK(sd_mount() == 0 && g_up);
  g_up = false; g_cfg = false; CHECK(sd_mount() == VOS_PRIM_E_NOSD); g_cfg = true; g_up = true;
  // formattazione: serve la parola di conferma, rifiuta se la scheda e in uso o non c'e la periferica
  CHECK(sd_format(nullptr) == VOS_PRIM_E_ARG && sd_format("si") == VOS_PRIM_E_ARG && g_nformat == 0);
  g_inuse = true; CHECK(sd_format("FORMATTA") == VOS_PRIM_E_OCCUPATO && g_nformat == 0); g_inuse = false;
  g_cfg = false; CHECK(sd_format("FORMATTA") == VOS_PRIM_E_NOSD && g_nformat == 0); g_cfg = true;
  CHECK(sd_format("FORMATTA") == 0 && g_nformat == 1);
  // smontaggio: rifiutato se in uso
  g_inuse = true; CHECK(sd_eject() == VOS_PRIM_E_OCCUPATO && g_nunmount == 0); g_inuse = false;
  CHECK(sd_eject() == 0 && g_nunmount == 1 && !g_up); g_up = true;
  // cancella e cartella
  CHECK(sd_rm("/sd/prova.txt") == 0 && sd_read("/sd/prova.txt", 0, b, 8) == VOS_PRIM_E_NONTROVATO);
  CHECK(sd_mkdir("/sd/cart") == 0);

  // listdir: pagine da 8, la voce segreta non c'e, buffer troppo piccolo = errore, mai troncato
  int tot = 0, giri = 0; next = 0;
  while (next >= 0 && giri < 10) {
    int n = listdir("/sd", next, out, sizeof out, &next);
    CHECK(n >= 0 && n <= 8);
    CHECK(strstr(out, "chiave") == nullptr);
    tot += n; giri++;
  }
  CHECK(tot == 10 && giri == 2 && next == -1);
  CHECK(listdir("/sd", 0, out, 100, &next) == VOS_PRIM_E_GRANDE);
  CHECK(listdir("/sd", -1, out, sizeof out, &next) == VOS_PRIM_E_ARG && listdir("/sd", 0, out, sizeof out, nullptr) == VOS_PRIM_E_ARG);
  CHECK(listdir("/sd", 5000, out, sizeof out, &next) == VOS_PRIM_E_ARG);
  CHECK(listdir("/", 0, out, sizeof out, &next) == VOS_PRIM_E_PERCORSO && listdir("/media", 0, out, sizeof out, &next) == VOS_PRIM_E_PERCORSO);
  CHECK(listdir("/sd/../etc", 0, out, sizeof out, &next) == VOS_PRIM_E_PERCORSO);
  CHECK(listdir("/sd/manca", 0, out, sizeof out, &next) == VOS_PRIM_E_NONTROVATO);
  listdir("/sd", 0, out, sizeof out, &next);
  CHECK(!strncmp(out, "F 10 a.txt\nF 12 b.txt\n", 22));

  // dev: nessun modulo in esecuzione = nessun permesso
  int val = -1;
  CHECK(dev_get("rele1", &val) == VOS_PRIM_E_USE && dev_set("rele1", 1) == VOS_PRIM_E_USE && dev_list(out, sizeof out) == 0);
  CHECK(!vos_prim_esec_ha("rele1") && vos_prim_esec_ruolo() == ROLE_GUEST);
  CHECK(vos_prim_esec_entra(ROLE_ADMIN, "dev rele1 bmp1"));
  CHECK(!vos_prim_esec_entra(ROLE_ADMIN, "dev"));                                   // un solo modulo alla volta
  CHECK(vos_prim_esec_ruolo() == ROLE_ADMIN && vos_prim_esec_ha("bmp1") && !vos_prim_esec_ha("adc1") && !vos_prim_esec_ha("bmp"));
  CHECK(dev_get("rele1", &val) == 0 && val == 0);
  CHECK(dev_set("rele1", 5) == 0 && g_rele == 1 && dev_get("rele1", &val) == 0 && val == 1);
  CHECK(dev_get("bmp1", &val) == 0 && val == 2150 && dev_get("bmp1.p", &val) == 0 && val == 101325);   // "bmp1.p" con "bmp1"
  CHECK(dev_set("bmp1.p", 1) == VOS_PRIM_E_USE);                                    // dev_set vuole il nome intero
  CHECK(dev_get("adc1", &val) == VOS_PRIM_E_USE);                                   // esiste ma non concessa
  CHECK(dev_get("nulla", &val) == VOS_PRIM_E_NONTROVATO && dev_set("nulla", 1) == VOS_PRIM_E_NONTROVATO);
  CHECK(dev_get(nullptr, &val) == VOS_PRIM_E_ARG && dev_get("rele1", nullptr) == VOS_PRIM_E_ARG && dev_get("", &val) == VOS_PRIM_E_ARG);
  CHECK(dev_get("rele1.aaaaaaaaaaaaaaaaaaaaaaaaaaaa", &val) == VOS_PRIM_E_ARG);
  CHECK(dev_set("bmp1", 1) == VOS_PRIM_E_IO);                                       // concessa, ma non e un'uscita: lo dice ioSet
  CHECK(dev_list(out, sizeof out) == 2 && !strcmp(out, "rele1 rele\nbmp1 i2c\n"));  // solo le concesse, mai adc1
  CHECK(dev_list(out, 8) == VOS_PRIM_E_GRANDE && out[0] == 0);                      // buffer corto: errore, mai troncato
  CHECK(dev_list(nullptr, 8) == VOS_PRIM_E_ARG && dev_list(out, 0) == VOS_PRIM_E_ARG);
  vos_prim_esec_esce();
  CHECK(dev_get("rele1", &val) == VOS_PRIM_E_USE && vos_prim_esec_ruolo() == ROLE_GUEST);
  CHECK(vos_prim_esec_entra(ROLE_OPER, "dev"));                                     // use= senza nomi: nessuna periferica
  CHECK(dev_get("rele1", &val) == VOS_PRIM_E_USE && dev_list(out, sizeof out) == 0);
  vos_prim_esec_esce();
  // use= troppo lungo: mai troncato in silenzio
  std::string u(VOS_PRIM_USE_MAX + 5, 'a');
  CHECK(vos_prim_esec_entra(ROLE_ADMIN, u.c_str()) && !vos_prim_esec_ha("a"));
  vos_prim_esec_esce();
  // ---- gruppi gpio, i2c, spi, time e print (1.8.13) ----
  typedef int (*F_i3)(const char*, int, int);
  typedef int (*F_i2)(const char*, int);
  typedef int (*F_i1)(const char*);
  typedef int (*F_rw)(const char*, int, void*, int);
  typedef int (*F_rwc)(const char*, int, const void*, int);
  typedef int (*F_sp)(const char*, const void*, void*, int);
  typedef int (*F_pr)(const char*);
  typedef uint32_t (*F_up)(void);
  typedef int (*F_dly)(int);
  F_i3 gpio_mode = P<F_i3>("gpio_mode", "gpio", ROLE_GUEST), gpio_write = P<F_i3>("gpio_write", "gpio", ROLE_GUEST);
  F_i2 gpio_read = P<F_i2>("gpio_read", "gpio", ROLE_GUEST);
  F_i1 i2c_probe = P<F_i1>("i2c_probe", "i2c", ROLE_GUEST);
  F_rw i2c_read = P<F_rw>("i2c_read", "i2c", ROLE_GUEST);
  F_rwc i2c_write = P<F_rwc>("i2c_write", "i2c", ROLE_GUEST);
  F_sp spi_transfer = P<F_sp>("spi_transfer", "spi", ROLE_GUEST);
  F_up uptime = P<F_up>("uptime", "time", ROLE_GUEST);
  F_dly delay_us = P<F_dly>("delay_us", "time", ROLE_GUEST);
  F_pr print = P<F_pr>("print", VOS_PRIM_GRUPPO_BASE, ROLE_GUEST);
  CHECK(gpio_mode && gpio_write && gpio_read && i2c_probe && i2c_read && i2c_write && spi_transfer && uptime && delay_us && print);
  if (g_fail) return done("prova_prim");
  uint8_t bb[8];
  const vos_prim_t* hr = vos_prim_trova("i2c_read", &g);
  CHECK(vos_prim_permessa(g, hr, ROLE_OPER, "i2c bmp280", 3) == VOS_PRIM_OK);            // il ruolo si controlla alla chiamata, non al caricamento
  CHECK(vos_prim_permessa(g, hr, ROLE_OPER, "gpio bmp280", 3) == VOS_PRIM_NO_USE);       // serve il gruppo i2c
  CHECK(vos_prim_permessa(g, hr, ROLE_OPER, "i2c", 2) == VOS_PRIM_NO_API);               // le funzioni nuove vogliono l'API 3
  // nessun modulo in esecuzione: nessun permesso
  CHECK(i2c_read("bmp280", 0xD0, bb, 1) == VOS_PRIM_E_USE && gpio_read("bmp280", 0) == VOS_PRIM_E_USE && g_hwCalls == 0);
  CHECK(vos_prim_esec_entra(ROLE_OPER, "i2c bmp280"));
  CHECK(i2c_read("bmp280", 0xD0, bb, 4) == 4 && bb[0] == 0x5a && !strcmp(g_hwDrv, "bmp280") && g_hwIdx == 0xD0);
  CHECK(i2c_probe("bmp280") == 1 && i2c_read("altro", 0, bb, 1) == VOS_PRIM_E_USE && i2c_read("bmp", 0, bb, 1) == VOS_PRIM_E_USE);
  int chiamate = g_hwCalls;
  CHECK(i2c_write("bmp280", 0xF4, bb, 1) == VOS_PRIM_E_VIETATO && gpio_write("bmp280", 0, 1) == VOS_PRIM_E_VIETATO);   // Operatore: solo letture
  CHECK(gpio_mode("bmp280", 0, 3) == VOS_PRIM_E_VIETATO && spi_transfer("bmp280", bb, bb, 2) == VOS_PRIM_E_VIETATO && g_hwCalls == chiamate);
  CHECK(gpio_read("bmp280", 1) == 1 && g_hwIdx == 1);
  CHECK(i2c_read(nullptr, 0, bb, 1) == VOS_PRIM_E_ARG && i2c_read("", 0, bb, 1) == VOS_PRIM_E_ARG);
  CHECK(i2c_read("bmp280aaaaaaaaaaaaaaaaaaaaaaaaaaaa", 0, bb, 1) == VOS_PRIM_E_ARG);      // nome oltre 24 caratteri: mai troncato
  vos_prim_esec_esce();
  CHECK(vos_prim_esec_entra(ROLE_ADMIN, "i2c spi gpio bmp280"));
  CHECK(i2c_write("bmp280", 0xF4, bb, 1) == 1 && gpio_mode("bmp280", 0, 3) == 0 && gpio_write("bmp280", 0, 1) == 0 && spi_transfer("bmp280", bb, bb, 2) == 2);
  CHECK(delay_us(50000) == 10000 && uptime() == 77);
  vos_prim_esec_esce();
  // piu moduli insieme: ognuno vede solo i suoi permessi (task diversi)
  vos_prim_task_set(chiFinto);
  g_tid = (void*)(intptr_t)11; int pa = vos_prim_esec_entra(ROLE_ADMIN, "i2c sens1");
  g_tid = (void*)(intptr_t)22; int pb = vos_prim_esec_entra(ROLE_OPER, "gpio sens2");
  CHECK(pa > 0 && pb > 0 && pa != pb);
  CHECK(vos_prim_esec_ha("sens2") && !vos_prim_esec_ha("sens1") && vos_prim_esec_ruolo() == ROLE_OPER);
  CHECK(gpio_read("sens2", 0) == 1 && gpio_read("sens1", 0) == VOS_PRIM_E_USE);
  g_tid = (void*)(intptr_t)11;
  CHECK(vos_prim_esec_ha("sens1") && !vos_prim_esec_ha("sens2") && vos_prim_esec_ruolo() == ROLE_ADMIN);
  CHECK(!vos_prim_esec_entra(ROLE_ADMIN, "log"));                                        // lo stesso task non entra due volte
  g_tid = (void*)(intptr_t)33; CHECK(!vos_prim_esec_ha("sens1") && vos_prim_esec_ruolo() == ROLE_GUEST);
  int tutti[VOS_PRIM_ESEC_MAX]; int dentro = 0;
  for (int i = 0; i < VOS_PRIM_ESEC_MAX + 2; i++) { g_tid = (void*)(intptr_t)(100 + i); int px = vos_prim_esec_entra(ROLE_OPER, "log"); if (px) tutti[dentro++] = px; }
  CHECK(dentro == VOS_PRIM_ESEC_MAX - 2);                                                // i posti finiti dicono no: mai oltre il tetto
  for (int i = 0; i < dentro; i++) vos_prim_esec_esce(tutti[i]);                         // si libera dal posto, anche da un altro task
  vos_prim_esec_esce(pa); vos_prim_esec_esce(pb);
  g_tid = (void*)(intptr_t)11; CHECK(!vos_prim_esec_ha("sens1") && vos_prim_esec_ruolo() == ROLE_GUEST);
  // uscita testuale: print scrive nel buffer del modulo (una riga per chiamata), mai oltre
  g_tid = (void*)(intptr_t)44; int po = vos_prim_esec_entra(ROLE_OPER, "log");
  char ob[24];
  vos_prim_esec_uscita(po, ob, sizeof ob);
  CHECK(print("st=ok") == 0 && print("v=21.5 C") == 0 && !strcmp(ob, "st=ok\nv=21.5 C\n") && vos_prim_esec_lunghezza(po) == strlen(ob));
  print("riga molto lunga che non ci sta nel buffer"); print("altra");
  CHECK(strlen(ob) < sizeof ob && vos_prim_esec_lunghezza(po) < sizeof ob);              // tagliato, chiuso con zero
  CHECK(print(nullptr) < 0);
  vos_prim_esec_esce(po);
  g_tid = (void*)(intptr_t)45; CHECK(vos_prim_esec_scrivi("x") == 0);                              // senza buffer: niente
  // gruppo files: la cartella dei dati dell'app (data_*). Il nome dell'app viene dal contesto del modulo, mai dagli argomenti
  typedef int (*F_dd)(char*, int);
  typedef int (*F_dw)(const char*, const void*, int, int);
  typedef int (*F_dr)(const char*, int, void*, int);
  typedef int (*F_dm)(const char*);
  F_dd data_dir = P<F_dd>("data_dir", "files", ROLE_OPER);
  F_dw data_write = P<F_dw>("data_write", "files", ROLE_OPER);
  F_dr data_read = P<F_dr>("data_read", "files", ROLE_OPER);
  F_dm data_rm = P<F_dm>("data_rm", "files", ROLE_OPER);
  CHECK(data_dir && data_write && data_read && data_rm);
  if (g_fail) return done("prova_prim");
  const vos_prim_t* dd = vos_prim_trova("data_write", &g);
  CHECK(vos_prim_permessa(g, dd, ROLE_OPER, "files", 4) == VOS_PRIM_OK && vos_prim_permessa(g, dd, ROLE_OPER, "files", 3) == VOS_PRIM_NO_API);   // API 4
  CHECK(vos_prim_permessa(g, dd, ROLE_OPER, "sd", 4) == VOS_PRIM_NO_USE && vos_prim_permessa(g, dd, ROLE_GUEST, "files", 4) == VOS_PRIM_NO_RUOLO);
  char dbuf[40];
  g_tid = (void*)(intptr_t)61;
  CHECK(data_dir(dbuf, sizeof dbuf) == VOS_PRIM_E_VIETATO && data_write("a", "x", 1, 0) == VOS_PRIM_E_VIETATO && g_dChiamate == 0);   // chi non e un'app (driver, shell) non ha cartella
  int pd = vos_prim_esec_entra(ROLE_OPER, "files", "cam");
  CHECK(pd > 0 && !strcmp(vos_prim_esec_nome(), "cam"));
  CHECK(data_dir(dbuf, sizeof dbuf) == 12 && !strcmp(dbuf, "/var/lib/cam") && g_dKind == PK_APP_E && g_dApp == "cam");
  CHECK(data_dir(dbuf, 5) == VOS_PRIM_E_GRANDE && dbuf[0] == 0 && data_dir(nullptr, 5) == VOS_PRIM_E_ARG && data_dir(dbuf, 0) == VOS_PRIM_E_ARG);   // mai una risposta troncata
  CHECK(data_write("a.txt", "ciao", 4, 1) == 4 && g_dFile == "a.txt" && g_dN == 4 && g_dAppend == 1 && g_dApp == "cam");
  CHECK(data_write("a.txt", nullptr, 3, 0) == VOS_PRIM_E_ARG && data_write("a.txt", "x", -1, 0) == VOS_PRIM_E_ARG && data_write(nullptr, "x", 1, 0) == VOS_PRIM_E_ARG);
  char nomeLungo[200]; memset(nomeLungo, 'a', sizeof nomeLungo - 1); nomeLungo[sizeof nomeLungo - 1] = 0;
  int prima = g_dChiamate;
  CHECK(data_write(nomeLungo, "x", 1, 0) == VOS_PRIM_E_ARG && data_rm(nomeLungo) == VOS_PRIM_E_ARG && g_dChiamate == prima);                         // nome oltre 40 caratteri: mai troncato
  CHECK(data_read("a.txt", 7, dbuf, 9) == 2 && g_dOff == 7 && g_dN == 9);
  CHECK(data_read("a.txt", -1, dbuf, 9) == VOS_PRIM_E_ARG && data_read("a.txt", 0, nullptr, 9) == VOS_PRIM_E_ARG && data_read("a.txt", 0, dbuf, 0) == VOS_PRIM_E_ARG);
  CHECK(data_rm("a.txt") == 0 && g_dFile == "a.txt");
  vos_prim_esec_esce(pd);
  CHECK(!strcmp(vos_prim_esec_nome(), ""));
  // copia dei testi
  char t[8];
  CHECK(vos_prim_copia_testo("abc", t, 8) && !strcmp(t, "abc"));
  CHECK(!vos_prim_copia_testo("abcdefgh", t, 8) && !vos_prim_copia_testo(nullptr, t, 8) && !vos_prim_copia_testo("a", t, 0));
  return done("prova_prim");
}
