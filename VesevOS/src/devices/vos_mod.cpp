// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_mod.cpp (vedi vos_mod.h): lettura e controllo delle schede modulo .vdb (colori, sezioni, modParse). L'elenco dei file, i JSON, il testo e l'installazione stanno in vos_mod_files.cpp.
#include "vos_mod.h"
#include <strings.h>
#include "vos_io.h"
#include "vos_mod_line.h"
#include "../core/vos_i18n.h"
#include "../core/vos_pacchetto.h"
#include "../core/vos_log.h"
#include "../core/vos_util.h"
#include "../core/vos_files.h"
#include "../core/vos_vdesc.h"

static bool txtOk(std::string_view s, size_t mx) {
  if (s.size() < 1 || s.size() > mx) return false;
  for (char ch : s) { unsigned char c = (unsigned char)ch; if (c < 32 || c == 127) return false; }
  return true;
}
static bool sigOk(std::string_view s) {
  if (s.size() < 1 || s.size() > 8) return false;
  for (char c : s) { if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false; }
  return true;
}
// Toglie gli spazi ai lati di un testo in un buffer (come trim di Arduino). Ritorna la nuova lunghezza.
static size_t trimBuf(char* b) {
  std::string_view t = trimView(std::string_view(b));
  memmove(b, t.data(), t.size());
  b[t.size()] = 0;
  return t.size();
}

// colori dei fili: nomi in inglese e italiano; cname e il nome base (la pagina lo traduce per chi non vede i colori)
bool modColor(const char* vIn, char* hex, char* cname) {
  static const struct { const char* n; const char* alias; const char* hex; } K[] = {
    {"red", "rosso", "#e53935"}, {"black", "nero", "#212121"}, {"blue", "blu", "#1e88e5"}, {"green", "verde", "#43a047"},
    {"yellow", "giallo", "#fdd835"}, {"orange", "arancio", "#fb8c00"}, {"white", "bianco", "#f5f5f5"}, {"brown", "marrone", "#6d4c41"},
    {"purple", "viola", "#8e24aa"}, {"gray", "grigio", "#9e9e9e"}, {"pink", "rosa", "#ec407a"}, {"cyan", "azzurro", "#00acc1"}};
  char v[16]; size_t n = 0;
  while (*vIn == ' ' || *vIn == '\t') vIn++;
  for (; *vIn && n < sizeof v - 1; vIn++) v[n++] = (*vIn >= 'A' && *vIn <= 'Z') ? (char)(*vIn + 32) : *vIn;
  if (*vIn) return false;
  while (n && (v[n - 1] == ' ' || v[n - 1] == '\t' || v[n - 1] == '\r')) n--;
  v[n] = 0;
  if (!strcmp(v, "arancione")) strcpy(v, "arancio");
  if (!strcmp(v, "grey")) strcpy(v, "gray");
  for (size_t i = 0; i < sizeof(K) / sizeof(K[0]); i++)
    if (!strcmp(v, K[i].n) || !strcmp(v, K[i].alias)) { strcpy(hex, K[i].hex); strcpy(cname, K[i].n); return true; }
  if (n == 7 && v[0] == '#') {
    for (int i = 1; i < 7; i++) if (!((v[i] >= '0' && v[i] <= '9') || (v[i] >= 'a' && v[i] <= 'f'))) return false;
    strcpy(hex, v); cname[0] = 0; return true;
  }
  return false;
}

bool modSectionKnown(std::string_view s) {
  static const char* const N[] = {"module", "signals", "icon", "wiring", "pins", "page", "code", "data", "commands", "cmd", "widget"};
  for (const char* n : N) if (viewEqI(s, n)) return true;
  return false;
}

bool modParse(std::string_view text, const char* fileName, ModInfo& m, ErrBuf& err) {
  memset(&m, 0, sizeof(m));
  strcpy(m.power, "-"); strcpy(m.ver, "1"); strcpy(m.who, "admin"); m.khz = IO_SD_KHZ_DEF;
  m.pull = 'n'; m.level = '3'; m.size = text.size(); err = "";
  if (text.size() > MOD_FILE_MAX) { err = tr("File troppo grande (massimo 16 KB)"); return false; }
  enum { NONE, MOD, SIG, ICO, WIR, PIN, PAG, COD, DAT, CMS, CMC, WGT };
  int sec = NONE; bool seen[12] = {false}; bool hasPull = false; bool hasFreq = false; bool hasKhz = false;
  size_t iconLen = 0, wiringLen = 0, codeLen = 0, cmdLen = 0; bool cmdHasCode = false, cmdHasWidget = false;                    // m.icon e m.wiring si riempiono riga per riga; di [code], [page] e [data] si contano solo le righe (le controlla vos_mod_app)
  int pageRows = 0, dataRows = 0;
  char cmdSubs[MOD_CMD_MAX][13];                                     // i sottocomandi gia visti (niente doppioni)
  ModCmd firstCmd{};                                                 // prima riga di [commands]: tutte le altre hanno la stessa parola
  LineIter lines(text);
  std::string_view raw;
  while (lines.next(raw)) {
    int line = lines.line();
    std::string_view t = trimView(raw);
    if (t.size() > 1 && t[0] == '[' && t.back() == ']') {
      std::string_view s = t.substr(1, t.size() - 2);
      int ns = viewEqI(s, "module") ? MOD : viewEqI(s, "signals") ? SIG : viewEqI(s, "icon") ? ICO : viewEqI(s, "wiring") ? WIR : viewEqI(s, "pins") ? PIN
             : viewEqI(s, "page") ? PAG : viewEqI(s, "code") ? COD : viewEqI(s, "data") ? DAT : viewEqI(s, "commands") ? CMS : viewEqI(s, "cmd") ? CMC : viewEqI(s, "widget") ? WGT : NONE;
      if (ns == NONE && sec != COD && sec != CMC) return utilLineErr(err, line, tr("Sezione sconosciuta (ammesse: module, signals, icon, wiring, pins, page, code, data, commands, cmd, widget)"));
      if (ns != NONE) {                                              // dentro [code] una riga tra parentesi quadre che non e una sezione e codice
        if (seen[ns]) return utilLineErr(err, line, tr("Sezione ripetuta"));
        seen[ns] = true; sec = ns; continue;
      }
    }
    if (sec == COD) {
      codeLen += raw.size() + 1;
      if (codeLen > MOD_CODE_MAX + 8) return utilLineErr(err, line, tr("Codice troppo lungo (massimo 8 KB)"));
      continue;
    }
    if (sec == CMC) {
      cmdLen += raw.size() + 1;
      if (!t.empty() && t[0] != '#') cmdHasCode = true;                  // righe vuote e commenti non sono codice
      if (t.rfind("def on_widget(", 0) == 0) cmdHasWidget = true;
      if (cmdLen > MOD_CMDCODE_MAX + 8) return utilLineErr(err, line, tr("Codice dei comandi troppo lungo (massimo 4 KB)"));
      continue;
    }
    if (t.empty() || t[0] == '#') continue;
    if (sec == NONE) return utilLineErr(err, line, tr("Riga fuori da una sezione"));
    if (sec == PAG) { if (++pageRows > MOD_PAGE_MAX) return utilLineErr(err, line, tr("Troppi controlli (massimo 12)")); continue; }
    if (sec == CMS) {
      ModCmd c; const char* why = nullptr;
      if (!modCmdLine(t, c, &why)) return utilLineErr(err, line, why);
      if (m.ncmd == 0) firstCmd = c;
      else if (strcmp(c.cmd, firstCmd.cmd)) return utilLineErr(err, line, tr("Tutti i comandi di un driver hanno la stessa prima parola"));
      if (m.ncmd >= MOD_CMD_MAX) return utilLineErr(err, line, tr("Troppi comandi (massimo 12)"));
      for (int k = 0; k < m.ncmd; k++) if (!strcmp(cmdSubs[k], c.sub)) return utilLineErr(err, line, tr("Sottocomando ripetuto"));
      memcpy(cmdSubs[m.ncmd], c.sub, sizeof cmdSubs[0]);
      m.ncmd++; snprintf(m.cmdWord, sizeof m.cmdWord, "%s", c.cmd); continue;
    }
    if (sec == DAT) { if (++dataRows > MOD_PAGE_MAX) return utilLineErr(err, line, tr("Troppi dati (massimo 12)")); continue; }
    if (sec == ICO) {
      if (iconLen + t.size() + 1 > MOD_ICON_MAX + 8) return utilLineErr(err, line, tr("Icona non ammessa (un solo svg con forme semplici, massimo 2 KB)"));
      bufAppend(m.icon, sizeof m.icon, iconLen, t); bufAppend(m.icon, sizeof m.icon, iconLen, "\n");
      continue;
    }
    if (sec == WIR) {
      if (wiringLen + t.size() + 1 > MOD_WIRING_MAX + 8) return utilLineErr(err, line, tr("Schema non ammesso (un solo svg con forme semplici, massimo 4 KB)"));
      bufAppend(m.wiring, sizeof m.wiring, wiringLen, t); bufAppend(m.wiring, sizeof m.wiring, wiringLen, "\n");
      continue;
    }
    if (sec == SIG) {
      if (!sigOk(t)) return utilLineErr(err, line, tr("Segnale non valido (1-8 caratteri: lettere, numeri, _)"));
      if (m.nsig >= MOD_SIG_MAX) return utilLineErr(err, line, tr("Troppi segnali (massimo 8)"));
      for (int i = 0; i < m.nsig; i++) if (viewEqI(t, m.sig[i])) return utilLineErr(err, line, tr("Segnale ripetuto"));
      viewCopy(m.sig[m.nsig], sizeof m.sig[0], t); m.nsig++; continue;
    }
    size_t q = t.find('=');
    if (q == std::string_view::npos || q < 1) return utilLineErr(err, line, tr("Serve chiave=valore"));
    if (sec == WGT) {
      const char* why = nullptr;
      if (!modWidgetKey(trimView(t.substr(0, q)), trimView(t.substr(q + 1)), m.widget, &why)) return utilLineErr(err, line, why);
      continue;
    }
    if (sec == PIN) {
      std::string_view lab = trimView(t.substr(0, q)), rest = trimView(t.substr(q + 1));
      size_t cm = rest.find(',');
      if (cm == std::string_view::npos || cm < 1) return utilLineErr(err, line, tr("pins: ETICHETTA=destinazione,colore"));
      std::string_view dst = trimView(rest.substr(0, cm)), col = trimView(rest.substr(cm + 1));
      if (!sigOk(lab) || !sigOk(dst)) return utilLineErr(err, line, tr("pins: ETICHETTA=destinazione,colore"));
      if (m.nwp >= MOD_PINS_MAX) return utilLineErr(err, line, tr("Troppi fili in [pins] (massimo 8)"));
      for (int i = 0; i < m.nwp; i++) if (viewEqI(lab, m.wp[i].lab)) return utilLineErr(err, line, tr("Pin ripetuto in [pins]"));
      ModInfo::WirePin& w = m.wp[m.nwp];
      char colb[24];                                                 // un colore piu lungo non e comunque valido
      if (!viewCopy(colb, sizeof colb, col) || !modColor(colb, w.hex, w.cname)) return utilLineErr(err, line, tr("Colore non valido (nome come red, rosso, nero, oppure #rrggbb)"));
      viewCopy(w.lab, sizeof w.lab, lab); viewCopy(w.dst, sizeof w.dst, dst);
      m.nwp++; continue;
    }
    std::string_view k = trimView(t.substr(0, q)), v = trimView(t.substr(q + 1));
    char vb[96];                                                     // il valore con lo zero finale, per class= e addr=: uno piu lungo non e comunque valido
    bool vfit = viewCopy(vb, sizeof vb, v);
    k = pacChiave(k);
    if (viewEqI(k, "name")) { if (!utilNameOk(v.data(), v.size()) || v != std::string_view(fileName)) return utilLineErr(err, line, tr("Nome non valido o diverso dal nome del file")); viewCopy(m.name, sizeof m.name, v); }
    else if (viewEqI(k, "title")) { if (!txtOk(v, 32)) return utilLineErr(err, line, tr("Testo non valido (1-32 caratteri, niente caratteri di controllo)")); viewCopy(m.title, sizeof m.title, v); }
    else if (viewEqI(k, "class")) {
      uint8_t c; if (!vfit || !ioParseCls(vb, c) || !viewCopy(m.cls, sizeof m.cls, v)) return utilLineErr(err, line, tr("Classe non valida (rele, led, pin, ingresso, analogico, i2c, sd)"));
    }
    else if (viewEqI(k, "driver")) { if (v != "bmp280") return utilLineErr(err, line, tr("Driver I2C sconosciuto (ammesso: bmp280)")); strcpy(m.drv, "bmp280"); }
    else if (viewEqI(k, "addr")) {
      char* e = nullptr; long a = vfit ? strtol(vb, &e, 0) : 0;
      if (!vfit || v.empty() || *e || a < 0x08 || a > 0x77) return utilLineErr(err, line, tr("Indirizzo I2C non valido (da 0x08 a 0x77)"));
      m.addr = (int)a;
    }
    else if (viewEqI(k, "khz")) {
      char* e = nullptr; long f = vfit ? strtol(vb, &e, 10) : 0;
      if (!vfit || v.empty() || *e || f < IO_SD_KHZ_MIN || f > IO_SD_KHZ_MAX) return utilLineErr(err, line, tr("khz: da 400 a 20000"));
      m.khz = (int)f; hasKhz = true;
    }
    else if (viewEqI(k, "use")) { if (!txtOk(v, 99)) return utilLineErr(err, line, tr("Permessi non validi (come use= di una app)")); viewCopy(m.use, sizeof m.use, v); }
    else if (viewEqI(k, "who")) { if (v != "admin" && v != "oper") return utilLineErr(err, line, tr("Chi la usa: admin oppure oper")); viewCopy(m.who, sizeof m.who, v); }
    else if (viewEqI(k, "autostart")) { if (v != "allowed" && v != "required" && v != "never") return utilLineErr(err, line, tr("autostart: allowed, required oppure never")); viewCopy(m.autostart, sizeof m.autostart, v == "allowed" ? std::string_view() : v); }
    else if (viewEqI(k, "freq")) { if (v != "100000" && v != "400000") return utilLineErr(err, line, tr("freq: 100000 oppure 400000")); m.fast = (v == "400000"); hasFreq = true; }
    else if (viewEqI(k, "active")) { if (v != "low" && v != "high") return utilLineErr(err, line, tr("active: low oppure high")); m.low = (v == "low"); }
    else if (viewEqI(k, "pull")) { if (v != "none" && v != "up" && v != "down") return utilLineErr(err, line, tr("pull: none, up oppure down")); m.pull = v == "up" ? 'u' : v == "down" ? 'd' : 'n'; hasPull = true; }
    else if (viewEqI(k, "power")) {
      if (!viewEqI(v, "3V3") && !viewEqI(v, "5V") && !viewEqI(v, "12V") && !viewEqI(v, "24V") && !viewEqI(v, "230V") && !viewEqI(v, "NONE")) return utilLineErr(err, line, tr("power: 3V3, 5V, 12V, 24V, 230V oppure none"));
      if (viewEqI(v, "NONE")) strcpy(m.power, "-"); else viewCopyUpper(m.power, sizeof m.power, v);
    }
    else if (viewEqI(k, "level")) { if (!viewEqI(v, "3V3") && !viewEqI(v, "5V")) return utilLineErr(err, line, tr("level: 3V3 oppure 5V")); m.level = viewEqI(v, "5V") ? '5' : '3'; }
    else if (viewEqI(k, "part")) {
      size_t c1 = v.find(','), c2 = v.rfind(',');
      if (c1 == std::string_view::npos || c1 < 1 || c2 <= c1) return utilLineErr(err, line, tr("part: serie|gnd|vcc,valore,segnale"));
      std::string_view pp = trimView(v.substr(0, c1)), pv = trimView(v.substr(c1 + 1, c2 - c1 - 1)), ps = trimView(v.substr(c2 + 1));
      if (!viewEqI(pp, "serie") && !viewEqI(pp, "gnd") && !viewEqI(pp, "vcc")) return utilLineErr(err, line, tr("part: serie|gnd|vcc,valore,segnale"));
      if (!txtOk(pv, 12) || pv.find('"') != std::string_view::npos || pv.find('\\') != std::string_view::npos || pv.find('<') != std::string_view::npos) return utilLineErr(err, line, tr("Valore del componente non valido (1-12 caratteri)"));
      if (!sigOk(ps)) return utilLineErr(err, line, tr("Segnale non valido (1-8 caratteri: lettere, numeri, _)"));
      if (m.npart >= MOD_PART_MAX) return utilLineErr(err, line, tr("Troppi componenti (massimo 4)"));
      m.part[m.npart].pos = viewEqI(pp, "serie") ? 's' : viewEqI(pp, "gnd") ? 'g' : 'v';
      viewCopy(m.part[m.npart].val, sizeof m.part[0].val, pv);
      viewCopy(m.part[m.npart].sig, sizeof m.part[0].sig, ps); m.npart++;
    }
    else if (viewEqI(k, "author")) { if (!txtOk(v, 24)) return utilLineErr(err, line, tr("Testo non valido (1-24 caratteri, niente caratteri di controllo)")); viewCopy(m.author, sizeof m.author, v); }
    else if (viewEqI(k, "license")) { if (!txtOk(v, 24)) return utilLineErr(err, line, tr("Testo non valido (1-24 caratteri, niente caratteri di controllo)")); viewCopy(m.lic, sizeof m.lic, v); }
    else if (viewEqI(k, "warn")) { if (!txtOk(v, 80)) return utilLineErr(err, line, tr("Avviso non valido (1-80 caratteri)")); viewCopy(m.warn, sizeof m.warn, v); }
    else if (viewEqI(k, "version")) {
      if (!pacVersioneOk(v)) return utilLineErr(err, line, tr("Versione non valida (solo cifre e punti, massimo 10)"));
      viewCopy(m.ver, sizeof m.ver, v);
    }
    else return utilLineErr(err, line, tr("Chiave sconosciuta"));
  }
  if (!seen[MOD] || m.name[0] == 0) { err = tr("Manca [module] con nome="); return false; }
  if (!m.cls[0]) { err = tr("Manca class="); return false; }
  if (m.nsig == 0) { err = tr("Manca la sezione [signals] con almeno un segnale"); return false; }
  for (int i = 0; i < m.npart; i++) {            // ogni componente deve riferirsi a un segnale del modulo
    bool f = false; for (int s = 0; s < m.nsig; s++) if (!strcasecmp(m.part[i].sig, m.sig[s])) { f = true; strncpy(m.part[i].sig, m.sig[s], 8); }
    if (!f) { err = tr("part: il segnale non e nella sezione [signals]"); return false; }
  }
  if (!strcmp(m.cls, "i2c")) {
    if (!m.drv[0]) { err = tr("Manca driver= (classe i2c)"); return false; }
    if (m.addr != 0x76 && m.addr != 0x77) { err = tr("Indirizzo I2C del BMP280: 0x76 oppure 0x77"); return false; }
    if (m.nsig != 2 || strcmp(m.sig[0], "SDA") || strcmp(m.sig[1], "SCL")) { err = tr("Classe i2c: i segnali sono SDA e SCL, in questo ordine"); return false; }
    if (m.low || hasPull) { err = tr("active e pull non valgono per la classe i2c"); return false; }
  } else if (!strcmp(m.cls, "sd")) {
    if (m.nsig != 4 || strcmp(m.sig[0], "CS") || strcmp(m.sig[1], "MOSI") || strcmp(m.sig[2], "SCK") || strcmp(m.sig[3], "MISO")) { err = tr("Classe sd: i segnali sono CS, MOSI, SCK, MISO, in questo ordine"); return false; }
    if (m.low || hasPull || m.drv[0] || m.addr || hasFreq) { err = tr("Classe sd: active, pull, driver, addr e freq non valgono"); return false; }
  } else if (m.drv[0] || m.addr || hasFreq) { err = tr("driver, addr e freq valgono solo per la classe i2c"); return false; }
  if (hasKhz && strcmp(m.cls, "sd")) { err = tr("khz vale solo per la classe sd"); return false; }
  m.hasApp = seen[COD] && codeLen > 0;
  if (seen[CMS] != seen[CMC] || (seen[CMS] && (m.ncmd == 0 || !cmdHasCode))) { err = tr("I comandi servono sia [commands] sia [cmd], entrambe non vuote"); return false; }
  m.hasCmd = seen[CMS];
  if (seen[WGT]) {                                  // il widget vive di on_widget() nel codice dei comandi
    if (!m.hasCmd || !cmdHasWidget) { err = tr("Il widget ha bisogno dei comandi: servono [commands] e [cmd] con la funzione on_widget()"); return false; }
    m.widget.on = true;
    if (!m.widget.who[0]) strcpy(m.widget.who, "oper");
  }
  if ((seen[PAG] || seen[DAT]) && !m.hasApp) { err = tr("Pagina e dati servono solo con la sezione [code]"); return false; }
  if (m.use[0] && !m.hasApp && !m.hasCmd) { err = tr("I permessi servono solo con [code] o con i comandi"); return false; }
  if (seen[PIN]) {                                  // ogni filo va a vcc, gnd o a un segnale del modulo; ogni segnale deve avere il suo filo
    bool wired[MOD_SIG_MAX] = {false};
    for (int i = 0; i < m.nwp; i++) {
      char* d = m.wp[i].dst;
      if (!strcasecmp(d, "vcc") || !strcasecmp(d, "gnd")) { strcpy(d, !strcasecmp(d, "vcc") ? "VCC" : "GND"); continue; }
      bool f = false; for (int s = 0; s < m.nsig; s++) if (!strcasecmp(d, m.sig[s])) { f = true; wired[s] = true; strncpy(m.wp[i].dst, m.sig[s], 8); }
      if (!f) { err = tr("pins: la destinazione non e vcc, gnd o un segnale di [signals]"); return false; }
    }
    for (int s = 0; s < m.nsig; s++) if (!wired[s]) { err = tr("pins: manca il filo di un segnale"); return false; }
  }
  if (hasPull && strcmp(m.cls, "ingresso")) { err = tr("pull vale solo per la classe ingresso"); return false; }
  if (m.low && !strcmp(m.cls, "analogico")) { err = tr("active=low non vale per la classe analogico"); return false; }
  if (m.title[0] == 0) strcpy(m.title, m.name);
  if (m.widget.on && !m.widget.title[0]) strcpy(m.widget.title, m.title);
  if (seen[ICO]) { trimBuf(m.icon); if (!utilSvgSafe(m.icon, MOD_ICON_MAX)) { err = tr("Icona non ammessa (un solo svg con forme semplici, massimo 2 KB)"); return false; } } else m.icon[0] = 0;
  if (seen[WIR]) { trimBuf(m.wiring); if (!utilSvgSafe(m.wiring, MOD_WIRING_MAX)) { err = tr("Schema non ammesso (un solo svg con forme semplici, massimo 4 KB)"); return false; } } else m.wiring[0] = 0;
  return true;
}
