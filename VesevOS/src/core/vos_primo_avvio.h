// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_primo_avvio.h
// Prima accensione (scheda non configurata): radio spenta finche non si preme il tasto BOOT, poi 10 minuti di hotspot aperto
// con il solo HTTPS. Le regole stanno in vos_primo_avvio_rule.h. A guida finita tutto torna come prima.
#pragma once
#include "vos_primo_avvio_rule.h"

void     primoAvvioInit(bool guidaFinita);   // una volta, dopo cfgLoad
bool     primoAvvioPressione();              // tasto BOOT: true se ha acceso la finestra adesso
void     primoAvvioFine();                   // la guida e finita (pagina o seriale): l'hotspot passa a protetto dopo 4 secondi
void     primoAvvioAttivita();               // richiesta con accesso durante la guida: rinnova i 10 minuti
PaEvento primoAvvioPasso(bool guidaFinita, int clienti);  // dal task di rete, una volta al secondo (clienti = collegati all'hotspot)
PaStato  primoAvvioStato();
bool     primoAvvioAttesa();                 // radio spenta in attesa del tasto?
bool     primoAvvioFinestra();               // hotspot aperto adesso?
bool     primoAvvioPendente();               // guida finita, ma il ritardo prima dell'evento FINITA non e ancora passato
bool     primoAvvioMinimo();                 // prima accensione (attesa o finestra): servizi ridotti al minimo
