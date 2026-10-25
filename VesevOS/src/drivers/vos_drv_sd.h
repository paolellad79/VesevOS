// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_sd.h
// STRATO 1 (driver): CONTRATTO della scheda microSD (file system FAT/FAT32).
//
// Questo file e solo il contratto: niente Arduino, niente esp_*, niente FreeRTOS, solo tipi semplici.
// Il codice sta nei backend:
//   - drivers/idf/vos_drv_sd_idf.cpp          ESP-IDF nativo (VOS_BACKEND = VOS_BACKEND_IDF, di fabbrica dalla fase 5)
//   - tools/hosttest/fake_sd.cpp              prove sul computer
// Per cambiare backend si sostituisce solo il pezzo: il resto del firmware parla con queste
// funzioni e non sa cosa c'e sotto.
//
// Test di contratto, uguale per tutti i backend:
//   - sul computer: tools/hosttest/test_sd_contract.cpp
//   - sulla scheda: voce "sd" dell'autodiagnosi
//
// Ogni funzione prende un blocco interno (massimo 3 secondi): si puo chiamare da piu task.
// Se manca la scheda non succede nulla di grave: le funzioni rispondono "no".
// Con VOS_WITH_SD = 0 il driver non esiste (e nemmeno la periferica).
#pragma once
#include <stdint.h>
#include <stddef.h>

enum DrvSdType : uint8_t {
  DSD_NONE = 0,
  DSD_MMC = 1,
  DSD_SD = 2,
  DSD_SDHC = 3,
  DSD_UNKNOWN = 4
};

// Perche drvSdBegin e fallito.
enum DrvSdFail : uint8_t {
  DSF_NONE = 0,
  DSF_NO_CARD = 1,       // nessuna scheda o nessun modulo
  DSF_EXFAT = 2,         // la scheda e exFAT
  DSF_BAD_FS = 3,        // settore valido ma non FAT
  DSF_NO_RESPONSE = 4    // il modulo risponde ma i dati non sono validi (tutti 0x00/0xFF o senza firma)
};

enum DrvSdFs : uint8_t {
  DFS_UNKNOWN = 0,
  DFS_FAT12 = 1,
  DFS_FAT16 = 2,
  DFS_FAT32 = 3,
  DFS_EXFAT = 4
};

struct DrvSdEntry {
  char name[48];
  bool dir;
  uint32_t size;
};

// ---- Montaggio ----

// Monta la scheda. false = manca, non risponde o non e FAT/FAT32.
bool drvSdBegin(int cs, int mosi, int sck, int miso, uint32_t hz);

// Smonta e libera i pin (si puo chiamare sempre).
void drvSdEnd();

bool drvSdUp();

// DrvSdFail dell'ultimo drvSdBegin.
uint8_t drvSdLastFail();

// ---- Stato della scheda montata ----

// DrvSdFs del volume montato (legge il boot sector).
uint8_t drvSdFsType();

// La scheda risponde ancora? Se e stata tolta: smonta da sola e ritorna false.
bool drvSdAlive();

// DrvSdType.
uint8_t drvSdType();

uint64_t drvSdCardBytes();     // capacita della scheda
uint64_t drvSdTotalBytes();    // dimensione del volume FAT
uint64_t drvSdUsedBytes();

// ---- File e cartelle ----

// Byte letti (al massimo max), -1 = errore.
int drvSdRead(const char* path, uint8_t* buf, size_t max);

bool drvSdWrite(const char* path, const uint8_t* buf, size_t n, bool append);

// Voci scritte in out, -1 = errore. more = ce ne sono altre.
int drvSdList(const char* dir, DrvSdEntry* out, int max, bool& more);

// Solo file.
bool drvSdRemove(const char* path);

bool drvSdMkdir(const char* path);

// ---- Prova della scheda ----

// Scrive "bytes" byte in un file temporaneo, li rilegge, li confronta e cancella il file.
// Misura i tempi.
enum DrvSdProve : uint8_t {
  DSP_OK = 0,
  DSP_NOT_UP = 1,
  DSP_NO_WRITE = 2,
  DSP_DIFFERENT = 3,
  DSP_NO_MEM = 4
};

uint8_t drvSdProve(uint32_t bytes, uint32_t& writeMs, uint32_t& readMs);

// ---- Aiuti comuni ai backend (drivers/vos_drv_sd_common.cpp) ----
// La logica che non dipende dalla libreria sotto.

// Legge un settore da 512 byte. false = errore.
typedef bool (*DrvSdSectorFn)(void* ctx, uint8_t* buf512, uint32_t lba);

// Com'e il settore 0.
enum DrvSdSector0 : uint8_t {
  DS0_UNREADABLE = 0,
  DS0_EMPTY = 1,           // tutto 0x00 o tutto 0xFF
  DS0_NO_SIGNATURE = 2,    // dati senza la firma 55AA
  DS0_VALID = 3
};

// Legge il settore 0 (e, se e un MBR, quello della prima partizione).
// fs = DrvSdFs, sector0 = DrvSdSector0. buf512 = 512 byte di lavoro.
void drvSdProbeFs(DrvSdSectorFn read, void* ctx, uint8_t* buf512, uint8_t& fs, uint8_t& sector0);

// Millisecondi (lo da il backend, serve solo alla prova).
uint32_t drvSdMs();
