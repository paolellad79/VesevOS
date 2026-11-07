#!/bin/bash
# Prova sul computer di core/vos_pacchetto.cpp (senza Arduino): chiavi italiane, versione dichiarata, nome corto dal nome del file.
set -e
R="$(cd "$(dirname "$0")/../.." && pwd)"
T="$(mktemp -d)"
trap 'rm -rf "$T"' EXIT
cat > "$T/p.cpp" <<'CPP'
#include <stdio.h>
#include <string.h>
#include "core/vos_pacchetto.h"
static int ko = 0;
static void uguale(const char* cosa, const char* v, const char* atteso) {
  if (strcmp(v, atteso)) { printf("KO %s: [%s] invece di [%s]\n", cosa, v, atteso); ko++; }
}
static void daFile(const char* radice, bool attesoOk, const char* atteso) {
  char n[25] = "";
  bool ok = pacNomeDaFile(radice, n, sizeof n);
  if (ok != attesoOk) { printf("KO daFile %s: %d\n", radice, ok); ko++; return; }
  if (ok) uguale(radice, n, atteso);
}
int main() {
  PacDati d;
  pacLeggi("# commento\n[manifest]\nnome=logmqtt\ntitolo=Log al broker\nversione=1.2\nautore=VesevOS\nchi=oper\n\n[icon]\nversione=9\n", d);
  uguale("nome", d.nome, "logmqtt"); uguale("titolo", d.descrizione, "Log al broker"); uguale("versione", d.versione, "1.2");
  uguale("autore", d.autore, "VesevOS"); uguale("chi", d.chi, "oper");
  pacLeggi("[module]\r\nname=sd-spi\r\nversion=1.0\r\nversione=2.0\r\n[signals]\r\nCS\r\n", d);
  uguale("vecchie chiavi", d.nome, "sd-spi"); uguale("vince l'ultima", d.versione, "2.0");
  pacLeggi("[module]\nname=a\nnome=b\n", d);
  uguale("primo nome", d.nome, "a");
  pacLeggi("# Torcia: accende il LED\n# versione=1.2\n# autore=Mimmo\n\nvar x = 1\n# versione=3\n", d);
  uguale("script descrizione", d.descrizione, "Torcia: accende il LED"); uguale("script versione", d.versione, "1.2"); uguale("script autore", d.autore, "Mimmo");
  pacLeggi("var x = 1\n", d);
  uguale("senza dati", d.versione, ""); uguale("senza dati", d.descrizione, ""); uguale("senza dati", d.nome, "");
  pacLeggi("# versione=1.2x\n", d);
  uguale("versione sbagliata", d.versione, "");
  pacLeggi(std::string_view("\x7f""ELF\0\0[module]", 14), d);
  uguale("binario", d.nome, "");
  if (std::string_view("name") != pacChiave("NOME") || std::string_view("use") != pacChiave("permessi") || std::string_view("bus") != pacChiave("bus")) { puts("KO chiavi"); ko++; }
  if (!pacVersioneOk("1.2") || !pacVersioneOk("1234567890") || pacVersioneOk("12345678901") || pacVersioneOk("") || pacVersioneOk("1a")) { puts("KO versione"); ko++; }
  daFile("torcia-1.2", true, "torcia");
  daFile("sd-spi-S3-1.0", true, "sd-spi");
  daFile("sd-spi-s3-1.0.1", true, "sd-spi");
  daFile("logmqtt-h2-10.2", true, "logmqtt");
  daFile("torcia", false, "");
  daFile("rele-2", false, "");
  daFile("-1.2", false, "");
  daFile("s3-1.2", true, "s3");
  daFile("sd-spi-1.2.x", false, "");
  if (!pacVersionePiuBassa("1.9", "1.10")) { puts("KO 1.9 < 1.10"); ko++; }
  if (pacVersionePiuBassa("1.10", "1.9")) { puts("KO 1.10 > 1.9"); ko++; }
  if (pacVersionePiuBassa("1.2", "1.2")) { puts("KO uguali"); ko++; }
  if (pacVersionePiuBassa("1.2", "1.2.0") || pacVersionePiuBassa("1.2.0", "1.2")) { puts("KO 1.2 = 1.2.0"); ko++; }
  if (!pacVersionePiuBassa("1.2", "1.2.1")) { puts("KO 1.2 < 1.2.1"); ko++; }
  if (pacVersionePiuBassa("", "1.2") || pacVersionePiuBassa("1.2", "")) { puts("KO non dichiarata"); ko++; }
  if (!pacVersionePiuBassa("9", "1234567890")) { puts("KO numeri grandi"); ko++; }
  if (pacVersionePiuBassa("1x2", "1x2")) { puts("KO caratteri strani"); ko++; }
  printf(ko ? "prova_pacchetto: %d KO\n" : "prova_pacchetto: ok\n", ko);
  return ko != 0;
}
CPP
g++ -std=gnu++17 -I"$R/VesevOS/src" -I"$R/tools/stub" "$T/p.cpp" "$R/VesevOS/src/core/vos_pacchetto.cpp" -o "$T/p"
"$T/p"

# Sostituzione di un pacchetto installato, su file veri in una cartella di prova (il controllo del contenuto e finto: vedi sotto).
cat > "$T/q.cpp" <<'CPP'
#include <stdio.h>
#include <string.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include "drivers/vos_drv_fs.h"
#include "packages/vos_pacchetto_nome.h"
#include "core/vos_i18n.h"
#include "core/vos_files.h"
#include "core/vos_util.h"
#include "devices/vos_mod.h"
static std::string radice;
static std::string vero(const char* p) { return radice + p; }
VosFile drvFsOpen(const char* path, const char* mode) { return VosFile::openPath(vero(path).c_str(), path, mode); }
bool drvFsExists(const char* path) { struct stat st; return stat(vero(path).c_str(), &st) == 0; }
bool drvFsRemove(const char* path) { return unlink(vero(path).c_str()) == 0; }
bool drvFsRename(const char* a, const char* b) { return rename(vero(a).c_str(), vero(b).c_str()) == 0; }
const char* tr(const char* s) { return s; }
size_t fsTextMax() { return 32768; }
bool utilNameOk(const char* s, size_t n) {
  if (n < 1 || n > 24) return false;
  for (size_t i = 0; i < n; i++) { char c = s[i]; if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return false; }
  return true;
}
bool modParse(std::string_view text, const char* fileName, ModInfo&, ErrBuf& err) {
  std::string atteso = std::string("name=") + fileName;
  if (text.find(atteso) == std::string_view::npos) { err = "non valido"; return false; }
  return true;
}
static int ko = 0;
static void scrivi(const char* p, const char* t, size_t n = 0) { FILE* f = fopen(vero(p).c_str(), "w"); fwrite(t, 1, n ? n : strlen(t), f); fclose(f); }
static std::string leggi(const char* p) { char b[300] = ""; FILE* f = fopen(vero(p).c_str(), "r"); if (!f) return "(manca)"; size_t n = fread(b, 1, 299, f); fclose(f); return std::string(b, n); }
static void vero_(const char* cosa, bool c) { if (!c) { printf("KO %s\n", cosa); ko++; } }
static void carica(const char* p, const char* t) { scrivi(p, t); }
int main(int, char** argv) {
  radice = argv[1];
  mkdir(vero("/bin").c_str(), 0755); mkdir(vero("/devs").c_str(), 0755);
  ErrBuf e, a;
  const char* v2 = "[module]\nname=rele\nversione=2.0\n";
  // nome lungo, file rotto: quello installato resta, il caricato sparisce, si vede l'errore
  scrivi("/devs/rele.vdb", v2);
  carica("/devs/rele-3.0.vdb", "[module]\nnome=rele\nversione=3.0\n");
  vero_("file rotto rifiutato", !pacchettoCaricato("/devs/rele-3.0.vdb", "/devs/rele-3.0.vdb", e, a));
  vero_("errore visibile", !e.empty());
  vero_("installato intatto", leggi("/devs/rele.vdb") == v2);
  vero_("caricato tolto", !drvFsExists("/devs/rele-3.0.vdb"));
  // versione piu bassa: avviso, ma il caricamento prosegue
  const char* v1 = "[module]\nname=rele\nversione=1.9\n";
  carica("/devs/rele-1.9.vdb", v1);
  e.clear(); a.clear();
  vero_("versione bassa non blocca", pacchettoCaricato("/devs/rele-1.9.vdb", "/devs/rele-1.9.vdb", e, a));
  vero_("avviso", strstr(a.c_str(), "1.9") && strstr(a.c_str(), "2.0"));
  vero_("sostituito", leggi("/devs/rele.vdb") == v1);
  // versione piu alta: nessun avviso
  const char* v3 = "[module]\nname=rele\nversione=1.10\n";
  carica("/devs/rele-1.10.vdb", v3);
  a.clear();
  vero_("versione alta", pacchettoCaricato("/devs/rele-1.10.vdb", "/devs/rele-1.10.vdb", e, a) && a.empty() && leggi("/devs/rele.vdb") == v3);
  // stesso nome: scritto a parte in .tmp~, poi controllato
  vero_("da proteggere", pacchettoDaProteggere("/devs/rele.vdb") && !pacchettoDaProteggere("/devs/altro.vdb") && !pacchettoDaProteggere("/note.txt"));
  carica("/devs/rele.vdb.tmp~", "rotto");
  e.clear();
  vero_("stesso nome rotto", !pacchettoCaricato("/devs/rele.vdb.tmp~", "/devs/rele.vdb", e, a) && leggi("/devs/rele.vdb") == v3 && !drvFsExists("/devs/rele.vdb.tmp~"));
  carica("/devs/rele.vdb.tmp~", "[module]\nname=rele\n");
  a.clear();
  vero_("versione non dichiarata", pacchettoCaricato("/devs/rele.vdb.tmp~", "/devs/rele.vdb", e, a) && a.empty() && leggi("/devs/rele.vdb") == "[module]\nname=rele\n");
  // script
  scrivi("/bin/torcia.vsb", "# versione=1.2\nvar x = 1\n");
  scrivi("/bin/torcia-1.3.vsb", "x\0y", 3);
  e.clear();
  vero_("script binario", !pacchettoCaricato("/bin/torcia-1.3.vsb", "/bin/torcia-1.3.vsb", e, a) && leggi("/bin/torcia.vsb")[0] == '#');
  scrivi("/bin/torcia-1.1.vsb", "# versione=1.1\nvar x = 2\n");
  a.clear();
  vero_("script piu vecchio", pacchettoCaricato("/bin/torcia-1.1.vsb", "/bin/torcia-1.1.vsb", e, a) && !a.empty() && leggi("/bin/torcia.vsb").find("x = 2") != std::string::npos);
  // nulla di installato e file fuori dalle cartelle: come prima
  scrivi("/bin/nuovo-1.0.vsb", "var x = 3\n");
  vero_("nuovo", pacchettoCaricato("/bin/nuovo-1.0.vsb", "/bin/nuovo-1.0.vsb", e, a) && drvFsExists("/bin/nuovo.vsb"));
  scrivi("/note.txt", "ciao");
  vero_("fuori", pacchettoCaricato("/note.txt", "/note.txt", e, a) && drvFsExists("/note.txt"));
  // solo script e driver Berry: app ed ELF hanno controlli propri, spenti in questa prova
  ErrBuf b; pacchettoAvviso("/devs/rele.vdb", "[module]\nversione=0.1\n", b);
  vero_("avviso senza versione installata", b.empty());
  printf(ko ? "prova_pacchetto (sostituzione): %d KO\n" : "prova_pacchetto (sostituzione): ok\n", ko);
  return ko != 0;
}
CPP
mkdir "$T/fs"
g++ -std=gnu++17 -w -DARDUINO=10800 -DESP32 -DVOS_WITH_BERRY=0 -DVOS_WITH_ELF=0 -I"$R/VesevOS/src" -I"$R/tools/stub" "$T/q.cpp" \
  "$R/VesevOS/src/packages/vos_pacchetto_nome.cpp" "$R/VesevOS/src/core/vos_pacchetto.cpp" "$R/VesevOS/src/drivers/vos_drv_file.cpp" -o "$T/q"
"$T/q" "$T/fs"
