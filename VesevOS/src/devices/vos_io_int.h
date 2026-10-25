// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_io_int.h
// Parti interne di vos_io: la tabella delle periferiche (IoDev) e il salvataggio sicuro di /io.cfg (vos_io_cfg.cpp). Non si include da fuori di devices/.
#pragma once
#include <stdint.h>
#include <stddef.h>

#define IO_FILE "/io.cfg"
#define IO_BAK  "/io.cfg.bak"                       // l'ultima versione buona di io.cfg
#define IO_TMP  "/io.cfg.tmp~"                      // nome riservato: i file .tmp~ non si toccano da fuori (vedi fsWriteText)
#define IO_JUNK_MAX 512                             // righe di io.cfg che questo avvio non ha caricato (pin occupato): si riscrivono uguali

// offAt: fine dell'impulso; blinkEnd: fine della prova lampeggio (0 = nessuna).
// i2c: gpio = SDA, gpio2 = SCL, addr = indirizzo, drv 1 = bmp280. sd: gpio = CS, gpio2 = MOSI, addr = SCK, drv = MISO, khz = frequenza SPI (in io.cfg e il settimo numero; se manca vale 1000).
struct IoDev { char name[12]; uint8_t cls; int8_t gpio; uint8_t flags; bool on; uint32_t last; uint32_t offAt; uint32_t blinkEnd, blinkNext; int8_t gpio2; uint8_t addr, drv; uint16_t khz; };

const char* ioCfgSave(const IoDev* list, int n, int skip, const IoDev* extra);   // scrive io.cfg (lista + righe tenute + extra, senza "skip"); nullptr = fatto, altrimenti il motivo. Chiamare con il blocco di vos_io
void ioCfgKeepRow(const char* row);      // riga valida ma non caricata (pin occupato): resta nel file
void ioCfgReset();                       // inizio di ioInit: dimentica righe tenute e avvisi
void ioCfgPend(uint8_t bits);            // avvisi dell'avvio da mandare all'audit: 1 copia usata, 2 illeggibile, 4 righe tenute
void ioCfgPendFlush();                   // li manda (ioTick, quando l'audit e pronto)

// Nomi (vos_io_name.cpp): sempre su buffer del chiamante, senza String.
#define IO_NAME_MAX 11                                  // caratteri di un nome (IoDev::name ne tiene 12 con lo zero finale)
bool        ioNameReserved(const char* n);              // parola con un altro significato (periferiche di sistema, permessi delle app, comandi)
const char* ioNameCheck(const char* n);                 // nullptr = nome valido (2-11 caratteri, a-z e 0-9, non libero), altrimenti il motivo
const char* ioNameNorm(const char* in, char* out);      // toglie gli spazi ai lati e mette in minuscolo; out >= 12 byte. nullptr = fatto (puo uscire vuoto), altrimenti il motivo (oltre 11 caratteri)

// Lettura della tabella per vos_io_text.cpp (elenco JSON e testo della shell). Si usano dentro il blocco (ioLockTake/ioLockGive).
bool          ioLockTake(uint32_t ticks);                 // true = preso (ricorsivo); false = scaduto o blocco non ancora creato
void          ioLockGive();
const IoDev*  ioDevAt(int i);                             // nullptr fuori elenco
const char*   ioClsName(uint8_t cls);
const char*   ioValueAt(int i, int& v);                   // valore della periferica i; nullptr = fatto, altrimenti il motivo (v resta com'e)
void          ioI2cRead(bool& have, int32_t& t100, uint32_t& pPa);   // ultima misura del sensore I2C
