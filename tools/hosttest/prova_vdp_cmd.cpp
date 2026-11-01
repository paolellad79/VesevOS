// VesevOS - prova sul computer: comandi dei driver nei file .vdb ([commands] e [cmd]).
// Compila i file VERI devices/vos_mod.cpp, vos_mod_line.cpp e vos_mod_cmd.cpp; sono finti solo il file system (una cartella del computer)
// e poche funzioni di appoggio (util, io). Non serve il nucleo Arduino. Avvio: tools/hosttest/prova_vdp_cmd.sh
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string>
#include <fstream>
#include <sstream>
#include "vos_mod.h"
#include "vos_mod_cmd.h"
#include "vos_drvwidget.h"
#include "vos_drvcmd.h"
#include "vos_app.h"
#include "vos_files.h"
#include "vos_io.h"
#include "vos_util.h"
#include "vos_i18n.h"

static std::string g_root;      // cartella del computer che fa da "/"
static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("KO riga %d: %s\n", __LINE__, #c); fails++; } } while (0)

// ---- finti pezzi ----
const char* tr(const char* s) { return s; }
void vlog(const char*, ...) {}
bool utilNameOk(const char* s, size_t n) { if (n < 1 || n > 24) return false; for (size_t i = 0; i < n; i++) { char c = s[i]; if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return false; } return true; }
bool utilLineErr(ErrBuf& err, int line, const char* msg) { char b[200]; snprintf(b, sizeof b, "Riga %d: %s", line, msg); err = b; return false; }
bool utilSvgSafe(std::string_view svg, size_t) { return svg.substr(0, 4) == "<svg"; }
bool ioParseCls(const char* s, uint8_t& c) { const char* N[] = {"", "rele", "led", "pin", "ingresso", "analogico", "i2c", "sd"}; for (uint8_t i = 1; i <= 7; i++) if (!strcmp(s, N[i])) { c = i; return true; } return false; }
bool ioNameOk(const char*, ErrBuf&) { return true; }
int  ioSuggest(unsigned char, int, int*) { return 0; }
int  ioCount() { return 0; }
bool ioAdd(const char*, int, const char*, const char*, char*, ErrBuf&) { return false; }
bool ioAddI2c(const char*, int, int, int, bool, const char*, char*, char*) { return false; }
bool ioAddSd(int, int, int, int, int, const char*, char*, char*) { return false; }
void sdAutoStart() {}                                       // il task della microSD non serve a questa prova
bool ioRemove(const char*, ErrBuf&) { return true; }
size_t fsTextMax() { return 32768; }
bool fsReadText(std::string_view p, HeapBuf& o, ErrBuf& e) {
  std::ifstream in(g_root + std::string(p)); if (!in) { e = "File non trovato"; return false; }
  std::stringstream ss; ss << in.rdbuf(); std::string t = ss.str(); o.clear(); o.add(t.data(), t.size()); return true; }
bool fsWriteText(std::string_view p, std::string_view t, ErrBuf&, bool) { std::ofstream out(g_root + std::string(p)); out.write(t.data(), (std::streamsize)t.size()); return true; }
bool fsMkdir(std::string_view, ErrBuf&) { return true; }
bool fsRemove(std::string_view p, ErrBuf&) { return remove((g_root + std::string(p)).c_str()) == 0; }
bool fsListBuild(std::string_view path, HeapBuf& j) {
  DIR* d = opendir((g_root + std::string(path)).c_str()); j.add("{\"ok\":true,\"list\":["); bool first = true;
  if (d) { struct dirent* e; while ((e = readdir(d))) { std::string n = e->d_name; if (n[0] == '.') continue; if (!first) j.add(","); first = false; j.add("{\"n\":\""); j.add(n.c_str()); j.add("\",\"d\":false}"); } closedir(d); }
  j.add("]}"); return true; }


// ---- finti per vos_drvwidget.cpp: tempo, Berry occupato e uscita di on_widget() li comanda la prova ----
long long g_fake_us = 1000000;
static bool g_berryBusy = false, g_berryOk = true, g_lastAdmin = true; static int g_berryRuns = 0; static std::string g_berryOut, g_lastCode;
bool berryRunning() { return g_berryBusy; }
bool berryRunCommand(const char* code, const char*, bool admin, const CmdCall&, HeapBuf& out, ErrBuf& err, uint32_t) {
  g_berryRuns++; g_lastAdmin = admin; g_lastCode = code;
  if (!g_berryOk) { err = "errore"; return false; }
  out.add(g_berryOut.c_str()); return true; }
// ---- finti per vos_drvcmd.cpp (app del driver messa in pausa): l'app "microsd" gira, il comando la ferma e la riavvia ----
static char g_appName[32] = ""; static int g_stops = 0, g_starts = 0; static bool g_stopOk = true; static uint8_t g_startRole = 0; static std::string g_startWho;
bool berryCmdArgsOk(const CmdCall&, ErrBuf&) { return true; }
bool berryAppRunning(char* name, size_t n) { if (!g_appName[0]) return false; snprintf(name, n, "%s", g_appName); return true; }
bool berryStop(ErrBuf& err) { g_stops++; if (!g_stopOk) { err = "non si ferma"; return false; } g_appName[0] = 0; return true; }
bool appStart(const char* name, ErrBuf&, uint8_t role, const char* who) { g_starts++; g_startRole = role; g_startWho = who ? who : ""; snprintf(g_appName, sizeof g_appName, "%s", name); return true; }

static std::string readAll(const std::string& p) { std::ifstream in(p); std::stringstream ss; ss << in.rdbuf(); return ss.str(); }
static std::string replace(std::string s, const std::string& a, const std::string& b) { size_t i = s.find(a); if (i != std::string::npos) s.replace(i, a.size(), b); return s; }

int main() {
  const char* r = getenv("VOS_REPO"); std::string repo = r ? r : "../..";
  char tmpl[] = "/tmp/vdpcmdXXXXXX"; g_root = mkdtemp(tmpl); mkdir((g_root + "/devs").c_str(), 0755);
  std::string good = readAll(repo + "/docs/esempi/driver/microsd.vdb");
  CHECK(good.size() > 1000);
  ModInfo* m = new ModInfo(); ErrBuf err;

  // ---- il driver microSD di esempio passa il controllo vero ----
  bool ok = modParse(std::string_view(good), "microsd", *m, err);
  if (!ok) printf("  errore: %s\n", err.c_str());
  CHECK(ok && m->hasCmd && m->ncmd == 11 && !strcmp(m->cmdWord, "sd") && m->hasApp && strstr(m->use, "files") && strstr(m->use, "sd"));

  // ---- righe sbagliate ----
  auto bad = [&](const std::string& t) { return !modParse(std::string_view(t), "microsd", *m, err); };
  CHECK(bad(replace(good, "sd status oper", "sd status root")));                                    // ruolo sconosciuto
  CHECK(bad(replace(good, "sd status oper", "sd status")));                                          // manca il ruolo
  CHECK(bad(replace(good, "sd ls oper", "sd status oper")));                                         // sottocomando doppio
  CHECK(bad(replace(good, "sd ls oper", "mm ls oper")));                                             // due parole di comando diverse
  CHECK(bad(replace(good, "sd rm admin", "sd rm admin extra")));                                     // parola in piu
  CHECK(bad(replace(good, "\n[commands]\n", "\n")));                                                 // [cmd] senza [commands]
  CHECK(bad(replace(good, "\n[cmd]\n", "\n[altro]\n")));                                             // sezione sconosciuta fuori dal codice
  { std::string t = good; size_t i = t.find("\n[cmd]\n"); t = t.substr(0, i) + "\n[cmd]\n"; CHECK(bad(t)); }   // [cmd] vuota
  { std::string t = replace(good, "sd mkdir admin\n", "sd mkdir admin\nsd s1 oper\nsd s2 oper\nsd s3 oper\n"); CHECK(bad(t)); }   // piu di 12 righe
  { std::string t = good + "x = 1\n"; CHECK(modParse(std::string_view(t), "microsd", *m, err)); }       // righe in fondo a [cmd] sono codice
  { std::string t = good; size_t i = t.find("[cmd]\n") + 6; t.insert(i, std::string(4200, '#') + "\n"); CHECK(bad(t)); }   // codice dei comandi oltre 4 KB

  // ---- widget: [widget] e on_widget() ----
  CHECK(modParse(std::string_view(good), "microsd", *m, err) && m->widget.on && !strcmp(m->widget.title, "Scheda microSD") && !strcmp(m->widget.alt, "Scheda microSD non presente") && !strcmp(m->widget.who, "oper"));
  CHECK(bad(replace(good, "title=Scheda microSD\nalt", "titolo=Scheda microSD\nalt")));                 // chiave sconosciuta nel widget
  CHECK(bad(replace(good, "who=oper\n\n[commands]", "who=root\n\n[commands]")));                       // ruolo sconosciuto
  CHECK(bad(replace(good, "title=Scheda microSD\nalt", "title=<b>x</b>\nalt")));                         // niente < > nel titolo
  CHECK(bad(replace(good, "alt=Scheda microSD non presente", "alt=" + std::string(60, 'a'))));           // alt troppo lungo
  CHECK(bad(replace(good, "def on_widget()", "def altra()")));                                          // [widget] senza on_widget()
  { std::string t = replace(good, "title=Scheda microSD\n", ""); CHECK(modParse(std::string_view(t), "microsd", *m, err) && !strcmp(m->widget.title, "Lettore microSD (SPI)")); }   // senza title: il titolo del driver
  { std::string t = replace(good, "who=oper\n\n[commands]", "\n[commands]"); CHECK(modParse(std::string_view(t), "microsd", *m, err) && !strcmp(m->widget.who, "oper")); }   // who predefinito
  { std::string t = replace(good, "[widget]\ntitle=Scheda microSD\nalt=Scheda microSD non presente\nwho=oper\n\n", ""); CHECK(modParse(std::string_view(t), "microsd", *m, err) && !m->widget.on); }   // il widget e facoltativo
  { ModWidgetVals v; modWidgetParseOut("st=ok\nv=Montata\r\ns=FAT32, 7421 MB\nxx=1\n", v); CHECK(!strcmp(v.st, "ok") && !strcmp(v.v, "Montata") && !strcmp(v.s, "FAT32, 7421 MB")); }
  { ModWidgetVals v; modWidgetParseOut("st=forse\nv=\x01\x7f" "a\xc3\xa8" "b\n", v); CHECK(!strcmp(v.st, "off") && !strcmp(v.v, "ab")); }                // stato strano = off; caratteri non ASCII tolti
  { ModWidgetVals v; std::string lung = "v=" + std::string(200, 'x') + "\ns=" + std::string(200, 'y'); modWidgetParseOut(lung.c_str(), v); CHECK(strlen(v.v) == MODW_V_MAX && strlen(v.s) == MODW_S_MAX && !strcmp(v.st, "off")); }
  { ModWidgetVals v; modWidgetParseOut("", v); CHECK(!strcmp(v.st, "off") && !v.v[0] && !v.s[0]); modWidgetParseOut(nullptr, v); CHECK(!strcmp(v.st, "off")); }

  // ---- il registro legge il driver da /devs ----
  { ErrBuf e; CHECK(modSave("microsd", std::string_view(good), e)); }
  { ModCmdEntry* tab = new ModCmdEntry[24]; int n = modCmdList(tab, 24);
    CHECK(n == 11 && !strcmp(tab[0].driver, "microsd") && !strcmp(tab[0].c.cmd, "sd") && !strcmp(tab[0].c.sub, "status") && !tab[0].c.admin);
    bool wr = false, ls = false; for (int i = 0; i < n; i++) { if (!strcmp(tab[i].c.sub, "write")) wr = tab[i].c.admin && tab[i].c.rest; if (!strcmp(tab[i].c.sub, "ls")) ls = !tab[i].c.admin && !tab[i].c.rest; }
    CHECK(wr && ls); delete[] tab; }

  { ErrBuf e; CHECK(modSave("microsd", std::string_view(good), e)); }
  { ModWidgetEntry* w = new ModWidgetEntry[2]; int n = modWidgetList(w, 2);
    CHECK(n == 1 && !strcmp(w[0].driver, "microsd") && !strcmp(w[0].w.title, "Scheda microSD") && !strncmp(w[0].icon, "<svg", 4)); delete[] w; }

  // ---- il codice [cmd] e i permessi ----
  { HeapBuf code(8192); char use[100]; ErrBuf e;
    CHECK(modCmdCode("microsd", code, use, sizeof use, e));
    CHECK(strstr(use, "sd") && strstr(use, "files"));
    CHECK(strstr(code.c_str(), "def on_cmd(sub, args)") && strstr(code.c_str(), "def sdpath(a)") && strstr(code.c_str(), "def on_widget()"));
    CHECK(!strstr(code.c_str(), "dev_set") && !strstr(code.c_str(), "[data]") && !strstr(code.c_str(), "stato=-") && !strstr(code.c_str(), "[commands]"));   // niente pagina, niente dati
    const char* outp = getenv("VDP_CMD_OUT");                                                            // il codice estratto dal vero modCmdCode, per provarlo in Berry
    if (outp) { FILE* f = fopen(outp, "w"); fputs(code.c_str(), f); fclose(f); }
    CHECK(!modCmdCode("nonesiste", code, use, sizeof use, e)); }


  // ---- un comando mentre gira l'app del driver: pausa e riavvio; app di un altro driver o script: niente pausa ----
  { ErrBuf e; CHECK(modSave("microsd", std::string_view(good), e)); drvCmdRefresh();
    DrvCmd d; CHECK(drvCmdFind("sd", "status", d) && !strcmp(d.driver, "microsd"));
    CmdCall* call = new CmdCall(); memset(call, 0, sizeof *call); snprintf(call->sub, sizeof call->sub, "status");
    HeapBuf out(512); ErrBuf er; g_berryOut = "microSD: montata\n"; g_berryOk = true; int runs0 = g_berryRuns;
    snprintf(g_appName, sizeof g_appName, "microsd");                                                    // l'app della pagina del driver e aperta
    CHECK(drvCmdRun(d, *call, true, out, er) && g_stops == 1 && g_starts == 1 && g_startRole == 2 && g_startWho == "comando" && !strcmp(g_appName, "microsd") && g_berryRuns == runs0 + 1);   // fermata, comando, riavviata
    snprintf(g_appName, sizeof g_appName, "altra"); int s0 = g_stops, a0 = g_starts;
    CHECK(drvCmdRun(d, *call, true, out, er) && g_stops == s0 && g_starts == a0 && !strcmp(g_appName, "altra"));                 // un'app di altro nome non si tocca (poi il VERO avvio dira "occupato")
    g_appName[0] = 0; s0 = g_stops; a0 = g_starts;
    CHECK(drvCmdRun(d, *call, true, out, er) && g_stops == s0 && g_starts == a0);                       // niente app: nessuna pausa
    snprintf(g_appName, sizeof g_appName, "microsd"); g_stopOk = false; s0 = g_stops; runs0 = g_berryRuns;
    CHECK(!drvCmdRun(d, *call, true, out, er) && g_berryRuns == runs0 && !strcmp(er.c_str(), "non si ferma")); g_stopOk = true;   // se l'app non si ferma il comando non parte
    g_appName[0] = 0; delete call; }

  // ---- widget in cache: JSON, ruoli, Berry occupato, validita 10 secondi ----
  { HeapBuf j(DRVW_JSON_MAX); drvWidgetRefresh(); g_berryRuns = 0;
    g_berryOut = "st=ok\nv=Montata\ns=FAT32, 7421 MB liberi su 7421 MB, 1000 kHz\n";
    CHECK(drvWidgetsJson(j, 0) && !strcmp(j.c_str(), "[]") && g_berryRuns == 0);                          // gli ospiti non vedono niente e on_widget non parte
    j.clear(); CHECK(drvWidgetsJson(j, 1) && g_berryRuns == 1 && !g_lastAdmin && strstr(g_lastCode.c_str(), "on_widget()"));   // l'operatore si; senza diritti di scrittura
    std::string s = j.c_str();
    CHECK(s[0] == '[' && s.back() == ']' && s.find("\"d\":\"microsd\"") != std::string::npos && s.find("\"t\":\"Scheda microSD\"") != std::string::npos && s.find("\"st\":\"ok\"") != std::string::npos && s.find("\"v\":\"Montata\"") != std::string::npos && s.find("viewBox=\\\"0 0 24 24") != std::string::npos);   // le virgolette dell'icona sono protette
    CHECK(s.find("\"i\":\"<svg") != std::string::npos);                                               // icona del driver (testo dentro virgolette JSON)
    j.clear(); g_fake_us += 5000000; CHECK(drvWidgetsJson(j, 2) && g_berryRuns == 1);                   // dopo 5 s la cache vale ancora: on_widget non riparte
    g_berryOut = "st=off\nv=Non montata\ns=0 MB liberi su 0 MB, 1000 kHz\n";
    j.clear(); g_fake_us += 6000000; CHECK(drvWidgetsJson(j, 2) && g_berryRuns == 2 && strstr(j.c_str(), "Non montata") && strstr(j.c_str(), "\"st\":\"off\""));   // dopo 11 s si rilegge
    g_berryBusy = true; g_berryOut = "st=ok\nv=Cambiato\n";
    j.clear(); g_fake_us += 20000000; CHECK(drvWidgetsJson(j, 1) && g_berryRuns == 2 && strstr(j.c_str(), "Non montata"));      // Berry occupato: resta l'ultimo valore noto
    g_berryBusy = false; g_berryOk = false;
    j.clear(); g_fake_us += 20000000; CHECK(drvWidgetsJson(j, 1) && g_berryRuns == 3 && strstr(j.c_str(), "Non montata"));       // errore di on_widget: resta l'ultimo valore noto
    g_berryOk = true; drvWidgetRefresh();                                                                 // driver ricaricato: si rilegge da zero
    g_berryOut = "st=ok\nv=Nuovo\n"; j.clear(); CHECK(drvWidgetsJson(j, 1) && strstr(j.c_str(), "Nuovo"));
    // driver solo per l'Admin: l'operatore non lo vede
    { std::string t = replace(good, "who=oper\n\n[commands]", "who=admin\n\n[commands]"); ErrBuf e; CHECK(modSave("microsd", std::string_view(t), e)); drvWidgetRefresh();
      g_fake_us += 20000000; j.clear(); CHECK(drvWidgetsJson(j, 1) && !strcmp(j.c_str(), "[]")); j.clear(); CHECK(drvWidgetsJson(j, 2) && strstr(j.c_str(), "microsd")); }
    // driver senza widget (o tolto): nessun widget
    { ErrBuf e; modDelete("microsd", e); drvWidgetRefresh(); j.clear(); CHECK(drvWidgetsJson(j, 2) && !strcmp(j.c_str(), "[]")); }
  }
  // ---- estensione .vdb e compatibilita con i vecchi .vdp (1.8.11) ----
  {
    auto here = [&](const char* f) { return access((g_root + "/devs/" + f).c_str(), F_OK) == 0; };
    auto put = [&](const char* f, const std::string& t) { std::ofstream o(g_root + "/devs/" + f); o << t; };
    auto drv = [&](const char* n) { return replace(good, "name=microsd\n", std::string("name=") + n + "\n"); };   // il name= deve essere quello del file
    char (*nm)[MOD_NAME_BUF] = new char[8][MOD_NAME_BUF]; ErrBuf e;
    CHECK(modSave("nuovo", std::string_view(drv("nuovo")), e) && here("nuovo.vdb") && !here("nuovo.vdp"));          // si scrive .vdb
    put("vecchio.vdp", drv("vecchio"));                                                                                // un driver gia installato con la vecchia estensione
    CHECK(modScan(nm, 8) == 2);                                                                              // si vede, e nuovo + vecchio
    { char pa[64]; modFilePath(pa, sizeof pa, "vecchio"); CHECK(!strcmp(pa, "/devs/vecchio.vdp")); modFilePath(pa, sizeof pa, "nuovo"); CHECK(!strcmp(pa, "/devs/nuovo.vdb")); modFilePath(pa, sizeof pa, "altro"); CHECK(!strcmp(pa, "/devs/altro.vdb")); }
    { HeapBuf code(8192); char use[100]; CHECK(modCmdCode("vecchio", code, use, sizeof use, e) && strstr(code.c_str(), "def on_cmd")); }   // il vecchio .vdp si legge
    put("nuovo.vdp", drv("nuovo"));                                                                                  // stesso nome con le due estensioni: una sola volta
    CHECK(modScan(nm, 8) == 2);
    { char pa[64]; modFilePath(pa, sizeof pa, "nuovo"); CHECK(!strcmp(pa, "/devs/nuovo.vdb")); }              // vale .vdb
    CHECK(modSave("vecchio", std::string_view(drv("vecchio")), e) && here("vecchio.vdb") && !here("vecchio.vdp"));      // salvando, il vecchio .vdp sparisce
    put("nuovo.vdp", drv("nuovo"));
    CHECK(modDelete("nuovo", e) && !here("nuovo.vdb") && !here("nuovo.vdp"));                                  // eliminando, spariscono tutti e due
    CHECK(!modDelete("nuovo", e));
    put("solo.vdp", drv("solo")); CHECK(modDelete("solo", e) && !here("solo.vdp"));
    delete[] nm;
  }
  if (fails) printf("FALLITE %d\n", fails); else printf("driver comandi: tutto OK\n");
  delete m;
  std::string rm = "rm -rf " + g_root; if (system(rm.c_str())) {}
  return fails != 0;
}
