// Prova sul computer di tabella dei moduli ELF (vos_elf_tab.cpp) e quarantena (vos_elf_quar.cpp), VERI, con un deposito in memoria.
#include <stdio.h>
#include <string.h>
#include <map>
#include <string>
#include "vos_elf_tab.h"
#include "vos_elf_quar.h"

static int g_fail = 0, g_n = 0;
#define CHECK(c, what) do { g_n++; if (!(c)) { g_fail++; printf("KO %d: %s\n", g_n, what); } } while (0)

struct MemStore : VosElfQuarStore {
  std::map<std::string, std::string> m;
  bool get(const char* k, char* out, size_t cap) override { out[0] = 0; auto i = m.find(k); if (i == m.end()) return false; snprintf(out, cap, "%s", i->second.c_str()); return true; }
  void put(const char* k, const char* v) override { if (v[0]) m[k] = v; else m.erase(k); }
};

static void guasto(const char* nome, bool crash) { vos_elf_quar_avvio(nome); vos_elf_quar_al_boot(crash); }

int main() {
  CHECK(vos_elf_nome_valido("sensore-1_a") && !vos_elf_nome_valido("") && !vos_elf_nome_valido("Maiuscola") && !vos_elf_nome_valido("a b") &&
        !vos_elf_nome_valido("../x") && !vos_elf_nome_valido("abcdefghijklmnopqrstuvwxy") && vos_elf_nome_valido("abcdefghijklmnopqrstuvwx"), "nomi validi e non validi");

  // tabella
  int e = 0;
  vos_elf_proc_t* p[VOS_ELF_RUN_MAX + 1];
  const char* nomi[] = { "a", "b", "c", "d", "e" };
  for (int i = 0; i < VOS_ELF_RUN_MAX; i++) { p[i] = vos_elf_tab_prendi(nomi[i], &e); CHECK(p[i] && e == 0, "posto preso"); }
  CHECK(!vos_elf_tab_prendi("e", &e) && e == VOS_ELF_E_PIENO, "oltre il tetto: PIENO");
  CHECK(vos_elf_tab_attivi() == VOS_ELF_RUN_MAX, "attivi = tetto");
  vos_elf_tab_rilascia(p[1]);
  CHECK(!vos_elf_tab_prendi("a", &e) && e == VOS_ELF_E_GIA, "stesso nome: GIA");
  vos_elf_proc_t* n = vos_elf_tab_prendi("e", &e);
  CHECK(n && n->id > p[3]->id, "posto liberato riusato con un numero nuovo");
  char idtxt[16]; snprintf(idtxt, sizeof idtxt, "%d", p[2]->id);
  CHECK(vos_elf_tab_trova(idtxt) == p[2] && vos_elf_tab_trova("c") == p[2] && !vos_elf_tab_trova("zz") && !vos_elf_tab_trova("99999"), "ricerca per numero e per nome");
  for (int i = 0; i < VOS_ELF_RUN_MAX; i++) vos_elf_tab_rilascia(vos_elf_tab_posto(i));
  CHECK(vos_elf_tab_attivi() == 0, "tutto rilasciato");

  // quarantena
  MemStore st;
  vos_elf_quar_usa(&st);
  guasto("m1", false);
  CHECK(!vos_elf_quar_dentro("m1") && st.m.count("run") == 0, "riavvio voluto: nessun punto, elenco 'in corso' pulito");
  guasto("m1", true); guasto("m1", true);
  CHECK(!vos_elf_quar_dentro("m1") && st.m["pt"] == "m1=2", "due guasti: 2 punti, non ancora in quarantena");
  guasto("m1", true);
  CHECK(vos_elf_quar_dentro("m1") && st.m.count("pt") == 0, "terzo guasto: quarantena");
  char q[8][VOS_ELF_NOME_MAX + 1];
  CHECK(vos_elf_quar_elenco(q, 8) == 1 && !strcmp(q[0], "m1"), "elenco quarantena");
  CHECK(vos_elf_quar_togli("m1") && !vos_elf_quar_dentro("m1") && !vos_elf_quar_togli("m1"), "tolto dalla quarantena (una volta sola)");
  guasto("m2", true); guasto("m2", true);
  vos_elf_quar_avvio("m2"); vos_elf_quar_fine("m2");
  guasto("m2", true);
  CHECK(!vos_elf_quar_dentro("m2") && st.m["pt"] == "m2=1", "fine pulita azzera i punti: serve ricominciare da capo");
  vos_elf_quar_usa(&st);
  vos_elf_quar_avvio("x1"); vos_elf_quar_avvio("x2"); vos_elf_quar_avvio("x1");
  CHECK(st.m["run"] == "x1,x2", "piu moduli in corso, senza doppioni");
  vos_elf_quar_al_boot(true);
  CHECK(st.m["pt"].find("x1=1") != std::string::npos && st.m["pt"].find("x2=1") != std::string::npos, "un guasto da' un punto a tutti quelli che giravano");
  for (int i = 0; i < 12; i++) { char nm[16]; snprintf(nm, sizeof nm, "w%d", i); vos_elf_quar_avvio(nm); }
  CHECK(st.m["run"].size() < 208, "elenco 'in corso' non supera il tetto");
  printf("prova_elfrun: %d prove, %d fallite\n", g_n, g_fail);
  return g_fail ? 1 : 0;
}
