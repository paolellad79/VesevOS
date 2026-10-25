// VesevOS - prove sul computer: ganci verso gli strati alti (core/vos_hooks.h, riempiti da src/vos_wire.cpp)
// 1) vosWire() riempie TUTTI i ganci della struttura (confronto dei testi: nomi nella struttura = nomi assegnati);
// 2) un gancio vuoto e innocuo: i valori sicuri dei hookXxx() e nessun uso senza controllo di nullo fuori da vos_hooks.h.
#include <Arduino.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>
#include <set>
#include <string>
#include "vos_hooks.h"
#include "mini.h"

namespace fs = std::filesystem;
static std::string slurp(const fs::path& p) { std::ifstream f(p); std::stringstream s; s << f.rdbuf(); return s.str(); }
static std::string noComments(std::string s) { s = std::regex_replace(s, std::regex("/\\*[\\s\\S]*?\\*/"), ""); return std::regex_replace(s, std::regex("//[^\n]*"), ""); }

// uscita che butta via tutto (il gancio vuoto non deve scrivere nulla)
struct NullSink : OutSink { size_t write(uint8_t) override { return 1; } };

int main() {
  const char* repo = getenv("VOS_REPO"); CHECK(repo != nullptr); if (!repo) return done("hooks");
  fs::path src = fs::path(repo) / "VesevOS" / "src";
  // 1) elenco unico dei ganci: i campi (*nome) della struttura VosHooks
  std::string h = noComments(slurp(src / "core" / "vos_hooks.h"));
  std::string body = h.substr(h.find("struct VosHooks"), h.find("};", h.find("struct VosHooks")) - h.find("struct VosHooks"));
  std::set<std::string> fields, wired;
  std::regex rf("\\(\\*\\s*(\\w+)\\s*\\)"), rw("g_hooks\\.(\\w+)\\s*=");
  for (std::sregex_iterator it(body.begin(), body.end(), rf), e; it != e; ++it) fields.insert((*it)[1]);
  std::string w = noComments(slurp(src / "vos_wire.cpp"));
  for (std::sregex_iterator it(w.begin(), w.end(), rw), e; it != e; ++it) wired.insert((*it)[1]);
  CHECK(fields.size() >= 20);                                        // la struttura ha i ganci (oggi 23)
  for (auto& f : fields) { if (!wired.count(f)) printf("  gancio NON riempito da vosWire(): %s\n", f.c_str()); CHECK(wired.count(f)); }
  for (auto& f : wired) { if (!fields.count(f)) printf("  vosWire() riempie un gancio che non esiste: %s\n", f.c_str()); CHECK(fields.count(f)); }
  // 2a) nessun uso di un gancio senza controllo (fuori da vos_hooks.h e vos_wire.cpp)
  std::regex use("g_hooks\\.(\\w+)");
  for (auto& de : fs::recursive_directory_iterator(src)) {
    if (!de.is_regular_file()) continue; auto ext = de.path().extension();
    if (ext != ".cpp" && ext != ".h") continue; std::string fn = de.path().filename();
    if (fn == "vos_hooks.h" || fn == "vos_wire.cpp") continue;
    std::istringstream in(noComments(slurp(de.path()))); std::string line; int ln = 0;
    while (std::getline(in, line)) { ln++;
      for (std::sregex_iterator it(line.begin(), line.end(), use), e; it != e; ++it) {
        std::string nm = (*it)[1];
        bool ok = line.find("g_hooks." + nm + " &&") != std::string::npos || line.find("if (g_hooks." + nm) != std::string::npos ||
                  line.find("hookCall(g_hooks." + nm + ")") != std::string::npos || line.find("g_hooks." + nm + " ?") != std::string::npos;
        if (!ok) printf("  uso senza controllo: %s:%d g_hooks.%s\n", fn.c_str(), ln, nm.c_str());
        CHECK(ok);
      } }
  }
  // 2b) ganci tutti vuoti (la scheda senza quel pezzo): risposte sicure e nessun crash
  g_hooks = VosHooks{};
  hookCall(g_hooks.webInit); hookCall(g_hooks.webStart); hookCall(g_hooks.webTrimIdle); hookCall(g_hooks.rulesInit);
  hookCall(g_hooks.mqttInit); hookCall(g_hooks.meshInit); hookCall(g_hooks.radiosOff); hookCall(g_hooks.ledInit);
  hookCall(g_hooks.netInit); hookCall(g_hooks.ledShutdown); hookCall(g_hooks.trafficTick); hookCall(g_hooks.ioAllOff);
  CHECK(!hookWebHttpsUp());
  CHECK(!hookMfaOn(0) && !hookMfaOn(5));
  CHECK(!hookMqttPublish("a", "b"));
  CHECK(hookNetState() == NET_BOOT);
  hookLedAlarm(2); hookLedFault(true); hookLedFault(false);
  CHECK(!hookDevIsOn("x") && !hookDevOff("x") && !hookDevIsOn(nullptr));
  CHECK(hookBtnWpsClicks() == 0);
  { NullSink ns; hookShellExec("info", ns, 0); }
  { char b[8] = "x"; CHECK(hookCpuTick(50.0f, 10) == 0 && hookCpuStatus(b, sizeof b) == 0 && b[0] == 0); hookCpuApplyMode(); }   // CPU: senza gancio = stato normale, nessun testo
  CHECK(hookWebIdleSec() == 0xFFFFFFFFUL);                          // senza pagina: nessuno la usa da sempre
  return done("hooks");
}
