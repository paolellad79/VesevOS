// VesevOS - prova sul computer delle ESTENSIONI dei programmi (.vsb/.vab/.vae e i vecchi .vsc/.vap/.vel): packages/vos_progext.cpp VERO con una
// memoria interna finta (un elenco di nomi). La lancia prova_progext.sh.
// Prova: lunghezza e taglio dell'estensione, percorso (nuovo se c'e, vecchio se c'e solo quello, nuovo per scrivere), il vecchio omonimo si toglie,
// nell'elenco il vecchio coperto dal nuovo non si conta.
#include <stdio.h>
#include <string.h>
#include <set>
#include <string>
#include "vos_progext.h"
#include "vos_errbuf.h"
#include "mini.h"

static std::set<std::string> g_files;

bool drvFsExists(const char* p) { return g_files.count(p) != 0; }
bool fsRemove(std::string_view p, ErrBuf&) { return g_files.erase(std::string(p)) != 0; }

int main() {
  // 1. estensione nuova e vecchia, di tutti e tre i tipi
  CHECK(progExtLen("a.vsb", PEXT_SCRIPT) == 4);
  CHECK(progExtLen("a.vsc", PEXT_SCRIPT) == 4);
  CHECK(progExtLen("a.vab", PEXT_APP_B) == 4 && progExtLen("a.vap", PEXT_APP_B) == 4);
  CHECK(progExtLen("a.vae", PEXT_APP_E) == 4 && progExtLen("a.vel", PEXT_APP_E) == 4);
  CHECK(progExtLen("a.txt", PEXT_SCRIPT) == 0 && progExtLen("a.vsb", PEXT_APP_B) == 0);   // un tipo non prende l'estensione di un altro
  CHECK(progHaExt("x.vsc", PEXT_SCRIPT) && !progHaExt("x.vsc", PEXT_APP_B) && !progHaExt(nullptr, PEXT_SCRIPT));
  // 2. taglio dell'estensione dal nome scritto dall'utente
  char n[40]; strcpy(n, "lampada.vap"); progTogliExt(n, strlen(n), PEXT_APP_B); CHECK(!strcmp(n, "lampada"));
  strcpy(n, "lampada.vab"); progTogliExt(n, strlen(n), PEXT_APP_B); CHECK(!strcmp(n, "lampada"));
  strcpy(n, "lampada"); progTogliExt(n, strlen(n), PEXT_APP_B); CHECK(!strcmp(n, "lampada"));
  // 3. percorso: nessun file -> il nome nuovo (quello da scrivere)
  char p[96];
  progPercorso(p, sizeof p, "/sbin", "lampeggio", PEXT_SCRIPT); CHECK(!strcmp(p, "/sbin/lampeggio.vsb"));
  // solo il vecchio -> il vecchio
  g_files.insert("/sbin/lampeggio.vsc");
  progPercorso(p, sizeof p, "/sbin", "lampeggio", PEXT_SCRIPT); CHECK(!strcmp(p, "/sbin/lampeggio.vsc"));
  // vecchio e nuovo -> il nuovo
  g_files.insert("/sbin/lampeggio.vsb");
  progPercorso(p, sizeof p, "/sbin", "lampeggio", PEXT_SCRIPT); CHECK(!strcmp(p, "/sbin/lampeggio.vsb"));
  // 4. elenco: il vecchio coperto dal nuovo non si conta; senza il nuovo si conta
  CHECK(progVecchioCoperto("/sbin", "lampeggio", PEXT_SCRIPT));
  CHECK(!progVecchioCoperto("/sbin", "altro", PEXT_SCRIPT));
  g_files.insert("/sbin/solovecchio.vsc");
  CHECK(!progVecchioCoperto("/sbin", "solovecchio", PEXT_SCRIPT));
  // 5. il vecchio omonimo si toglie (e il nuovo resta); se non c'e non fa nulla
  CHECK(progToglieVecchio("/sbin", "lampeggio", PEXT_SCRIPT));
  CHECK(g_files.count("/sbin/lampeggio.vsb") && !g_files.count("/sbin/lampeggio.vsc"));
  CHECK(!progToglieVecchio("/sbin", "lampeggio", PEXT_SCRIPT));
  // 6. app Berry e app ELF nella stessa cartella: i tipi non si mescolano
  g_files.insert("/bin/sensore.vel");
  progPercorso(p, sizeof p, "/bin", "sensore", PEXT_APP_E); CHECK(!strcmp(p, "/bin/sensore.vel"));
  progPercorso(p, sizeof p, "/bin", "sensore", PEXT_APP_B); CHECK(!strcmp(p, "/bin/sensore.vab"));
  return done("prova_progext");
}
