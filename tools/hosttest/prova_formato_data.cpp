// Prova di parita, lato firmware: legge tools/hosttest/casi_formato_data.txt e controlla core/vos_datefmt.cpp.
// La stessa riga di casi la legge prova_formato_data.js per la pagina (web/src/js_dataora.js).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include "vos_datefmt.h"
#include "mini.h"

static std::vector<std::string> campi(const std::string& riga) {
  std::vector<std::string> v;
  size_t da = 0;
  for (;;) {
    size_t t = riga.find('\t', da);
    if (t == std::string::npos) {
      v.push_back(riga.substr(da));
      break;
    }
    v.push_back(riga.substr(da, t - da));
    da = t + 1;
  }
  return v;
}

static void dividi(const std::string& s, char sep, std::vector<std::string>& out) {
  size_t da = 0;
  for (;;) {
    size_t t = s.find(sep, da);
    if (t == std::string::npos) {
      out.push_back(s.substr(da));
      break;
    }
    out.push_back(s.substr(da, t - da));
    da = t + 1;
  }
}

static void riempi(char (*dst)[DF_NOME_MAX], int n, const std::string& lista) {
  std::vector<std::string> v;
  dividi(lista, ',', v);
  for (int i = 0; i < n && i < (int)v.size(); i++) snprintf(dst[i], DF_NOME_MAX, "%s", v[i].c_str());
}

int main(int argc, char** argv) {
  if (argc < 2) {
    printf("uso: prova_formato_data <casi.txt>\n");
    return 2;
  }
  FILE* f = fopen(argv[1], "r");
  if (!f) {
    printf("file dei casi non trovato: %s\n", argv[1]);
    return 2;
  }
  DfNomi nomi;
  memset(&nomi, 0, sizeof nomi);
  char buf[1024];
  int riga = 0;
  while (fgets(buf, sizeof buf, f)) {
    riga++;
    std::string r = buf;
    while (!r.empty() && (r.back() == '\n' || r.back() == '\r')) r.pop_back();
    if (r.empty() || r[0] == '#') continue;
    std::vector<std::string> c = campi(r);
    if (c.size() != 5) {
      printf("  KO riga %d: servono 5 campi\n", riga);
      g_fail++;
      continue;
    }
    const std::string& tipo = c[0];
    if (tipo == "NB") { riempi(nomi.giornoBreve, 7, c[1]); continue; }
    if (tipo == "NL") { riempi(nomi.giornoLungo, 7, c[1]); continue; }
    if (tipo == "NM") { riempi(nomi.meseLungo, 12, c[1]); continue; }
    if (tipo == "NF") {
      std::vector<std::string> v;
      dividi(c[1], ';', v);
      for (int i = 0; i < 12 && i < (int)v.size(); i++) snprintf(nomi.fraseMese[i], DF_FRASE_MAX, "%s", v[i].c_str());
      continue;
    }
    char out[128];
    out[0] = 0;
    if (tipo == "M") {
      dfMeseBreve(c[1].c_str(), out, sizeof out);
    } else {
      DfData d = {};
      if (sscanf(c[3].c_str(), "%d %d %d %d %d %d %d", &d.anno, &d.mese, &d.giorno, &d.giornoSett, &d.ore, &d.minuti, &d.secondi) != 7) {
        printf("  KO riga %d: data illeggibile\n", riga);
        g_fail++;
        continue;
      }
      DfImpost im = {};
      im.dataCortaTesto = "";
      im.dataLungaTesto = "";
      im.oraLungaTesto = "";
      if (tipo == "F") {
        DfErr e = dfControlla(c[1].c_str());
        if (e != DF_OK) snprintf(out, sizeof out, "!%d", (int)e);
        else dfFormatta(c[1].c_str(), d, &nomi, out, sizeof out);
      } else if (tipo == "DC") {
        im.dataCorta = atoi(c[1].c_str());
        im.dataCortaTesto = c[2].c_str();
        dfDataCorta(im, d, nomi, out, sizeof out);
      } else if (tipo == "DL") {
        im.dataLunga = atoi(c[1].c_str());
        im.dataLungaTesto = c[2].c_str();
        dfDataLunga(im, d, nomi, out, sizeof out);
      } else if (tipo == "OC") {
        im.ampm = atoi(c[1].c_str()) != 0;
        dfOraCorta(im, d, out, sizeof out);
      } else if (tipo == "OL") {
        int a = 0, s = 0, o = 0;
        sscanf(c[1].c_str(), "%d %d %d", &a, &s, &o);
        im.ampm = a != 0;
        im.oraLungaSec = s != 0;
        im.oraLunga = o;
        im.oraLungaTesto = c[2].c_str();
        dfOraLunga(im, d, nomi, out, sizeof out);
      } else {
        printf("  KO riga %d: tipo sconosciuto %s\n", riga, tipo.c_str());
        g_fail++;
        continue;
      }
    }
    g_n++;
    if (c[4] != out) {
      g_fail++;
      printf("  KO riga %d (%s %s): atteso [%s], ottenuto [%s]\n", riga, tipo.c_str(), c[1].c_str(), c[4].c_str(), out);
    }
  }
  fclose(f);
  return done("formato data (firmware)");
}
