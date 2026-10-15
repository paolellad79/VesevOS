// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_io.h
// Periferiche a classi create dall'utente: rele, led, pin (uscita), ingresso, analogico, i2c (sensore BMP280: due pin SDA e SCL). Ognuna ha un nome (rele1, led1...),
// un pin controllato dal registro dei pin, e si usa da pagina, shell, app (use=rele1) e regole.
#pragma once
#include <Arduino.h>

#define IO_MAX 16

enum IoClass : uint8_t { IC_NONE = 0, IC_RELE = 1, IC_LED = 2, IC_PIN = 3, IC_IN = 4, IC_ADC = 5, IC_I2C = 6 };
#define IOF_INV   1     // attivo basso (acceso = livello 0)
#define IOF_UP    2     // ingresso con pull-up
#define IOF_DOWN  4     // ingresso con pull-down
#define IOF_FAST  8     // i2c a 400 kHz (altrimenti 100 kHz)

void   ioInit();                                         // carica /io.cfg (o /io.cfg.bak se illeggibile) e mette tutte le uscite nello stato sicuro (spente); non scrive mai
int    ioCount();
String ioNameAt(int i);
bool   ioHas(const String& name);
bool   ioAdd(const String& cls, int gpio, const String& name, const String& opt, String& outName, String& err);   // name vuoto = automatico
#define IO_ERR_MAX 80
bool   ioAddI2c(const char* drv, int sda, int scl, int addr, bool fast, const char* name, char* outName, char* err);   // un solo sensore I2C (un bus); outName >= 16 byte, err >= IO_ERR_MAX byte
bool   ioRemove(const String& name, String& err);              // ioAdd/ioAddI2c/ioRemove/ioRename: se io.cfg non si scrive, rispondono errore e l'elenco non cambia
bool   ioRename(const String& from, const String& to, String& err);
bool   ioSet(const String& name, int val, int role, String& err);   // uscite: 0/1 (rele solo Admin, minimo 1 s tra due cambi)
bool   ioGet(const String& name, int& val, String& err);           // uscite: acceso 0/1; ingresso: attivo 0/1; analogico: 0..4095; i2c: centesimi di grado (nome.p = pascal)
bool   ioIsOut(const String& name);
const char* ioClassName(const String& name);
int    ioState(const String& name);                      // per il registro: 1 acceso/presente, 0 spento
bool   ioCanSwitch(const String& name);
void   ioAllOff();                                       // tutte le uscite spente (watchdog, arresto)
String ioListJson();                                     // {"io":[{n,c,g,f,v,out},...],"max":16}; i2c aggiunge g2 (SCL), a (indirizzo), d (driver), p (pascal), ok (0/1)
String ioText();                                         // per la shell
bool   ioNameOk(const String& n, String& err);           // forma e parole riservate
bool   ioParseCls(const String& s, uint8_t& c);
int    ioSuggest(uint8_t cls, int n, int* out);          // fino a n pin liberi consigliati per la classe; ritorna quanti ne ha trovati
bool   ioPulse(const String& name, uint32_t ms, int role, String& err);   // acceso per ms (100-5000) poi si spegne da solo
bool   ioBlink(const String& name, uint32_t sec, int role, String& err);  // lampeggia per sec (1-10) poi si spegne (non per il rele)
void   ioTick();                                                          // da chiamare nel loop
