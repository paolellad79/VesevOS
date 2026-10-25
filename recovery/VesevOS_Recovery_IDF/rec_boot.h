// VesevOS Recovery - partizione app0 (VesevOS), segni in NVS, passaggio a VesevOS, diagnostica dell'avvio.
#pragma once
#include <stddef.h>
#include <esp_partition.h>

const esp_partition_t* recApp0();
bool app0Valid(char* name, size_t nl, char* ver, size_t vl);   // c'e un programma valido in app0? (nome e versione se si)
bool app0Fw();                                                  // ... ed e VesevOS (marchio trovato)? Il risultato si ricorda
void app0FwForget();                                            // dopo aver scritto app0: ricontrolla
void app0FwSet(bool isFw);

bool stayFlag(bool set, bool val);   // segno lasciato da VesevOS ("recovery now"): resta nel recovery
bool handFlag(bool set, bool val);   // segno "sto passando a VesevOS": se al riavvio e ancora acceso, VesevOS non e partito

void startApp();                     // imposta app0 come avvio e riavvia (se non riesce, torna indietro dopo aver scritto il motivo)
void recDiag();                      // riga di diagnostica sulla seriale
void recInfo();                      // info complete (comando seriale i)
