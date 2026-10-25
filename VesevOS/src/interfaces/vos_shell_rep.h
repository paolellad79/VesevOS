// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_rep.h
// Piccoli aiuti per i riepiloghi di testo della shell (vesevos-config, vesevos-network): titolo di sezione, righe "etichetta  valore"
// allineate in colonna, stampa con tetto. Il testo si scrive in un HeapBuf con tetto e si stampa alla fine, tutto insieme.
// Le etichette e i titoli si scrivono con TRK(testo) (si traducono qui); i valori sono gia testi finiti.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "../core/vos_heapbuf.h"
#include "../core/vos_i18n.h"
#include "../drivers/vos_drv_print.h"
#include "vos_shell_cmdtab.h"      // TRK

namespace shx {

const int REP_LABEL = 16;          // larghezza della colonna delle etichette

// Titolo di una sezione, con una riga vuota prima (tranne la prima sezione: h vuoto).
inline void repHead(HeapBuf& h, const char* titleKey) {
  if (h.length()) h.add('\n');
  h.addf("%s\n", tr(titleKey));
}

// Una riga "  etichetta   valore". L'etichetta si traduce; il valore e gia un testo finito.
inline void repRow(HeapBuf& h, const char* labelKey, const char* value) {
  h.addf("  %-*s %s\n", REP_LABEL, tr(labelKey), value);
}

// Come repRow, ma l'etichetta e un nome proprio o una sigla (HTTP, mDNS...): non si traduce.
inline void repRaw(HeapBuf& h, const char* label, const char* value) {
  h.addf("  %-*s %s\n", REP_LABEL, label, value);
}

// Una riga "  comando   spiegazione" per l'elenco dei comandi che cambiano un'impostazione (il comando e scritto cosi com'e).
inline void repCmd(HeapBuf& h, const char* command, const char* briefKey) {
  h.addf("  %-34s %s\n", command, tr(briefKey));
}

// Stampa il testo; se manca la memoria o si e superato il tetto lo dice.
inline void repFlush(OutSink& o, const HeapBuf& h) {
  if (h.oom()) { o.println(tr("Memoria insufficiente")); return; }
  o.print(h.c_str());
  if (h.over()) o.println(tr("[uscita troncata: supera il tetto]"));
}

// Byte in forma corta con un decimale: 512 B, 3.4 KB, 12.0 MB, 1.9 GB (1 KB = 1024 byte).
inline void repBytes(uint64_t b, char* out, size_t n) {
  if (b < 1024ULL) { snprintf(out, n, "%lu B", (unsigned long)b); return; }
  const char* unit = "KB";
  uint64_t div = 1024ULL;
  if (b >= 1024ULL * 1024ULL * 1024ULL) { unit = "GB"; div = 1024ULL * 1024ULL * 1024ULL; }
  else if (b >= 1024ULL * 1024ULL) { unit = "MB"; div = 1024ULL * 1024ULL; }
  snprintf(out, n, "%lu.%lu %s", (unsigned long)(b / div), (unsigned long)((b % div) * 10ULL / div), unit);
}

inline const char* repYesNo(bool v) { return v ? tr("si") : tr("no"); }
inline const char* repOnOff(bool v) { return v ? tr("acceso") : tr("spento"); }

}  // namespace shx
