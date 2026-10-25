// VesevOS Recovery - LED di stato (LED RGB WS2812, pin e luminosita letti da /vesevos.conf).
//   arancione lento = in attesa; azzurro veloce = scrittura in corso; verde fisso = finito; rosso fisso = errore (dopo 10 s torna in attesa).
// Lo stato si cambia da qualunque task (recLedSet); il LED lo muove solo recLedTick(), chiamata dal ciclo principale.
#pragma once

enum { LS_WAIT, LS_WRITE, LS_DONE, LS_ERR };

void recLedStart(int pin, int brightness);   // da qui in poi il recovery comanda il LED
void recLedSet(int state);
void recLedTick();
