// Prova sul computer: la microSD si monta da sola appena la scheda risponde (sdAutoStep di vos_sd.cpp, VERO) con la scheda finta di fake_sd.
// Si controlla: scheda presente = montata; assente = niente blocchi e un solo messaggio nel registro; inserita dopo = montata; tolta = non rilevata e
// poi rimontata; espulsa a mano = ferma fino a sdMount; formato non supportato = nessun tentativo; operazione in corso = salta il giro.
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "vos_sd.h"
#include "fake_sd.h"

static int g_fail = 0, g_n = 0, g_logs = 0;
#define CHECK(c, what) do { g_n++; if (!(c)) { g_fail++; printf("KO %d: %s\n", g_n, what); } else printf("ok %d: %s\n", g_n, what); } while (0)

bool ioSdPins(int* p) { p[0] = 10; p[1] = 11; p[2] = 12; p[3] = 13; p[4] = 1000; return true; }
void vlog(const char* f, ...) { (void)f; g_logs++; }
const char* tr(const char* s) { return s; }
static bool g_fmt = false;                                       // la vera sdFormatting() sta in vos_sd_fmt.cpp (task): qui un interruttore
bool sdFormatting() { return g_fmt; }
const char* sdBusyText() { return g_fmt ? "microSD occupata (formattazione)" : "Operazione gia in corso"; }

int main() {
  char err[SD_ERR_MAX];
  fakeSdReset();

  CHECK(!sdUp(), "all'inizio non montata");
  sdAutoStep();
  CHECK(sdUp() && g_sd.begins == 1, "scheda presente: il passo la monta");

  g_sd.gone = true;                                   // tolta mentre e montata
  sdAutoStep();
  SdVolume v;
  sdVolume(v);
  CHECK(!sdUp() && v.state == SD_ST_NO_CARD, "tolta: passa a 'scheda non presente' (non rilevata)");

  g_sd.gone = false;
  fakeSdScenario(FSC_NO_CARD);
  int b = g_sd.begins;
  int logs0 = g_logs;
  sdAutoStep(); sdAutoStep(); sdAutoStep();
  CHECK(g_sd.begins == b + 3 && !sdUp(), "assente: ritenta a ogni giro senza montare");
  CHECK(g_logs - logs0 == 1, "assente: un solo messaggio nel registro per tutti i tentativi");

  fakeSdScenario(FSC_CARD_OK);
  g_sd.card = true;
  sdAutoStep();
  CHECK(sdUp(), "reinserita: si rimonta da sola");

  CHECK(sdUnmount(err, sizeof err) && !sdUp(), "espulsione a mano");
  b = g_sd.begins;
  sdAutoStep(); sdAutoStep();
  CHECK(g_sd.begins == b && !sdUp(), "espulsa a mano: il sondaggio non la rimonta");
  CHECK(sdMount(err, sizeof err) && sdUp(), "mount a mano: torna montata");
  CHECK(sdUnmount(err, sizeof err), "espulsione a mano di nuovo");
  // durante la formattazione: niente montaggio da solo, niente monta/smonta a mano, e il motivo e quello giusto
  fakeSdReset();
  g_sd.up = false;
  g_fmt = true;
  b = g_sd.begins;
  sdAutoStep(); sdAutoStep();
  CHECK(g_sd.begins == b && !sdUp(), "durante la formattazione il sondaggio non monta");
  CHECK(!sdMount(err, sizeof err) && !strcmp(err, "microSD occupata (formattazione)") && g_sd.begins == b, "durante la formattazione 'monta' risponde occupata");
  CHECK(!sdUnmount(err, sizeof err) && !strcmp(err, "microSD occupata (formattazione)"), "durante la formattazione 'smonta' risponde occupata");
  g_fmt = false;
  sdAutoStep();
  CHECK(sdUp(), "finita la formattazione il sondaggio rimonta");

  // il blocco NON si salva: al riavvio (stato statico nuovo) si rimonta -> si simula con sdMount che lo toglie; il flag e solo in memoria
  CHECK(sdMount(err, sizeof err), "dopo mount il sondaggio e di nuovo attivo");

  CHECK(sdUnmount(err, sizeof err), "smonta");
  CHECK(sdMount(err, sizeof err), "rimonta");

  fakeSdReset();
  g_sd.up = false;
  fakeSdScenario(FSC_EXFAT);
  sdAutoStep();                                       // primo tentativo: scopre il formato
  b = g_sd.begins;
  sdAutoStep(); sdAutoStep();
  sdVolume(v);
  CHECK(v.state == SD_ST_BAD_FORMAT && g_sd.begins == b, "formato non supportato: stato dedicato, nessun altro tentativo");
  CHECK(!sdMount(err, sizeof err) && g_sd.begins == b + 1, "...ma 'monta' a mano ritenta");

  printf("prova_sdauto: %d prove, %d fallite\n", g_n, g_fail);
  return g_fail ? 1 : 0;
}
