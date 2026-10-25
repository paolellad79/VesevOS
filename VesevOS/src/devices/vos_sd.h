// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_sd.h
// Vista della periferica "sd" di io.cfg (modulo SPI a 3,3 V, FAT/FAT32; pin e frequenza si scelgono da Periferiche): si monta e si smonta a richiesta (mai nell'avvio), si guarda lo stato,
// si fa una prova di scrittura/lettura, si elencano i file e si leggono/scrivono file piccoli. Si usa dall'API (/api/sd),
// dalla shell (comando "sd") e dalla pagina (riquadro nella scheda Periferiche). Hardware solo da drivers/vos_drv_sd.
// Scritto SENZA Arduino (niente String, niente millis): testo in buffer di char con lunghezza, cosi non aggiunge debito a checklayers.
// Con VOS_WITH_SD = 0 non esiste nulla di tutto questo.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../core/vos_common.h"
#if VOS_WITH_SD

#define SD_HOME_MAX 160         // grandezza consigliata del buffer di sdHomeJson
#define SD_OUT_MAX 5120         // grandezza consigliata del buffer "out" di sdAct (elenco di 32 voci nel caso peggiore)
#define SD_ERR_MAX 112          // grandezza consigliata del buffer "err"

bool   sdUp();                                                  // montata adesso?
bool   sdConfigured();                                          // c'e una periferica "sd" in io.cfg? (senza, nessuna azione e nessun pin occupato)
bool   sdMount(char* err, size_t en);                           // monta sui 4 pin e alla frequenza della periferica "sd" di io.cfg; se manca la scheda: errore chiaro, pin liberati, nessun riavvio
bool   sdUnmount(char* err, size_t en);                         // smonta e libera i pin (sicuro anche se la scheda e gia stata tolta)
bool   sdPathOk(const char* path, char* err, size_t en);        // "/cartella/file.txt": solo ASCII sicuro, mai "..", al massimo 100 caratteri
void   sdStatusJson(char* out, size_t on);                      // {"rows":[[etichetta,valore],...],"up":0/1,"type":..,"cap":KB,"total":KB,"used":KB}
bool   sdAct(const char* a, const char* arg, int role, char* out, size_t on, char* err, size_t en);   // state mount unmount test ls read write append rm mkdir
bool   sdProve(char* out, size_t on, char* err, size_t en);     // scrive 4 KB, li rilegge, confronta, cancella (solo se montata); out = JSON con i tempi
void   sdHomeJson(char* out, size_t on);                         // {"s":1..4,"fs":"FAT32","cap":KB,"free":KB,"used":KB,"khz":N} per il widget della Home; testo vuoto se la microSD non e configurata

// Stato del volume per vesevos-drive_manager. state: 1 montata, 2 non montata, 3 scheda non presente, 4 formato non supportato (come la Home).
enum : uint8_t { SD_ST_MOUNTED = 1, SD_ST_NOT_MOUNTED = 2, SD_ST_NO_CARD = 3, SD_ST_BAD_FORMAT = 4 };
struct SdVolume {
  uint8_t  state;
  char     fs[8];                 // "FAT32"...; vuoto se non e montata
  uint32_t totalKb;               // volume FAT (0 se non montata)
  uint32_t usedKb;
  uint32_t khz;                   // frequenza SPI scelta nella periferica
};
bool   sdVolume(SdVolume& v);                                   // false = nessuna periferica "sd" in io.cfg
bool   sdInUse();                                               // un'azione sui file o un montaggio e in corso adesso? (per chiedere conferma prima di smontare)
void   sdOpEnter();                                             // uso interno di sdAct: conta le azioni in corso
void   sdOpLeave();
bool   sdSpaceKb(uint32_t& totalKb, uint32_t& usedKb);          // dimensione e usato del volume in KB (per df); false se non e montata
void   sdText(char* out, size_t on);                            // una riga di stato (shell e autodiagnosi)

#endif
