// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_hw.h
// STRATO 6 (packages): accesso ai pin, al bus I2C e al bus SPI dei DRIVER ELF (.vde), indipendente dal linguaggio. Una sola scrittura, due
// collegamenti: ELF (vos_prim_hw.cpp, simboli vos_gpio_read...) e Berry (vos_berry_hw.cpp, gpio_read...). Stessi nomi, stessi numeri di ritorno.
// REGOLE
//   - Si lavora solo sul driver NOMINATO (istanza di devices/vos_drvres): usa solo i pin che l'utente gli ha dato; mai pin a scelta del modulo.
//   - Qui NON si controllano ruolo e use=: lo fa chi collega (il registro delle primitive, o Berry).
//   - Il bus I2C e uno solo e appartiene all'ultimo driver che lo usa; se esiste il sensore I2C delle periferiche a classi il bus e suo e si risponde "occupato".
//   - Ritorno: 0 o un numero >= 0 = riuscito; negativo = errore (stessi valori di VOS_PRIM_E_* in vos_prim.h).
#pragma once
#include <stdint.h>
#include "../core/vos_common.h"

#if VOS_WITH_ELF
enum { HW_E_ARG = -1, HW_E_NONTROVATO = -3, HW_E_GRANDE = -6, HW_E_IO = -7, HW_E_OCCUPATO = -8 };
enum { HW_MODO_IN = 0, HW_MODO_PULLUP = 1, HW_MODO_PULLDOWN = 2, HW_MODO_OUT = 3 };
#define HW_I2C_MAX 32                 // byte per lettura (la scrittura arriva a 30: il registro prende un byte)
#define HW_SPI_MAX 64                 // byte per scambio
#define HW_DELAY_US_MAX 10000

int hwGpioMode(const char* drv, int idx, int modo);                   // idx = posizione nei pin del driver (0 = il primo segnale)
int hwGpioRead(const char* drv, int idx);                             // 0 o 1
int hwGpioWrite(const char* drv, int idx, int livello);               // solo dopo hwGpioMode(..., HW_MODO_OUT) su quel pin
int hwI2cProbe(const char* drv);                                      // 1 = l'indirizzo del driver risponde, 0 = no
int hwI2cRead(const char* drv, int reg, uint8_t* buf, int n);         // legge n byte dal registro; ritorna n
int hwI2cWrite(const char* drv, int reg, const uint8_t* buf, int n);
int hwSpiTransfer(const char* drv, const uint8_t* tx, uint8_t* rx, int n);   // tx/rx possono essere nullptr; ritorna n
int hwDelayUs(int us);                                                // attesa breve, tetto HW_DELAY_US_MAX; ritorna i microsecondi attesi
uint32_t hwUptime();                                                  // secondi dall'avvio
void hwRilascia(const char* drv);                                     // il driver e stato tolto: chiude il bus se era suo e dimentica i pin in uscita
#endif
