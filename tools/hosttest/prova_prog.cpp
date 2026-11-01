// VesevOS - prova sul computer di FUNZIONALITA e PERMESSI DEI MODULI ELF (packages/vos_func.cpp, vos_elf_use.cpp, vos_prim.cpp). Senza scheda.
// Prova: i gruppi che un'app ELF chiede (letti dai simboli che importa), il blocco delle funzionalita tolte nel registro delle primitive,
// l'interruttore (bit in configurazione), i ganci verso chi le usa e il JSON. La lancia prova_prog.sh.
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include "vos_prim.h"
#include "vos_func.h"
#include "vos_config.h"
#include "vos_elf_file.h"
#include "mini.h"

VosConfig cfg;
const char* tr(const char* it) { return it; }
void vlog(const char*, ...) {}
void cfgSetOrigin(const char*) {}
bool cfgSave() { return true; }

void vos_prim_log_registra() {}
void vos_prim_sd_registra() {}
void vos_prim_dev_registra() {}
void vos_prim_hw_registra() {}
void vos_prim_files_registra() {}      // i gruppi veri non servono qui: si registrano a mano quelli di prova
static int fnOk(void) { return 0; }

// Un ELF32 minimo: intestazione, tre sezioni (nulla, .symtab, .strtab); i simboli indefiniti sono quelli importati.
static std::vector<uint8_t> costruisciElf(const std::vector<std::string>& importati, bool conDefinito) {
  std::string str(1, '\0');
  std::vector<uint8_t> sym(16, 0);                                   // simbolo nullo
  auto aggiungi = [&](const std::string& nome, uint16_t shndx) {
    uint32_t off = (uint32_t)str.size();
    str += nome; str.push_back('\0');
    uint8_t s[16] = {0};
    memcpy(s, &off, 4);
    memcpy(s + 14, &shndx, 2);
    sym.insert(sym.end(), s, s + 16);
  };
  if (conDefinito) aggiungi("main", 1);
  for (auto& n : importati) aggiungi(n, 0);
  size_t symOff = 52, strOff = symOff + sym.size(), shOff = (strOff + str.size() + 3) & ~(size_t)3;
  std::vector<uint8_t> f(shOff + 3 * 40, 0);
  memcpy(f.data(), "\x7f" "ELF", 4);
  f[4] = 1; f[5] = 1; f[6] = 1;
  uint16_t ehsize = 52, shentsize = 40, shnum = 3;
  uint32_t shoff = (uint32_t)shOff;
  memcpy(&f[32], &shoff, 4); memcpy(&f[40], &ehsize, 2); memcpy(&f[46], &shentsize, 2); memcpy(&f[48], &shnum, 2);
  memcpy(&f[symOff], sym.data(), sym.size());
  memcpy(&f[strOff], str.data(), str.size());
  auto sezione = [&](int i, uint32_t tipo, uint32_t off, uint32_t size, uint32_t link) {
    uint8_t* h = &f[shOff + i * 40];
    memcpy(h + 4, &tipo, 4); memcpy(h + 16, &off, 4); memcpy(h + 20, &size, 4); memcpy(h + 24, &link, 4);
  };
  sezione(1, 2, (uint32_t)symOff, (uint32_t)sym.size(), 2);
  sezione(2, 3, (uint32_t)strOff, (uint32_t)str.size(), 0);
  return f;
}

static int g_users = 0, g_stopped = 0;
static std::string g_stopId;
static int hookUsers(const char* id) { return !strcmp(id, "sd") ? g_users : 0; }
static void hookStop(const char* id) { g_stopped++; g_stopId = id; }
static bool concessa(const char* g) { return funcOn(g); }

int main() {
  // registro delle primitive di prova: base, log, sd
  static const vos_prim_t base[] = { { "millis", (const void*)fnOk, "x", ROLE_GUEST, 1 } };
  static const vos_prim_t log[] = { { "log", (const void*)fnOk, "x", ROLE_GUEST, 1 } };
  static const vos_prim_t sd[] = { { "sd_status", (const void*)fnOk, "x", ROLE_OPER, 2 }, { "sd_write", (const void*)fnOk, "x", ROLE_ADMIN, 2 } };
  CHECK(vos_prim_registra("base", base, 1) == 0);
  CHECK(vos_prim_registra("log", log, 1) == 0);
  CHECK(vos_prim_registra("sd", sd, 2) == 0);

  // 1. i gruppi chiesti da un modulo: dai simboli indefiniti, nell'ordine, senza ripetizioni, senza il gruppo base, senza simboli ignoti
  char use[64];
  auto elf = costruisciElf({ "vos_sd_status", "vos_log", "vos_millis", "vos_sd_write", "vos_non_esiste", "strlen" }, true);
  CHECK(vos_elf_gruppi_richiesti(elf.data(), elf.size(), use, sizeof use) == VOS_ELF_OK);
  CHECK(!strcmp(use, "sd log"));
  auto soloBase = costruisciElf({ "vos_millis" }, true);
  CHECK(vos_elf_gruppi_richiesti(soloBase.data(), soloBase.size(), use, sizeof use) == VOS_ELF_OK && use[0] == 0);
  auto definito = costruisciElf({}, true);                          // "main" definito dal modulo stesso: non e una primitiva
  CHECK(vos_elf_gruppi_richiesti(definito.data(), definito.size(), use, sizeof use) == VOS_ELF_OK && use[0] == 0);
  char piccolo[4];                                                   // buffer troppo piccolo: si taglia, mai fuori misura
  CHECK(vos_elf_gruppi_richiesti(elf.data(), elf.size(), piccolo, sizeof piccolo) == VOS_ELF_OK && strlen(piccolo) < sizeof piccolo);
  CHECK(vos_elf_gruppi_richiesti(elf.data(), 20, use, sizeof use) != VOS_ELF_OK);                // troncato
  uint8_t rotto[80]; memset(rotto, 0, sizeof rotto);
  CHECK(vos_elf_gruppi_richiesti(rotto, sizeof rotto, use, sizeof use) == VOS_ELF_E_MAGIC);      // non e un ELF
  auto corrotto = elf; uint32_t shoffGrande = 0x7fffffff; memcpy(&corrotto[32], &shoffGrande, 4); // tabella oltre il file
  CHECK(vos_elf_gruppi_richiesti(corrotto.data(), corrotto.size(), use, sizeof use) == VOS_ELF_E_CORTO);
  auto strCorrotta = elf; strCorrotta[strCorrotta.size() - 3 * 40 - 1] = 'x';                    // stringhe non chiuse da zero: la tabella si salta
  CHECK(vos_elf_gruppi_richiesti(strCorrotta.data(), strCorrotta.size(), use, sizeof use) == VOS_ELF_OK);

  // 2. il registro: permessi, e funzionalita tolta
  const char* g = nullptr;
  const vos_prim_t* p = vos_prim_trova_elf("vos_sd_status", &g);
  CHECK(p && !strcmp(g, "sd"));
  CHECK(vos_prim_permessa("sd", p, ROLE_OPER, "sd", 3) == VOS_PRIM_OK);
  CHECK(vos_prim_permessa("sd", p, ROLE_OPER, "log", 3) == VOS_PRIM_NO_USE);
  vos_prim_funz_set(concessa);
  CHECK(vos_prim_permessa("sd", p, ROLE_OPER, "sd", 3) == VOS_PRIM_OK);
  ErrBuf e;
  CHECK(funcSet(funcIndex("sd"), false, e));
  CHECK(!funcOn("sd") && funcOn("log"));
  CHECK(vos_prim_permessa("sd", p, ROLE_OPER, "sd", 3) == VOS_PRIM_NO_FUNZ);
  CHECK(vos_prim_permessa("sd", p, ROLE_OPER, "log", 3) == VOS_PRIM_NO_USE);   // il permesso mancante vince sulla funzionalita tolta
  CHECK(vos_prim_permessa("log", vos_prim_trova("log", nullptr), ROLE_GUEST, "log", 3) == VOS_PRIM_OK);

  // 3. Funzionalita: bit in configurazione, id sconosciuti, concessa di nuovo
  CHECK(cfg.funcOff == (1u << funcIndex("sd")));
  CHECK(funcOn("nome-che-non-c-e"));                                  // un id sconosciuto e concesso (non e una funzionalita)
  CHECK(!funcSet(-1, false, e) && !funcSet(kFuncN, true, e));
  CHECK(funcMissing("log sd") && !strcmp(funcMissing("log sd"), "sd"));
  CHECK(funcMissing("log,sd") && !strcmp(funcMissing("log,sd"), "sd"));   // anche separati da virgola (use= delle app Berry)
  CHECK(funcMissing("log") == nullptr && funcMissing("") == nullptr && funcMissing(nullptr) == nullptr);
  CHECK(funcMissing("sdx") == nullptr);                               // nome intero, non un pezzo
  CHECK(funcSet(funcIndex("sd"), true, e) && funcOn("sd") && cfg.funcOff == 0);
  CHECK(vos_prim_permessa("sd", p, ROLE_OPER, "sd", 3) == VOS_PRIM_OK);

  // 4. ganci: chi la usa e chi la ferma (solo quando la si toglie)
  g_users = 2;
  funcHooksSet(hookUsers, hookStop);
  CHECK(funcUsers(funcIndex("sd")) == 2 && funcUsers(funcIndex("log")) == 0);
  CHECK(funcSet(funcIndex("sd"), false, e) && g_stopped == 1 && g_stopId == "sd");
  CHECK(funcSet(funcIndex("sd"), false, e) && g_stopped == 1);         // era gia tolta: non ferma di nuovo
  CHECK(funcSet(funcIndex("sd"), true, e) && g_stopped == 1);          // concederla non ferma nessuno
  // tutte e nove, in ordine, e ognuna col suo bit
  for (int i = 0; i < kFuncN; i++) { CHECK(funcSet(i, false, e)); CHECK((cfg.funcOff & (1u << i)) != 0); }
  CHECK(cfg.funcOff == (1u << kFuncN) - 1);
  for (int i = 0; i < kFuncN; i++) CHECK(funcSet(i, true, e));
  CHECK(cfg.funcOff == 0);

  // 5. JSON
  CHECK(funcSet(funcIndex("gpio"), false, e));
  HeapBuf j(FUNC_JSON_MAX);
  CHECK(funcJson(j));
  std::string js(j.c_str(), j.length());
  CHECK(js.find("\"id\":\"sd\"") != std::string::npos && js.find("\"id\":\"ble\"") != std::string::npos);
  CHECK(js.find("\"id\":\"gpio\"") != std::string::npos);
  size_t gp = js.find("\"id\":\"gpio\""), fine = js.find('}', gp);
  CHECK(gp != std::string::npos && js.substr(gp, fine - gp).find("\"on\":false") != std::string::npos);
  size_t lg = js.find("\"id\":\"log\""); fine = js.find('}', lg);
  CHECK(lg != std::string::npos && js.substr(lg, fine - lg).find("\"on\":true") != std::string::npos);
  CHECK(js.front() == '{' && js.back() == '}');
  return done("prova_prog");
}
