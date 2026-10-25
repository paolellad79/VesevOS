// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_sd.cpp
// Comando "sd" della shell (scheda microSD). Scritto senza il tipo String di Arduino: la riga arriva come testo semplice.
#include "vos_shell_int.h"
#include "../devices/vos_sd.h"
#if VOS_WITH_SD

#define SHP 100                 // lunghezza massima del percorso (come in vos_sd.cpp)

// parola numero idx (0 = comando) della riga, senza copiare; ritorna l'inizio e la lunghezza (0 = non c'e)
static const char* word(const char* s, int idx, size_t& len) {
  len = 0;
  for (int i = 0; ; i++) {
    while (*s == ' ' || *s == '\t') s++;
    if (!*s) return s;
    const char* b = s;
    while (*s && *s != ' ' && *s != '\t') s++;
    if (i == idx) { len = (size_t)(s - b); return b; }
  }
}

void shSd(const char* line, OutSink& o, int role) {
  size_t l1 = 0, l2 = 0, l3 = 0;
  const char* w1 = word(line, 1, l1);
  const char* w2 = word(line, 2, l2);
  const char* w3 = word(line, 3, l3);
  char a[12] = "", p[SHP + 2] = "", arg[SHP + 520] = "";
  if (l1 >= sizeof a) { o.println(tr("Azione non valida")); return; }
  memcpy(a, w1, l1); a[l1] = 0;
  if (l2 > SHP) { o.println(tr("Percorso non valido: parte da / e usa solo lettere, numeri, spazio e _ - . ( )")); return; }
  memcpy(p, w2, l2); p[l2] = 0;
  char err[SD_ERR_MAX];
  char* out = (char*)malloc(SD_OUT_MAX);                       // sulla pila non c'e posto per 5 KB
  if (!out) { o.println(tr("Memoria insufficiente")); return; }
  if (!a[0] || !strcmp(a, "status")) { sdText(out, SD_OUT_MAX); o.println(out); free(out); return; }
  if (!strcmp(a, "write") || !strcmp(a, "append")) snprintf(arg, sizeof arg, "%s,%s", p, l3 ? w3 : "");   // il testo e il resto della riga
  else snprintf(arg, sizeof arg, "%s", p);
  if (!sdAct(a, arg, role, out, SD_OUT_MAX, err, sizeof err)) o.println(err);
  else o.println(out[0] ? out : "ok");
  free(out);
}

#endif
