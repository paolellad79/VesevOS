// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_io.h
// Periferiche a classi create dall'utente: rele, led, pin (uscita), ingresso, analogico, i2c (sensore BMP280: due pin SDA e SCL), sd (lettore microSD: quattro pin CS, MOSI, SCK, MISO). Ognuna ha un nome (rele1, led1...),
// un pin controllato dal registro dei pin, e si usa da pagina, shell, app (use=rele1) e regole.
#pragma once
#include "../core/vos_errbuf.h"
#include "../drivers/vos_drv_string.h"
#include "../core/vos_heapbuf.h"

#define IO_MAX 16

enum IoClass : uint8_t { IC_NONE = 0, IC_RELE = 1, IC_LED = 2, IC_PIN = 3, IC_IN = 4, IC_ADC = 5, IC_I2C = 6, IC_SD = 7 };
#define IOF_INV   1     // attivo basso (acceso = livello 0)
#define IOF_UP    2     // ingresso con pull-up
#define IOF_DOWN  4     // ingresso con pull-down
#define IOF_FAST  8     // i2c a 400 kHz (altrimenti 100 kHz)

void   ioInit();                                         // carica /io.cfg (o /io.cfg.bak se illeggibile) e mette tutte le uscite nello stato sicuro (spente); non scrive mai
int    ioCount();
const char* ioNameAt(int i);   // nome della periferica i ("" se non c'e); vale finche la periferica esiste
bool   ioHas(const char* name);                           // i nomi e i testi in ingresso sono const char* (non nullo, terminato): una funzione che modifica l'elenco li copia prima
bool   ioAdd(const char* cls, int gpio, const char* name, const char* opt, char* outName, ErrBuf& err);   // name vuoto = automatico; outName >= 16 byte
#define IO_ERR_MAX 160
bool   ioAddI2c(const char* drv, int sda, int scl, int addr, bool fast, const char* name, char* outName, char* err);   // un solo sensore I2C (un bus); outName >= 16 byte, err >= IO_ERR_MAX byte
bool   ioAddSd(int cs, int mosi, int sck, int miso, int khz, const char* name, char* outName, char* err);   // una sola microSD (4 pin, controllati come gli altri); outName >= 16 byte, err >= IO_ERR_MAX byte
bool   ioSdPins(int* p);                                       // p[5] = CS, MOSI, SCK, MISO, kHz della microSD in io.cfg; false = non configurata
bool   ioRemove(const char* name, ErrBuf& err);              // ioAdd/ioAddI2c/ioRemove/ioRename: se io.cfg non si scrive, rispondono errore e l'elenco non cambia
bool   ioRename(const char* from, const char* to, ErrBuf& err);
bool   ioSet(const char* name, int val, int role, ErrBuf& err);   // uscite: 0/1 (rele solo Admin, minimo 1 s tra due cambi)
bool   ioGet(const char* name, int& val, ErrBuf& err);           // uscite: acceso 0/1; ingresso: attivo 0/1; analogico: 0..4095; i2c: centesimi di grado (nome.p = pascal)
bool   ioIsOut(const char* name);
const char* ioClassName(const char* name);
int    ioState(const char* name);                      // per il registro: 1 acceso/presente, 0 spento
bool   ioCanSwitch(const char* name);
void   ioAllOff();                                       // tutte le uscite spente (watchdog, arresto)
#define IO_JSON_MAX 4096                                  // tetto del JSON e del testo (16 periferiche: ne servono circa 2 KB)
bool   ioListJson(HeapBuf& out);                         // {"io":[{n,c,g,f,v,out},...],"max":16}; i2c aggiunge g2 (SCL), a (indirizzo), d (driver), p (pascal), ok (0/1). false = manca la memoria: mai JSON troncato
bool   ioText(HeapBuf& out);                             // per la shell. false = manca la memoria
bool   ioNameOk(const char* n, ErrBuf& err);             // forma e parole riservate
bool   ioParseCls(const char* s, uint8_t& c);
int    ioSuggest(uint8_t cls, int n, int* out);          // fino a n pin liberi consigliati per la classe; ritorna quanti ne ha trovati
bool   ioPulse(const char* name, uint32_t ms, int role, ErrBuf& err);   // acceso per ms (100-5000) poi si spegne da solo
bool   ioBlink(const char* name, uint32_t sec, int role, ErrBuf& err);  // lampeggia per sec (1-10) poi si spegne (non per il rele)
void   ioTick();                                                          // da chiamare nel loop
