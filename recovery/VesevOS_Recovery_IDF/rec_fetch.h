// VesevOS Recovery - modo 1 dell'ordine: scarica l'immagine da un indirizzo HTTPS e la installa (senza pagina e senza login).
#pragma once

// Esegue l'ordine di modo 1 (la rete di casa deve essere gia attiva). Scrive sempre l'esito in NVS.
// Se riesce riavvia la scheda e non ritorna; se fallisce ritorna e il recovery resta con la sua pagina normale.
void recFetchRun();
