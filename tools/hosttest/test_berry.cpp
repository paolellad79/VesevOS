// VesevOS - prove sul computer: package Berry (vos_berry.cpp con l'interprete VERO di third_party/berry)
// Il file del package e incluso qui, cosi si provano anche le parti "static" (task, memoria, limiti).
#include <Arduino.h>
#include "mini.h"
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <atomic>
#include <chrono>
#define BERRY_BURST_MS 300UL      // nei test: 300 ms invece di 3 s

// ---- finti pezzi della scheda ----
static long g_realLive = 0;          // blocchi veri ancora vivi (per vedere le perdite)
void* heap_caps_malloc(size_t n, uint32_t) { g_realLive++; return malloc(n); }
void  heap_caps_free(void* p) { if (p) g_realLive--; free(p); }
size_t heap_caps_get_largest_free_block(uint32_t) { return 100000; }
void vTaskDelete(TaskHandle_t) {}
#include <unistd.h>
#define vTaskDelay(t) usleep((t) * 1000)      // il vero ritardo (il finto del progetto non aspetta)
bool timeValid() { return true; }
float sysCpuTemp() { return 41.5f; }
static String g_lastAct;
static int32_t g_ledScript = -1; static int g_ledCalls = 0;
static int32_t g_ledLast = -1;
void ledScript(int32_t rgb) { g_ledScript = rgb < 0 ? -1 : rgb; if (rgb >= 0) g_ledLast = rgb; g_ledCalls++; }
bool rulesAction(const String& a, String& err, const char*) {
  g_lastAct = a;
  if (a.startsWith("bad") || a == "led-color zzzzzz") { err = "Azione non ammessa"; return false; }
  return true;
}
// finto file system: script in /scripts (.vsc), un vecchio script di 1.7.48 in /apps (.be), file e cartelle estranei
#include <map>
#include <string>
#include <string_view>
#include "../../VesevOS/src/core/vos_heapbuf.h"
static bool g_dirOk = true;                       // esiste /scripts?
static std::map<std::string, std::string> g_fs;
static int g_listCalls = 0;
static void fsInit() {
  g_fs.clear(); g_dirOk = true;
  g_fs["/scripts/demo.vsc"] = "print('hola') var i = 0 while i < 3 sleep(30) i += 1 end print('fine')";
  g_fs["/scripts/loop.vsc"] = "while true sleep(20) end";
  g_fs["/scripts/busy.vsc"] = "while true end";
  g_fs["/scripts/spam.vsc"] = "for i: 0 .. 400 print('0123456789abcdef') sleep(0) end print('FINE')";
  g_fs["/scripts/big.vsc"] = std::string(9000, 'a');
  g_fs["/scripts/x.txt"] = "abc";
  g_fs["/scripts/old.be"] = "print(1)";                  // dentro /scripts un .be NON conta
  g_fs["/scripts/sub/k.vsc"] = "print(2)";               // cartella dentro /scripts: non conta
  g_fs["/apps/vecchio.be"] = "print('vecchio')";          // eredita 1.7.48: si sposta
  g_fs["/apps/Bad Name.be"] = "print(3)";                 // nome non valido: resta dove e
  g_fs["/apps/lampada.vap"] = "[manifest]";               // futura app: non si tocca
}
bool fsReadText(const String& p, String& out, String& err) {
  auto it = g_fs.find(p.c_str());
  if (it == g_fs.end()) { err = "File non trovato"; return false; }
  out = it->second.c_str(); return true;
}
static std::string listJsonStr(const std::string& d) {
  g_listCalls++;
  if (d == "/scripts" && !g_dirOk) return "{\"ok\":false}";
  if (d != "/scripts" && d != "/apps") return "{\"ok\":false}";
  std::string j = "{\"ok\":true,\"path\":\"" + d + "\",\"list\":[";
  bool first = true; std::string seenDir;
  for (auto& kv : g_fs) {
    if (kv.first.compare(0, d.size() + 1, d + "/") != 0) continue;
    std::string rest = kv.first.substr(d.size() + 1);
    size_t sl = rest.find('/');
    if (sl != std::string::npos) { std::string sub = rest.substr(0, sl); if (sub == seenDir) continue; seenDir = sub;
      j += std::string(first ? "" : ",") + "{\"n\":\"" + sub + "\",\"d\":true,\"s\":10}"; first = false; continue; }
    j += std::string(first ? "" : ",") + "{\"n\":\"" + rest + "\",\"d\":false,\"s\":" + std::to_string(kv.second.size()) + "}"; first = false;
  }
  return j + "],\"more\":false}";
}
String fsListJson(const String& dir) { return String(listJsonStr(dir.c_str()).c_str()); }
bool fsListBuild(std::string_view path, HeapBuf& j) { std::string t = listJsonStr(std::string(path)); return j.add(t.c_str(), t.size()); }
bool fsMkdir(const String& p, String&) { if (p == "/scripts") g_dirOk = true; return true; }
bool fsRename(const String& from, const String& to, String& err) {
  auto it = g_fs.find(from.c_str());
  if (it == g_fs.end() || g_fs.count(to.c_str())) { err = "no"; return false; }
  g_fs[to.c_str()] = it->second; g_fs.erase(it); return true;
}
// periferiche a classi finte: rele1 (uscita, rifiuta il valore 7) e in1 (ingresso, vale 1)
static int g_relay = 0; static bool g_relayFail = false;
bool ioHas(const char* n) { return !strcmp(n, "rele1") || !strcmp(n, "in1"); }
bool ioGet(const char* n, int& v, String& e) { if (!strcmp(n, "rele1")) v = g_relay; else v = 1; return true; }
bool ioSet(const char* n, int v, int role, String& e) { if (strcmp(n, "rele1")) { e = "non e un'uscita"; return false; } if (g_relayFail) { e = "troppo presto"; return false; } g_relay = v; return true; }
// tasto BOOT finto: coda di click liberi
static int g_bq[8], g_bn = 0, g_bi = 0; static bool g_battached = false; static int g_battachCalls = 0;
void btnScriptAttach(bool on) { g_battached = on; g_battachCalls++; }
int  btnScriptPop() { return g_bi < g_bn ? g_bq[g_bi++] : 0; }
static String g_mqttT, g_mqttP; static int g_mqttN = 0; static bool g_mqttUp = true;
bool mqttPublishRel(const char* sub, const char* payload, bool) { g_mqttN++; g_mqttT = sub; g_mqttP = payload; return g_mqttUp; }
#include "../../VesevOS/src/packages/vos_berry.cpp"
#include "../../VesevOS/src/packages/vos_berry_mem.cpp"       // 1.7.62e: vos_berry.cpp diviso in 3 file
#include "../../VesevOS/src/packages/vos_berry_scripts.cpp"
#include "../../VesevOS/src/packages/vos_app_rt.h"
// aiuti: le funzioni di Berry scrivono in un HeapBuf (heap, con tetto); qui si rileggono come String
static String BL() { HeapBuf h(BERRY_TEXT_MAX); berryList(h); return String(h.c_str()); }
static String BO() { HeapBuf h(BERRY_TEXT_MAX); berryOutText(h); return String(h.c_str()); }
static String BS() { HeapBuf h(BERRY_TEXT_MAX); berryStatusText(h); return String(h.c_str()); }
static String BJ() { HeapBuf h(BERRY_STATE_MAX); bool ok = berryStateJson(h); return ok ? String(h.c_str()) : String("KO"); }

// esegue uno script come fa berryEval, ma nello stesso thread (il task finto non parte)
static bool run(const char* code, uint32_t ms = 2000) {
  g_code = strdup(code); g_olen = 0; g_obuf[0] = 0; g_otrunc = false; g_elen = 0; g_ebuf[0] = 0; g_toErr = false;
  g_done = false; g_abandoned = false; g_beats = 0; g_deadline = millis() + ms; g_busy = true; g_bg = false; g_app = false; g_stop = false; g_lastYield = millis();
  runTask(NULL);
  g_busy = false;
  return g_runOk;
}
#define OUT() String(g_obuf)
#define ERR() String(g_ebuf)

int main() {
  fsInit();
  // 0) tasto BOOT: btn() legge i click liberi, senza aspettare
  g_bq[0] = 3; g_bq[1] = 1; g_bn = 2; g_bi = 0;
  CHECK(run("print(btn(), btn(), btn())") && OUT() == "3 1 0\n" && !g_battached && g_battachCalls >= 2);
  g_bn = 0; g_bi = 0;
  // 0a) esempio docs/esempi/script/torcia.vsc: un click accende il LED bianco, il click dopo lo spegne; spegnimento da solo dopo MINUTI
  {
    std::string tp = std::string(getenv("VOS_REPO") ? getenv("VOS_REPO") : ".") + "/docs/esempi/script/torcia.vsc", tx;
    FILE* tf = fopen(tp.c_str(), "rb"); CHECK(tf != NULL);
    if (tf) { char b[512]; size_t n; while ((n = fread(b, 1, sizeof b, tf)) > 0) tx.append(b, n); fclose(tf); }
    auto rep = [&](std::string x, const std::string& from, const std::string& to) { size_t p = x.find(from); if (p != std::string::npos) x.replace(p, from.size(), to); return x; };
    std::string fin = rep(tx, "while true\n", "var giri = 0\nwhile giri < 12\n  giri += 1\n");   // il ciclo infinito diventa breve
    CHECK(fin != tx);
    g_bq[0] = 3; g_bq[1] = 1; g_bn = 2; g_bi = 0; g_ledLast = -1; g_ledScript = -1;
    CHECK(run(fin.c_str(), 6000) && OUT().indexOf("torcia accesa") >= 0 && OUT().indexOf("torcia spenta (click)") >= 0 && OUT().indexOf("tempo") < 0 && g_ledLast == 0xFFFFFF && g_ledScript == -1);
    g_bq[0] = 2; g_bn = 1; g_bi = 0; g_ledLast = -1;
    CHECK(run(rep(fin, "var MINUTI = 10", "var MINUTI = 0.001").c_str(), 6000) && OUT().indexOf("torcia accesa") >= 0 && OUT().indexOf("tempo scaduto") >= 0 && g_ledScript == -1);
    g_bq[0] = 1; g_bn = 1; g_bi = 0;
    CHECK(run(rep(fin, "var MINUTI = 10", "var MINUTI = 0").c_str(), 6000) && OUT().indexOf("torcia accesa") >= 0 && OUT().indexOf("spenta") < 0 && g_ledScript == -1);    // MINUTI = 0: mai da solo
    g_bn = 0; g_bi = 0;
  }
  // 0b) logtail(n) e mqtt(nome, testo): negli script sempre, nelle app solo con logread / mqtt in use=
  logInit(); vlog("prova riga uno"); vlog("prova riga due"); vlog("prova riga tre");
  CHECK(run("import string var t = logtail(2) print(string.find(t, 'riga uno') < 0, string.find(t, 'riga due') >= 0, string.find(t, 'riga tre') >= 0)") && OUT() == "true true true\n");
  CHECK(run("print(size(logtail(0)) > 0, size(logtail(999)) <= 2000, size(logtail()) > 0)") && OUT() == "true true true\n");
  g_mqttN = 0; g_mqttUp = true;
  CHECK(run("print(mqtt('log', 'ciao'))") && OUT() == "true\n" && g_mqttT == "app/log" && g_mqttP == "ciao");
  g_mqttUp = false; CHECK(run("print(mqtt('a/b-c_1', 'x'))") && OUT() == "false\n" && g_mqttT == "app/a/b-c_1"); g_mqttUp = true;
  { const char* bad[] = {"mqtt('', 'x')", "mqtt('/cmd', 'x')", "mqtt('cmd/', 'x')", "mqtt('a//b', 'x')", "mqtt('A', 'x')", "mqtt('a#', 'x')", "mqtt('a+', 'x')", "mqtt('a b', 'x')", "mqtt(1, 'x')", "mqtt('a')"};
    for (const char* b : bad) { int n0 = g_mqttN; CHECK(!run(b) && g_mqttN == n0); } }
  { std::string big(2049, 'x'); String c = String("mqtt('a', '") + big.c_str() + "')"; int n0 = g_mqttN; CHECK(!run(c.c_str()) && g_mqttN == n0); }
  g_mqCnt = 0; g_mqttN = 0; CHECK(run("var k = 0 for i: 0 .. 29 if mqtt('r', 'x') k += 1 end end print(k)") && OUT() == "20\n" && g_mqttN == 20); g_mqCnt = 0;
  // 1) calcoli e print
  CHECK(run("print(1+2)") && OUT() == "3\n" && g_used == 0);
  CHECK(run("var a = 10 var b = 4 print(a*b, a/b, a%b)") && OUT() == "40 2 2\n");
  CHECK(run("print(1.5 + 1)") && OUT().startsWith("2.5"));
  CHECK(run("def f(x) return x * 2 end print(f(21))") && OUT() == "42\n");
  CHECK(run("l = [1,2,3] l.push(4) print(l.size())") && OUT() == "4\n");
  // 2) moduli ammessi
  CHECK(run("import string print(string.format('%d-%s', 7, 'x'))") && OUT() == "7-x\n");
  CHECK(run("import math print(math.sqrt(16))") && OUT().startsWith("4"));
  CHECK(!run("import json print('caricato')") && OUT().indexOf("caricato") < 0);      // json spento (profondita senza controllo)
  // 3) errori di sintassi e di esecuzione (messaggio in err, non in out)
  CHECK(!run("print(") && ERR().indexOf("syntax_error") >= 0 && OUT() == "");
  CHECK(!run("print(undefined_name_x.y)") && ERR().length() > 0);
  // 4) sandbox: niente os, sys, debug, introspect, global, solidify, time, file
  const char* banned[] = {"os", "sys", "debug", "introspect", "global", "solidify", "time", "strict"};
  for (const char* m : banned) {
    String s = String("import ") + m + " print('caricato')";
    CHECK(!run(s.c_str()) && OUT().indexOf("caricato") < 0);
  }
  CHECK(!run("var f = open('/x','r')"));                  // niente file
  CHECK(g_used == 0 && g_realLive == 0);
  // 5) limite di tempo
  uint32_t t0 = millis();
  CHECK(!run("while true end", 300) && ERR().indexOf("timeout_error") >= 0);
  CHECK(millis() - t0 < 1500 && g_used == 0);
  CHECK(!run("var i = 0 while true i += 1 end", 200) && ERR().indexOf("timeout_error") >= 0);
  // 5b) troppi livelli di chiamate nidificate (map dentro map...): fermato dal controllo dello stack, non dal crash
  CHECK(!run("class A def tostring() return str(self) end end print(A())") && ERR().indexOf("stack_error") >= 0 && g_used == 0);
  CHECK(!run("def f(n) return call(f, n + 1) end f(0)") && ERR().indexOf("stack_error") >= 0 && g_used == 0);
  CHECK(!run("def f(n) return f(n + 1) + 1 end f(0)") && g_used == 0);
  // 6) limite di memoria: nessuna esplosione, errore pulito, memoria tutta liberata
  CHECK(!run("var s = 'abcdefghij' while true s = s + s end", 5000) && ERR().indexOf("memory_error") >= 0 && g_peak <= BERRY_MEM_LIMIT);
  CHECK(g_used == 0 && g_realLive == 0 && g_live == NULL);
  CHECK(!run("var l = [] while true l.push('x' + str(l.size())) end", 5000) && ERR().indexOf("memory_error") >= 0 && g_peak <= BERRY_MEM_LIMIT);
  CHECK(g_used == 0 && g_realLive == 0 && g_live == NULL);
  printf("  (memoria non liberata da Berry dopo errore di memoria: %u byte, liberata a forza)\n", g_lastLeak);
  // dopo un errore di memoria l'esecuzione successiva funziona
  CHECK(run("print('ancora vivo')") && OUT() == "ancora vivo\n");
  // 7) funzioni di VesevOS
  g_lastAct = "";
  CHECK(run("led('ff0000')") && g_ledLast == 0xFF0000 && g_ledScript == -1 && g_lastAct == "");       // a fine script il LED torna normale; led() NON passa dalle azioni (niente salvataggio)
  CHECK(run("led('00FF7f') print('x')") && OUT() == "x\n" && g_ledLast == 0x00FF7F && g_lastAct == "");
  { int n0 = g_ledCalls; run("led('0000ff')"); CHECK(g_ledCalls == n0 + 2); }   // imposta + rilascio finale
  CHECK(run("led('ff0000') sleep(1)") && g_ledScript == -1);
  CHECK(!run("led('zzzzzz')") && ERR().indexOf("value_error") >= 0 && g_ledScript == -1);
  CHECK(!run("led('ff00')") && ERR().indexOf("value_error") >= 0);
  CHECK(run("led('auto')") && g_ledScript == -1);
  CHECK(!run("led(5)") && ERR().indexOf("type_error") >= 0);
  CHECK(run("print(act('gpio 4 1'))") && OUT() == "ok\n" && g_lastAct == "gpio 4 1");
  // le azioni led* salvano la configurazione: tetto di 6 al minuto
  CHECK(run("var n = 0 for i: 1 .. 10 if act('led-bright 10') == 'ok' n += 1 end end print(n)") && OUT() == "6\n");
  CHECK(run("print(act('led-bright 10'))") && OUT().indexOf("troppe modifiche") == 0);
  CHECK(run("print(act('gpio 4 0'))") && OUT() == "ok\n");                       // le altre azioni non sono limitate
  CHECK(run("print(act('bad cosa'))") && OUT() == "Azione non ammessa\n");
  CHECK(run("print(temp())") && OUT().startsWith("42"));
  CHECK(run("print(time() > 1000000)") && OUT() == "true\n");
  CHECK(run("var a = millis() sleep(60) print(millis() - a >= 50)") && OUT() == "true\n");
  CHECK(!run("sleep(5000)", 150) && ERR().indexOf("timeout_error") >= 0);
  CHECK(run("log('prova')"));
  // 8) uscita troppo lunga: tagliata, niente scrittura fuori dal buffer
  CHECK(run("for i: 0 .. 600 print('0123456789') end") && g_otrunc && g_olen <= BERRY_OUT_MAX && g_obuf[g_olen] == 0);
  // 9) errore lungo: tagliato nel buffer di err
  CHECK(!run("def f(n) return f(n+1) + 1 end f(0)") && g_elen < sizeof(g_ebuf));
  // 10) berryEval: input vuoto/lungo, occupato, task che non risponde
  String o, e;
  CHECK(!berryEval("", o, e) && e.length());
  String big; for (int i = 0; i < BERRY_CODE_MAX + 1; i++) big += "a";
  CHECK(!berryEval(big.c_str(), o, e) && e.length());
  CHECK(!berryEval("print(1)", o, e, 0) && e == "Lo script non risponde" && g_busy && g_abandoned);   // il task finto non parte
  CHECK(!berryEval("print(2)", o, e) && e == "Berry e occupato con un altro script");
  runTask(NULL);                                                    // il "task" finisce: libera da solo il busy perche abbandonato
  CHECK(!g_busy);
  // 10b) script salvati in /scripts (.vsc) (il task finto non parte: lo faccio girare io con runTask)
  String er;
  CHECK(!berryStart("Demo!", er) && er.length() && !g_busy);              // nome non valido
  CHECK(!berryStart("../x", er) && !berryStart("a b", er) && !berryStart("", er));
  CHECK(g_fs.count("/apps/vecchio.be") == 1 && g_fs.count("/scripts/vecchio.vsc") == 0);   // prima: il vecchio script e in /apps
  CHECK(!berryStart("nope", er) && er.indexOf("Script non trovato: scrivi 'script list'") == 0 && !g_busy);
  // migrazione (una volta per avvio): solo /apps/<nome valido>.be -> /scripts/<nome>.vsc; il resto non si tocca
  CHECK(g_fs.count("/apps/vecchio.be") == 0 && g_fs["/scripts/vecchio.vsc"] == "print('vecchio')");
  CHECK(g_fs.count("/apps/Bad Name.be") == 1 && g_fs.count("/apps/lampada.vap") == 1);
  CHECK(berryStart("vecchio.be", er) && berryRunning()); runTask(NULL);       // il vecchio nome .be funziona ancora nei comandi
  CHECK(g_runOk && String(g_obuf) == "vecchio\n" && !berryRunning());
  { int c0 = g_listCalls; g_fs["/apps/ancora.be"] = "print(9)"; BL(); CHECK(g_fs.count("/apps/ancora.be") == 1 && g_listCalls == c0 + 1); }   // niente seconda migrazione
  CHECK(!berryStart("big", er) && er.length() && !g_busy);                // oltre BERRY_FILE_MAX
  CHECK(berryStart("DEMO.vsc", er) && berryRunning() && g_bg);            // maiuscole e .vsc accettati
  CHECK(!berryStart("demo", er) && er == "Berry e occupato con un altro script");
  CHECK(BL().indexOf("demo  (70 B)  <- in corso") > 0);
  runTask(NULL);
  CHECK(!berryRunning() && g_runOk && String(g_obuf) == "hola\nfine\n");
  CHECK(BO().indexOf("Script: demo") == 0 && BO().indexOf("hola\nfine") > 0);
  String l = BL();
  CHECK(l.indexOf("Script in /scripts: 6") == 0 && l.indexOf("demo  (70 B)") > 0 && l.indexOf("vecchio  (") > 0 && l.indexOf("x.txt") < 0 && l.indexOf("old") < 0 && l.indexOf("sub") < 0 && l.indexOf("in corso") < 0);
  g_dirOk = false; CHECK(BL().indexOf("Script in /scripts") == 0 && g_dirOk);       // cartella mancante: creata
  // sottofondo: senza pause fermato dopo BERRY_BURST_MS; con sleep puo girare piu a lungo; stop lo ferma
  uint32_t tb = millis();
  CHECK(berryStart("busy", er)); runTask(NULL);
  CHECK(!g_runOk && ERR().indexOf("timeout_error") >= 0 && ERR().indexOf("senza pause") >= 0 && !berryRunning() && millis() - tb < 1500 && g_used == 0);
  CHECK(berryStart("loop", er));
  String stopErr; bool stopOk = false;
  std::thread th([&] { std::this_thread::sleep_for(std::chrono::milliseconds(700)); stopOk = berryStop(stopErr); });   // 700 ms > 300 ms di BERRY_BURST_MS: sta in piedi grazie a sleep
  uint32_t tl = millis();
  runTask(NULL);
  th.join();
  CHECK(stopOk && !g_runOk && ERR().indexOf("stopped_error") >= 0 && !berryRunning() && millis() - tl >= 650 && g_used == 0);
  CHECK(!berryStop(er) && er == "Nessuno script in corso");
  CHECK(berryStart("spam", er)); runTask(NULL);
  CHECK(g_runOk && g_otrunc && g_olen <= BERRY_OUT_MAX && String(g_obuf).endsWith("FINE\n") && g_obuf[g_olen] == 0);
  CHECK(run("print('di nuovo comando')") && OUT() == "di nuovo comando\n");   // il modo "comando" torna normale
  // 10c) stato per la pagina (JSON): elenco, in corso, uscita ed errore (con caratteri da proteggere)
  { String j = BJ();
    CHECK(j.startsWith("{\"ok\":true,\"ver\":\"") && j.indexOf("\"busy\":false") > 0 && j.indexOf("{\"n\":\"demo\",\"s\":") > 0 && j.indexOf("\"n\":\"vecchio\"") > 0);
    CHECK(j.indexOf("x.txt") < 0 && j.indexOf("old") < 0 && j.indexOf("\"n\":\"sub\"") < 0 && j.indexOf("\"max\":8192") > 0 && j.endsWith("}"));
    g_fs["/scripts/virg.vsc"] = "print('a\"b\\\\c') print(1/0)";
    CHECK(berryStart("virg", er) && berryRunning()); runTask(NULL);
    j = BJ();
    CHECK(j.indexOf("\"busy\":false") > 0 && j.indexOf("\"last\":0") > 0 && j.indexOf("\"out\":\"a\\\"b\\\\c\\n\"") > 0 && j.indexOf("divzero_error") > 0 && j.indexOf("\"name\":\"virg\"") > 0);
    g_fs["/scripts/lungo.vsc"] = "print('x') sleep(200) print('y')";
    CHECK(berryStart("lungo", er)); j = BJ();
    CHECK(j.indexOf("\"busy\":true,\"bg\":true") > 0 && j.indexOf("\"name\":\"lungo\"") > 0 && j.indexOf("\"err\":\"\"") > 0);   // mentre gira: nessun errore vecchio
    runTask(NULL); CHECK(!berryRunning()); }
  // 10c2) 1.7.66a-string7: buffer sull'heap con tetto, stesso escape di jsonEscape(), elenco grande, lettura mentre il task scrive
  { HeapBuf tiny(60); CHECK(!berryStateJson(tiny));                                  // non entra: false (mai un JSON troncato dato per buono)
    { std::string all; for (int c = 1; c < 256; c++) all += (char)c;                  // ogni byte tranne lo zero: stesso testo di jsonEscape()
      HeapBuf h(4096); CHECK(h.addJson(all.data(), all.size()));
      CHECK(String(h.c_str()) == jsonEscape(String(all.c_str()))); }
    auto savedFs = g_fs; g_fs.clear(); g_dirOk = true;                              // i file delle altre prove si rimettono alla fine
    for (int i = 0; i < 200; i++) g_fs[std::string("/scripts/s") + std::to_string(i) + ".vsc"] = "print(1)";
    String bj = BJ(); CHECK(bj != "KO" && bj.startsWith("{\"ok\":true") && bj.endsWith("}") && bj.indexOf("\"n\":\"s199\"") > 0);   // 200 script: entrano nel tetto
    String bl = BL(); CHECK(bl.indexOf("Script in /scripts: 200") == 0 && bl.indexOf("  s199  (8 B)") > 0);
    g_fs = savedFs; g_dirOk = true;
    g_bg = true; g_toErr = false; g_olen = 0; g_obuf[0] = 0; g_otrunc = false; g_elen = 0; g_ebuf[0] = 0;
    std::atomic<bool> stopW(false);
    std::thread w([&] { for (int i = 0; i < 30000 && !stopW; i++) vos_berry_write("abcdefghij\n", 11); });
    bool allOk = true;
    for (int i = 0; i < 300 && allOk; i++) {                                         // il task scrive: l'uscita letta ha solo i caratteri scritti
      String t = BJ(); int a = t.indexOf("\"out\":\""), b = t.indexOf("\",\"err\"");
      if (t == "KO" || a < 0 || b < a) { allOk = false; break; }
      for (int k = a + 7; k < b; k++) if (!strchr("abcdefghij\\n", t[k])) allOk = false;
    }
    stopW = true; w.join(); CHECK(allOk);
    g_bg = false; g_olen = 0; g_obuf[0] = 0; g_otrunc = false; }
  // 10d) coda (azione "script <nome>" delle Automazioni): parte subito o va in coda (max 4, senza doppioni); stesso task; stop la svuota
  g_fs["/scripts/qa.vsc"] = "print('A')"; g_fs["/scripts/qb.vsc"] = "print('B')"; g_fs["/scripts/qc.vsc"] = "print('C')";
  g_fs["/scripts/qd.vsc"] = "print('D')"; g_fs["/scripts/qe.vsc"] = "print('E')";
  CHECK(!berryQueue("Bad Name", er, "t") && er.indexOf("Nome script non valido") == 0 && !berryRunning());
  CHECK(!berryQueue("nonesiste", er, "t") && er.indexOf("Script non trovato") == 0 && !berryRunning() && g_qn == 0);   // libero + errore vero: non si accoda
  uint32_t runs0 = g_runs;
  CHECK(berryQueue("qa", er, "regola") && berryRunning() && g_qn == 0);        // libero: parte subito
  CHECK(berryQueue("qb", er, "regola") && g_qn == 1);                          // occupato: in coda
  CHECK(!berryQueue("QB.vsc", er, "regola") && er == "Script gia in coda" && g_qn == 1);   // doppione (nome ripulito)
  CHECK(berryQueue("qc", er, "mqtt") && berryQueue("nonesiste", er, "t") && berryQueue("qd", er, "t") && g_qn == 4);
  CHECK(!berryQueue("qe", er, "t") && er.indexOf("Coda piena") == 0 && g_qn == 4);
  CHECK(BJ().indexOf("\"q\":4") > 0);
  runTask(NULL);                                                                // qa, qb, qc, (nonesiste saltato), qd: tutti dallo stesso task
  CHECK(!berryRunning() && g_qn == 0 && g_runs == runs0 + 4 && String(g_name) == "qd" && String(g_obuf) == "D\n" && g_runOk);
  // stop a mano: svuota la coda e lo script in corso si ferma senza passare al prossimo
  CHECK(berryQueue("loop", er, "t") && berryQueue("qa", er, "t") && g_qn == 1);
  { String se; bool so = false;
    std::thread th2([&] { std::this_thread::sleep_for(std::chrono::milliseconds(500)); so = berryStop(se); });
    runTask(NULL); th2.join();
    CHECK(so && !berryRunning() && g_qn == 0 && String(g_name) == "loop" && ERR().indexOf("stopped_error") >= 0); }
  // anti-ricorsione: uno script NON puo avviare altri script (ne con act("script ..."), ne con spazi davanti)
  g_lastAct = "";
  CHECK(run("print(act('script qa'))") && OUT() == "uno script non puo avviare altri script o app\n" && g_lastAct == "" && g_qn == 0);
  CHECK(run("print(act('  script qa'))") && OUT() == "uno script non puo avviare altri script o app\n" && g_lastAct == "");
  CHECK(run("print(act('note ciao'))") && OUT() == "ok\n" && g_lastAct == "note ciao");        // le altre azioni passano
  CHECK(berryNameValid("QA.vsc") && !berryNameValid("a b") && !berryNameValid(""));

  // 10e) APP (.vap): codice che resta vivo, on_set dalla pagina, on_tick ogni secondo, dev_get / dev_set, permessi del manifest
  {
    auto mk = [](AppInfo& a) {
      a.name = "t1"; a.nctl = 0; a.data = "power=0\nlum=128\ninfo=ciao\nextra=hello\n"; a.ndata = 4;
      struct D { char k; const char* id; int mn, mx, pts; } d[] = {{'s', "power", 0, 1, 0}, {'l', "lum", 0, 255, 0}, {'t', "info", 0, 0, 0}, {'c', "col", 0, 0, 0}, {'b', "reset", 0, 0, 0}, {'g', "temp", 0, 0, 10}};
      for (auto& x : d) { AppCtl& c = a.ctl[a.nctl++]; memset(&c, 0, sizeof(c)); c.k = x.k; strcpy(c.id, x.id); strcpy(c.label, x.id); c.mn = x.mn; c.mx = x.mx; c.pts = x.pts; }
    };
    auto getS = [](const char* id) { bool n; long v; char sv[41]; rtGet(id, n, v, sv, sizeof(sv)); return String(sv); };
    auto getN = [](const char* id) { bool n; long v; char sv[41]; rtGet(id, n, v, sv, sizeof(sv)); return v; };
    AppInfo* A = new AppInfo(); mk(*A); String er;
    // codice semplice senza funzioni: finisce da solo; dev_get/dev_set
    rtInit(*A);
    CHECK(berryStartApp("t1", "print(dev_get('power'), dev_get('info'), dev_get('lum'), dev_get('extra')) dev_set('info', 'nuovo') dev_set('temp', 21) dev_set('temp', 22)", "", er) && berryRunning() && g_app); runTask(NULL);
    CHECK(!berryRunning() && g_runOk && OUT() == "0 ciao 128 hello\n" && getS("info") == "nuovo" && getN("temp") == 22);
    { String n; CHECK(!berryAppRunning(n) && berryLastIsApp("t1") && !berryLastIsApp("t2")); }
    // btn() nelle app: serve "btn" in use=
    g_bq[0] = 2; g_bn = 1; g_bi = 0; rtInit(*A);
    CHECK(berryStartApp("t1", "print(btn())", "", er)); runTask(NULL); CHECK(!g_runOk && ERR().indexOf("btn") >= 0 && !g_battached);
    g_bi = 0; rtInit(*A);
    CHECK(berryStartApp("t1", "print(btn(), btn())", "btn", er)); runTask(NULL); CHECK(g_runOk && OUT() == "2 0\n" && !g_battached);
    // validazioni di dev_set / dev_get (ogni errore ferma l'app con un messaggio chiaro)
    rtInit(*A);
    CHECK(berryStartApp("t1", "dev_set('lum', 999) dev_set('power', 5) dev_set('col', '#FF00aa') dev_set('reset', 1) dev_set('temp', 3.9) print(dev_get('lum'), dev_get('power'), dev_get('col'), dev_get('reset'), dev_get('temp'))", "", er)); runTask(NULL);
    CHECK(g_runOk && OUT() == "255 1 ff00aa 1 3\n");
    const char* bad[] = {"dev_set('zzz', 1)", "dev_get('zzz')", "dev_set('col', 'zz')", "dev_set('col', 5)", "dev_set('info', 'x123456789x123456789x123456789x123456789x')", "dev_set('lum', 'abc')", "dev_get(5)", "dev_set('lum')"};
    for (const char* c : bad) { rtInit(*A); CHECK(berryStartApp("t1", c, "", er)); runTask(NULL); CHECK(!g_runOk && ERR().length() > 0 && !berryRunning()); }
    // permessi: led solo se use=led; act solo se use=act; time/temp/log
    rtInit(*A); CHECK(berryStartApp("t1", "led('ff0000')", "", er)); runTask(NULL); CHECK(!g_runOk && ERR().length() > 0);
    g_ledLast = -1; rtInit(*A); CHECK(berryStartApp("t1", "led('ff0000')", "led,log", er)); runTask(NULL); CHECK(g_runOk && g_ledLast == 0xFF0000);
    rtInit(*A); CHECK(berryStartApp("t1", "print(act('note ciao'))", "led", er)); runTask(NULL); CHECK(!g_runOk);
    g_lastAct = ""; rtInit(*A); CHECK(berryStartApp("t1", "print(act('note ciao'))", "act", er)); runTask(NULL); CHECK(g_runOk && OUT() == "ok\n" && g_lastAct == "note ciao");
    rtInit(*A); CHECK(berryStartApp("t1", "print(act('app t2'))", "act", er)); runTask(NULL); CHECK(g_runOk && OUT() == "uno script non puo avviare altri script o app\n");
    rtInit(*A); CHECK(berryStartApp("t1", "print(temp() > 0, time() > 0)", "temp,time", er)); runTask(NULL); CHECK(g_runOk && OUT() == "true true\n");
    rtInit(*A); CHECK(berryStartApp("t1", "print(temp())", "time", er)); runTask(NULL); CHECK(!g_runOk);
    // periferiche a classi: servono nel use=; senza il permesso l'app si ferma con un messaggio
    g_relay = 0; rtInit(*A); CHECK(berryStartApp("t1", "dev_set('rele1', 1) print(dev_get('rele1'), dev_get('in1'))", "rele1,in1", er)); runTask(NULL); CHECK(g_runOk && g_relay == 1 && OUT() == "1 1\n");
    g_relay = 0; rtInit(*A); CHECK(berryStartApp("t1", "dev_set('rele1', 1)", "log", er)); runTask(NULL); CHECK(!g_runOk && g_relay == 0 && ERR().indexOf("permesso mancante") >= 0);
    rtInit(*A); CHECK(berryStartApp("t1", "print(dev_get('rele1'))", "in1", er)); runTask(NULL); CHECK(!g_runOk && ERR().indexOf("permesso mancante") >= 0);
    g_relay = 0; rtInit(*A); CHECK(berryStartApp("t1", "dev_set('rele1', true) dev_set('rele1', 0)", "rele1", er)); runTask(NULL); CHECK(g_runOk && g_relay == 0);
    g_relayFail = true; rtInit(*A); CHECK(berryStartApp("t1", "dev_set('rele1', 1)", "rele1", er)); runTask(NULL); CHECK(!g_runOk && ERR().indexOf("troppo presto") >= 0); g_relayFail = false;
    rtInit(*A); CHECK(berryStartApp("t1", "dev_set('in1', 1)", "in1", er)); runTask(NULL); CHECK(!g_runOk);
    // logtail e mqtt nelle app: servono logread e mqtt in use=
    rtInit(*A); CHECK(berryStartApp("t1", "print(logtail(1))", "", er)); runTask(NULL); CHECK(!g_runOk && ERR().indexOf("logtail") >= 0);
    rtInit(*A); CHECK(berryStartApp("t1", "print(mqtt('x', 'y'))", "log,logread", er)); runTask(NULL); CHECK(!g_runOk && ERR().indexOf("mqtt") >= 0);
    rtInit(*A); CHECK(berryStartApp("t1", "print(size(logtail(1)) > 0)", "logread", er)); runTask(NULL); CHECK(g_runOk && OUT() == "true\n");
    g_mqttUp = true; { int n0 = g_mqttN; rtInit(*A); CHECK(berryStartApp("t1", "print(mqtt('x', 'y'))", "mqtt", er)); runTask(NULL); CHECK(g_runOk && OUT() == "true\n" && g_mqttN == n0 + 1); }
    // un app non vede le funzioni degli script senza permesso, ma uno script normale non ha dev_get
    CHECK(!run("dev_get('x')"));
    // on_set dalla pagina + on_tick; stop a mano = fine normale (non e un errore)
    rtInit(*A);
    CHECK(berryStartApp("t1", "var n = 0 def on_set(id, v) dev_set('info', id + '=' + str(v)) end def on_tick() n += 1 dev_set('temp', n) end print('pronta')", "", er));
    { std::thread th([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        String e2; bool a1 = rtSetWeb("power", "1", e2), a2 = rtSetWeb("lum", "77", e2), a3 = !rtSetWeb("lum", "300", e2), a4 = !rtSetWeb("temp", "1", e2), a5 = !rtSetWeb("nope", "1", e2);
        std::this_thread::sleep_for(std::chrono::milliseconds(2400));
        String s2; bool so = berryStop(s2);
        CHECK(a1 && a2 && a3 && a4 && a5 && so);
      });
      runTask(NULL); th.join(); }
    CHECK(!berryRunning() && g_runOk && ERR() == "" && OUT() == "pronta\n");
    CHECK(getN("power") == 1 && getN("lum") == 77 && getS("info") == "lum=77" && getN("temp") >= 2);
    { String j = rtValuesJson(); CHECK(j.indexOf("\"id\":\"power\",\"n\":1") > 0 && j.indexOf("\"id\":\"info\",\"s\":\"lum=77\"") > 0 && j.indexOf("\"id\":\"temp\",\"a\":[1,2") > 0 && j.indexOf("extra") < 0); }
    // colore e testo dalla pagina; pulsante genera un evento ma non cambia il valore
    rtInit(*A);
    CHECK(berryStartApp("t1", "def on_set(id, v) dev_set('info', id + ':' + str(v)) end", "", er));
    { std::thread th([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        String e2; CHECK(rtSetWeb("col", "#00FF7F", e2)); std::this_thread::sleep_for(std::chrono::milliseconds(120)); CHECK(getS("info") == "col:00ff7f");
        CHECK(!rtSetWeb("col", "zzzzzz", e2) && !rtSetWeb("col", "12345", e2));
        CHECK(rtSetWeb("info", "ciao mondo", e2) && !rtSetWeb("info", "x123456789x123456789x123456789x123456789x", e2) && !rtSetWeb("info", "a\tb", e2));
        std::this_thread::sleep_for(std::chrono::milliseconds(120)); CHECK(getS("info") == "info:ciao mondo");
        CHECK(rtSetWeb("reset", "xyz", e2)); std::this_thread::sleep_for(std::chrono::milliseconds(120)); CHECK(getS("info") == "reset:1" && getN("reset") == 0);
        String s2; berryStop(s2);
      });
      runTask(NULL); th.join(); }
    CHECK(g_runOk && ERR() == "");
    // errore dentro on_set o on_tick: l'app si ferma con il messaggio (e nel registro)
    rtInit(*A);
    CHECK(berryStartApp("t1", "def on_set(id, v) var x = nil print(x.y) end", "", er));
    { std::thread th([&] { std::this_thread::sleep_for(std::chrono::milliseconds(150)); String e2; rtSetWeb("power", "1", e2); }); runTask(NULL); th.join(); }
    CHECK(!berryRunning() && !g_runOk && ERR().length() > 0 && g_lastOk == false);
    rtInit(*A);
    CHECK(berryStartApp("t1", "def on_set(id, v) while true end end", "", er));          // callback che non finisce: fermata dal limite senza pause
    { std::thread th([&] { std::this_thread::sleep_for(std::chrono::milliseconds(150)); String e2; rtSetWeb("power", "1", e2); }); runTask(NULL); th.join(); }
    CHECK(!g_runOk && ERR().indexOf("timeout_error") >= 0);
    // un solo motore: app e script si escludono; stato JSON dello script vede l'app
    rtInit(*A); CHECK(berryStartApp("t1", "def on_tick() end", "", er));
    { String n; CHECK(berryRunning() && berryAppRunning(n) && n == "t1" && !berryStartApp("t2", "print(1)", "", er) && er == "Berry e occupato con un altro script" && !berryStart("demo", er)); }
    { std::thread th([&] { std::this_thread::sleep_for(std::chrono::milliseconds(200)); String s2; berryStop(s2); }); runTask(NULL); th.join(); }
    CHECK(!berryRunning() && g_runOk);
    { HeapBuf h(BERRY_STATE_MAX); CHECK(berryAppOutJson(h)); String o(h.c_str()); CHECK(o.indexOf("\"out\":\"") == 0 && o.indexOf("\"last\":1") > 0); }
    // dopo le app uno script normale funziona ancora e non ha le funzioni delle app
    CHECK(run("print('script ok')") && OUT() == "script ok\n" && !g_app);
    CHECK(g_used == 0 && g_realLive == 0 && g_live == NULL);
    rtFree(); delete A;
  }
  // 10c) pulizia del nome: cleanNameTo (senza String) uguale al vecchio cleanName + utilNameOk su 20000 testi a caso
  {
    auto oldClean = [](const String& in) { String n = in; n.trim(); n.toLowerCase(); if (n.endsWith(SCRIPT_EXT)) n.remove(n.length() - 4); else if (n.endsWith(".be")) n.remove(n.length() - 3); return n; };
    const char* parts[] = {"a", "B", "z9", "-", "_", ".", ".vsc", ".VSC", ".be", ".BE", " ", "\t", "\r\n", "x", "0", "/", "..", "\xc3\xa8", "vsc", "be"};
    srand(7); int bad = 0;
    for (int i = 0; i < 20000; i++) {
      std::string t; int np = rand() % 14; for (int k = 0; k < np; k++) t += parts[rand() % 20];
      if (i % 50 == 0) t = std::string(20 + rand() % 40, 'a') + (rand() % 2 ? ".vsc" : "") + (rand() % 2 ? "  " : "");   // lunghi, vicini a 24 e a 40
      String o = oldClean(t.c_str()); bool ov = utilNameOk(o);
      char n[NAME_BUF]; bool nv = cleanNameTo(t.c_str(), n);
      if (nv != ov || (nv && o != n)) { if (bad++ < 3) printf("diverso: [%s]\n", t.c_str()); }
    }
    CHECK(bad == 0);
    char nb[NAME_BUF];
    CHECK(!cleanNameTo(NULL, nb) && !cleanNameTo("", nb) && !cleanNameTo(".vsc", nb) && !cleanNameTo(".be", nb) && !cleanNameTo(std::string(24, 'a').append("x").c_str(), nb));
    CHECK(cleanNameTo("  Demo.VSC \n", nb) && String(nb) == "demo" && cleanNameTo(std::string(24, 'a').c_str(), nb) && strlen(nb) == 24);
    CHECK(!cleanNameTo(std::string(39, 'a').c_str(), nb) && !cleanNameTo(std::string(40, 'a').c_str(), nb) && !cleanNameTo(std::string(200, 'a').c_str(), nb));
  }
  // 10d) migrateOld con un elenco strano: al massimo 16 spostati per volta, solo nomi validi .be file, cartelle e nomi lunghi no
  {
    fsInit(); g_migDone = false;
    g_fs.erase("/apps/vecchio.be");
    for (int i = 0; i < 20; i++) { char nm[40]; snprintf(nm, sizeof nm, "/apps/m%02d.be", i); g_fs[nm] = "print(1)"; }
    g_fs["/apps/" + std::string(24, 'q') + ".be"] = "print(2)";          // 24 caratteri: valido
    g_fs["/apps/" + std::string(25, 'q') + ".be"] = "print(3)";          // 25: non valido
    g_fs["/apps/.be"] = "print(4)";                                      // nome vuoto
    g_fs["/apps/sotto/k.be"] = "print(5)";                               // dentro una cartella: non conta
    int mv = migrateOld();
    int rimasti = 0; for (int i = 0; i < 20; i++) { char nm[40]; snprintf(nm, sizeof nm, "/apps/m%02d.be", i); rimasti += g_fs.count(nm); }
    CHECK(mv == 16 && g_migDone);
    CHECK(rimasti == 4 && g_fs.count("/scripts/m00.vsc") == 1 && g_fs.count("/scripts/m15.vsc") == 1 && g_fs.count("/scripts/m16.vsc") == 0);   // vanno in ordine alfabetico: m00..m15
    CHECK(g_fs.count("/apps/.be") == 1 && g_fs.count("/apps/" + std::string(25, 'q') + ".be") == 1 && g_fs.count("/apps/sotto/k.be") == 1);
    CHECK(migrateOld() == 0);                                            // una volta per avvio
    g_migDone = false; CHECK(migrateOld() == 5 && g_fs.count("/scripts/" + std::string(24, 'q') + ".vsc") == 1 && g_fs.count("/scripts/m19.vsc") == 1);   // al giro dopo gli altri 4 e il nome da 24
    CHECK(g_fs.count("/apps/.be") == 1 && g_fs.count("/apps/" + std::string(25, 'q') + ".be") == 1 && g_fs.count("/apps/sotto/k.be") == 1);   // nomi non validi e cartelle restano
    g_dirOk = false; g_migDone = false; g_fs.clear(); CHECK(migrateOld() == 0 && g_migDone);   // niente /apps: zero
    fsInit();
  }
  // 11) stato
  CHECK(BS().indexOf("Berry 1.1.0") == 0 && BS().indexOf("Esecuzioni:") > 0);
  return done("test_berry");
}
