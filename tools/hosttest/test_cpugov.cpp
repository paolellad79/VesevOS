// VesevOS - prove sul computer: governor della CPU (core/vos_cpugov.*). Carichi finti, decisioni e isteresi.
#include "vos_cpugov.h"
#include "mini.h"
#include <string.h>

// Un campione: carico occupato (%) misurato mentre il tetto in vigore era cur.
static uint32_t step(CpuGov g, CpuGovState& st, uint32_t& now, uint32_t busy, uint32_t cur, bool boost = false, const CpuGovParams& p = CPUGOV_DEFAULTS) {
  CpuGovInput in = {busy, cur, boost, now++};
  return cpuGovDecide(g, p, st, in);
}

int main() {
  // parametri
  CHECK(cpuGovParamsValid(80, 20, 10));
  CHECK(!cpuGovParamsValid(49, 20, 10));
  CHECK(!cpuGovParamsValid(96, 20, 10));
  CHECK(!cpuGovParamsValid(80, 4, 10));
  CHECK(!cpuGovParamsValid(80, 51, 10));
  CHECK(!cpuGovParamsValid(80, 70, 10));     // discesa troppo vicina alla salita
  CHECK(cpuGovParamsValid(80, 50, 10));
  CHECK(cpuGovParamsValid(60, 49, 10));
  CHECK(!cpuGovParamsValid(60, 50, 10));
  CHECK(!cpuGovParamsValid(80, 20, 1));
  CHECK(!cpuGovParamsValid(80, 20, 121));
  // nomi
  CpuGov g = GOV_SCHEDUTIL;
  CHECK(cpuGovParse("ondemand", g) && g == GOV_ONDEMAND);
  CHECK(cpuGovParse("conservative", g) && g == GOV_CONSERVATIVE);
  CHECK(cpuGovParse("schedutil", g) && g == GOV_SCHEDUTIL);
  CHECK(!cpuGovParse("performance", g) && !cpuGovParse("", g) && !cpuGovParse(nullptr, g));
  CHECK(!strcmp(cpuGovName(GOV_ONDEMAND), "ondemand"));
  // carico riportato a 240 MHz
  CHECK(cpuGovEquiv240(100, 80) == 33);
  CHECK(cpuGovEquiv240(100, 240) == 100);
  CHECK(cpuGovEquiv240(50, 160) == 33);
  CHECK(cpuGovEquiv240(150, 240) == 100);

  { // ondemand: prima decisione in quiete = 80, poi carico alto = 240 subito, poi discesa solo dopo 10 s
    CpuGovState st; memset(&st, 0, sizeof st); uint32_t now = 100;
    CHECK(step(GOV_ONDEMAND, st, now, 5, 240) == 80);
    CHECK(step(GOV_ONDEMAND, st, now, 100, 80) == 80);     // un solo secondo dopo il cambio: aspetta il minimo fra due cambi
    CHECK(step(GOV_ONDEMAND, st, now, 100, 80) == 240);    // sopra la soglia: salta al massimo
    CHECK(st.why == GW_UP);
    uint32_t cap = 240; int downAt = -1;
    for (int i = 0; i < 20; i++) { cap = step(GOV_ONDEMAND, st, now, 10, 240); if (cap < 240 && downAt < 0) downAt = i; }
    CHECK(downAt >= 9 && downAt <= 11);                     // scende dopo ~10 s (hold)
    CHECK(cap == 80 && st.why == GW_DOWN);
  }
  { // ondemand: carico medio a 240 (60% = 144 MHz equivalenti) -> 240 per soglia (60 < 80) ma serve = 60*240/70 = 205 -> resta 240
    CpuGovState st; memset(&st, 0, sizeof st); uint32_t now = 0;
    step(GOV_ONDEMAND, st, now, 100, 240);                  // parte da 240
    for (int i = 0; i < 30; i++) step(GOV_ONDEMAND, st, now, 60, 240);
    CHECK(st.cap == 240);
    // carico 33% a 240: serve = 33*240/70 = 113 -> 160
    for (int i = 0; i < 30; i++) step(GOV_ONDEMAND, st, now, 33 * 240 / st.cap, st.cap);   // stesso lavoro: a tetto piu basso la CPU e piu occupata
    CHECK(st.cap == 160);
  }
  { // isteresi: carico che oscilla non fa scendere (il livello alto torna a ogni picco entro l'attesa)
    CpuGovState st; memset(&st, 0, sizeof st); uint32_t now = 0;
    step(GOV_ONDEMAND, st, now, 100, 240);
    for (int i = 0; i < 60; i++) step(GOV_ONDEMAND, st, now, (i % 5 == 0) ? 95 : 5, 240);   // picco ogni 5 s < hold 10 s
    CHECK(st.cap == 240);
  }
  { // al massimo un cambio ogni 2 secondi, anche con carico a dente di sega
    CpuGovState st; memset(&st, 0, sizeof st); uint32_t now = 0; uint32_t prev = 0, lastChangeAt = 0; int changes = 0; bool bad = false;
    CpuGov all[3] = {GOV_ONDEMAND, GOV_CONSERVATIVE, GOV_SCHEDUTIL};
    for (CpuGov gv : all) {
      memset(&st, 0, sizeof st); now = 0; prev = 0; changes = 0;
      CpuGovParams fast = {80, 20, 2};
      for (int i = 0; i < 400; i++) {
        uint32_t busy = ((i / 3) % 2) ? 100 : 0;
        uint32_t cap = step(gv, st, now, busy, prev ? prev : 240, false, fast);
        if (prev && cap != prev) { changes++; if (now - 1 - lastChangeAt < CPUGOV_MIN_GAP_SEC) bad = true; lastChangeAt = now - 1; }
        prev = cap;
      }
      CHECK(!bad && changes > 0);
    }
  }
  { // conservative: un gradino alla volta in salita e in discesa
    CpuGovState st; memset(&st, 0, sizeof st); uint32_t now = 0;
    CHECK(step(GOV_CONSERVATIVE, st, now, 50, 240) == 160);   // parte a meta
    now += 5;
    CHECK(step(GOV_CONSERVATIVE, st, now, 100, 160) == 240);  // un gradino su
    now += 5;
    CHECK(step(GOV_CONSERVATIVE, st, now, 100, 240) == 240);  // gia al massimo
    int steps = 0; uint32_t prev = 240;
    for (int i = 0; i < 40; i++) { uint32_t c = step(GOV_CONSERVATIVE, st, now, 5, prev); if (c != prev) { steps++; CHECK(c == (prev == 240 ? 160u : 80u)); } prev = c; }
    CHECK(prev == 80 && steps == 2);
    // carico fra le due soglie: nessun cambio
    now += 5; uint32_t before = st.cap;
    for (int i = 0; i < 30; i++) step(GOV_CONSERVATIVE, st, now, 50, st.cap);
    CHECK(st.cap == before);
  }
  { // schedutil: segue il lavoro con un quarto di margine
    CpuGovState st; memset(&st, 0, sizeof st); uint32_t now = 0;
    CHECK(step(GOV_SCHEDUTIL, st, now, 5, 240) == 80);
    now += 5;
    CHECK(step(GOV_SCHEDUTIL, st, now, 100, 80) == 160);      // 100% a 80 MHz = 33% di 240 -> 99 MHz -> 160
    now += 5;
    CHECK(step(GOV_SCHEDUTIL, st, now, 100, 160) == 240);     // 100% a 160 = 67% -> 200 MHz -> 240
    for (int i = 0; i < 20; i++) step(GOV_SCHEDUTIL, st, now, 50, st.cap);   // 50% a 240 = 50% -> 150 -> 160
    CHECK(st.cap == 160);
    for (int i = 0; i < 20; i++) step(GOV_SCHEDUTIL, st, now, 50, st.cap);   // 50% a 160 = 33% -> 100 -> 160: stabile
    CHECK(st.cap == 160);
  }
  { // prenotazione: massimo subito in tutti i governor, poi scende con l'attesa
    CpuGov all[3] = {GOV_ONDEMAND, GOV_CONSERVATIVE, GOV_SCHEDUTIL};
    for (CpuGov gv : all) {
      CpuGovState st; memset(&st, 0, sizeof st); uint32_t now = 0;
      step(gv, st, now, 0, 240);
      for (int i = 0; i < 20; i++) step(gv, st, now, 0, st.cap);
      CHECK(st.cap == 80);
      CHECK(step(gv, st, now, 0, 80, true) == 240 && st.why == GW_BOOST);
      CHECK(step(gv, st, now, 0, 240, true) == 240);
      uint32_t c = 240; int n = 0;
      while (c == 240 && n < 40) { c = step(gv, st, now, 0, 240); n++; }
      CHECK(n >= 9 && c < 240 && st.why != GW_BOOST);
    }
  }
  return done("test_cpugov");
}
