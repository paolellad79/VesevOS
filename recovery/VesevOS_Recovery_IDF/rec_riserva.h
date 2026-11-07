// VesevOS Recovery - copia di riserva del firmware sulla microSD (contratto sez. 3 quater). Spazio NVS "vosris", scritto da VesevOS.
#pragma once
#include "rec_riserva_scelta.h"

RisScelta recRiservaScelta();          // all'avvio, se non c'e un ordine da eseguire
void recRiservaRiprova();              // secondo avvio del firmware nuovo; se ritorna, non e partito
void recRiservaUsa();                  // rimette la riserva: se riesce riavvia e non torna; se torna, resta la pagina
void recRiservaChiudi();               // un ripristino interrotto: si resta nella pagina
void recRiservaDisarma();              // app0 non e stata toccata: la riserva non serve
