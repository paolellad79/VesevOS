// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_io_int.h
// Parti interne di vos_io: la tabella delle periferiche (IoDev) e il salvataggio sicuro di /io.cfg (vos_io_cfg.cpp). Non si include da fuori di devices/.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../core/vos_common.h"
#include "../core/vos_errbuf.h"
#include <stdlib.h>
#include <string.h>
#include "../drivers/vos_drv_pwm.h"
#include "../drivers/vos_drv_contatore.h"

#define IO_FILE "/io.cfg"
#define IO_BAK  "/io.cfg.bak"                       // l'ultima versione buona di io.cfg
#define IO_TMP  "/io.cfg.tmp~"                      // nome riservato: i file .tmp~ non si toccano da fuori (vedi fsWriteText)
#define IO_JUNK_MAX 512                             // righe di io.cfg che questo avvio non ha caricato (pin occupato): si riscrivono uguali

// offAt: fine dell'impulso; blinkEnd: fine della prova lampeggio (0 = nessuna).
// i2c: gpio = SDA, gpio2 = SCL, addr = indirizzo, drv 1 = bmp280. sd: gpio = CS, gpio2 = MOSI, addr = SCK, drv = MISO, khz = frequenza SPI (in io.cfg e il settimo numero; se manca vale 1000).
// pwm: gpio = pin, gpio2 = -1, addr = duty 0-100 (non si salva: al riavvio 0), khz = frequenza in Hz (si salva, settimo numero), pwm = canale + 1 (0 = chiuso).
// contatore: gpio = pin, gpio2 = -1, flags = IOF_UP/IOF_DOWN (resistenza), addr = fronte (0 salita, 1 discesa, 2 entrambi), khz = filtro in ns (0 = nessuno; in io.cfg e il settimo numero), cnt = unita PCNT + 1 (0 = chiuso),
// cPrev = conteggio all'ultima stima, cT = ora dell'ultima stima (ms), cMhz = frequenza stimata in millihertz. Niente di questo si salva tranne pin, resistenza, fronte e filtro.
// led_rgb: gpio = pin, gpio2 = -1, on = acceso, cPrev = colore 0xRRGGBB, addr = luminosita 0-255, drv = ordine dei byte (VOS_LED_ORD_*), led = canale RMT + 1 (0 = chiuso).
// Si salva tutto (in io.cfg: flags con IOF_ACCESO, colore, luminosita, ordine): al riavvio il LED torna com'era.
struct IoDev { char name[12]; uint8_t cls; int8_t gpio; uint8_t flags; bool on; uint32_t last; uint32_t offAt; uint32_t blinkEnd, blinkNext; int8_t gpio2; uint8_t addr, drv; uint16_t khz; uint8_t pwm; uint8_t cnt; uint8_t led; uint32_t cPrev, cT, cMhz; };

// Numero dopo "chiave=" in una stringa di opzioni (def se manca)
inline int ioOptNum(const char* opt, const char* key, int def) {
  const char* q = strstr(opt, key);
  return q ? (int)strtol(q + strlen(key), nullptr, 10) : def;
}

const char* ioCfgSave(const IoDev* list, int n, int skip, const IoDev* extra);   // scrive io.cfg (lista + righe tenute + extra, senza "skip"); nullptr = fatto, altrimenti il motivo. Chiamare con il blocco di vos_io
bool ioCfgParseRiga(const char* l, char* nm, size_t nmCap, unsigned& c, int& g, unsigned& fl, int& g2, unsigned& ad, unsigned& dr, unsigned& kh);   // legge una riga; false = da saltare
void ioCfgKeepRow(const char* row);      // riga valida ma non caricata (pin occupato): resta nel file
void ioCfgReset();                       // inizio di ioInit: dimentica righe tenute e avvisi
void ioCfgPend(uint8_t bits);            // avvisi dell'avvio da mandare all'audit: 1 copia usata, 2 illeggibile, 4 righe tenute
void ioCfgPendFlush();                   // li manda (ioTick, quando l'audit e pronto)

// Migrazione dei nomi salvati (vos_io_migra.cpp): PUNTO UNICO. Un nome che ora e di una periferica di bordo (es. "led1" a mano su C6/C3) si cambia
// nel primo numero libero della sua classe. occupato(n) >= 0 = nome gia preso da una periferica caricata. Scrive il nuovo nome in out (>= 16 byte).
bool ioMigraNomeBordo(const char* nm, const char* prefisso, int (*occupato)(const char*), char* out);   // true = rinominato
bool ioMigraFatta();                                     // dall'ultimo ioInit ci sono state rinomine (si azzera leggendo): ioInit deve riscrivere io.cfg
int  ioMigraNumeroIniziale(const char* prefisso);        // primo numero per i nomi automatici di una classe: LED di bordo della classe + 1

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
void          ioI2cRead(bool& have, int32_t& t100, uint32_t& pPa);   // ultima misura del sensore I2C (vos_io_i2c.cpp)
void          ioI2cReset();                                          // sensore nuovo: stato da zero
bool          ioI2cHave();                                           // c'e una misura valida
void          ioI2cTick(uint32_t now);                               // misura nel loop (con il blocco preso)
IoDev*        ioFindDev(const char* name);                // la periferica per nome (nullptr se manca); valida dentro il blocco
const char*   ioSaveCfg();                                // riscrive io.cfg con l'elenco attuale; nullptr = fatto, altrimenti il motivo

// PWM (vos_io_pwm.cpp). Senza il package restano funzioni vuote, cosi vos_io.cpp non ha #if.
#if VOS_WITH_PWM
const char*   ioPwmStart(IoDev& d, const char* opt);      // legge hz= e duty= dalle opzioni e apre il canale. nullptr = fatto, altrimenti il motivo
const char*   ioPwmOpen(IoDev& d);                        // apre il canale con hz e duty della periferica (se non e gia aperto)
void          ioPwmClose(IoDev& d);
void          ioPwmOff(IoDev& d);                         // duty a 0 (arresto, watchdog)
#else
inline const char* ioPwmStart(IoDev&, const char*) { return nullptr; }
inline const char* ioPwmOpen(IoDev&) { return nullptr; }
inline void        ioPwmClose(IoDev&) {}
inline void        ioPwmOff(IoDev&) {}
#endif

// LED a colori a mano (vos_io_ledrgb.cpp). Senza il package restano funzioni vuote, cosi vos_io.cpp non ha #if.
#if VOS_WITH_LED_RGB_A_MANO
const char*   ioLedStart(IoDev& d, const char* opt);       // legge ordine= dalle opzioni, mette i valori di fabbrica e apre il canale. nullptr = fatto, altrimenti il motivo
const char*   ioLedOpen(IoDev& d);                         // apre il canale e riporta il LED a com'era (se non e gia aperto)
void          ioLedClose(IoDev& d);                        // spegne il LED e libera il canale
void          ioLedOff(IoDev& d);                          // spento (arresto, watchdog): non si salva, al riavvio torna com'era
bool          ioLedCfgOk(unsigned colore, unsigned lum, unsigned ordine);   // valori letti da io.cfg
void          ioLedCarica(IoDev& d, unsigned flags, unsigned colore, unsigned lum, unsigned ordine);   // riempie la periferica con i valori di io.cfg
bool          ioLedSetOn(IoDev& d, bool on, int role, ErrBuf& err);          // acceso/spento (Operatore), salva in io.cfg
const char*   ioLedOrdNome(uint8_t ordine);                // "grb", "rgb"...
#else
inline const char* ioLedStart(IoDev&, const char*) { return nullptr; }
inline const char* ioLedOpen(IoDev&) { return nullptr; }
inline void        ioLedClose(IoDev&) {}
inline void        ioLedOff(IoDev&) {}
inline bool        ioLedCfgOk(unsigned, unsigned, unsigned) { return false; }
inline void        ioLedCarica(IoDev&, unsigned, unsigned, unsigned, unsigned) {}
inline bool        ioLedSetOn(IoDev&, bool, int, ErrBuf&) { return false; }
inline const char* ioLedOrdNome(uint8_t) { return ""; }
#endif

// Contatore di impulsi (vos_io_contatore.cpp). Senza il package restano funzioni vuote, cosi vos_io.cpp non ha #if.
#if VOS_WITH_CONTATORE
const char*   ioCntStart(IoDev& d, const char* opt);      // legge fronte= e filtro= dalle opzioni e apre l'unita. nullptr = fatto, altrimenti il motivo
const char*   ioCntOpen(IoDev& d);                        // apre l'unita con fronte e filtro della periferica (se non e gia aperta)
void          ioCntClose(IoDev& d);
bool          ioCntCfgOk(unsigned fronte, unsigned filtroNs);   // valori letti da io.cfg
int           ioCntRead(const IoDev& d);                  // impulsi (0..2147483647)
void          ioCntTick(IoDev& d, uint32_t now);          // stima la frequenza (con il blocco preso)
#else
inline const char* ioCntStart(IoDev&, const char*) { return nullptr; }
inline const char* ioCntOpen(IoDev&) { return nullptr; }
inline void        ioCntClose(IoDev&) {}
inline bool        ioCntCfgOk(unsigned, unsigned) { return false; }
inline int         ioCntRead(const IoDev&) { return 0; }
inline void        ioCntTick(IoDev&, uint32_t) {}
#endif
