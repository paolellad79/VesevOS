// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_berry_https.cpp: la funzione Berry scarica(url, dimMax).
//
//   scarica("https://...", dimMax)  ->  testo del file (al massimo dimMax byte, da 1 a 32768)
//
// Passa dal driver vos_drv_https (certificato controllato, niente reindirizzamenti, tempo massimo).
// Accetta solo indirizzi https:// senza spazi e senza @. Non manda intestazioni nostre: nessun dato personale.
// In caso di errore lancia un'eccezione con il codice del driver (e il codice HTTP, se c'e).
// PERMESSI: gli script la hanno sempre; le app solo con "https" in use=.
#include "vos_berry_int.h"
#include "../core/vos_common.h"
#if VOS_WITH_BERRY
#include "../third_party/berry/src/berry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../core/vos_i18n.h"
#include "../drivers/vos_drv_https.h"

#define HTTPS_DIM_MAX   32768UL   // tetto assoluto di scarica()
#define HTTPS_TEMPO_MS  15000UL   // tempo massimo della scarica (collegamento compreso)
#define HTTPS_URL_MAX   200       // lunghezza massima dell'indirizzo

// Controlla l'indirizzo: solo https://, solo caratteri visibili ASCII, nessuna @ (niente utente e password nell'indirizzo).
static bool urlOk(const char* u) {
  size_t n = strlen(u);
  if (n < 9 || n > HTTPS_URL_MAX) return false;
  if (strncmp(u, "https://", 8) != 0) return false;
  for (size_t i = 0; i < n; i++) {
    if (u[i] <= ' ' || u[i] > '~') return false;
    if (u[i] == '@') return false;
  }
  return true;
}

// Testo di un esito del driver (solo per l'utente: il numero resta nel messaggio).
static const char* esitoTesto(DrvHttpsEsito e) {
  switch (e) {
    case HTTPS_E_MEMORIA:      return tr("memoria insufficiente");
    case HTTPS_E_COLLEGAMENTO: return tr("collegamento non riuscito (rete, certificato o ora)");
    case HTTPS_E_RISPOSTA:     return tr("il server ha risposto con un errore");
    case HTTPS_E_TROPPO:       return tr("il file e piu grande del tetto");
    case HTTPS_E_TEMPO:        return tr("tempo scaduto");
    case HTTPS_E_SCRITTURA:    return tr("lettura fermata");
    default:                   return tr("errore sconosciuto");
  }
}

static int bfScarica(bvm* vm) {
  if (be_top(vm) < 2 || !be_isstring(vm, 1) || !be_isint(vm, 2)) {
    be_raise(vm, "type_error", tr("scarica: serve un indirizzo https e il tetto in byte"));
  }
  const char* url = be_tostring(vm, 1);
  long dimMax = (long)be_toint(vm, 2);
  if (!urlOk(url)) be_raise(vm, "value_error", tr("scarica: l'indirizzo deve essere https:// (senza spazi, senza @, massimo 200 caratteri)"));
  if (dimMax < 1 || dimMax > (long)HTTPS_DIM_MAX) be_raise(vm, "value_error", tr("scarica: il tetto va da 1 a 32768 byte"));

  char* buf = (char*)malloc((size_t)dimMax + 1);      // il driver mette lo zero finale: il buffer ha un byte in piu
  if (!buf) be_raise(vm, "memory_error", tr("scarica: memoria insufficiente"));
  size_t n = 0;
  int stato = 0;
  DrvHttpsEsito e = drvHttpsScarica(url, buf, (size_t)dimMax + 1, n, HTTPS_TEMPO_MS, stato);
  if (e != HTTPS_OK) {
    char msg[160];
    {
      TrText m = trf("scarica: %s (codice %d, http %d)", esitoTesto(e), (int)e, stato);
      snprintf(msg, sizeof msg, "%s", m.c_str());
    }
    free(buf);                                        // prima di be_raise: non torna piu qui
    be_raise(vm, "io_error", msg);
  }
  be_pushnstring(vm, buf, n);
  free(buf);
  be_return(vm);
}

void berryHttpsRegister(bvm* vm) {
  be_regfunc(vm, "scarica", bfScarica);
}
#else
struct bvm;
void berryHttpsRegister(bvm*) {}
#endif
