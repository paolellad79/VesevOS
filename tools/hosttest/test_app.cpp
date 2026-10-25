// VesevOS - prove sul computer: lettura e controllo dei file .vap (vos_app.cpp)
#include <Arduino.h>
#include "mini.h"
#include <string.h>
#include <map>
#include <string>
#include <dirent.h>

static std::map<std::string, std::string> g_fs;
static bool g_dirOk = true;
bool fsReadText(const String& p, String& out, String& err) {
  auto it = g_fs.find(p.c_str());
  if (it == g_fs.end()) { err = "File non trovato"; return false; }
  out = it->second.c_str(); return true;
}
String fsListJson(const String& dir) {
  if (dir != "/apps" || !g_dirOk) return "{\"ok\":false}";
  std::string j = "{\"ok\":true,\"path\":\"/apps\",\"list\":[";
  bool first = true;
  for (auto& kv : g_fs) {
    if (kv.first.compare(0, 6, "/apps/") != 0) continue;
    j += std::string(first ? "" : ",") + "{\"n\":\"" + kv.first.substr(6) + "\",\"d\":false,\"s\":" + std::to_string(kv.second.size()) + "}"; first = false;
  }
  return String((j + "],\"more\":false}").c_str());
}
bool fsMkdir(const String& p, String&) { if (p == "/apps") g_dirOk = true; return true; }
#include "../../VesevOS/src/core/vos_heapbuf.h"
// finto motore Berry (qui si prova solo la parte "app": ruoli, avvio, stop, stato)
static bool g_aRun = false, g_busyOther = false, g_lastApp = false; static String g_aName, g_aUse, g_aCode; static int g_starts = 0;
bool berryRunning() { return g_aRun || g_busyOther; }
bool berryAppRunning(String& n) { if (g_aRun) { n = g_aName; return true; } return false; }
bool berryLastIsApp(const char* n) { return g_lastApp && g_aName == n; }
bool berryAppOutJson(HeapBuf& o) { return o.add("\"out\":\"ciao\",\"err\":\"\",\"ms\":5,\"last\":1"); }
bool berryStartApp(const char* n, const char* c, const char* u, String& err) { if (g_busyOther || g_aRun) { err = "Berry e occupato"; return false; } g_aRun = true; g_aName = n; g_aUse = u; g_aCode = c; g_lastApp = true; g_starts++; return true; }
bool berryStop(String& err) { if (!g_aRun) { err = "Nessuno script in corso"; return false; } g_aRun = false; return true; }
void berryOutText(HeapBuf& o) { o.add("uscita\n"); }
#include "../../VesevOS/src/packages/vos_app.cpp"
#include "../../VesevOS/src/packages/vos_app_rt.h"

static const char* ICON = "<svg viewBox=\"0 0 24 24\"><path d=\"M5 19l4-9 3 4 3-6 4 11z\" fill=\"none\" stroke=\"#c0392b\" stroke-width=\"2\"/></svg>";
static std::string good(const char* name = "lamp") {
  std::string s = "# lampada di prova\n[manifest]\nname=" + std::string(name) + "\ntitle=Lampada\nversion=1.2\nauthor=Mimmo\nuse=led, gpio:5,gpio:06\nwho=oper\n\n[icon]\n";
  s += ICON; s += "\n\n[page]\nswitch id=power label=\"Accesa\"\nslider id=lum label=\"Luminosita\" min=0 max=255\ncolor id=col label=\"Colore\"\ntext id=info label=\"Stato\"\nbutton id=reset label=\"Azzera\"\nchart id=temp label=\"Temperatura\" points=60\n\n[code]\nvar x = [1,2]\nprint(x[0])\ndef on_set(id, v) end\n\n[data]\npower=0\nlum=128\n";
  return s;
}
static bool P(const std::string& t, AppInfo& a, String& err, const char* name = "lamp") { return appParse(String(t.c_str()), String(name), a, err); }
static std::string rep(std::string s, const std::string& a, const std::string& b) { size_t p = s.find(a); if (p != std::string::npos) s.replace(p, a.size(), b); return s; }

int main() {
  AppInfo* A = new AppInfo(); AppInfo& a = *A; String err;
  // 1) file buono
  CHECK(P(good(), a, err) && err == "");
  CHECK(a.name == "lamp" && a.title == "Lampada" && a.ver == "1.2" && a.author == "Mimmo" && a.who == "oper");
  CHECK(a.use == "led,gpio:5,gpio:6" && a.nctl == 6 && a.ndata == 2 && a.icon.startsWith("<svg") && a.icon.endsWith("</svg>"));
  CHECK(a.ctl[0].k == 's' && String(a.ctl[0].id) == "power" && String(a.ctl[0].label) == "Accesa");
  CHECK(a.ctl[1].k == 'l' && a.ctl[1].mn == 0 && a.ctl[1].mx == 255 && a.ctl[5].k == 'g' && a.ctl[5].pts == 60);
  CHECK(a.codeLen > 20 && a.size == good().size());
  { std::string t = rep(good(), "\n", "\r\n"); CHECK(P(t, a, err) && a.nctl == 6); }          // fine riga Windows
  { std::string t = "[manifest]\nname=lamp\n[code]\nprint(1)\n"; CHECK(P(t, a, err) && a.title == "lamp" && a.who == "admin" && a.icon == "" && a.nctl == 0 && a.use == ""); }   // minimo
  // 2) il codice puo contenere righe tipo "[1]" o "[altro]" senza cambiare sezione
  { std::string t = "[manifest]\nname=lamp\n[code]\nvar l = [1]\n[altro]\nprint(2)\n"; CHECK(P(t, a, err) && a.codeLen > 10); }
  // 3) errori con numero di riga
  CHECK(!P(rep(good(), "name=lamp", "name=altro"), a, err) && err.startsWith("Riga 3: ") && err.indexOf("diverso") > 0);
  CHECK(!P(rep(good(), "name=lamp", "name=Lamp"), a, err));
  CHECK(!P(rep(good(), "[page]", "[pagina]"), a, err) && err.indexOf("Sezione sconosciuta") > 0);
  CHECK(!P(rep(good(), "[data]", "[page]"), a, err) && err.indexOf("Sezione ripetuta") > 0);
  CHECK(!P("name=lamp\n", a, err) && err.indexOf("fuori da una sezione") > 0);
  CHECK(!P(rep(good(), "version=1.2", "version=1a"), a, err) && err.indexOf("Versione") > 0);
  CHECK(!P(rep(good(), "version=1.2", "version=12345678901"), a, err));
  CHECK(!P(rep(good(), "who=oper", "who=root"), a, err) && err.indexOf("admin oppure oper") > 0);
  CHECK(!P(rep(good(), "use=led, gpio:5,gpio:06", "use=WiFi"), a, err) && err.indexOf("Permesso") > 0);
  CHECK(!P(rep(good(), "use=led, gpio:5,gpio:06", "use=a"), a, err) && !P(rep(good(), "use=led, gpio:5,gpio:06", "use=rele-1"), a, err) && !P(rep(good(), "use=led, gpio:5,gpio:06", "use=1rele"), a, err) && !P(rep(good(), "use=led, gpio:5,gpio:06", "use=abcdefghijkl"), a, err));
  CHECK(P(rep(good(), "use=led, gpio:5,gpio:06", "use=rele1,led1, in2"), a, err) && a.use == "rele1,led1,in2");   // nomi di periferiche a classi
  CHECK(!P(rep(good(), "gpio:5", "gpio:49"), a, err) && !P(rep(good(), "gpio:5", "gpio:"), a, err) && !P(rep(good(), "gpio:5", "gpio:-1"), a, err));
  CHECK(!P(rep(good(), "use=led,", "use=led,,"), a, err));
  CHECK(!P(rep(good(), "title=Lampada", "titolo=Lampada"), a, err) && err.indexOf("sconosciuta nel manifest") > 0);
  CHECK(!P(rep(good(), "title=Lampada", "title="), a, err));
  CHECK(!P(rep(good(), "title=Lampada", "title=Lam\tpada"), a, err));
  CHECK(!P(rep(good(), "title=Lampada", "title=" + std::string(25, 'x')), a, err));
  CHECK(!P(rep(good(), "author=Mimmo", "mimmo"), a, err) && err.indexOf("chiave=valore") > 0);
  CHECK(!P(rep(good(), "switch id=power", "interruttore id=power"), a, err) && err.indexOf("Controllo non valido") > 0);
  CHECK(!P(rep(good(), "color id=col", "color id=power"), a, err) && err.indexOf("Id ripetuto") > 0);
  CHECK(!P(rep(good(), "switch id=power", "switch id=Power"), a, err) && err.indexOf("Serve id") > 0);
  CHECK(!P(rep(good(), "switch id=power label=\"Accesa\"", "switch label=\"Accesa\""), a, err) && err.indexOf("Serve id") > 0);
  CHECK(!P(rep(good(), "switch id=power label=\"Accesa\"", "switch id=power"), a, err) && err.indexOf("Testo non valido") > 0);
  CHECK(!P(rep(good(), "min=0 max=255", "min=9 max=9"), a, err) && err.indexOf("Parametro") > 0);
  CHECK(!P(rep(good(), "min=0 max=255", "min=x max=9"), a, err));
  CHECK(!P(rep(good(), "points=60", "points=5"), a, err) && !P(rep(good(), "points=60", "points=500"), a, err));
  CHECK(!P(rep(good(), "points=60", "min=1"), a, err));                                  // parametro di un altro tipo
  CHECK(!P(rep(good(), "switch id=power label=\"Accesa\"", "switch id=power label=\"Accesa"), a, err));   // virgolette aperte
  { std::string t = "[manifest]\nname=lamp\n[page]\n"; for (int i = 0; i < 13; i++) t += "switch id=s" + std::to_string(i) + " label=\"S\"\n"; t += "[code]\nprint(1)\n";
    CHECK(!P(t, a, err) && err.indexOf("Troppi controlli") > 0); }
  { std::string t = "[manifest]\nname=lamp\n[code]\nprint(1)\n[data]\n"; for (int i = 0; i < 13; i++) t += "k" + std::to_string(i) + "=1\n";
    CHECK(!P(t, a, err) && err.indexOf("Troppi dati") > 0); }
  CHECK(P(rep(good(), "power=0", "Power=0"), a, err));                                  // chiavi senza distinzione maiuscole
  CHECK(!P(rep(good(), "lum=128", "power=1"), a, err) && err.indexOf("Id ripetuto") > 0);
  CHECK(!P(rep(good(), "lum=128", "lum=" + std::string(41, 'z')), a, err));
  // 4) sezioni obbligatorie e limiti
  CHECK(!P("[code]\nprint(1)\n", a, err) && err.indexOf("Manca [manifest]") == 0);
  CHECK(!P("[manifest]\ntitle=Ciao\n[code]\nprint(1)\n", a, err) && err.indexOf("Manca [manifest]") == 0);
  CHECK(!P("[manifest]\nname=lamp\n", a, err) && err.indexOf("[code]") > 0);
  CHECK(!P("[manifest]\nname=lamp\n[code]\n   \n\n", a, err) && err.indexOf("[code]") > 0);
  { std::string t = "[manifest]\nname=lamp\n[code]\n" + std::string(9000, 'a') + "\n"; CHECK(!P(t, a, err) && err.indexOf("Codice troppo lungo") >= 0); }
  { std::string t = good() + std::string(17000, ' '); CHECK(!P(t, a, err) && err.indexOf("troppo grande") > 0); }
  // 5) icona: solo forme semplici
  const char* okIcons[] = {ICON, "<svg viewBox='0 0 24 24'><circle cx='12' cy='12' r='9'/><rect x='5' y='5' width='4' height='4'/><g><line x1='1' y1='1' x2='2' y2='2'/></g></svg>"};
  for (const char* i : okIcons) CHECK(appIconSafe(String(i)));
  const char* badIcons[] = {
    "<svg><script>alert(1)</script></svg>", "<svg onload=\"x()\"><path d=\"M0 0\"/></svg>", "<svg><path onclick=x d=\"M0 0\"/></svg>",
    "<svg><path d=\"M0 0\" ONCLICK=\"x\"/></svg>", "<svg><a href=\"http://x\"><path d=\"M0 0\"/></a></svg>", "<svg><image href=\"x.png\"/></svg>",
    "<svg><use xlink:href=\"#a\"/></svg>", "<svg><style>*{}</style><path d=\"M0 0\"/></svg>", "<svg><path style=\"fill:red\" d=\"M0 0\"/></svg>",
    "<svg><foreignObject><div/></foreignObject></svg>", "<svg><path fill=\"url(#a)\" d=\"M0 0\"/></svg>", "<svg><path d=\"M0 0\"/>&#x3c;</svg>",
    "<svg><!-- x --><path d=\"M0 0\"/></svg>", "<!DOCTYPE svg><svg><path d=\"M0 0\"/></svg>", "<?xml version=\"1.0\"?><svg><path d=\"M0 0\"/></svg>",
    "<svg><path d=\"M0 0\"", "<svg>", "x<svg><path d=\"M0 0\"/></svg>", "<svg><path d=\"M0 0\"/></svg>x", "<svg><path d=\"javascript:1\"/></svg>",
    "<svg><circle r=\"1\" fill=\"data:text/html,x\"/></svg>", "<svg><iframe/></svg>", "<svg><animate attributeName=\"x\"/></svg>", "<svg><set/></svg>"};
  for (const char* i : badIcons) { bool r = appIconSafe(String(i)); if (r) printf("  icona accettata: %s\n", i); CHECK(!r); }
  CHECK(!P(rep(good(), ICON, "<svg><script>1</script></svg>"), a, err) && err.indexOf("Icona non ammessa") == 0);
  { std::string big = "<svg>"; while (big.size() < 2100) big += "<path d=\"M0 0\"/>"; big += "</svg>"; CHECK(!appIconSafe(String(big.c_str()))); }
  // 6) elenco, informazioni (testo e JSON)
  g_fs.clear();
  g_fs["/apps/lamp.vap"] = good("lamp");
  g_fs["/apps/rotta.vap"] = rep(good("rotta"), "[code]", "[codice]");
  g_fs["/apps/nome.vap"] = good("altro");                   // name diverso dal file
  g_fs["/apps/vecchio.be"] = "print(1)";                    // non e .vap
  g_fs["/apps/Bad Name.vap"] = "x";
  String lt = appListText();
  CHECK(lt.indexOf("App in /apps: 4") == 0 && lt.indexOf("lamp  Lampada  v1.2  OK") > 0 && lt.indexOf("rotta  ERRORE: Riga") > 0 && lt.indexOf("nome  ERRORE") > 0 && lt.indexOf("vecchio") < 0);
  String it = appInfoText("LAMP.vap");
  CHECK(it.indexOf("Lampada (lamp) v1.2 - Mimmo") == 0 && it.indexOf("Permessi: led,gpio:5,gpio:6") > 0 && it.indexOf("Controlli: 6") > 0 && it.indexOf("switch power  Accesa") > 0 && it.indexOf("Dati iniziali: 2") > 0 && it.indexOf("Icona: si") > 0);
  CHECK(appInfoText("rotta").indexOf("ERRORE: Riga") == 0 && appInfoText("../x").indexOf("Nome app non valido") == 0 && appInfoText("manca").indexOf("ERRORE: File non trovato") == 0);
  String lj = appListJson();
  CHECK(lj.startsWith("{\"ok\":true,\"max\":16384,\"more\":false,\"list\":[") && lj.endsWith("]}"));
  CHECK(lj.indexOf("{\"n\":\"lamp\",\"ok\":true,\"t\":\"Lampada\",\"v\":\"1.2\",\"a\":\"Mimmo\",\"w\":\"oper\",\"u\":\"led,gpio:5,gpio:6\",\"c\":6,\"s\":") > 0);
  CHECK(lj.indexOf("\"icon\":\"<svg viewBox=\\\"0 0 24 24\\\">") > 0 && lj.indexOf("{\"n\":\"rotta\",\"ok\":false,\"e\":\"Riga ") > 0 && lj.indexOf("vecchio") < 0 && lj.indexOf("{\"n\":\"Bad Name\",\"ok\":false,\"e\":\"Nome app non valido\"}") > 0);
  String ij = appInfoJson("lamp");
  CHECK(ij.startsWith("{\"ok\":true,\"n\":\"lamp\"") && ij.indexOf("{\"k\":\"slider\",\"id\":\"lum\",\"l\":\"Luminosita\",\"mn\":0,\"mx\":255}") > 0 && ij.indexOf("{\"k\":\"chart\",\"id\":\"temp\",\"l\":\"Temperatura\",\"p\":60}") > 0 && ij.indexOf("\"data\":2") > 0);
  CHECK(appInfoJson("manca").indexOf("\"ok\":false") == 1 && appInfoJson("a b").indexOf("Nome app non valido") > 0);
  g_fs.clear(); g_dirOk = false;
  CHECK(appListJson().indexOf("\"list\":[]") > 0 && g_dirOk && appListText().indexOf("Nessuna app") == 0);       // cartella mancante: creata
  for (int i = 0; i < 15; i++) g_fs["/apps/a" + std::to_string(i) + ".vap"] = good(("a" + std::to_string(i)).c_str());
  CHECK(appListJson().indexOf("\"more\":true") > 0);
  // ---- esecuzione: ruoli, avvio, stop, valori ----
  g_fs.clear(); g_dirOk = true;
  g_fs["/apps/lamp.vap"] = good("lamp");                                         // who=oper
  g_fs["/apps/solo.vap"] = rep(good("solo"), "who=oper", "who=admin");             // solo Admin
  g_fs["/apps/rotta.vap"] = "[manifest]\nname=rotta\n[page]\nswitch id=\n";
  { String e;
    // elenco e dettaglio per ruolo
    String lo = appListJson(1), la = appListJson(2);
    CHECK(lo.indexOf("\"n\":\"lamp\"") > 0 && lo.indexOf("solo") < 0 && lo.indexOf("rotta") < 0 && la.indexOf("solo") > 0 && la.indexOf("rotta") > 0);
    CHECK(appInfoJson("lamp", 1).indexOf("\"ok\":true") == 1 && appInfoJson("solo", 1).indexOf("\"code\":\"forbidden\"") > 0 && appInfoJson("solo", 2).indexOf("\"ok\":true") == 1 && appInfoJson("rotta", 1).indexOf("\"ok\":false") == 1);
    // avvio: ruolo, nome, errori
    CHECK(!appStart("solo", e, 1, "t") && e.indexOf("ruolo") > 0 && !g_aRun);
    CHECK(!appStart("rotta", e, 2, "t") && e.indexOf("Riga") == 0 && !g_aRun && !rtLoaded());
    CHECK(!appStart("a b", e, 2, "t") && e == "Nome app non valido" && !appStart("manca", e, 2, "t"));
    CHECK(appStart("LAMP.vap", e, 1, "t") && g_aRun && g_aName == "lamp" && g_aUse == "led,gpio:5,gpio:6" && g_aCode.indexOf("on_set") >= 0 && rtLoaded() && rtName() == "lamp");
    CHECK(!appStart("lamp", e, 2, "t") && e == "App gia in esecuzione" && !appStart("solo", e, 2, "t") && e.indexOf("occupato") >= 0 && g_starts == 1);
    // stato: valori iniziali da [data] (power=0, lum=128), controlli, uscita
    String st = appStateJson("lamp", 1);
    CHECK(st.indexOf("\"run\":true") > 0 && st.indexOf("\"out\":\"ciao\"") > 0 && st.indexOf("{\"id\":\"power\",\"n\":0}") > 0 && st.indexOf("{\"id\":\"lum\",\"n\":128}") > 0 && st.indexOf("{\"id\":\"col\",\"s\":\"000000\"}") > 0 && st.indexOf("\"a\":[]") > 0);
    CHECK(appStateJson("solo", 1).indexOf("forbidden") > 0 && appStateJson("solo", 2).indexOf("\"run\":false") > 0 && appStateJson("solo", 2).indexOf("\"vals\":[]") > 0);
    // comandi dalla pagina: solo se l'app e in corso, con il ruolo giusto, valori controllati
    CHECK(appSet("lamp", "power", "1", e, 1) && !appSet("lamp", "lum", "999", e, 1) && !appSet("solo", "power", "1", e, 1) && e.indexOf("ruolo") > 0);
    CHECK(!appSet("lamp", "temp", "5", e, 2) && !appSet("lamp", "zzz", "1", e, 2) && !appSet("manca", "power", "1", e, 2));
    { bool n; long v; char sv[41]; char id[13]; CHECK(rtGet("power", n, v, sv, sizeof(sv)) && v == 1);
      CHECK(rtPop(id, n, v, sv, sizeof(sv)) && String(id) == "power" && n && v == 1 && !rtPop(id, n, v, sv, sizeof(sv))); }
    // coda piena: l'ultimo comando e rifiutato, non perso in silenzio
    int okn = 0; for (int i = 0; i < 12; i++) if (appSet("lamp", "lum", String(i).c_str(), e, 2)) okn++;
    CHECK(okn == RT_EVQ && e.indexOf("occupata") > 0);
    // stop: ruolo, nome, stato
    CHECK(!appStop("lamp", e, 1) == false && !g_aRun);
    CHECK(!appStop("lamp", e, 2) && e == "L'app non e in esecuzione" && !appSet("lamp", "power", "1", e, 2) && e == "L'app non e in esecuzione");
    CHECK(appStateJson("lamp", 2).indexOf("\"run\":false") > 0 && appStateJson("lamp", 2).indexOf("{\"id\":\"lum\",\"n\":") > 0);   // i valori restano dopo lo stop
    CHECK(appStart("solo", e, 2, "t") && !appStop("solo", e, 1) && e.indexOf("ruolo") > 0 && g_aRun && appStop("solo", e, 2));            // l'operatore non ferma un'app solo Admin
    g_busyOther = true; CHECK(!appStart("lamp", e, 2, "t") && e.indexOf("occupato") >= 0); g_busyOther = false;
    CHECK(appRunText() == "uscita\n");
  }
  // le app di esempio in docs/esempi/app devono passare il vero controllo (cosi ogni nuova demo e validata)
  {
    std::string dir = std::string(getenv("VOS_REPO") ? getenv("VOS_REPO") : ".") + "/docs/esempi/app";
    DIR* d = opendir(dir.c_str()); CHECK(d != NULL);
    int found = 0;
    while (d) {
      struct dirent* de = readdir(d); if (!de) break;
      std::string fn = de->d_name; if (fn.size() < 5 || fn.compare(fn.size() - 4, 4, ".vap") != 0) continue;
      std::string tx; FILE* f = fopen((dir + "/" + fn).c_str(), "rb"); CHECK(f != NULL);
      if (f) { char b[512]; size_t n; while ((n = fread(b, 1, sizeof b, f)) > 0) tx.append(b, n); fclose(f); }
      AppInfo* X = new AppInfo(); String ee;
      bool ok = P(tx, *X, ee, fn.substr(0, fn.size() - 4).c_str());
      if (!ok) printf("esempio %s: %s\n", fn.c_str(), ee.c_str());
      CHECK(ok && X->name == String(fn.substr(0, fn.size() - 4).c_str()));
      delete X; found++;
    }
    if (d) closedir(d);
    CHECK(found >= 1);
  }
  delete A;
  return done("test_app");
}
