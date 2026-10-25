// VesevOS - prova sul computer: gestione file e cartelle (core/vos_files.cpp), scritta per il gruppo 6 della migrazione delle String.
// NON si chiama test_*: la lancia tools/hosttest/prova_files.sh (che run.sh chiama alla fine), perche serve un file system finto CON CARTELLE
// (tools/hosttest/dirfs/FS.h al posto di tools/stub/FS.h), che controlla anche: nessuna voce cancellata mentre una cartella e aperta per
// scorrerla, nessuna apertura lasciata senza close.
// Non si prova qui: la memoria finita (malloc nullo), lo stack del web server e il taglio della corrente (serve la scheda).
#include <Arduino.h>
#include <stdlib.h>
#include <string>
#include <LittleFS.h>
#include "vos_files.h"
#include "vos_util.h"
#include "vos_drv_fs.h"
#include "mini.h"

DirFsFaults g_df;
namespace fs {
std::map<std::string, DNode> g_tree;
std::string dfParent(const std::string& p) { size_t s = p.rfind('/'); return s == 0 || s == std::string::npos ? "/" : p.substr(0, s); }
bool dfIsDir(const std::string& p) { if (p == "/") return true; auto it = g_tree.find(p); return it != g_tree.end() && it->second.dir; }
std::vector<std::string> dfKids(const std::string& dir) {
  std::vector<std::string> k;
  std::string pre = dir == "/" ? "/" : dir + "/";
  for (auto& kv : g_tree) {
    if (kv.first.size() > pre.size() && kv.first.compare(0, pre.size(), pre) == 0 && kv.first.find('/', pre.size()) == std::string::npos) k.push_back(kv.first.substr(pre.size()));
  }
  return k;
}
}

static void reset() { fs::g_tree.clear(); g_df = DirFsFaults(); }
static void dirp(const char* p) { fs::g_tree[p].dir = true; }
static void put(const std::string& p, const std::string& d) { fs::g_tree[p].dir = false; fs::g_tree[p].data = d; }
static bool has(const std::string& p) { return fs::g_tree.count(p) > 0; }
static std::string txt(const std::string& p) { auto it = fs::g_tree.find(p); return it == fs::g_tree.end() ? std::string("<manca>") : it->second.data; }
static std::string S(const String& s) { return std::string(s.c_str(), s.length()); }
static std::string list(const char* p) { return S(fsListJson(String(p))); }
static bool clean(const char* in, std::string& out) { String c = fsClean(String(in)); out = S(c); return c.length() > 0; }
static bool clean(const std::string& in, std::string& out) { String c = fsClean(String(in.data(), (unsigned)in.size())); out = S(c); return c.length() > 0; }
// nessuna apertura rimasta e nessuna cartella modificata mentre la si scorreva
static bool clean_fs() { return g_df.open == 0 && !g_df.mutatedWhileOpen; }

// il codice di prima (con String), come riferimento per la pulizia del percorso
static String oldClean(const String& in) {
  String p = in; p.trim();
  if (p.length() == 0) p = "/";
  if (p[0] != '/') p = "/" + p;
  if (p.length() > 100) return "";
  if (p.indexOf("..") >= 0 || p.indexOf("//") >= 0 || p.indexOf('\\') >= 0) return "";
  for (size_t i = 0; i < p.length(); i++) { unsigned char c = (unsigned char)p[i]; if (c < 32 || c >= 127) return ""; }
  if (p.length() > 1 && p[p.length() - 1] == '/') p.remove(p.length() - 1);
  return p;
}

// controlla che il testo sia un JSON ben formato (parentesi bilanciate fuori dalle stringhe, stringhe chiuse)
static bool jsonOk(const std::string& j) {
  std::string st; bool in = false;
  for (size_t i = 0; i < j.size(); i++) {
    char c = j[i];
    if (in) { if (c == '\\') i++; else if (c == '"') in = false; continue; }
    if (c == '"') in = true;
    else if (c == '{' || c == '[') st += c;
    else if (c == '}') { if (st.empty() || st.back() != '{') return false; st.pop_back(); }
    else if (c == ']') { if (st.empty() || st.back() != '[') return false; st.pop_back(); }
  }
  return !in && st.empty() && !j.empty();
}
static int countOf(const std::string& s, const char* sub) { int n = 0; for (size_t p = s.find(sub); p != std::string::npos; p = s.find(sub, p + 1)) n++; return n; }

int main() {
  String e;
  // ---- pulizia del percorso: casi scelti ----
  std::string o;
  CHECK(clean("", o) && o == "/");
  CHECK(clean("   ", o) && o == "/");
  CHECK(clean("a", o) && o == "/a");
  CHECK(clean("/a/", o) && o == "/a");
  CHECK(clean(" /a/b/ ", o) && o == "/a/b");
  CHECK(clean("\t/a\r\n", o) && o == "/a");
  CHECK(clean("/", o) && o == "/");
  CHECK(!clean("/a//b", o) && o == "");
  CHECK(!clean("//", o));
  CHECK(!clean("/a/..", o));
  CHECK(!clean("/..a", o));
  CHECK(!clean("a..b", o));
  CHECK(clean("/a.b/c.d", o) && o == "/a.b/c.d");
  CHECK(!clean("/a\\b", o));
  CHECK(!clean("/a\x01" "b", o));
  CHECK(!clean("/a\x7f", o));
  CHECK(!clean("/caff\xC3\xA8", o));                       // non ASCII
  CHECK(clean("/" + std::string(99, 'x'), o) && o.size() == 100);
  CHECK(!clean("/" + std::string(100, 'x'), o));           // 101
  CHECK(!clean(std::string(100, 'x'), o));                 // senza barra iniziale diventa 101
  CHECK(clean(std::string(99, 'x'), o) && o.size() == 100);
  CHECK(clean(std::string(500, ' ') + "/a", o) && o == "/a");   // gli spazi ai lati non contano nella lunghezza
  { std::string z("/a\0../b", 7); CHECK(!clean(z, o)); }     // uno zero dentro non tronca in silenzio: si rifiuta
  { std::string z("/a\0", 3); CHECK(!clean(z, o)); }
  { char small[100]; CHECK(!fsCleanTo("/a", small, sizeof small)); }   // buffer troppo piccolo
  { char buf[FS_PATH_MAX + 1]; CHECK(fsCleanTo("a/b/", buf, sizeof buf) && !strcmp(buf, "/a/b")); }
  // ---- pulizia del percorso: 300000 testi a caso contro il codice di prima ----
  {
    const char alpha[] = {'/', '/', '.', '.', 'a', 'b', 'x', ' ', '\\', '\t', '\0', '\x01', '\xC3', '\xA9', '-', '_', '~'};
    unsigned seed = 12345; int bad = 0;
    auto rnd = [&]() { seed = seed * 1103515245u + 12345u; return (seed >> 16) & 0x7fff; };
    for (int it = 0; it < 300000; it++) {
      size_t n = rnd() % (it % 50 == 0 ? 130 : 12);
      std::string s; for (size_t i = 0; i < n; i++) s += alpha[rnd() % sizeof alpha];
      String in(s.data(), (unsigned)s.size());
      String want = oldClean(in);
      std::string got = S(fsClean(in));
      if (got != S(want)) { if (++bad < 4) printf("  diverso: \"%s\" len %zu\n", s.c_str(), s.size()); }
    }
    CHECK(bad == 0);
  }
  // ---- file di sistema ----
  CHECK(fsProtected("/vesevos.conf") && fsProtected("/vesevos.conf.bak") && fsProtected("/vesevos.conff"));
  CHECK(fsProtected("/audit.log") && fsProtected("/crash.txt"));
  CHECK(!fsProtected("/audit.log2") && !fsProtected("/x/audit.log") && !fsProtected("/") && !fsProtected("/vesevos.con"));
  CHECK(fsProtected(String("/crash.txt")) && !fsProtected(String("/a")));

  // ---- HeapBuf ----
  {
    HeapBuf b(10);
    CHECK(b.add("abcdef") && b.length() == 6 && !strcmp(b.c_str(), "abcdef"));
    CHECK(!b.add("123456") && b.over() && !b.oom() && b.length() == 6 && !strcmp(b.c_str(), "abcdef"));   // non scrive niente
    CHECK(b.add("1234") && b.length() == 10 && !strcmp(b.c_str(), "abcdef1234"));
    HeapBuf u(100); CHECK(u.addUint(0) && u.addUint(4294967295u) && !strcmp(u.c_str(), "04294967295"));
    HeapBuf big(100000); std::string ref; for (int i = 0; i < 20000; i++) { big.add("0123456789"); ref += "0123456789"; if (i == 9999) break; }
    CHECK(big.length() == ref.size() && ref == big.c_str());    // crescita a raddoppio, contenuto intero
    HeapBuf em(0); CHECK(em.length() == 0 && !strcmp(em.c_str(), "") && em.add("", 0));
    // addJson uguale a jsonEscape su tutti i byte
    bool same = true;
    for (int c = 1; c < 256; c++) { char t[2] = {(char)c, 0}; HeapBuf h(100); h.addJson(t, 1); if (S(jsonEscape(String(t))) != h.c_str()) same = false; }
    CHECK(same);
  }

  // ---- elenco di una cartella ----
  reset();
  put("/a.txt", "abc"); put("/vesevos.conf", "SEGRETO"); put("/audit.log", "xx"); put("/crash.txt", "yy");
  dirp("/d"); put("/d/x", std::string(10, 'x')); dirp("/d/e"); put("/d/e/y", std::string(100, 'y'));
  {
    std::string j = list("/");
    char want[400];
    snprintf(want, sizeof want, "{\"ok\":true,\"path\":\"/\",\"list\":[{\"n\":\"a.txt\",\"d\":false,\"s\":3},{\"n\":\"d\",\"d\":true,\"s\":110}],\"more\":false,\"used\":%u,\"total\":%u,\"max\":32768}", (unsigned)drvFsUsed(), (unsigned)drvFsTotal());
    CHECK(j == want);
    CHECK(j.find("vesevos") == std::string::npos && j.find("audit") == std::string::npos && j.find("crash") == std::string::npos);   // i file di sistema non si vedono
    CHECK(jsonOk(j));
    g_df.fullNames = true; CHECK(list("/") == want); g_df.fullNames = false;       // name() col percorso intero: stesso risultato
    CHECK(list("/d").find("\"path\":\"/d\",\"list\":[{\"n\":\"e\",\"d\":true,\"s\":100},{\"n\":\"x\",\"d\":false,\"s\":10}]") != std::string::npos);
    CHECK(list("d/").find("\"path\":\"/d\"") != std::string::npos && list("").find("\"path\":\"/\"") != std::string::npos);
    CHECK(list("/a/../d") == "{\"ok\":false,\"err\":\"Percorso non valido\"}");
    CHECK(list("/nope") == "{\"ok\":false,\"err\":\"Cartella non trovata\"}");
    CHECK(list("/a.txt") == "{\"ok\":false,\"err\":\"Cartella non trovata\"}");
    { std::string z("/d\0/..", 6); CHECK(S(fsListJson(String(z.data(), 6))) == "{\"ok\":false,\"err\":\"Percorso non valido\"}"); }
    // la stessa cosa scritta nel buffer del chiamante
    HeapBuf hb(FS_JSON_MAX); CHECK(fsListBuild("/d", hb) && std::string(hb.c_str()) == list("/d"));
    HeapBuf eb(FS_JSON_MAX); CHECK(fsListBuild("/x/../y", eb) && std::string(eb.c_str()) == "{\"ok\":false,\"err\":\"Percorso non valido\"}");   // errore di percorso: true, JSON di errore
    CHECK(clean_fs());
  }
  // nomi con virgolette, barre e byte strani (creati da altri, non passano da fsClean): si scrivono come jsonEscape()
  {
    std::string nm = std::string("q\"b\\n\n\t") + "\xC3\xA9" + "\x01z";
    put("/d/" + nm, "k");
    std::string j = list("/d");
    CHECK(jsonOk(j) && j.find("{\"n\":\"" + S(jsonEscape(String(nm.c_str()))) + "\",") != std::string::npos);
    fs::g_tree.erase("/d/" + nm);
  }
  // sottocartelle fino a 8 livelli nelle dimensioni (il nono non conta, come prima)
  {
    reset(); std::string p;
    for (int i = 1; i <= 11; i++) { p += "/l" + std::to_string(i); dirp(p.c_str()); put(p + "/f", "1"); }
    CHECK(list("/").find("{\"n\":\"l1\",\"d\":true,\"s\":8}") != std::string::npos);   // f in l1..l8 (l9 e a profondita 9: 0)
    CHECK(clean_fs());
  }
  // 250 voci: ne escono 200 e more:true
  {
    reset(); dirp("/m");
    for (int i = 0; i < 250; i++) { char n[40]; snprintf(n, sizeof n, "/m/f%03d", i); put(n, "z"); }
    std::string j = list("/m");
    CHECK(jsonOk(j) && countOf(j, "{\"n\":") == 200 && j.find("\"more\":true") != std::string::npos && j.find("f199") != std::string::npos && j.find("f200") == std::string::npos);
    reset(); dirp("/m"); for (int i = 0; i < 200; i++) { char n[40]; snprintf(n, sizeof n, "/m/f%03d", i); put(n, "z"); }
    j = list("/m"); CHECK(countOf(j, "{\"n\":") == 200 && j.find("\"more\":false") != std::string::npos);
  }
  // nomi lunghissimi: il tetto di 40 KB ferma l'elenco prima delle 200 voci, il JSON resta valido
  {
    reset(); dirp("/m");
    for (int i = 0; i < 199; i++) { char n[8]; snprintf(n, sizeof n, "%03d", i); put("/m/" + std::string(n) + std::string(250, 'k'), "z"); }
    std::string j = list("/m");
    int n = countOf(j, "{\"n\":");
    CHECK(jsonOk(j) && n > 100 && n < 199 && j.find("\"more\":true") != std::string::npos && j.size() < FS_JSON_MAX);
    CHECK(clean_fs());
  }

  // ---- elenco di tutte le cartelle ----
  reset(); put("/f", "1"); dirp("/a"); dirp("/a/b"); dirp("/c"); put("/a/g", "2"); dirp("/a/b/\"q");
  { HeapBuf j(FS_DIRS_MAX); CHECK(fsDirsBuild(j) && std::string(j.c_str()) == "[\"/\",\"/a\",\"/a/b\",\"/a/b/\\\"q\",\"/c\"]"); g_df.fullNames = true; HeapBuf k(FS_DIRS_MAX); CHECK(fsDirsBuild(k) && std::string(k.c_str()) == j.c_str()); g_df.fullNames = false; }
  reset();
  for (int i = 0; i < 80; i++) { char n[16]; snprintf(n, sizeof n, "/d%02d", i); dirp(n); }
  { HeapBuf j(FS_DIRS_MAX); CHECK(fsDirsBuild(j) && jsonOk(j.c_str()) && countOf(j.c_str(), "\"/d") == 64); }   // al massimo 64 cartelle
  CHECK(clean_fs());

  // ---- crea cartella ----
  reset(); dirp("/a");
  CHECK(fsMkdir("/a/b", e) && fs::g_tree["/a/b"].dir);
  CHECK(!fsMkdir("/a/b", e) && e == "Esiste gia");
  CHECK(!fsMkdir("/x/../y", e) && e == "Percorso non valido");
  CHECK(!fsMkdir("/vesevos.conf2", e) && e == "File di sistema protetto");
  CHECK(!fsMkdir("/no/dir", e) && e == "Impossibile creare la cartella");
  CHECK(fsMkdir("nuova/", e) && has("/nuova"));

  // ---- cancella ----
  reset(); put("/keep", "k"); dirp("/t");
  for (int i = 0; i < 60; i++) { char n[24]; snprintf(n, sizeof n, "/t/f%02d", i); put(n, "x"); }     // piu di un lotto da 24 di prima
  for (int s = 0; s < 5; s++) { char d[16]; snprintf(d, sizeof d, "/t/s%d", s); dirp(d); for (int i = 0; i < 5; i++) put(std::string(d) + "/g" + std::to_string(i), "y"); dirp((std::string(d) + "/n").c_str()); put(std::string(d) + "/n/h", "z"); }
  CHECK(fsRemove("/t", e) && !has("/t") && !has("/t/f00") && !has("/t/s4/n/h") && !has("/t/s4/n") && has("/keep"));
  CHECK(clean_fs() && fs::g_tree.size() == 1);
  g_df.fullNames = true; dirp("/u"); dirp("/u/v"); put("/u/v/w", "1"); put("/u/z", "2");
  CHECK(fsRemove("/u", e) && !has("/u") && !has("/u/v/w")); g_df.fullNames = false;                 // anche col percorso intero in name()
  put("/solo", "s"); CHECK(fsRemove("/solo", e) && !has("/solo"));
  CHECK(!fsRemove("/non-c-e", e) && e == "Non trovato");
  CHECK(!fsRemove("/", e) && e == "Non si puo eliminare la radice");
  CHECK(!fsRemove("/vesevos.conf", e) && e == "File di sistema protetto");
  CHECK(!fsRemove("/a/../b", e) && e == "Percorso non valido");
  dirp("/e0"); CHECK(fsRemove("/e0", e) && !has("/e0"));                                              // cartella vuota
  // una voce che non si cancella ferma tutto
  dirp("/p"); put("/p/a", "1"); put("/p/b", "2"); g_df.failRemove = "/p/b";
  CHECK(!fsRemove("/p", e) && e == "Eliminazione non riuscita" && has("/p/b") && !has("/p/a")); g_df.failRemove = "";
  // oltre 8 livelli: si ferma, i livelli 1..9 sono svuotati dei file e il decimo resta
  { reset(); std::string p; for (int i = 1; i <= 12; i++) { p += "/l" + std::to_string(i); dirp(p.c_str()); put(p + "/f", "1"); }
    CHECK(!fsRemove("/l1", e) && e == "Eliminazione non riuscita" && !has("/l1/f") && !has("/l1/l2/l3/l4/l5/l6/l7/l8/l9/f") && has("/l1/l2/l3/l4/l5/l6/l7/l8/l9/l10/f")); }
  CHECK(clean_fs());

  // ---- rinomina ----
  reset(); put("/a", "A"); put("/b", "B"); dirp("/d"); put("/d/x", "X"); dirp("/vuota");
  CHECK(fsRename("/a", "/c", e) && !has("/a") && txt("/c") == "A");
  CHECK(!fsRename("/c", "/b", e) && e == "La destinazione esiste gia" && txt("/b") == "B");
  CHECK(!fsRename("/zz", "/y", e) && e == "Origine non trovata");
  CHECK(fsRename("/d", "/dd", e) && !has("/d") && txt("/dd/x") == "X");
  CHECK(!fsRename("/c", "/vesevos.conf", e) && e == "File di sistema protetto");
  CHECK(!fsRename("/vesevos.conf", "/c2", e) && e == "File di sistema protetto");
  CHECK(!fsRename("/c", "/q/../r", e) && e == "Percorso non valido");
  CHECK(!fsRename("/c", "/nodir/z", e) && e == "Rinomina non riuscita");
  CHECK(fsRename(" c ", "/da spazi", e) && txt("/da spazi") == "A");
  CHECK(clean_fs());

  // ---- copia ----
  reset(); { std::string big; for (int i = 0; i < 1500; i++) big += (char)('a' + i % 26); put("/g", big); put("/zero", ""); dirp("/dir"); put("/pieno", "p");
    CHECK(fsCopy("/g", "/g2", e) && txt("/g2") == big);
    CHECK(fsCopy("/zero", "/zero2", e) && has("/zero2") && txt("/zero2") == "");
    CHECK(!fsCopy("/g", "/g2", e) && e == "La destinazione esiste gia");
    CHECK(!fsCopy("/dir", "/dir2", e) && e == "Origine non e un file");
    CHECK(!fsCopy("/nope", "/n2", e) && e == "Origine non e un file");
    CHECK(!fsCopy("/g", "/vesevos.conf", e) && e == "File di sistema protetto");
    CHECK(!fsCopy("/g", "/no/dir/x", e) && e == "Impossibile creare la destinazione");
    g_df.full = true; CHECK(!fsCopy("/g", "/g3", e) && e == "Spazio esaurito" && !has("/g3")); g_df.full = false;    // disco pieno a meta: la copia a meta si toglie
    g_df.total = 5000; CHECK(!fsCopy("/g", "/g4", e) && e == "Spazio esaurito" && !has("/g4")); g_df.total = 64 * 1024;   // poco spazio libero: nemmeno si prova
    CHECK(clean_fs()); }

  // ---- scrittura sicura ----
  reset(); dirp("/d");
  CHECK(fsWriteText("/d/n.txt", "uno", e) && txt("/d/n.txt") == "uno" && !has("/d/n.txt.tmp~"));
  CHECK(fsWriteText("/d/n.txt", "due", e) && txt("/d/n.txt") == "due" && !has("/d/n.txt.tmp~"));      // sostituisce il vecchio
  CHECK(!fsWriteText("/d/n.txt", "tre", e, true) && e == "Esiste gia" && txt("/d/n.txt") == "due");
  CHECK(fsWriteText("/d/nuovo", "x", e, true) && txt("/d/nuovo") == "x");
  CHECK(!fsWriteText("/", "x", e) && e == "Nome file mancante");
  CHECK(!fsWriteText("/a.tmp~", "x", e) && e == "Nome non valido");
  CHECK(!fsWriteText("/d", "x", e) && e == "Esiste una cartella con questo nome");
  CHECK(!fsWriteText("/no/x", "x", e) && e == "Cartella non trovata");
  CHECK(!fsWriteText("/x/../y", "x", e) && e == "Percorso non valido");
  CHECK(!fsWriteText("/vesevos.conf", "x", e) && e == "File di sistema protetto" && !has("/vesevos.conf"));
  CHECK(fsWriteText("/w", String(std::string(32768, 'q').c_str()), e) && txt("/w").size() == 32768);
  CHECK(!fsWriteText("/w2", String(std::string(32769, 'q').c_str()), e) && e == "Testo troppo lungo (max 32 KB)" && !has("/w2"));
  CHECK(fsWriteText("/acc", "caff\xC3\xA8 \xE2\x82\xAC", e) && txt("/acc") == "caff\xC3\xA8 \xE2\x82\xAC");       // UTF-8 com'e
  g_df.full = true; CHECK(!fsWriteText("/d/n.txt", "quattro", e) && e == "Spazio esaurito" && txt("/d/n.txt") == "due" && !has("/d/n.txt.tmp~")); g_df.full = false;   // disco pieno: il vecchio resta
  g_df.total = drvFsUsed() + 4000; CHECK(!fsWriteText("/zz", "x", e) && e == "Spazio esaurito"); g_df.total = 64 * 1024;
  // sovrascrivere = un solo rename sopra il vecchio, senza cancellarlo prima (come lfs_rename: stesso tipo, sostituzione in un passo)
  { int rm0 = g_df.removes, rn0 = g_df.renames;
    CHECK(fsWriteText("/d/n.txt", "sei", e) && txt("/d/n.txt") == "sei" && !has("/d/n.txt.tmp~"));
    CHECK(g_df.removes == rm0 && g_df.renames == rn0 + 1); }                                  // nessuna remove del vecchio: la finestra "file mancante" non esiste piu
  // se lo scambio finale fallisce (es. file aperto da un altro), il vecchio resta intero e il temporaneo si toglie
  g_df.failRename = "/d/n.txt"; CHECK(!fsWriteText("/d/n.txt", "cinque", e) && e == "Impossibile scrivere"); g_df.failRename = "";
  CHECK(txt("/d/n.txt") == "sei" && !has("/d/n.txt.tmp~"));
  CHECK(clean_fs());
  // taglio di corrente simulato tra la scrittura del temporaneo e la rinomina: il vecchio contenuto e intero
  put("/d/n.txt.tmp~", "mezzo");
  CHECK(txt("/d/n.txt") == "sei");
  CHECK(fsWriteText("/d/n.txt", "sette", e) && txt("/d/n.txt") == "sette" && !has("/d/n.txt.tmp~"));   // al salvataggio dopo il riavvio il temporaneo avanzato si riscrive
  // destinazione = cartella: rifiutata prima di scrivere, niente temporaneo
  CHECK(!fsWriteText("/d", "x", e) && e == "Esiste una cartella con questo nome" && !has("/d.tmp~"));
  CHECK(clean_fs());

  // ---- tetto di voci per cartella (FS_DIR_MAX = 20) ----
  {
    const char* full = "Cartella piena: al massimo 20 voci. Usa una sottocartella";
    CHECK(FS_DIR_MAX == 20);
    reset(); dirp("/c");
    for (int i = 0; i < 19; i++) put("/c/f" + std::to_string(i), "x");
    CHECK(fsWriteText("/c/f19", "x", e) && has("/c/f19"));                         // la ventesima voce entra
    CHECK(!fsWriteText("/c/f20", "x", e) && e == full && !has("/c/f20") && !has("/c/f20.tmp~"));   // la ventunesima no, e non resta nulla a meta
    CHECK(!fsMkdir("/c/sub", e) && e == full && !has("/c/sub"));
    CHECK(fsWriteText("/c/f3", "nuovo", e) && txt("/c/f3") == "nuovo");            // sovrascrivere non aggiunge voci: si puo
    CHECK(!fsWriteText("/c/f3", "x", e, true) && e == "Esiste gia");               // "solo nuovo" su uno che c'e: dice che esiste, non che e piena
    CHECK(!fsCopy("/c/f0", "/c/copia", e) && e == full && !has("/c/copia"));
    CHECK(fsRename("/c/f0", "/c/f0b", e) && has("/c/f0b") && !has("/c/f0"));   // rinominare nella stessa cartella: ok anche se piena
    CHECK(clean_fs());
    // da un'altra cartella dentro quella piena: no; verso una con posto: si
    put("/altro", "A"); dirp("/libera");
    CHECK(!fsRename("/altro", "/c/z", e) && e == full && has("/altro") && !has("/c/z"));
    CHECK(fsRename("/altro", "/libera/z", e) && has("/libera/z"));
    CHECK(fsRename("/c/f1", "/libera/f1", e) && !has("/c/f1"));                    // una voce in meno: ora c'e posto
    CHECK(fsWriteText("/c/ancora", "x", e) && has("/c/ancora"));
    // sottocartelle e file contano allo stesso modo
    reset(); dirp("/m");
    for (int i = 0; i < 10; i++) { put("/m/f" + std::to_string(i), "x"); dirp(("/m/d" + std::to_string(i)).c_str()); }
    CHECK(!fsMkdir("/m/nuova", e) && e == full);
    // la radice: tetto uguale, i file di sistema contano
    reset(); put("/vesevos.conf", "c"); put("/audit.log", "a"); put("/crash.txt", "k");
    for (int i = 0; i < 17; i++) put("/r" + std::to_string(i), "x");
    CHECK(!fsWriteText("/ultimo", "x", e) && e == full && !has("/ultimo"));
    CHECK(!fsMkdir("/cartella", e) && e == full);
    // fsRoomFor (usata dal caricamento): nullptr = si. Esiste -> si; nuovo in cartella piena -> errore; cartella che non c'e -> si (verra creata)
    CHECK(fsRoomFor("/r1") == nullptr && !strcmp(fsRoomFor("/nuovo"), full));
    reset(); dirp("/c"); for (int i = 0; i < 20; i++) put("/c/f" + std::to_string(i), "x");
    CHECK(fsRoomFor("/c/f5") == nullptr && !strcmp(fsRoomFor("/c/zzz"), full) && fsRoomFor("/nodir/x") == nullptr && fsRoomFor("/c2/x/y") == nullptr);
    CHECK(!strcmp(fsRoomFor("/x/../y"), "Percorso non valido") && !strcmp(fsRoomFor("/vesevos.conf"), "File di sistema protetto"));
    // una cartella gia piena di prima (250 voci): non si riempie ancora, ma si legge e si svuota
    reset(); dirp("/m"); for (int i = 0; i < 250; i++) put("/m/f" + std::to_string(i), "z");
    CHECK(!fsWriteText("/m/nuovo", "x", e) && e == full);
    CHECK(list("/m").find("\"more\":true") != std::string::npos);
    CHECK(fsRemove("/m", e) && !has("/m") && !has("/m/f249"));
    CHECK(clean_fs());
  }


  // ---- pulizia dei temporanei .tmp~ (taglio di corrente tra la scrittura e lo scambio) ----
  {
    FsTmpStat st;
    reset(); dirp("/d");
    put("/d/a.txt", "vecchio"); put("/d/a.txt.tmp~", "nuovo a meta");
    fsTmpClean("/d", st);
    CHECK(st.removed == 1 && st.orphan == 0 && st.failed == 0 && !st.more && st.scanned == 2);
    CHECK(!has("/d/a.txt.tmp~") && txt("/d/a.txt") == "vecchio");                  // il definitivo resta com'era
    CHECK(clean_fs());
    // temporaneo senza definitivo: puo essere l'unica copia, si lascia e si segnala
    put("/d/b.txt.tmp~", "unica copia");
    st = FsTmpStat(); fsTmpClean("/d", st);
    CHECK(st.removed == 0 && st.orphan == 1 && has("/d/b.txt.tmp~") && txt("/d/b.txt.tmp~") == "unica copia" && std::string(st.firstOrphan) == "/d/b.txt");
    // definitivo che e una cartella: non e il file vecchio, si lascia
    dirp("/d/e"); put("/d/e.tmp~", "x");
    st = FsTmpStat(); fsTmpClean("/d", st);
    CHECK(st.removed == 0 && st.orphan == 2 && has("/d/e.tmp~") && has("/d/b.txt.tmp~"));
    // nomi strani: UTF-8, spazi, ".tmp~" dentro il nome, e un nome fatto solo di ".tmp~" (non e un temporaneo)
    reset(); dirp("/d");
    put("/d/caff\xC3\xA8 uno.txt", "v"); put("/d/caff\xC3\xA8 uno.txt.tmp~", "t");
    put("/d/x.tmp~", "v"); put("/d/x.tmp~.tmp~", "t");
    put("/d/.tmp~", "solo"); dirp("/d/dir.tmp~");
    st = FsTmpStat(); fsTmpClean("/d", st);
    CHECK(st.removed == 2 && !has("/d/caff\xC3\xA8 uno.txt.tmp~") && !has("/d/x.tmp~.tmp~") && has("/d/x.tmp~") && has("/d/.tmp~") && has("/d/dir.tmp~"));
    CHECK(st.orphan == 1 && std::string(st.firstOrphan) == "/d/x" && clean_fs());                  // "x.tmp~" e anche il temporaneo di "x", che non c'e: si lascia e si segnala
    // file protetti: nemmeno i loro temporanei si toccano. I temporanei degli altri in radice si, anche /io.cfg.tmp~ se c'e /io.cfg
    reset();
    put("/vesevos.conf", "c"); put("/vesevos.conf.tmp~", "t"); put("/audit.log", "a"); put("/audit.log.tmp~", "t");
    put("/io.cfg", "1"); put("/io.cfg.tmp~", "2");
    st = FsTmpStat(); fsTmpClean("/", st);
    CHECK(st.removed == 1 && !has("/io.cfg.tmp~") && has("/vesevos.conf.tmp~") && has("/audit.log.tmp~") && st.orphan == 0);
    // la cancellazione fallisce: si conta, il temporaneo resta, niente altro cambia
    reset(); dirp("/d"); put("/d/a.txt", "v"); put("/d/a.txt.tmp~", "t"); put("/d/z.txt", "v"); put("/d/z.txt.tmp~", "t");
    g_df.failRemove = "/d/a.txt.tmp~";
    st = FsTmpStat(); fsTmpClean("/d", st);
    CHECK(st.failed == 1 && st.removed == 1 && has("/d/a.txt.tmp~") && !has("/d/z.txt.tmp~") && txt("/d/a.txt") == "v");
    g_df.failRemove = "";
    CHECK(clean_fs());
    // piu temporanei del tetto per giro: se ne trattano FS_TMP_CAND_MAX, il resto si dice e si fa al giro dopo
    reset(); dirp("/f");
    for (int i = 0; i < 10; i++) { put("/f/k" + std::to_string(i), "d"); put("/f/k" + std::to_string(i) + ".tmp~", "t"); }
    st = FsTmpStat(); fsTmpClean("/f", st);
    CHECK(st.removed == FS_TMP_CAND_MAX && st.more);
    st = FsTmpStat(); fsTmpClean("/f", st);
    CHECK(st.removed == 10 - FS_TMP_CAND_MAX && !st.more);
    for (int i = 0; i < 10; i++) CHECK(!has("/f/k" + std::to_string(i) + ".tmp~") && has("/f/k" + std::to_string(i)));
    CHECK(clean_fs());
    // cartella piena di prima (45 voci): si guardano solo FS_TMP_SCAN_MAX voci; quello che sta oltre resta e si dice "more"
    reset(); dirp("/g");
    for (int i = 0; i < 45; i++) { char n[16]; snprintf(n, sizeof(n), "/g/f%02d", i); put(n, "x"); }
    put("/g/zz", "d"); put("/g/zz.tmp~", "t");
    st = FsTmpStat(); fsTmpClean("/g", st);
    CHECK(st.more && st.removed == 0 && has("/g/zz.tmp~") && st.scanned == FS_TMP_SCAN_MAX);
    CHECK(clean_fs());
    // nome troppo lungo per un percorso nostro: non si tocca
    reset(); dirp("/d");
    put("/d/" + std::string(150, 'n'), "v"); put("/d/" + std::string(150, 'n') + ".tmp~", "t");
    st = FsTmpStat(); fsTmpClean("/d", st);
    CHECK(st.removed == 0 && st.more && has("/d/" + std::string(150, 'n') + ".tmp~"));
    // percorso intero in name() (alcune versioni del core) e cartella che non c'e
    reset(); dirp("/d"); put("/d/a", "v"); put("/d/a.tmp~", "t");
    g_df.fullNames = true; st = FsTmpStat(); fsTmpClean("/d", st); g_df.fullNames = false;
    CHECK(st.removed == 1 && !has("/d/a.tmp~"));
    st = FsTmpStat(); fsTmpClean("/nodir", st); CHECK(st.scanned == 0 && st.removed == 0 && !st.more);
    st = FsTmpStat(); fsTmpClean("/d/a", st); CHECK(st.scanned == 0 && st.removed == 0);      // un file al posto di una cartella
    CHECK(clean_fs());
    // le cartelle note: radice, /scripts, /apps, /lang; un'altra cartella non si guarda
    reset(); dirp("/scripts"); dirp("/apps"); dirp("/lang"); dirp("/altro");
    const char* dd[] = {"/p", "/scripts/p", "/apps/p", "/lang/p", "/altro/p"};
    for (const char* x : dd) { put(x, "v"); put(std::string(x) + ".tmp~", "t"); }
    st = FsTmpStat(); fsTmpCleanKnown(st);
    CHECK(st.removed == 4 && !has("/p.tmp~") && !has("/scripts/p.tmp~") && !has("/apps/p.tmp~") && !has("/lang/p.tmp~") && has("/altro/p.tmp~"));
    CHECK(clean_fs());
    // un salvataggio dopo la pulizia funziona e lascia la cartella senza temporanei
    String e2;
    CHECK(fsWriteText("/p", "nuovo", e2) && txt("/p") == "nuovo" && !has("/p.tmp~"));
  }

  // ---- lettura ----
  reset(); { std::string big; for (int i = 0; i < 1500; i++) big += (char)('A' + i % 26); put("/t1", big); put("/vuoto", ""); put("/utf", "caff\xC3\xA8 \xE2\x82\xAC\n"); dirp("/dir");
    String out;
    CHECK(fsReadText("/t1", out, e) && S(out) == big);
    CHECK(fsReadText("/vuoto", out, e) && out.length() == 0);
    CHECK(fsReadText("/utf", out, e) && S(out) == "caff\xC3\xA8 \xE2\x82\xAC\n");
    CHECK(!fsReadText("/dir", out, e) && e == "File non trovato");
    CHECK(!fsReadText("/nope", out, e) && e == "File non trovato");
    CHECK(!fsReadText("/vesevos.conf", out, e) && e == "File di sistema protetto");
    CHECK(!fsReadText("/a/../b", out, e) && e == "Percorso non valido");
    std::string bin = big; bin[700] = 0; put("/bin", bin);                       // uno zero oltre il primo pezzo da 512
    CHECK(!fsReadText("/bin", out, e) && e == "File binario: non si puo modificare, scaricalo" && out.length() == 0);
    bin = big; bin[3] = 0; put("/bin2", bin); CHECK(!fsReadText("/bin2", out, e) && out.length() == 0);
    put("/max", std::string(32768, 'm')); CHECK(fsReadText("/max", out, e) && out.length() == 32768);
    put("/oltre", std::string(32769, 'm')); CHECK(!fsReadText("/oltre", out, e) && e == "File troppo grande per l'editor (max 32 KB): scaricalo");
    CHECK(fsTextMax() == 32768);
    CHECK(clean_fs()); }
  return done("files");
}
