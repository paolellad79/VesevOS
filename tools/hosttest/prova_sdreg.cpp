// VesevOS - prova sul computer: la REGISTRAZIONE VERA delle funzioni sd_* (packages/vos_berry_sd.cpp) con il codice [cmd] vero del driver microSD.
// Un sottocomando di sola lettura (status) lanciato senza permesso di scrittura deve COMPILARE; sd_write da non-Admin deve dare "permesso negato".
// Il codice [cmd] arriva da prova_vdp_cmd.sh (file cmd.be, estratto dal vero modCmdCode). Uso: prova_sdreg.sh
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <string>
#include "berry.h"
#include "vos_berry_int.h"
#include "vos_sd.h"
#include "vos_paths.h"
#include "vos_pathres.h"
#include "vos_log.h"
#include "vos_i18n.h"

namespace vosberry { volatile bool g_stop = false; }

// ---- finti: microSD "montata" in memoria, percorsi semplici ----
static std::string g_last;                        // ultima scrittura (percorso)
bool sdUp() { return true; }
bool sdMount(char*, size_t) { return true; }
bool sdUnmount(char*, size_t) { return true; }
bool sdInUse() { return false; }
static int g_formats = 0;
bool sdFormat(char*, size_t) { g_formats++; return true; }
bool sdFormatStart(char*, size_t) { g_formats++; return true; }      // la vera avvia un task: qui basta contare
bool sdFormatting() { return false; }
void sdFormatInfo(SdFmtInfo& f) { memset(&f, 0, sizeof f); }
bool sdPathOk(const char*, char*, size_t) { return true; }
bool sdVolume(SdVolume& v) { memset(&v, 0, sizeof v); v.state = SD_ST_MOUNTED; strcpy(v.fs, "FAT32"); v.totalKb = 7600000; v.usedKb = 10; v.khz = 1000; return true; }
int  sdReadAt(const char*, uint32_t, uint8_t*, size_t, char*, size_t) { return -1; }
bool sdWriteBytes(const char* p, const uint8_t*, size_t, bool, char*, size_t) { g_last = p; return true; }
bool sdRemove(const char*, char*, size_t) { return true; }
bool sdMakeDir(const char*, char*, size_t) { return true; }
bool pathResolve(const char*, const char* arg, char* out, size_t cap) { snprintf(out, cap, "%s", arg); return true; }
const char* pathSdRel(const char* clean) { return strncmp(clean, "/media/sd", 9) == 0 ? clean + 9 : (strncmp(clean, "/sd", 3) == 0 ? clean + 3 : nullptr); }
bool pathIsSystemFile(const char*) { return false; }
bool pathIsSecret(const char*) { return false; }
void vlog(const char*, ...) {}
const char* tr(const char* it) { return it; }
extern "C" {
void* vos_berry_malloc(size_t n) { return malloc(n); }
void vos_berry_free(void* p) { free(p); }
void* vos_berry_realloc(void* p, size_t n) { return realloc(p, n); }
void vos_berry_write(const char* b, size_t l) { fwrite(b, 1, l, stdout); }
}

// Aiuti di vos_berry_files.cpp (stesso codice): mappa grezza -> oggetto "map".
void berryPutInt(bvm* vm, const char* k, long v) { be_pushstring(vm, k); be_pushint(vm, (bint)v); be_data_insert(vm, -3); be_pop(vm, 2); }
void berryPutBool(bvm* vm, const char* k, bool v) { be_pushstring(vm, k); be_pushbool(vm, v); be_data_insert(vm, -3); be_pop(vm, 2); }
void berryPutText(bvm* vm, const char* k, const char* v) { be_pushstring(vm, k); be_pushstring(vm, v); be_data_insert(vm, -3); be_pop(vm, 2); }
void berryWrap(bvm* vm, const char* cls) { be_getbuiltin(vm, cls); be_pushvalue(vm, -2); be_call(vm, 1); be_moveto(vm, -2, -3); be_pop(vm, 2); }

static std::string readAll(const char* path) {
  std::string s; FILE* f = fopen(path, "rb"); if (!f) return s;
  char b[4096]; size_t n; while ((n = fread(b, 1, sizeof b, f)) > 0) s.append(b, n);
  fclose(f); return s;
}

// Funzionalita tolta dall'utente (vos_func.cpp): qui un interruttore finto, la prova vera della logica e prova_prog.
static bool g_funcOff = false;
void berryNeedFunc(bvm* vm, const char* g) {
  if (!g_funcOff) return;
  char m[64]; snprintf(m, sizeof m, "funzionalita %s spenta (Sistema > Funzionalita)", g);
  be_raise(vm, "permission_error", m);
}

// Esegue codice Berry con le funzioni sd_* registrate come fa il firmware; admin = chi lancia puo scrivere. Torna 0 = bene.
static int run(const std::string& code, bool admin, const char* what) {
  bvm* vm = be_vm_new();
  berrySdRegister(vm, true, admin);
  int r = be_loadstring(vm, code.c_str());
  if (!r) r = be_pcall(vm, 0);
  if (r) { printf("[%s] ", what); be_dumpexcept(vm); }
  return r;
}

int main(int argc, char** argv) {
  if (argc < 2) return 2;
  std::string cmd = readAll(argv[1]);                       // codice [cmd] vero del driver
  if (cmd.empty()) { printf("KO: manca %s\n", argv[1]); return 2; }
  // listdir non e di vos_berry_sd.cpp: lo mette un finto in Berry davanti al codice.
  std::string pre = "def listdir(p, i) return {'items': [], 'more': false, 'next': 0} end\n";
  int bad = 0;
  // 1) sottocomando di sola lettura senza permesso di scrittura: il codice deve compilare e girare
  if (run(pre + cmd + "\non_cmd('status', [])\n", false, "status senza Admin")) { printf("KO 1: status da non-Admin\n"); bad++; } else printf("ok 1: status da non-Admin compila e gira\n");
  // 2) sd_write da non-Admin: permesso negato, e nulla scritto
  g_last.clear();
  std::string t = "import string\ntry\n sd_write('/sd/x.txt', 'a')\n raise 'x', 'scritto'\nexcept 'permission_error' as e, m\n if string.find(m, 'permesso negato') < 0 raise 'x', 'messaggio: ' + m end\nend\n";
  if (run(t, false, "sd_write senza Admin") || !g_last.empty()) { printf("KO 2: sd_write da non-Admin\n"); bad++; } else printf("ok 2: sd_write da non-Admin = permesso negato, nulla scritto\n");
  // 3) stesse cose per mount, eject, rm, mkdir
  const char* fn[] = {"sd_mount()", "sd_eject()", "sd_rm('/sd/x')", "sd_mkdir('/sd/d')", "sd_format('FORMATTA')"};
  for (const char* f : fn) {
    std::string u = std::string("try\n ") + f + "\n raise 'x', 'eseguito'\nexcept 'permission_error' as e, m\nend\n";
    if (run(u, false, f)) { printf("KO 3: %s da non-Admin\n", f); bad++; } else printf("ok 3: %s da non-Admin = permesso negato\n", f);
  }
  // 4) da Admin tutto funziona come prima
  g_last.clear();
  if (run(pre + cmd + "\nsd_write('/sd/x.txt', 'a')\non_cmd('status', [])\n", true, "Admin") || g_last != "/x.txt") { printf("KO 4: Admin\n"); bad++; } else printf("ok 4: Admin scrive e legge lo stato\n");
  // 4b) sd_format: da non-Admin permesso negato e nulla formattato; da Admin serve la parola di conferma
  {
    int prima = g_formats;
    if (g_formats != prima) { printf("KO 4b: formattata da non-Admin\n"); bad++; }
    if (run("try\n sd_format('FORMATTA')\n raise 'x', 'eseguito'\nexcept 'permission_error' as e, m\nend\n", false, "sd_format non-Admin") || g_formats != prima) { printf("KO 4b: sd_format da non-Admin\n"); bad++; } else printf("ok 4b: sd_format da non-Admin = permesso negato, nulla formattato\n");
    if (run("try\n sd_format('si')\n raise 'x', 'eseguito'\nexcept 'value_error' as e, m\nend\n", true, "sd_format senza parola") || g_formats != prima) { printf("KO 4c: sd_format senza la parola\n"); bad++; } else printf("ok 4c: sd_format senza la parola di conferma = errore, nulla formattato\n");
    if (run("if sd_format('FORMATTA') != true raise 'x', 'no' end\n", true, "sd_format Admin") || g_formats != prima + 1) { printf("KO 4d: sd_format da Admin\n"); bad++; } else printf("ok 4d: sd_format da Admin con la parola = formattazione avviata\n");
  }
  // 5) sd_status e sd_read restano per tutti
  if (run("var s = sd_status()\nif s['state'] != 1 || s['fs'] != 'FAT32' raise 'x', 'stato' end\n", false, "sd_status")) { printf("KO 5: sd_status\n"); bad++; } else printf("ok 5: sd_status da non-Admin\n");
  // 6) Funzionalita sd tolta: il file che nomina le funzioni compila lo stesso, e ogni chiamata risponde "funzionalita spenta"
  g_funcOff = true;
  if (run(pre + cmd + "\non_cmd('status', [])\n", true, "sd spenta")) { printf("KO 6: sd spenta\n"); bad++; } else printf("ok 6: con la funzionalita sd tolta il file del driver compila e il comando risponde con un errore leggibile (non si ferma)\n");
  if (run("import string\ntry\n sd_read('/sd/x.txt')\n raise 'x', 'eseguito'\nexcept 'permission_error' as e, m\n if string.find(m, 'spenta') < 0 raise 'x', 'messaggio' end\nend\n", false, "sd_read spenta")) { printf("KO 6b\n"); bad++; } else printf("ok 6b: anche sd_read (lettura) risponde 'spenta'\n");
  g_funcOff = false;
  return bad ? 1 : 0;
}
