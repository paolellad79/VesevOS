// VesevOS - prove sul computer: regola dell'antiloop del watchdog (core/vos_wd_rule.h)
#include "vos_wd_rule.h"
#include "mini.h"

int main() {
  // non ricordato
  CHECK(!wdTooMany(5000, 0));
  // dentro l'ora / fuori dall'ora
  CHECK(wdTooMany(1000, 500));
  CHECK(wdTooMany(4099, 500));            // 3599 s: dentro
  CHECK(!wdTooMany(4100, 500));           // 3600 s: fuori
  CHECK(!wdTooMany(100000, 500));         // istante vecchio
  // caso del difetto: il contaore e tornato indietro, l'istante ricordato e nel futuro (senza segno dava 'no')
  CHECK(wdTooMany(1000, 1600));           // 600 s nel futuro
  CHECK(wdTooMany(100, 300));
  uint32_t d = 100u - 300u; CHECK(d > 3600u);   // com'era prima: differenza senza segno enorme
  // riavvii ogni ~60 s con contaore salvato (dopo la correzione): al quarto controllo dice "troppi"
  uint32_t t = 7200, w1 = 0, w2 = 0, w3 = 0; int stop = 0;
  for (int i = 0; i < 6 && !stop; i++) {
    if (wdTooMany(t, w3)) { stop = i + 1; break; }
    w3 = w2; w2 = w1; w1 = t; t += 60;
  }
  CHECK(stop == 4);
  // giro a 32 bit del contaore
  CHECK(wdTooMany(10, 0xFFFFFF00u));       // 266 s dopo, a cavallo del giro
  CHECK(!wdTooMany(5000, 0xFFFFFF00u));    // 5096 s dopo: fuori
  CHECK(wdTooMany(0xFFFFFF00u, 10));       // 266 s "nel futuro" a cavallo del giro: dentro la finestra, recente
  CHECK(!wdTooMany(0xFFFFF000u, 10));      // 4106 s "nel futuro": oltre la finestra, ignorato
  // finestra di plausibilita sul futuro (WD_FUTURE_MAX_S = 900)
  CHECK(wdTooMany(1000, 1000));            // 0 s: adesso
  CHECK(wdTooMany(1000, 1899));            // +899 s: recente
  CHECK(wdTooMany(1000, 1900));            // +900 s: ancora dentro
  CHECK(!wdTooMany(1000, 1901));           // +901 s: ignorato
  CHECK(!wdTooMany(1000, 0x7FFFFFFFu));    // enorme
  CHECK(wdTooMany(1000, 0xFFFFFFFFu));     // a 32 bit 0xFFFFFFFF e "1 s prima del giro": 1001 s fa, dentro l'ora (non distinguibile da dato valido)
  CHECK(!wdTooMany(1000, 0x80000000u));    // meta' del giro: oltre la finestra
  CHECK(wdFutureSec(1000, 1900) == 900 && wdFutureSec(1000, 900) == 0);
  CHECK(!wdFutureBad(1000, 1900) && wdFutureBad(1000, 1901) && !wdFutureBad(1000, 0));
  CHECK(wdTooMany(0xFFFFFF00u, 0xFFFFFF10u));   // +16 s a fine giro: recente
  CHECK(wdTooMany(0xFFFFFFF0u, 0x20u));    // c dopo il giro: 0x20-0xFFFFFFF0 = +48 s nel futuro: recente
  return done("test_wd");
}
