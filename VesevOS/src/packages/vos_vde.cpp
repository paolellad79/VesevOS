// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_vde.cpp (vedi vos_vde.h): lettura e controllo del file .vde (testo + ELF) e la sua scrittura in /devs.
// Installazione, rimozione e JSON: vos_vde_inst.cpp. Esecuzione di comandi e widget: vos_vde_run.cpp.
#include "vos_vde.h"

#if VOS_WITH_ELF
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vos_elf.h"
#include "../core/vos_files.h"
#include "../core/vos_i18n.h"
#include "../core/vos_pacchetto.h"
#include "../core/vos_util.h"
#include "../core/vos_vdesc.h"
#include "../devices/vos_io.h"
#include "../drivers/vos_drv_fs.h"

namespace {

const char kMarca[] = "\n[elf] ";

bool testoOk(std::string_view s, size_t mx) {
  if (s.size() < 1 || s.size() > mx) return false;
  for (char ch : s) { unsigned char c = (unsigned char)ch; if (c < 32 || c == 127) return false; }
  return true;
}

bool segnaleOk(std::string_view s) {
  if (s.size() < 1 || s.size() > 8) return false;
  for (char c : s) if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
  return true;
}

// Cerca "\n[elf] N\n" nei primi byte; ritorna l'inizio del blob e N, o false.
bool dividi(const uint8_t* f, size_t len, size_t& testo, size_t& off, size_t& n, ErrBuf& err) {
  size_t cerca = len < VDE_TEXT_MAX + 64 ? len : VDE_TEXT_MAX + 64;
  size_t k = 0, m = sizeof(kMarca) - 1;
  for (; k + m < cerca; k++) if (!memcmp(f + k, kMarca, m)) break;
  if (k + m >= cerca) { err = tr("Manca la riga [elf] con la lunghezza dell'ELF"); return false; }
  size_t p = k + m, v = 0, cifre = 0;
  while (p < len && f[p] >= '0' && f[p] <= '9' && cifre < 8) { v = v * 10 + (f[p] - '0'); p++; cifre++; }
  if (!cifre || p >= len || f[p] != '\n') { err = tr("[elf] vuole la lunghezza in byte, sulla stessa riga"); return false; }
  p++;
  if (v == 0 || len - p != v) { err = tr("La lunghezza dopo [elf] non corrisponde al file"); return false; }
  testo = k + 1; off = p; n = v;
  return true;
}

bool busDa(std::string_view v, uint8_t& b) {
  b = viewEqI(v, "gpio") ? VB_GPIO : viewEqI(v, "i2c") ? VB_I2C : viewEqI(v, "spi") ? VB_SPI : viewEqI(v, "sd") ? VB_SD : 0;
  return b != 0;
}

bool numero(std::string_view v, long mn, long mx, int& out) {
  char b[16];
  if (!viewCopy(b, sizeof b, v) || v.empty()) return false;
  char* e = nullptr; long x = strtol(b, &e, 0);
  if (*e || x < mn || x > mx) return false;
  out = (int)x;
  return true;
}

// Una riga chiave=valore di [module]. false = err gia scritto con la riga.
bool chiaveModulo(VdeInfo& v, std::string_view k, std::string_view val, const char* nomeFile, int riga, ErrBuf& err, bool& haBus) {
  k = pacChiave(k);
  if (viewEqI(k, "name")) {
    if (!utilNameOk(val.data(), val.size()) || val != std::string_view(nomeFile)) return utilLineErr(err, riga, tr("Nome non valido o diverso dal nome del file"));
    viewCopy(v.name, sizeof v.name, val);
  } else if (viewEqI(k, "title")) {
    if (!testoOk(val, 32)) return utilLineErr(err, riga, tr("Testo non valido (1-32 caratteri, niente caratteri di controllo)"));
    viewCopy(v.title, sizeof v.title, val);
  } else if (viewEqI(k, "bus")) {
    if (!busDa(val, v.bus)) return utilLineErr(err, riga, tr("Bus non valido (gpio, i2c, spi, sd)"));
    haBus = true;
  } else if (viewEqI(k, "addr")) {
    if (!numero(val, 0x08, 0x77, v.addr)) return utilLineErr(err, riga, tr("Indirizzo I2C non valido (da 0x08 a 0x77)"));
  } else if (viewEqI(k, "freq")) {
    if (val != "100000" && val != "400000") return utilLineErr(err, riga, tr("freq: 100000 oppure 400000"));
    v.khz = val == "400000" ? 400 : 100;
  } else if (viewEqI(k, "khz")) {
    if (!numero(val, 100, 20000, v.khz)) return utilLineErr(err, riga, tr("khz: da 100 a 20000"));
  } else if (viewEqI(k, "use")) {
    if (!testoOk(val, VDE_USE_MAX)) return utilLineErr(err, riga, tr("Permessi non validi (come use= di una app, al massimo 32 caratteri)"));
    viewCopy(v.use, sizeof v.use, val);
  } else if (viewEqI(k, "who")) {
    if (val != "admin" && val != "oper") return utilLineErr(err, riga, tr("Chi la usa: admin oppure oper"));
    viewCopy(v.who, sizeof v.who, val);
  } else if (viewEqI(k, "power")) {
    if (!viewEqI(val, "3V3") && !viewEqI(val, "5V") && !viewEqI(val, "none")) return utilLineErr(err, riga, tr("power: 3V3, 5V oppure none"));
    if (viewEqI(val, "none")) strcpy(v.power, "-"); else viewCopyUpper(v.power, sizeof v.power, val);
  } else if (viewEqI(k, "level")) {
    if (!viewEqI(val, "3V3") && !viewEqI(val, "5V")) return utilLineErr(err, riga, tr("level: 3V3 oppure 5V"));
    v.level = viewEqI(val, "5V") ? '5' : '3';
  } else if (viewEqI(k, "author")) {
    if (!testoOk(val, 24)) return utilLineErr(err, riga, tr("Testo non valido (1-24 caratteri, niente caratteri di controllo)"));
    viewCopy(v.author, sizeof v.author, val);
  } else if (viewEqI(k, "license")) {
    if (!testoOk(val, 24)) return utilLineErr(err, riga, tr("Testo non valido (1-24 caratteri, niente caratteri di controllo)"));
    viewCopy(v.lic, sizeof v.lic, val);
  } else if (viewEqI(k, "warn")) {
    if (!testoOk(val, 80)) return utilLineErr(err, riga, tr("Avviso non valido (1-80 caratteri)"));
    viewCopy(v.warn, sizeof v.warn, val);
  } else if (viewEqI(k, "version")) {
    if (!pacVersioneOk(val)) return utilLineErr(err, riga, tr("Versione non valida (solo cifre e punti, massimo 10)"));
    viewCopy(v.ver, sizeof v.ver, val);
  } else {
    return utilLineErr(err, riga, tr("Chiave sconosciuta"));
  }
  return true;
}

// I segnali devono essere quelli del bus, nell'ordine giusto.
bool segnaliOk(const VdeInfo& v, ErrBuf& err) {
  static const char* const I2C[] = {"SDA", "SCL"};
  static const char* const SPI[] = {"CS", "MOSI", "MISO", "SCK"};
  static const char* const SD[] = {"CS", "MOSI", "SCK", "MISO"};
  const char* const* att = v.bus == VB_I2C ? I2C : v.bus == VB_SPI ? SPI : v.bus == VB_SD ? SD : nullptr;
  int n = v.bus == VB_I2C ? 2 : (v.bus == VB_SPI || v.bus == VB_SD) ? 4 : 0;
  if (!att) { if (v.nsig < 1 || v.nsig > VDE_SIG_MAX) { err = tr("Bus gpio: da 1 a 4 segnali"); return false; } return true; }
  bool ok = v.nsig == n;
  for (int i = 0; ok && i < n; i++) if (strcmp(v.sig[i], att[i])) ok = false;
  if (!ok) err = v.bus == VB_I2C ? tr("Bus i2c: i segnali sono SDA e SCL, in questo ordine")
               : v.bus == VB_SPI ? tr("Bus spi: i segnali sono CS, MOSI, MISO, SCK, in questo ordine")
               : tr("Bus sd: i segnali sono CS, MOSI, SCK, MISO, in questo ordine");
  return ok;
}

}  // namespace

bool vdeParse(const uint8_t* file, size_t len, const char* fileName, VdeInfo& v, uint8_t** elf, ErrBuf& err) {
  if (elf) *elf = nullptr;
  memset(&v, 0, sizeof v);
  strcpy(v.power, "-"); strcpy(v.ver, "1"); strcpy(v.who, "admin"); v.level = '3'; v.khz = 0;
  err = "";
  size_t testo = 0;
  if (!file || len < 32 || !fileName) { err = tr("File vuoto o troppo corto"); return false; }
  if (len > VDE_TEXT_MAX + 64 + vos_elf_caps()->file_max) { err = tr("File troppo grande"); return false; }
  if (!dividi(file, len, testo, v.elfOff, v.elfLen, err)) return false;
  if (testo > VDE_TEXT_MAX) { err = tr("La parte di testo supera 8 KB"); return false; }
  v.textLen = testo;
  enum { NONE, MOD, SIG, ICO, CMS, WGT };
  int sec = NONE; bool visto[6] = {false}, haBus = false;
  size_t iconLen = 0;
  LineIter it(std::string_view((const char*)file, testo)); std::string_view raw;
  while (it.next(raw)) {
    int riga = it.line();
    std::string_view t = trimView(raw);
    for (char ch : raw) if ((unsigned char)ch > 126 || ((unsigned char)ch < 32 && ch != '\t')) return utilLineErr(err, riga, tr("Il testo deve essere ASCII"));
    if (t.size() > 2 && t[0] == '[' && t.back() == ']') {
      std::string_view s = t.substr(1, t.size() - 2);
      int ns = viewEqI(s, "module") ? MOD : viewEqI(s, "signals") ? SIG : viewEqI(s, "icon") ? ICO : viewEqI(s, "commands") ? CMS : viewEqI(s, "widget") ? WGT : NONE;
      if (ns == NONE) return utilLineErr(err, riga, tr("Sezione sconosciuta (ammesse: module, signals, icon, widget, commands)"));
      if (visto[ns]) return utilLineErr(err, riga, tr("Sezione ripetuta"));
      visto[ns] = true; sec = ns; continue;
    }
    if (t.empty() || t[0] == '#') continue;
    if (sec == NONE) return utilLineErr(err, riga, tr("Riga fuori da una sezione"));
    if (sec == ICO) {
      if (iconLen + t.size() + 1 > MOD_ICON_MAX + 8) return utilLineErr(err, riga, tr("Icona non ammessa (un solo svg con forme semplici, massimo 2 KB)"));
      memcpy(v.icon + iconLen, t.data(), t.size()); iconLen += t.size(); v.icon[iconLen++] = '\n'; v.icon[iconLen] = 0;
      continue;
    }
    if (sec == SIG) {
      if (!segnaleOk(t)) return utilLineErr(err, riga, tr("Segnale non valido (1-8 caratteri: lettere, numeri, _)"));
      if (v.nsig >= VDE_SIG_MAX) return utilLineErr(err, riga, tr("Troppi segnali (massimo 4)"));
      for (int i = 0; i < v.nsig; i++) if (viewEqI(t, v.sig[i])) return utilLineErr(err, riga, tr("Segnale ripetuto"));
      viewCopy(v.sig[v.nsig++], sizeof v.sig[0], t);
      continue;
    }
    if (sec == CMS) {
      ModCmd c; const char* why = nullptr;
      if (!modCmdLine(t, c, &why)) return utilLineErr(err, riga, why);
      if (v.ncmd == 0) snprintf(v.cmdWord, sizeof v.cmdWord, "%s", c.cmd);
      else if (strcmp(c.cmd, v.cmdWord)) return utilLineErr(err, riga, tr("Tutti i comandi di un driver hanno la stessa prima parola"));
      if (v.ncmd >= MOD_CMD_MAX) return utilLineErr(err, riga, tr("Troppi comandi (massimo 12)"));
      for (int k = 0; k < v.ncmd; k++) if (!strcmp(v.cmd[k].sub, c.sub)) return utilLineErr(err, riga, tr("Sottocomando ripetuto"));
      if (!strcmp(c.sub, "widget")) return utilLineErr(err, riga, tr("Il sottocomando widget e riservato"));
      v.cmd[v.ncmd++] = c;
      continue;
    }
    size_t q = t.find('=');
    if (q == std::string_view::npos || q < 1) return utilLineErr(err, riga, tr("Serve chiave=valore"));
    std::string_view k = trimView(t.substr(0, q)), val = trimView(t.substr(q + 1));
    if (sec == WGT) {
      const char* why = nullptr;
      if (viewEqI(k, "source")) {
        if (val != "sd") return utilLineErr(err, riga, tr("source: l'unica sorgente del firmware e sd"));
        viewCopy(v.source, sizeof v.source, val);
      } else if (!modWidgetKey(k, val, v.widget, &why)) return utilLineErr(err, riga, why);
      continue;
    }
    if (!chiaveModulo(v, k, val, fileName, riga, err, haBus)) return false;
  }
  if (!visto[MOD] || !v.name[0]) { err = tr("Manca [module] con nome="); return false; }
  if (!haBus) { err = tr("Manca bus="); return false; }
  if (!visto[SIG]) { err = tr("Manca la sezione [signals] con almeno un segnale"); return false; }
  if (!segnaliOk(v, err)) return false;
  if (v.bus == VB_I2C && (v.addr == 0 || v.khz == 0)) { err = tr("Bus i2c: servono addr= e freq="); return false; }
  if (v.bus != VB_I2C && v.addr) { err = tr("addr vale solo per il bus i2c"); return false; }
  if ((v.bus == VB_SPI || v.bus == VB_SD) && v.khz == 0) v.khz = v.bus == VB_SD ? IO_SD_KHZ_DEF : 1000;
  if (v.bus == VB_SD && (v.khz < IO_SD_KHZ_MIN || v.khz > IO_SD_KHZ_MAX)) { err = tr("khz: da 400 a 20000"); return false; }
  if (v.bus == VB_SPI && v.khz > 10000) { err = tr("SPI: velocita da 100 a 10000 kHz"); return false; }
  if (v.bus == VB_GPIO && v.khz) { err = tr("freq e khz non valgono per il bus gpio"); return false; }
  v.widget.on = visto[WGT];
  if (v.widget.on) {
    if (!v.widget.who[0]) strcpy(v.widget.who, "oper");
    if (!v.widget.title[0]) snprintf(v.widget.title, sizeof v.widget.title, "%s", v.title[0] ? v.title : v.name);
    if (!v.widget.alt[0]) snprintf(v.widget.alt, sizeof v.widget.alt, "%s", v.widget.title);
  } else if (v.source[0]) { err = tr("source= serve dentro [widget]"); return false; }
  if (v.title[0] == 0) strcpy(v.title, v.name);
  if (visto[ICO]) {
    std::string_view ic = trimView(std::string_view(v.icon, iconLen));
    memmove(v.icon, ic.data(), ic.size()); v.icon[ic.size()] = 0;
    if (!utilSvgSafe(v.icon, MOD_ICON_MAX)) { err = tr("Icona non ammessa (un solo svg con forme semplici, massimo 2 KB)"); return false; }
  }
  vos_elf_param_t par; memset(&par, 0, sizeof par);                // l'ELF: struttura e simboli (il file, non i diritti)
  par.ruolo = ROLE_ADMIN;
  par.use = "log sd dev gpio i2c spi time";
  vos_elf_info_t info; memset(&info, 0, sizeof info);
  uint8_t* blob = (uint8_t*)malloc(v.elfLen);                       // il controllo vuole un blocco allineato a 4: il file ha il testo davanti
  if (!blob) { err = tr("Memoria insufficiente"); return false; }
  memcpy(blob, file + v.elfOff, v.elfLen);
  int e = vos_elf_valida(blob, v.elfLen, &par, &info);
  if (e != VOS_ELF_OK) { free(blob); err = vos_elf_err_testo(e); return false; }
  if (elf) *elf = blob; else free(blob);
  return true;
}
#endif
