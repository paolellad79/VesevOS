// Prova sul computer del formato dei driver ELF (.vde): packages/vos_vde.cpp VERO e devices/vos_mod_line.cpp VERO (comandi e widget), con il controllo
// dell'ELF finto (vos_elf_valida accetta solo un blocco che comincia per 0x7f 'ELF' e non vuoto). Legge anche i due esempi veri in docs/esempi/driver-elf.
// Prova: file giusto, ogni errore del contenitore (marca [elf], lunghezza, sezioni, chiavi, bus e segnali, comandi, widget e source), copia allineata dell'ELF.
// NON prova: l'ELF vero (caricatore, serve la scheda) ne l'installazione sui pin (prova_drvres).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include "vos_vde.h"
#include "vos_elf.h"
#include "mini.h"

// ---- finti ----
const char* tr(const char* it) { return it; }
bool utilNameOk(const char* s, size_t n) {
  if (n < 1 || n > 24) return false;
  for (size_t i = 0; i < n; i++) { char c = s[i]; if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return false; }
  return true;
}
bool utilLineErr(ErrBuf& err, int line, const char* msg) { err.setf("Riga %d: %s", line, msg); return false; }
bool utilSvgSafe(std::string_view svg, size_t mx) { return svg.size() <= mx && svg.rfind("<svg", 0) == 0 && svg.find("<script") == std::string_view::npos; }
static vos_elf_caps_t g_caps;
const vos_elf_caps_t* vos_elf_caps() { g_caps.file_max = 131072; return &g_caps; }
int vos_elf_valida(const uint8_t* p, size_t len, const vos_elf_param_t*, vos_elf_info_t*) {
  if (reinterpret_cast<uintptr_t>(p) % 4) return VOS_ELF_E_ALLINEA;              // il vero controllo vuole un blocco allineato a 4
  return len >= 4 && !memcmp(p, "\x7f""ELF", 4) ? VOS_ELF_OK : VOS_ELF_E_MAGIC;
}
const char* vos_elf_err_testo(int e) { return e == VOS_ELF_E_MAGIC ? "non e un file ELF" : e == VOS_ELF_E_ALLINEA ? "allineamento" : "errore ELF"; }

static const char* BASE =
  "[module]\\nname=prova\\ntitle=Prova\\nbus=i2c\\naddr=0x76\\nfreq=100000\\nuse=i2c\\n[signals]\\nSDA\\nSCL\\n"
  "[commands]\\nprv leggi oper\\nprv scrivi admin testo\\n[widget]\\ntitle=Prova\\nalt=Non risponde\\nwho=oper\\n";

static std::string costruisci(std::string testo, const std::string& blob, long dichiarato = -1) {
  std::string t;
  for (size_t i = 0; i < testo.size(); i++) {
    if (testo[i] == '\\' && i + 1 < testo.size() && testo[i + 1] == 'n') { t += '\n'; i++; } else t += testo[i];
  }
  if (!t.empty() && t.back() != '\n') t += '\n';
  return t + "[elf] " + std::to_string(dichiarato >= 0 ? dichiarato : (long)blob.size()) + "\n" + blob;
}

static bool analizza(const std::string& f, const char* nome, VdeInfo& v, std::string& err, uint8_t** elf = nullptr) {
  ErrBuf e;
  bool ok = vdeParse((const uint8_t*)f.data(), f.size(), nome, v, elf, e);
  err = e.c_str();
  return ok;
}
static std::string leggi(const std::string& p) {
  FILE* fp = fopen(p.c_str(), "rb"); std::string s;
  if (!fp) return s;
  char b[4096]; size_t n;
  while ((n = fread(b, 1, sizeof b, fp)) > 0) s.append(b, n);
  fclose(fp);
  return s;
}
static std::string elfFinto() { return std::string("\x7f""ELF") + "corpo del modulo....."; }

int main() {
  VdeInfo* v = new VdeInfo();
  std::string err, b = elfFinto(), f;

  // file giusto
  f = costruisci(BASE, b);
  uint8_t* elf = nullptr;
  CHECK(analizza(f, "prova", *v, err, &elf) && err.empty());
  CHECK(!strcmp(v->name, "prova") && v->bus == VB_I2C && v->addr == 0x76 && v->khz == 100 && v->nsig == 2 && !strcmp(v->sig[1], "SCL"));
  CHECK(v->ncmd == 2 && !strcmp(v->cmdWord, "prv") && v->cmd[1].admin && v->cmd[1].rest && !v->cmd[0].admin);
  CHECK(v->widget.on && !strcmp(v->widget.who, "oper") && !v->source[0] && !strcmp(v->use, "i2c"));
  CHECK(elf && (reinterpret_cast<uintptr_t>(elf) % 4) == 0 && v->elfLen == b.size() && !memcmp(elf, b.data(), b.size()));   // copia allineata, uguale all'ELF
  free(elf);
  CHECK(v->textLen < f.size() && f.compare(v->elfOff, 4, "\x7f""ELF") == 0);

  // contenitore
  CHECK(!analizza("[module]\nname=prova\n", "prova", *v, err) && err.find("corto") != std::string::npos);
  CHECK(!analizza(costruisci(BASE, b).substr(0, f.size() - 3), "prova", *v, err) && err.find("lunghezza") != std::string::npos);   // ELF troncato
  CHECK(!analizza(costruisci(BASE, b, (long)b.size() + 1), "prova", *v, err) && err.find("lunghezza") != std::string::npos);
  CHECK(!analizza(costruisci(BASE, b, 0), "prova", *v, err));
  CHECK(!analizza(std::string(BASE).substr(0, 20) + std::string(80, 'x') + "\n", "prova", *v, err));          // senza [elf]
  CHECK(!analizza(costruisci(BASE, std::string("niente ELF qui, solo testo")), "prova", *v, err) && err.find("ELF") != std::string::npos);   // blocco non ELF
  std::string tanto(9000, 'a'); tanto += "\n";
  CHECK(!analizza(costruisci(std::string("[module]\\nname=prova\\n#") + tanto, b), "prova", *v, err));        // testo oltre 8 KB
  CHECK(!analizza(f, "altro", *v, err) && err.find("Riga 2") != std::string::npos);                          // name diverso dal file

  // moduli e chiavi
  auto prova = [&](const char* testo, const char* atteso) {
    bool ok = analizza(costruisci(testo, b), "prova", *v, err);
    bool bene = atteso ? (!ok && err.find(atteso) != std::string::npos) : ok;
    if (!bene) printf("  (testo: %.60s -> ok=%d err=%s)\n", testo, ok, err.c_str());
    return bene;
  };
  CHECK(prova("[module]\\nname=prova\\nbus=i2c\\naddr=0x76\\nfreq=100000\\n[signals]\\nSDA\\nSCL\\n", nullptr));           // il minimo
  CHECK(prova("[module]\\nnome=prova\\ntitolo=Prova\\nversione=1.2\\nautore=Io\\nbus=i2c\\nindirizzo=0x76\\nfrequenza=100000\\n[signals]\\nSDA\\nSCL\\n", nullptr));   // chiavi italiane
  CHECK(prova("[module]\\nname=prova\\n[signals]\\nSDA\\nSCL\\n", "bus="));
  CHECK(prova("[module]\\nname=prova\\nbus=uart\\n[signals]\\nTX\\n", "Bus non valido"));
  CHECK(prova("[module]\\nname=prova\\nbus=i2c\\nfreq=100000\\n[signals]\\nSDA\\nSCL\\n", "addr"));
  CHECK(prova("[module]\\nname=prova\\nbus=i2c\\naddr=0x76\\n[signals]\\nSDA\\nSCL\\n", "freq"));
  CHECK(prova("[module]\\nname=prova\\nbus=i2c\\naddr=0x05\\nfreq=100000\\n[signals]\\nSDA\\nSCL\\n", "Indirizzo"));
  CHECK(prova("[module]\\nname=prova\\nbus=i2c\\naddr=0x76\\nfreq=250000\\n[signals]\\nSDA\\nSCL\\n", "freq"));
  CHECK(prova("[module]\\nname=prova\\nbus=i2c\\naddr=0x76\\nfreq=100000\\n[signals]\\nSCL\\nSDA\\n", "ordine"));   // segnali scambiati
  CHECK(prova("[module]\\nname=prova\\nbus=i2c\\naddr=0x76\\nfreq=100000\\n[signals]\\nSDA\\n", "ordine"));
  CHECK(prova("[module]\\nname=prova\\nbus=spi\\n[signals]\\nCS\\nMOSI\\nMISO\\nSCK\\n", nullptr) && v->khz == 1000);   // spi: 1000 kHz di fabbrica
  CHECK(prova("[module]\\nname=prova\\nbus=spi\\nkhz=20000\\n[signals]\\nCS\\nMOSI\\nMISO\\nSCK\\n", "SPI"));
  CHECK(prova("[module]\\nname=prova\\nbus=spi\\nkhz=1000\\n[signals]\\nCS\\nMOSI\\nSCK\\nMISO\\n", "ordine"));        // l'ordine spi non e quello sd
  CHECK(prova("[module]\\nname=prova\\nbus=sd\\n[signals]\\nCS\\nMOSI\\nSCK\\nMISO\\n", nullptr) && v->khz == 1000);
  CHECK(prova("[module]\\nname=prova\\nbus=sd\\nkhz=300\\n[signals]\\nCS\\nMOSI\\nSCK\\nMISO\\n", "khz"));
  CHECK(prova("[module]\\nname=prova\\nbus=gpio\\n[signals]\\nA\\nB\\nC\\nD\\n", nullptr) && v->nsig == 4);
  CHECK(prova("[module]\\nname=prova\\nbus=gpio\\n[signals]\\nA\\nB\\nC\\nD\\nE\\n", "Troppi segnali"));
  CHECK(prova("[module]\\nname=prova\\nbus=gpio\\n[signals]\\nA\\nA\\n", "ripetuto"));
  CHECK(prova("[module]\\nname=prova\\nbus=gpio\\nkhz=100\\n[signals]\\nA\\n", "gpio"));
  CHECK(prova("[module]\\nname=prova\\nbus=gpio\\naddr=0x20\\n[signals]\\nA\\n", "addr"));
  CHECK(prova("[module]\\nname=prova\\nbus=gpio\\n[signals]\\nA\\n[page]\\nx\\n", "Sezione sconosciuta"));         // niente [page], [code], [cmd]
  CHECK(prova("[module]\\nname=prova\\nbus=gpio\\n[signals]\\nA\\n[code]\\nprint(1)\\n", "Sezione sconosciuta"));
  CHECK(prova("[module]\\nname=prova\\nbus=gpio\\n[signals]\\nA\\n[signals]\\nB\\n", "ripetuta"));
  CHECK(prova("[module]\\nname=prova\\nbus=gpio\\nuse=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\\n[signals]\\nA\\n", "Permessi"));   // use oltre 32
  CHECK(prova("[module]\\nname=prova\\nbus=gpio\\nuso=1\\n[signals]\\nA\\n", "Chiave sconosciuta"));
  CHECK(prova("[module]\\nname=prova\\nbus=gpio\\nwho=tutti\\n[signals]\\nA\\n", "admin oppure oper"));
  CHECK(prova("name=prova\\n", "fuori da una sezione"));
  CHECK(prova("[module]\\nname=prova\\nbus=gpio\\n[signals]\\nA\\n[icon]\\n<script>x</script>\\n", "Icona"));
  CHECK(prova("[module]\\nname=prova\\nbus=gpio\\n[signals]\\nA\\n[icon]\\n<svg viewBox=\\\"0 0 1 1\\\"></svg>\\n", nullptr) && v->icon[0] == '<');
  CHECK(prova("[module]\\nname=prova\\nbus=gpio\\n[signals]\\nA\\n[commands]\\nx a oper\\ny b oper\\n", "stessa prima parola"));
  CHECK(prova("[module]\\nname=prova\\nbus=gpio\\n[signals]\\nA\\n[commands]\\nx a oper\\nx a admin\\n", "ripetuto"));
  CHECK(prova("[module]\\nname=prova\\nbus=gpio\\n[signals]\\nA\\n[commands]\\nx widget oper\\n", "riservato"));
  CHECK(prova("[module]\\nname=prova\\nbus=gpio\\n[signals]\\nA\\n[commands]\\nx a tutti\\n", "admin oppure oper"));
  // widget e source
  CHECK(prova("[module]\\nname=prova\\nbus=sd\\n[signals]\\nCS\\nMOSI\\nSCK\\nMISO\\n[widget]\\ntitle=SD\\nsource=sd\\n", nullptr) && !strcmp(v->source, "sd") && v->widget.on && !strcmp(v->widget.who, "oper"));
  CHECK(prova("[module]\\nname=prova\\nbus=sd\\n[signals]\\nCS\\nMOSI\\nSCK\\nMISO\\n[widget]\\nsource=gps\\n", "unica sorgente"));
  CHECK(prova("[module]\\nname=prova\\nbus=gpio\\n[signals]\\nA\\n[widget]\\ntitle=<b>\\n", "Titolo"));
  CHECK(prova("[module]\\nname=prova\\nbus=gpio\\ntitle=Mio\\n[signals]\\nA\\n[widget]\\nwho=admin\\n", nullptr) && !strcmp(v->widget.title, "Mio") && !strcmp(v->widget.who, "admin"));   // titolo dal driver
  CHECK(analizza(costruisci("[module]\\nname=prova\\nbus=gpio\\n[signals]\\nA\\n", b), "prova", *v, err) && !v->widget.on);

  // testo non ASCII
  std::string acc = costruisci("[module]\\nname=prova\\ntitle=Prova", b); acc.replace(acc.find("Prova"), 5, "Pr\xc3\xa8va");
  CHECK(!analizza(acc, "prova", *v, err) && err.find("ASCII") != std::string::npos);

  // gli esempi veri
  std::string dir = std::string(REPO) + "/docs/esempi/driver-elf/";
  std::string bmp = leggi(dir + "bmp280.vde"), sdc = leggi(dir + "sd-spi.vde");
  CHECK(!bmp.empty() && !sdc.empty());
  CHECK(analizza(bmp, "bmp280", *v, err) && v->bus == VB_I2C && v->ncmd == 3 && !strcmp(v->cmdWord, "bmp") && v->widget.on && !v->source[0] && v->elfLen > 100);
  bool okSd = analizza(sdc, "sd-spi", *v, err);
  if (!okSd) printf("  (sd-spi: %s)\n", err.c_str());
  CHECK(okSd && v->bus == VB_SD && v->ncmd == 10 && !strcmp(v->cmdWord, "sd") && !strcmp(v->source, "sd") && v->khz == 1000 && v->icon[0] == '<');
  CHECK(!analizza(sdc, "bmp280", *v, err));
  delete v;
  return done("prova_vde");
}
