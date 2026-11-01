// VesevOS - prova sul computer dei DATI DELLE APP (dove salvano: flash o microSD): packages/vos_prog_data.cpp VERO e vos_drv_file.cpp vero, con una
// cartella finta al posto della memoria interna e della microSD. La lancia prova_progdata.sh.
// Prova: scelta (di fabbrica flash, SD solo con la scheda utilizzabile, massimo 8, ritorno alla flash), cartelle (/var/lib/<nome>, /media/sd/app/<nome>),
// scrivere, aggiungere, leggere a pezzi, cancellare, nomi non ammessi, tetti, scheda sparita, prima di avviare, JSON.
#include <stdio.h>
#include <string.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include "vos_prog_data.h"
#include "vos_config.h"
#include "vos_drv_fs.h"
#include "vos_files.h"
#include "mini.h"

VosConfig cfg;
static std::string g_root;
static bool g_sdUp = true;
static bool g_formatting = false;
static int g_saved = 0, g_ops = 0;
static size_t g_free = 1u << 30;

// ---- finti
const char* tr(const char* it) { return it; }
void cfgSetOrigin(const char*) {}
bool cfgSave() { g_saved++; return true; }
bool sdUp() { return g_sdUp; }
bool sdFormatting() { return g_formatting; }
void sdOpEnter() { g_ops++; }
void sdOpLeave() { g_ops--; }
size_t fsFreeAt(const char*) { return g_free; }
static std::string real(const char* p) { return g_root + p; }
bool drvFsExists(const char* p) { struct stat st; return stat(real(p).c_str(), &st) == 0; }
bool drvFsMkdir(const char* p) { struct stat st; std::string r = real(p); if (stat(r.c_str(), &st) == 0) return S_ISDIR(st.st_mode); return mkdir(r.c_str(), 0777) == 0; }
bool drvFsRemove(const char* p) { return unlink(real(p).c_str()) == 0; }
VosFile drvFsOpen(const char* p, const char* mode) { return VosFile::openPath(real(p).c_str(), p, mode ? mode : "r"); }

static std::string leggi(const char* app, const char* file) {
  uint8_t b[PD_IO_MAX + 1];
  int n = progDataRead(PK_APP_B, app, file, 0, b, PD_IO_MAX);
  return n < 0 ? std::string("<err") + std::to_string(n) + ">" : std::string((char*)b, (size_t)n);
}

int main() {
  char tmpl[] = "/tmp/vos_progdataXXXXXX";
  g_root = mkdtemp(tmpl);
  mkdir((g_root + "/var").c_str(), 0777);
  mkdir((g_root + "/media").c_str(), 0777);
  mkdir((g_root + "/media/sd").c_str(), 0777);
  ErrBuf e;
  const uint8_t* A = (const uint8_t*)"abcdef";

  // 1. scelta: di fabbrica flash; microSD solo se utilizzabile; massimo 8; ritorno alla flash
  CHECK(progWhere(PK_APP_B, "cam") == PW_FLASH && progWhere(PK_APP_E, "cam") == PW_FLASH && progWhere(PK_SCRIPT, "cam") == PW_FLASH);
  g_sdUp = false;
  CHECK(!progWhereStore(PK_APP_B, "cam", PW_SD, e) && progWhere(PK_APP_B, "cam") == PW_FLASH);        // senza scheda: resta la flash
  g_sdUp = true;
  CHECK(progWhereStore(PK_APP_B, "cam", PW_SD, e) && progWhere(PK_APP_B, "cam") == PW_SD && g_saved == 1);
  CHECK(progWhere(PK_APP_E, "cam") == PW_FLASH);                                                      // la stessa parola per un'app ELF e un'altra cosa
  CHECK(progWhereStore(PK_APP_B, "cam", PW_SD, e) && g_saved == 1);                                   // gia cosi: niente da salvare
  CHECK(!progWhereStore(PK_APP_B, "Cam!", PW_SD, e) && !progWhereStore(PK_SCRIPT, "cam", PW_SD, e) && !progWhereStore(PK_APP_B, "cam", 7, e));
  for (int i = 1; i < VOS_BOOTPROG_MAX; i++) { char n[8]; snprintf(n, sizeof n, "a%d", i); CHECK(progWhereStore(PK_APP_E, n, PW_SD, e)); }
  CHECK(cfg.dataSdN == VOS_BOOTPROG_MAX && !progWhereStore(PK_APP_E, "troppe", PW_SD, e));           // al nono no
  CHECK(progWhereStore(PK_APP_E, "a3", PW_FLASH, e) && cfg.dataSdN == VOS_BOOTPROG_MAX - 1 && progWhere(PK_APP_E, "a3") == PW_FLASH && progWhere(PK_APP_E, "a4") == PW_SD);
  CHECK(progWhereStore(PK_APP_B, "cam", PW_FLASH, e));
  CHECK(progWhere(PK_APP_B, "cam") == PW_FLASH);

  // 2. cartelle e file sulla flash
  char dir[FS_PATH_MAX + 1];
  CHECK(progDataDir(PK_APP_B, "cam", dir, sizeof dir, e) && !strcmp(dir, "/var/lib/cam") && drvFsExists("/var/lib/cam"));
  CHECK(progDataWrite(PK_APP_B, "cam", "a.txt", A, 6, false) == 6);
  CHECK(leggi("cam", "a.txt") == "abcdef");
  CHECK(progDataWrite(PK_APP_B, "cam", "a.txt", (const uint8_t*)"XY", 2, true) == 2 && leggi("cam", "a.txt") == "abcdefXY");
  CHECK(progDataWrite(PK_APP_B, "cam", "a.txt", (const uint8_t*)"Q", 1, false) == 1 && leggi("cam", "a.txt") == "Q");   // senza append azzera
  uint8_t b[16];
  progDataWrite(PK_APP_B, "cam", "b.bin", A, 6, false);
  CHECK(progDataRead(PK_APP_B, "cam", "b.bin", 2, b, 3) == 3 && !memcmp(b, "cde", 3));                 // a pezzi
  CHECK(progDataRead(PK_APP_B, "cam", "b.bin", 6, b, 3) == 0 && progDataRead(PK_APP_B, "cam", "b.bin", 99, b, 3) == 0);   // oltre la fine
  CHECK(progDataRead(PK_APP_B, "cam", "non-c-e", 0, b, 3) == PD_E_NONTROVATO);
  CHECK(progDataRemove(PK_APP_B, "cam", "b.bin") == 0 && progDataRemove(PK_APP_B, "cam", "b.bin") == PD_E_NONTROVATO && !drvFsExists("/var/lib/cam/b.bin"));
  CHECK(progDataWrite(PK_APP_B, "cam", "vuoto", nullptr, 0, false) == 0 && drvFsExists("/var/lib/cam/vuoto"));

  // 3. nomi non ammessi e tetti: l'app non esce dalla sua cartella
  const char* brutti[] = { "", ".nascosto", "a/b", "../x", "a..b", "x.", "a b", "a:b", "a*b", "/etc/x", "uno\\due" };
  for (const char* f : brutti) CHECK(progDataWrite(PK_APP_B, "cam", f, A, 1, false) == PD_E_PERCORSO);
  CHECK(progDataWrite(PK_APP_B, "cam", std::string(PD_FILE_MAX + 1, 'a').c_str(), A, 1, false) == PD_E_PERCORSO);
  CHECK(progDataWrite(PK_APP_B, "cam", std::string(PD_FILE_MAX, 'a').c_str(), A, 1, false) == 1);
  uint8_t grande[PD_IO_MAX + 1] = {0};
  CHECK(progDataWrite(PK_APP_B, "cam", "g", grande, PD_IO_MAX + 1, false) == PD_E_GRANDE && progDataWrite(PK_APP_B, "cam", "g", grande, PD_IO_MAX, false) == PD_IO_MAX);
  CHECK(progDataRead(PK_APP_B, "cam", "g", 0, b, PD_IO_MAX + 1) == PD_E_GRANDE && progDataRead(PK_APP_B, "cam", "g", 0, nullptr, 3) == PD_E_ARG);
  CHECK(progDataWrite(PK_APP_B, "CAM", "a", A, 1, false) == PD_E_ARG && progDataWrite(PK_SCRIPT, "cam", "a", A, 1, false) == PD_E_ARG);
  CHECK(progDataWrite(PK_APP_B, "cam", "a", nullptr, 3, false) == PD_E_ARG);
  g_free = 100;
  CHECK(progDataWrite(PK_APP_B, "cam", "p", A, 6, false) == PD_E_PIENO);                              // meno del margine di 8 KB: spazio esaurito
  g_free = 1u << 30;

  // 4. sulla microSD
  CHECK(progWhereStore(PK_APP_B, "tele", PW_SD, e));
  CHECK(progDataDir(PK_APP_B, "tele", dir, sizeof dir, e) && !strcmp(dir, "/media/sd/app/tele") && drvFsExists("/media/sd/app/tele"));
  CHECK(progDataWrite(PK_APP_B, "tele", "video.log", A, 6, false) == 6 && g_ops == 0 && drvFsExists("/media/sd/app/tele/video.log"));
  CHECK(!drvFsExists("/var/lib/tele") && leggi("tele", "video.log") == "abcdef");
  CHECK(progDataReady(PK_APP_B, "tele", e) && progDataReady(PK_APP_B, "cam", e) && progDataReady(PK_SCRIPT, "tele", e));
  g_sdUp = false;                                                                                     // scheda sparita
  CHECK(progDataWrite(PK_APP_B, "tele", "x", A, 1, false) == PD_E_NOSD && progDataRead(PK_APP_B, "tele", "video.log", 0, b, 3) == PD_E_NOSD && progDataRemove(PK_APP_B, "tele", "video.log") == PD_E_NOSD);
  CHECK(!progDataReady(PK_APP_B, "tele", e) && e.length() > 0 && !progDataDir(PK_APP_B, "tele", dir, sizeof dir, e));
  CHECK(progDataReady(PK_APP_B, "cam", e) && progDataWrite(PK_APP_B, "cam", "ok", A, 1, false) == 1);   // chi e sulla flash non si accorge di niente
  g_sdUp = true; g_formatting = true;                                                                 // in formattazione: come se non ci fosse
  CHECK(progDataWrite(PK_APP_B, "tele", "x", A, 1, false) == PD_E_NOSD && !progDataReady(PK_APP_B, "tele", e));
  g_formatting = false;
  CHECK(progDataRead(PK_APP_B, "tele", "video.log", 0, b, 3) == 3 && g_ops == 0);                     // ripresa: i dati ci sono ancora, nessuna operazione rimasta in corso

  // 5. JSON per la pagina
  HeapBuf j(2048);
  CHECK(progDataJson(j) && strstr(j.c_str(), "\"sd\":true") && strstr(j.c_str(), "{\"k\":\"b\",\"n\":\"tele\"}") && strstr(j.c_str(), "\"k\":\"e\",\"n\":\"a1\""));
  HeapBuf j2(2048);
  g_sdUp = false;
  CHECK(progDataJson(j2) && strstr(j2.c_str(), "\"sd\":false") && strstr(j2.c_str(), "tele"));      // la scelta resta anche se la scheda non c'e

  std::string rm = "rm -rf " + g_root;
  if (system(rm.c_str())) {}
  return done("prova_progdata");
}
