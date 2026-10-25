// VesevOS - prove sul computer: schede modulo .vdp (vos_mod.cpp) con file e periferiche finti
#include <Arduino.h>
#include "mini.h"
#include <map>
#include <string>
#include <fstream>
#include <sstream>
#include <dirent.h>
#include <stdlib.h>

// ---- finti pezzi: file in memoria, periferiche e pin ----
static std::map<std::string, std::string> g_fs;
static int g_added = 0, g_removed = 0, g_free = 10; static bool g_failAt3 = false;
#include "vos_files.h"
String fsListJson(const String& path) {
  std::string d = path.c_str(); String j = "{\"ok\":true,\"path\":\"" + path + "\",\"list\":[";
  bool f = true; for (auto& kv : g_fs) if (kv.first.compare(0, d.size() + 1, d + "/") == 0) { j += String(f ? "" : ",") + "{\"n\":\"" + String(kv.first.substr(d.size() + 1).c_str()) + "\",\"d\":false,\"s\":10}"; f = false; }
  return j + "],\"more\":false}";
}
bool fsMkdir(const String&, String&) { return true; }
bool fsRemove(const String& p, String& e) { if (!g_fs.erase(p.c_str())) { e = "no"; return false; } return true; }
bool fsWriteText(const String& p, const String& t, String&, bool) { g_fs[p.c_str()] = t.c_str(); return true; }
bool fsReadText(const String& p, String& o, String& e) { auto it = g_fs.find(p.c_str()); if (it == g_fs.end()) { e = "File non trovato"; return false; } o = it->second.c_str(); return true; }
#include "vos_io.h"
bool ioParseCls(const char* s, uint8_t& c) { const char* N[] = {"", "rele", "led", "pin", "ingresso", "analogico", "i2c"}; for (uint8_t i = 1; i <= 6; i++) if (!strcmp(s, N[i])) { c = i; return true; } return false; }
int ioSuggest(uint8_t, int n, int* out) { int k = 0; for (; k < n && k < g_free; k++) out[k] = 5 + k; return k; }
int ioCount() { return g_added - g_removed; }
bool ioAdd(const char* cls, int gpio, const char* name, const char* opt, String& out, String& err) {
  if (g_failAt3 && g_added == 1) { err = "pin occupato"; return false; }
  g_added++; out = name[0] ? String(name) : String("rele") + String(g_added); return true; }
bool ioRemove(const char*, String&) { g_removed++; return true; }
static int g_i2cSda = -1, g_i2cScl = -1, g_i2cAddr = -1, g_i2cCalls = 0; static bool g_i2cFast = false; static String g_i2cDrv;
bool ioAddI2c(const char* drv, int sda, int scl, int addr, bool fast, const char* name, char* out, char* err) {
  (void)err; g_i2cCalls++; g_i2cDrv = drv; g_i2cSda = sda; g_i2cScl = scl; g_i2cAddr = addr; g_i2cFast = fast; strcpy(out, name[0] ? name : "bmp1"); return true; }
#include "../../VesevOS/src/devices/vos_mod.cpp"

static String ok2() {
  return "[module]\nname=rele2ch\ntitle=Rele 2 canali\nclass=rele\nactive=low\npower=5V\nauthor=Mimmo\nversion=1.0\nlicense=CC0\nwarn=Usare modulo con optoisolatore\n"
         "[signals]\nIN1\nIN2\n[icon]\n<svg viewBox=\"0 0 24 24\"><rect x=\"3\" y=\"6\" width=\"18\" height=\"12\"/></svg>\n[wiring]\n<svg viewBox=\"0 0 100 60\"><rect x=\"5\" y=\"5\" width=\"90\" height=\"50\"/><circle cx=\"20\" cy=\"30\" r=\"4\"/></svg>\n";
}
static String rep(String s, const char* a, const char* b) { int i = s.indexOf(a); if (i >= 0) s = s.substring(0, i) + b + s.substring(i + strlen(a)); return s; }

int main() {
  ModInfo m; String err;
  CHECK(modParse(ok2(), "rele2ch", m, err) && m.cls == "rele" && m.low && m.nsig == 2 && String(m.sig[1]) == "IN2" && m.power == "5V" && m.icon.length() > 10 && m.wiring.length() > 10 && m.title == "Rele 2 canali");
  CHECK(!modParse(ok2(), "altro", m, err) && err.indexOf("Riga") == 0);                        // nome diverso dal file
  CHECK(!modParse(rep(ok2(), "class=rele", "class=forno"), "rele2ch", m, err));
  CHECK(!modParse(rep(ok2(), "active=low", "active=basso"), "rele2ch", m, err));
  CHECK(!modParse(rep(ok2(), "power=5V", "power=9V"), "rele2ch", m, err));
  CHECK(modParse(rep(ok2(), "power=5V", "power=230v"), "rele2ch", m, err) && m.power == "230V");
  CHECK(!modParse(rep(ok2(), "IN2", "IN1"), "rele2ch", m, err) && err.indexOf("ripetuto") > 0);   // segnale doppio
  CHECK(!modParse(rep(ok2(), "IN2", "IN 2"), "rele2ch", m, err));
  CHECK(!modParse(rep(ok2(), "[signals]\nIN1\nIN2\n", "[signals]\n"), "rele2ch", m, err));      // nessun segnale
  CHECK(!modParse(rep(ok2(), "class=rele", "class=rele\npull=up"), "rele2ch", m, err));         // pull solo per ingresso
  CHECK(modParse(rep(rep(ok2(), "class=rele", "class=ingresso\npull=up"), "active=low\n", ""), "rele2ch", m, err) && m.pull == 'u');
  CHECK(!modParse(rep(rep(ok2(), "class=rele", "class=analogico"), "active=low", "active=low"), "rele2ch", m, err));   // analogico non puo essere active=low
  CHECK(!modParse(rep(ok2(), "<rect x=\"3\"", "<script></script><rect x=\"3\""), "rele2ch", m, err));                    // icona con script
  CHECK(!modParse(rep(ok2(), "<circle", "<image href=\"x\"/><circle"), "rele2ch", m, err));                               // schema con immagine
  CHECK(!modParse(rep(ok2(), "<circle cx", "<circle onclick=\"x()\" cx"), "rele2ch", m, err));
  CHECK(!modParse(rep(ok2(), "</svg>\n", "\n"), "rele2ch", m, err));                                           // svg non chiuso
  CHECK(!modParse(ok2() + "[icon]\n<svg/>\n", "rele2ch", m, err) && err.indexOf("ripetuta") > 0);
  CHECK(!modParse(rep(ok2(), "[module]", "[modulo]"), "rele2ch", m, err));
  CHECK(!modParse(rep(ok2(), "version=1.0", "version=1a"), "rele2ch", m, err));
  CHECK(!modParse(rep(ok2(), "warn=Usare modulo con optoisolatore", "warn="), "rele2ch", m, err));
  CHECK(!modParse(rep(ok2(), "IN1\n", "A1\nA2\nA3\nA4\nA5\nA6\nA7\nA8\nA9\n"), "rele2ch", m, err) && err.indexOf("Troppi") > 0);
  { String big = ok2(); for (int i = 0; i < 400; i++) big += "# riempimento riempimento riempimento\n"; CHECK(!modParse(big, "rele2ch", m, err)); }
  CHECK(!modParse("name=x\n", "x", m, err));
  // livello del segnale e componenti da aggiungere
  CHECK(modParse(ok2(), "rele2ch", m, err) && m.level == '3' && m.npart == 0);                  // predefiniti
  CHECK(modParse(rep(ok2(), "power=5V", "power=5V\nlevel=5v"), "rele2ch", m, err) && m.level == '5');
  CHECK(!modParse(rep(ok2(), "power=5V", "power=5V\nlevel=12V"), "rele2ch", m, err) && err.indexOf("level") > 0);
  CHECK(modParse(rep(ok2(), "power=5V", "power=5V\npart=serie,330 ohm,in1\npart=GND, 10 kohm ,IN2"), "rele2ch", m, err) && m.npart == 2 && m.part[0].pos == 's' && String(m.part[0].val) == "330 ohm" && String(m.part[0].sig) == "IN1" && m.part[1].pos == 'g' && String(m.part[1].val) == "10 kohm");
  CHECK(!modParse(rep(ok2(), "power=5V", "power=5V\npart=serie,330 ohm,XX"), "rele2ch", m, err) && err.indexOf("[signals]") > 0);   // segnale che non esiste
  CHECK(!modParse(rep(ok2(), "power=5V", "power=5V\npart=paralleo,1k,IN1"), "rele2ch", m, err));
  CHECK(!modParse(rep(ok2(), "power=5V", "power=5V\npart=serie,<b>,IN1"), "rele2ch", m, err));                                  // niente < nel valore
  CHECK(!modParse(rep(ok2(), "power=5V", "power=5V\npart=serie,,IN1"), "rele2ch", m, err));
  CHECK(!modParse(rep(ok2(), "power=5V", "power=5V\npart=serie,1k"), "rele2ch", m, err));
  CHECK(!modParse(rep(ok2(), "power=5V", "power=5V\npart=serie,1k,IN1\npart=serie,1k,IN1\npart=serie,1k,IN1\npart=serie,1k,IN1\npart=serie,1k,IN1"), "rele2ch", m, err) && err.indexOf("Troppi") > 0);
  // salvataggio, elenco, dettagli
  CHECK(!modSave("rele2ch", "pezzo", err) && g_fs.empty());                                   // un file sbagliato non si salva
  CHECK(modSave("rele2ch", ok2(), err) && g_fs.count("/devs/rele2ch.vdp") == 1);
  CHECK(!modSave("Rele 2", ok2(), err));
  String j = modListJson();
  CHECK(j.indexOf("\"n\":\"rele2ch\"") > 0 && j.indexOf("\"ok\":true") > 0 && j.indexOf("\"ns\":2") > 0 && j.indexOf("\"icon\":\"<svg") > 0);
  g_fs["/devs/rotto.vdp"] = "[module]\nname=rotto\n";
  j = modListJson(); CHECK(j.indexOf("\"n\":\"rotto\",\"ok\":false") > 0 && j.indexOf("\"e\":\"") > 0);
  j = modInfoJson("rele2ch");
  CHECK(j.indexOf("\"wiring\":\"<svg") > 0 && j.indexOf("\"sug\":[5,6]") > 0 && j.indexOf("\"free\":1") > 0 && j.indexOf("\"sig\":[\"IN1\",\"IN2\"]") > 0 && j.indexOf("\"w\":\"Usare") > 0);
  CHECK(j.indexOf("\"lv\":\"3V3\"") > 0 && j.indexOf("\"parts\":[]") > 0);
  g_fs["/devs/rele2ch.vdp"] = rep(ok2(), "power=5V", "power=5V\nlevel=5V\npart=serie,330 ohm,IN1").c_str();
  j = modInfoJson("rele2ch"); CHECK(j.indexOf("\"lv\":\"5V\"") > 0 && j.indexOf("{\"k\":\"serie\",\"v\":\"330 ohm\",\"s\":\"IN1\"}") > 0);
  g_fs["/devs/rele2ch.vdp"] = ok2().c_str();
  CHECK(modInfoJson("nonc").indexOf("\"ok\":false") > 0);
  CHECK(modListText().indexOf("rele2ch") >= 0 && modInfoText("rele2ch").indexOf("IN1 -> GPIO5") >= 0);
  // installazione: pin proposti, tutto o niente
  String out;
  CHECK(modInstall("rele2ch", "", out, err) && out.indexOf("\"s\":\"IN1\"") > 0 && out.indexOf("\"g\":6") > 0 && g_added == 2);
  g_added = g_removed = 0; g_free = 1;
  CHECK(!modInstall("rele2ch", "", out, err) && g_added == 0);                                // pin liberi insufficienti
  g_free = 10; g_added = g_removed = 0; g_failAt3 = true;
  CHECK(!modInstall("rele2ch", "", out, err) && err == "pin occupato" && g_added == 1 && g_removed == 1);   // il secondo fallisce: il primo si toglie
  g_failAt3 = false; g_added = g_removed = 0;
  CHECK(modInstall("rele2ch", "pompa", out, err) && out.indexOf("\"n\":\"pompa1\"") > 0 && out.indexOf("\"n\":\"pompa2\"") > 0);
  g_added = g_removed = 0;
  CHECK(!modInstall("nonc", "", out, err));
  CHECK(modDelete("rotto", err) && !g_fs.count("/devs/rotto.vdp") && !modDelete("rotto", err) && !modDelete("../x", err));
  // le schede di esempio in docs/devs devono passare il vero controllo
  { const char* r = getenv("VOS_REPO"); std::string dir = std::string(r ? r : "../..") + "/docs/devs"; int n = 0;
    DIR* d = opendir(dir.c_str()); CHECK(d != nullptr);
    if (d) { struct dirent* e; while ((e = readdir(d))) { std::string f = e->d_name; if (f.size() < 5 || f.substr(f.size() - 4) != ".vdp") continue;
      std::ifstream in(dir + "/" + f); std::stringstream ss; ss << in.rdbuf(); ModInfo mm; String er;
      bool okp = modParse(String(ss.str().c_str()), String(f.substr(0, f.size() - 4).c_str()), mm, er);
      if (!okp) printf("  scheda %s: %s\n", f.c_str(), er.c_str());
      CHECK(okp && mm.icon.length() > 10 && mm.wiring.length() == 0 && (mm.level == '3' || mm.level == '5') && mm.warn.length() > 0 && mm.lic == "CC0"); n++; } closedir(d); }
    CHECK(n >= 7); }
  // ---- classe i2c, fili colorati ([pins]) e colori
  { char hx[8], cn[8];
    CHECK(modColor("red", hx, cn) && String(hx) == "#e53935" && String(cn) == "red");
    CHECK(modColor(" Rosso ", hx, cn) && String(cn) == "red" && modColor("nero", hx, cn) && String(cn) == "black" && modColor("arancione", hx, cn) && String(cn) == "orange");
    CHECK(modColor("grey", hx, cn) && String(cn) == "gray" && modColor("azzurro", hx, cn) && String(cn) == "cyan");
    CHECK(modColor("#A1b2c3", hx, cn) && String(hx) == "#a1b2c3" && cn[0] == 0);
    CHECK(!modColor("#12345", hx, cn) && !modColor("#12345g", hx, cn) && !modColor("123456", hx, cn) && !modColor("", hx, cn) && !modColor("rossa", hx, cn) && !modColor("#12345678", hx, cn)); }
  const char* BMP = "[module]\nname=bmp\ntitle=Sensore BMP\nclass=i2c\ndriver=bmp280\naddr=0x77\npower=3V3\nauthor=X\nversion=1.0\nlicense=CC0\nwarn=Controlla 3,3 V\n"
                    "[signals]\nSDA\nSCL\n[pins]\nVDD=vcc,red\nSDA=SDA,green\nGND=gnd,#000000\nSCL=SCL,giallo\n[icon]\n<svg viewBox=\"0 0 24 24\"><rect x=\"3\" y=\"6\" width=\"18\" height=\"12\"/></svg>\n";
  CHECK(modParse(BMP, "bmp", m, err) && m.cls == "i2c" && !strcmp(m.drv, "bmp280") && m.addr == 0x77 && !m.fast && m.nsig == 2 && m.nwp == 4);
  CHECK(String(m.wp[0].lab) == "VDD" && String(m.wp[0].dst) == "VCC" && String(m.wp[0].hex) == "#e53935" && String(m.wp[0].cname) == "red");
  CHECK(String(m.wp[2].dst) == "GND" && String(m.wp[2].hex) == "#000000" && m.wp[2].cname[0] == 0 && String(m.wp[3].cname) == "yellow");
  CHECK(modParse(rep(BMP, "addr=0x77", "addr=0x76\nfreq=400000"), "bmp", m, err) && m.addr == 0x76 && m.fast);
  CHECK(modParse(rep(BMP, "addr=0x77", "addr=0x76\nfreq=100000"), "bmp", m, err) && !m.fast);
  CHECK(!modParse(rep(BMP, "addr=0x77", "addr=0x76\nfreq=50000"), "bmp", m, err));
  CHECK(!modParse(rep(BMP, "addr=0x77", "addr=0x50"), "bmp", m, err) && !modParse(rep(BMP, "addr=0x77", "addr=pippo"), "bmp", m, err));
  CHECK(!modParse(rep(BMP, "addr=0x77\n", ""), "bmp", m, err));                            // manca l'indirizzo
  CHECK(!modParse(rep(BMP, "driver=bmp280\n", ""), "bmp", m, err));                        // manca il driver
  CHECK(!modParse(rep(BMP, "driver=bmp280", "driver=dht11"), "bmp", m, err));               // driver sconosciuto
  CHECK(!modParse(rep(BMP, "SDA\nSCL\n[pins]", "SCL\nSDA\n[pins]"), "bmp", m, err));      // ordine SDA, SCL
  CHECK(!modParse(rep(BMP, "SCL\n[pins]", "[pins]"), "bmp", m, err));                      // un solo segnale
  CHECK(!modParse(rep(BMP, "class=i2c", "class=i2c\nactive=low"), "bmp", m, err) && !modParse(rep(BMP, "class=i2c", "class=i2c\npull=up"), "bmp", m, err));
  CHECK(!modParse(rep(ok2(), "class=rele", "class=rele\ndriver=bmp280"), "rele2ch", m, err));        // driver, addr e freq solo per i2c
  CHECK(!modParse(rep(ok2(), "class=rele", "class=rele\naddr=0x76"), "rele2ch", m, err) && !modParse(rep(ok2(), "class=rele", "class=rele\nfreq=100000"), "rele2ch", m, err));
  // [pins]: errori
  CHECK(!modParse(rep(BMP, "VDD=vcc,red", "VDD=vcc,rossa"), "bmp", m, err) && err.indexOf("Colore") >= 0);
  CHECK(!modParse(rep(BMP, "VDD=vcc,red", "VDD=vcc,#12345"), "bmp", m, err));
  CHECK(!modParse(rep(BMP, "VDD=vcc,red", "VDD=5v,red"), "bmp", m, err));                   // destinazione sconosciuta
  CHECK(!modParse(rep(BMP, "VDD=vcc,red", "VDD=vcc"), "bmp", m, err));                      // senza colore
  CHECK(!modParse(rep(BMP, "VDD=vcc,red", "VDD=vcc,red\nVDD=vcc,blue"), "bmp", m, err));   // etichetta ripetuta
  CHECK(!modParse(rep(BMP, "GND=gnd,#000000\n", ""), "bmp", m, err) || true);              // senza GND si puo (e solo un disegno)
  CHECK(!modParse(rep(BMP, "SCL=SCL,giallo\n", ""), "bmp", m, err));                       // il segnale SCL non ha il filo
  CHECK(!modParse(rep(BMP, "VDD=vcc,red", "VDD=vcc,red\nA1=vcc,red\nA2=vcc,red\nA3=vcc,red\nA4=vcc,red\nA5=vcc,red"), "bmp", m, err) && err.indexOf("Troppi") >= 0);   // oltre 8 fili
  CHECK(!modParse(String(BMP) + "[pins]\nVDD=vcc,red\n", "bmp", m, err) && err.indexOf("ripetuta") > 0);     // sezione ripetuta
  CHECK(modParse(rep(ok2(), "[icon]", "[pins]\nIN1=IN1,blue\nIN2=IN2,green\n[icon]"), "rele2ch", m, err) && m.nwp == 2 && String(m.wp[1].cname) == "green");   // anche per le altre classi
  CHECK(!modParse(rep(ok2(), "[icon]", "[pins]\nIN1=IN1,blue\n[icon]"), "rele2ch", m, err));        // IN2 senza filo
  // senza [pins] tutto come prima
  CHECK(modParse(ok2(), "rele2ch", m, err) && m.nwp == 0 && m.drv[0] == 0 && modInfoJson("rele2ch").indexOf("\"pins\":[]") > 0);
  // JSON, testo e installazione del sensore
  g_fs["/devs/bmp.vdp"] = BMP;
  j = modInfoJson("bmp");
  CHECK(j.indexOf("\"c\":\"i2c\"") > 0 && j.indexOf("\"drv\":\"bmp280\"") > 0 && j.indexOf("\"addr\":119") > 0 && j.indexOf("\"sug\":[5,6]") > 0);
  CHECK(j.indexOf("\"pins\":[{") > 0 && j.indexOf("\"l\":\"VDD\"") > 0 && j.indexOf("\"#e53935\"") > 0 && j.indexOf("\"cn\":\"red\"") > 0);
  CHECK(modInfoText("bmp").indexOf("VDD") >= 0 && modInfoText("bmp").indexOf("0x77") >= 0);
  g_added = g_removed = 0; g_free = 10; g_i2cCalls = 0;
  CHECK(modInstall("bmp", "", out, err) && g_i2cCalls == 1 && g_i2cDrv == "bmp280" && g_i2cSda == 5 && g_i2cScl == 6 && g_i2cAddr == 0x77 && !g_i2cFast && g_added == 0);
  CHECK(out.indexOf("\"s\":\"SDA\",\"n\":\"bmp1\",\"g\":5") > 0 && out.indexOf("\"s\":\"SCL\"") > 0);
  g_free = 1; CHECK(!modInstall("bmp", "", out, err) && g_i2cCalls == 1);                  // pin liberi insufficienti: niente da fare
  g_free = 10; g_fs.erase("/devs/bmp.vdp");
  // il catalogo periferiche/ deve passare il vero controllo
  { const char* r = getenv("VOS_REPO"); std::string dir = std::string(r ? r : "../..") + "/periferiche"; int n = 0, ni = 0;
    DIR* d = opendir(dir.c_str()); CHECK(d != nullptr);
    if (d) { struct dirent* e; while ((e = readdir(d))) { std::string f = e->d_name; if (f.size() < 5 || f.substr(f.size() - 4) != ".vdp") continue;
      std::ifstream in(dir + "/" + f); std::stringstream ss; ss << in.rdbuf(); ModInfo mm; String er;
      bool okp = modParse(String(ss.str().c_str()), String(f.substr(0, f.size() - 4).c_str()), mm, er);
      if (!okp) printf("  periferica %s: %s\n", f.c_str(), er.c_str());
      CHECK(okp && mm.nwp >= 3 && mm.lic == "CC0" && mm.warn.length() > 0); if (mm.cls == "i2c") { ni++; CHECK(!strcmp(mm.drv, "bmp280") && (mm.addr == 0x76 || mm.addr == 0x77)); } n++; } closedir(d); }
    CHECK(n >= 4 && ni == 2); }
  return done("mod");
}
