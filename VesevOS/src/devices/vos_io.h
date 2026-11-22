// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_io.h
// Periferiche a classi create dall'utente: rele, led, pin (uscita), ingresso, analogico, i2c (sensore BMP280: due pin SDA e SCL), sd (lettore microSD: quattro pin CS, MOSI, SCK, MISO). Ognuna ha un nome (rele1, led1...),
// un pin controllato dal registro dei pin, e si usa da pagina, shell, app (use=rele1) e regole.
#pragma once
#include "../core/vos_errbuf.h"
#include "../drivers/vos_drv_string.h"
#include "../core/vos_heapbuf.h"
#include "../core/vos_common.h"

#define IO_MAX 16
#define IO_SD_KHZ_MIN 400                            // frequenza SPI della microSD: da 400 kHz (prova iniziale, fili lunghi) a 20 MHz
#define IO_SD_KHZ_MAX 20000
#define IO_SD_KHZ_DEF 1000                           // di fabbrica 1 MHz: prudente per i fili volanti dei moduli economici

enum IoClass : uint8_t { IC_NONE = 0, IC_RELE = 1, IC_LED = 2, IC_PIN = 3, IC_IN = 4, IC_ADC = 5, IC_I2C = 6, IC_SD = 7, IC_PWM = 8, IC_CNT = 9, IC_LEDRGB = 10, IC_WAKE = 11 };
#define IOF_INV   1     // attivo basso (acceso = livello 0)
#define IOF_UP    2     // ingresso con pull-up
#define IOF_DOWN  4     // ingresso con pull-down
#define IOF_FAST  8     // i2c a 400 kHz (altrimenti 100 kHz)
#define IOF_ACCESO 16   // led_rgb: acceso (solo nel file io.cfg; in memoria lo stato sta in IoDev::on)

void   ioInit();                                         // carica /io.cfg (o /io.cfg.bak se illeggibile) e mette tutte le uscite nello stato sicuro (spente); non scrive mai
int    ioCount();
const char* ioNameAt(int i);   // nome della periferica i ("" se non c'e); vale finche la periferica esiste
// Ripresa (classe "ripresa", installata dal driver "ripresa"): un tasto che risveglia la scheda dallo standby. Ne basta uno. I piedini ammessi per ogni
// scheda stanno nella griglia del driver ([griglia] di ripresa.vdb), non nel firmware. "inv" = attivo basso, "up"/"down" = resistenza interna.
// Quando viene preparata si presenta da sola al driver del sonno (drvSonnoRipresa); lo standby sa solo se c'e (drvSonnoRipresaPresente).
bool   ioHas(const char* name);                           // i nomi e i testi in ingresso sono const char* (non nullo, terminato): una funzione che modifica l'elenco li copia prima
bool   ioAdd(const char* cls, int gpio, const char* name, const char* opt, char* outName, ErrBuf& err, bool daDriver = false);   // daDriver: solo l'installazione di un driver lo mette a true (la classe "ripresa" non si crea a mano). name vuoto = automatico; outName >= 16 byte
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
bool   ioHasI2c();                                        // c'e il sensore I2C a classi (tiene il bus): i driver ELF non lo usano
int    ioSuggest(uint8_t cls, int n, int* out, uint64_t soloQuesti = ~0ULL);          // fino a n pin liberi consigliati per la classe; soloQuesti = griglia del driver (un bit per pin: sono ammessi solo quelli); ritorna quanti ne ha trovati
bool   ioPulse(const char* name, uint32_t ms, int role, ErrBuf& err);   // acceso per ms (100-5000) poi si spegne da solo
bool   ioBlink(const char* name, uint32_t sec, int role, ErrBuf& err);  // lampeggia per sec (1-10) poi si spegne (non per il rele)
void   ioTick();                                                          // da chiamare nel loop

// PWM dell'utente (classe "pwm", package VOS_WITH_PWM): un pin, una frequenza (10-40000 Hz) e un duty 0-100%. Dopo il riavvio parte spento (duty 0).
#if VOS_WITH_PWM
bool ioPwmSet(const char* name, int hz, int pct, int role, ErrBuf& err);   // hz 0 = invariata (cambiarla: Admin, si salva), pct -1 = invariato (Operatore)
bool ioPwmAct(const char* name, const char* act, const char* arg, int role, HeapBuf& out, ErrBuf& err);   // azioni di /api/dev/act: get, set "duty[,hz]"
bool ioPwmGet(const char* name, int& hz, int& pct);                          // frequenza e duty di una periferica PWM; false = non esiste o non e un PWM
int  ioPwmFree();                                                          // canali PWM ancora liberi
#endif

// LED a colori WS2812 a mano (classe "led_rgb", package VOS_WITH_LED_RGB_A_MANO): un pin, un LED (un pixel). Acceso/spento con ioSet/ioGet (Operatore); colore RRGGBB e luminosita 0-255 con l'azione "colore"; ordine dei byte con "ordine" (Admin). Si salvano in io.cfg.
#if VOS_WITH_LED_RGB_A_MANO
bool ioLedAct(const char* name, const char* act, const char* arg, int role, HeapBuf& out, ErrBuf& err);   // azioni di /api/dev/act: colore "RRGGBB[,luminosita]", ordine "grb|rgb|brg|gbr|rbg|bgr"
int  ioLedFree();                                                                                          // canali RMT ancora liberi per i LED a mano
#endif

// Contatore di impulsi dell'utente (classe "contatore", package VOS_WITH_CONTATORE): un pin di ingresso, fronte e filtro; il valore riparte da 0 al riavvio (a 2147483647 torna a 0).
#if VOS_WITH_CONTATORE
bool ioCntAct(const char* name, const char* act, HeapBuf& out, int role, ErrBuf& err);   // azioni di /api/dev/act: get, reset (Operatore)
bool ioCntReset(const char* name, int role, ErrBuf& err);                              // valore e frequenza a zero
bool ioCntGet(const char* name, int& count, int& mhz);                                 // impulsi e frequenza in millihertz; false = non esiste o non e un contatore
int  ioCntFree();                                                                      // unita PCNT ancora libere
#endif
