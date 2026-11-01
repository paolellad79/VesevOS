// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_prim_hw.cpp
// STRATO 6 (packages): gruppi "gpio", "i2c", "spi" e "time" del registro delle primitive (vedi vos_prim.h), collegamento ELF del nucleo vos_hw.
// ELF: simboli vos_gpio_mode, vos_gpio_read, vos_gpio_write, vos_i2c_probe, vos_i2c_read, vos_i2c_write, vos_spi_transfer, vos_uptime, vos_delay_us.
// Il primo argomento e il NOME del driver (istanza di devices/vos_drvres): serve nel use= del modulo, come per le periferiche (use=i2c bmp1).
// Il modulo lavora solo sui pin che l'utente ha dato a quel driver. Il ruolo si controlla ALLA CHIAMATA e non al caricamento: un driver con comandi
// di lettura e di scrittura si carica anche per l'Operatore, che puo usare le letture; la scrittura risponde VOS_PRIM_E_VIETATO.
#include "vos_prim.h"

#if VOS_WITH_ELF
#include "vos_hw.h"

#define NOME_MAX 25

namespace {

// Nome del driver -> buffer. Ritorna 0 oppure il codice di errore (argomento, driver non concesso, ruolo troppo basso).
int accesso(const char* in, char* nome, int ruoloMin) {
  if (!vos_prim_copia_testo(in, nome, NOME_MAX) || !nome[0]) return VOS_PRIM_E_ARG;
  if (!vos_prim_esec_ha(nome)) return VOS_PRIM_E_USE;           // prima il permesso (use=), poi il ruolo
  if (vos_prim_esec_ruolo() < ruoloMin) return VOS_PRIM_E_VIETATO;
  return 0;
}

int primGpioMode(const char* drv, int idx, int modo) {
  char n[NOME_MAX]; int e = accesso(drv, n, ROLE_ADMIN);
  return e ? e : hwGpioMode(n, idx, modo);
}
int primGpioRead(const char* drv, int idx) {
  char n[NOME_MAX]; int e = accesso(drv, n, ROLE_OPER);
  return e ? e : hwGpioRead(n, idx);
}
int primGpioWrite(const char* drv, int idx, int livello) {
  char n[NOME_MAX]; int e = accesso(drv, n, ROLE_ADMIN);
  return e ? e : hwGpioWrite(n, idx, livello);
}
int primI2cProbe(const char* drv) {
  char n[NOME_MAX]; int e = accesso(drv, n, ROLE_OPER);
  return e ? e : hwI2cProbe(n);
}
int primI2cRead(const char* drv, int reg, void* buf, int len) {
  char n[NOME_MAX]; int e = accesso(drv, n, ROLE_OPER);
  return e ? e : hwI2cRead(n, reg, (uint8_t*)buf, len);
}
int primI2cWrite(const char* drv, int reg, const void* buf, int len) {
  char n[NOME_MAX]; int e = accesso(drv, n, ROLE_ADMIN);
  return e ? e : hwI2cWrite(n, reg, (const uint8_t*)buf, len);
}
int primSpiTransfer(const char* drv, const void* tx, void* rx, int len) {
  char n[NOME_MAX]; int e = accesso(drv, n, ROLE_ADMIN);
  return e ? e : hwSpiTransfer(n, (const uint8_t*)tx, (uint8_t*)rx, len);
}
uint32_t primUptime() { return hwUptime(); }
int primDelayUs(int us) { return hwDelayUs(us); }

const vos_prim_t kGpio[] = {
  { "gpio_mode",  (const void*)primGpioMode,  "int vos_gpio_mode(const char *drv, int idx, int modo)", ROLE_GUEST, 3 },
  { "gpio_read",  (const void*)primGpioRead,  "int vos_gpio_read(const char *drv, int idx)", ROLE_GUEST, 3 },
  { "gpio_write", (const void*)primGpioWrite, "int vos_gpio_write(const char *drv, int idx, int livello)", ROLE_GUEST, 3 },
};
const vos_prim_t kI2c[] = {
  { "i2c_probe", (const void*)primI2cProbe, "int vos_i2c_probe(const char *drv)", ROLE_GUEST, 3 },
  { "i2c_read",  (const void*)primI2cRead,  "int vos_i2c_read(const char *drv, int reg, void *buf, int n)", ROLE_GUEST, 3 },
  { "i2c_write", (const void*)primI2cWrite, "int vos_i2c_write(const char *drv, int reg, const void *buf, int n)", ROLE_GUEST, 3 },
};
const vos_prim_t kSpi[] = {
  { "spi_transfer", (const void*)primSpiTransfer, "int vos_spi_transfer(const char *drv, const void *tx, void *rx, int n)", ROLE_GUEST, 3 },
};
const vos_prim_t kTime[] = {
  { "uptime",   (const void*)primUptime,  "uint32_t vos_uptime(void)", ROLE_GUEST, 3 },
  { "delay_us", (const void*)primDelayUs, "int vos_delay_us(int us)", ROLE_GUEST, 3 },
};

}  // namespace

void vos_prim_hw_registra() {
  vos_prim_registra("gpio", kGpio, sizeof(kGpio) / sizeof(kGpio[0]));
  vos_prim_registra("i2c", kI2c, sizeof(kI2c) / sizeof(kI2c[0]));
  vos_prim_registra("spi", kSpi, sizeof(kSpi) / sizeof(kSpi[0]));
  vos_prim_registra("time", kTime, sizeof(kTime) / sizeof(kTime[0]));
}
#endif
