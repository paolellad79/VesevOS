// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_sd.h
// Vista della periferica "sd" di io.cfg (modulo SPI a 3,3 V, FAT/FAT32; pin e frequenza si scelgono quando si installa il driver): si monta e si smonta a richiesta (mai nell'avvio),
// si guarda lo stato, si fa una prova di scrittura/lettura e si leggono/scrivono file piccoli. Sono PRIMITIVE: i comandi e la pagina di gestione stanno nel driver (docs/progetto/regole-driver.md),
// scritti in Berry (packages/vos_berry_sd.cpp). Hardware solo da drivers/vos_drv_sd.
// Scritto SENZA Arduino (niente String, niente millis): testo in buffer di char con lunghezza, cosi non aggiunge debito a checklayers.
// Con VOS_WITH_SD = 0 non esiste nulla di tutto questo.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../core/vos_common.h"
#if VOS_WITH_SD

#define SD_ERR_MAX 112
#define SD_FORMAT_WORD "FORMATTA"   // la parola da scrivere per confermare la formattazione (pagina, shell e primitive)          // grandezza consigliata del buffer "err"

bool   sdUp();                                                  // montata adesso?
bool   sdConfigured();                                          // c'e una periferica "sd" in io.cfg? (senza, nessuna azione e nessun pin occupato)
bool   sdMount(char* err, size_t en);                           // monta sui 4 pin e alla frequenza della periferica "sd" di io.cfg; se manca la scheda: errore chiaro, pin liberati, nessun riavvio
bool   sdUnmount(char* err, size_t en);                         // (espulsione a mano: da qui il rimontaggio automatico resta fermo fino a sdMount o al riavvio)
bool   sdFormat(char* err, size_t en);                          // formatta in FAT32 e ASPETTA la fine (CANCELLA TUTTO; solo su richiesta confermata di un Admin; anche se la scheda non si monta); alla fine resta montata. Lo chiama il task di vos_sd_fmt.cpp: gli altri usano sdFormatStart

// Formattazione in un task a parte (vos_sd_fmt.cpp): chi chiede torna subito, lo stato si guarda con sdFormatInfo.
struct SdFmtInfo {
  bool     running;               // formattazione in corso adesso
  uint32_t secs;                  // secondi trascorsi (solo se in corso)
  uint8_t  last;                  // ultima formattazione: 0 mai fatta dopo l'avvio, 1 riuscita, 2 non riuscita
  uint32_t lastSecs;              // durata dell'ultima
  char     err[SD_ERR_MAX];       // motivo, se non riuscita
};
bool   sdFormatStart(char* err, size_t en);                     // avvia il task; false (e err) se non si puo avviare adesso. Vale la parola di conferma gia controllata da chi chiama
bool   sdFormatting();                                          // una formattazione e in corso?
void   sdFormatInfo(SdFmtInfo& f);
const char* sdBusyText();                                       // "microSD occupata (formattazione)" durante la formattazione, altrimenti "Operazione gia in corso"
void   sdAutoStep();                                            // un passo del sondaggio di rimontaggio (vos_sd_auto.cpp lo chiama ogni 2-3 s)
typedef void (*SdLostFn)();
void   sdSetLostHook(SdLostFn fn);                              // chiamata (dal task del sondaggio, ogni 2,5 s al massimo) quando la microSD passa da montata a non montata: tolta, smontata a mano o in formattazione. Una sola funzione
void   sdAutoStart();                                           // avvia il task del sondaggio, una volta, a fine avvio (vos_sd_auto.cpp)                         // smonta e libera i pin (sicuro anche se la scheda e gia stata tolta)
bool   sdPathOk(const char* path, char* err, size_t en);        // "/cartella/file.txt": solo ASCII sicuro, mai "..", al massimo 100 caratteri
bool   sdProve(char* out, size_t on, char* err, size_t en);     // scrive 4 KB, li rilegge, confronta, cancella (solo se montata); out = JSON con i tempi

// Stato del volume per vesevos-drive_manager. state: 1 montata, 2 non montata, 3 scheda non presente, 4 formato non supportato (come la Home).
enum : uint8_t { SD_ST_MOUNTED = 1, SD_ST_NOT_MOUNTED = 2, SD_ST_NO_CARD = 3, SD_ST_BAD_FORMAT = 4 };
struct SdVolume {
  uint8_t  state;
  char     fs[8];                 // "FAT32"...; vuoto se non e montata
  uint32_t totalKb;               // volume FAT (0 se non montata)
  uint32_t usedKb;
  uint32_t khz;                   // frequenza SPI scelta nella periferica
};
int    sdStateNow();                                            // stato come SdVolume::state (1-4), 0 se la microSD non e configurata; non tocca la scheda
bool   sdSnapshot(SdVolume& v);                                 // ultima fotografia dello stato, SENZA toccare la scheda (la tiene il task della microSD, vos_sd_snap.cpp); false = non configurata
void   sdSnapshotStep();                                        // aggiorna la fotografia (lo chiama il task della microSD ogni 2-3 s)
bool   sdVolume(SdVolume& v);                                   // false = nessuna periferica "sd" in io.cfg
bool   sdInUse();                                               // un'operazione sui file o un montaggio e in corso adesso? (per chiedere conferma prima di smontare)
void   sdOpEnter();                                             // uso interno delle primitive dei file: conta le operazioni in corso
void   sdOpLeave();
bool   sdSpaceKb(uint32_t& totalKb, uint32_t& usedKb);          // dimensione e usato del volume in KB (per df); false se non e montata
// File a byte (per Berry, vos_sd_io.cpp). Il percorso e quello della scheda ("/dir/file", senza /media/sd) e passa da sdPathOk; la radice "/"
// non si scrive e non si cancella. Ogni chiamata conta come azione in corso (sdOpEnter/Leave): chi smonta la vede. Blocchi di al massimo SD_IO_MAX byte.
#define SD_IO_MAX 1024
int    sdReadAt(const char* path, uint32_t off, uint8_t* buf, size_t n, char* err, size_t en);        // byte letti (0 = oltre la fine), -1 = errore
bool   sdWriteBytes(const char* path, const uint8_t* buf, size_t n, bool append, char* err, size_t en);
// Un file grande in un solo passaggio: resta aperto per tutto il lavoro e conta come azione in corso dall'apertura alla chiusura. Uno alla volta.
bool   sdFlussoApri(const char* path, char* err, size_t en);
bool   sdFlussoScrivi(const uint8_t* buf, size_t n, char* err, size_t en);
bool   sdFlussoChiudi(char* err, size_t en);                    // sempre dopo Apri riuscita; false = il file puo non essere completo
bool   sdRemove(const char* path, char* err, size_t en);        // un file o una cartella vuota
bool   sdRename(const char* da, const char* a, char* err, size_t en);   // la destinazione non deve esistere
bool   sdMakeDir(const char* path, char* err, size_t en);
void   sdText(char* out, size_t on);                            // una riga di stato (shell e autodiagnosi)

#endif
