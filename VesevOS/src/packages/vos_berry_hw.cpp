// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_berry_hw.cpp: funzioni Berry gpio_*, i2c_*, spi_transfer, uptime, delay_us. Stessi nomi e stessi numeri delle primitive ELF (vos_prim_hw.cpp);
// il lavoro vero lo fa packages/vos_hw.cpp, sui soli pin che l'utente ha dato al driver nominato (devices/vos_drvres).
//
//   gpio_mode(drv, idx, modo) / gpio_read(drv, idx) / gpio_write(drv, idx, livello)   modo: 0 ingresso, 1 pull-up, 2 pull-down, 3 uscita
//   i2c_probe(drv)  ->  true/false     i2c_read(drv, reg, n)  ->  bytes (n da 1 a 32)     i2c_write(drv, reg, dati)  ->  true
//   spi_transfer(drv, dati)  ->  bytes (stessa lunghezza, al massimo 64)     uptime()  ->  secondi     delay_us(us)  ->  us attesi (tetto 10000)
// PERMESSI: le app solo con il gruppo in use= (gpio, i2c, spi, time) e il NOME del driver in use=; gli script (Admin) li hanno sempre.
// Le letture sono di tutti; gpio_mode, gpio_write, i2c_write e spi_transfer solo per l'Admin (script lanciato dall'Admin, o app con who=admin):
// le funzioni ci sono sempre, a chi non puo rispondono "permesso negato" (un file che le nomina deve compilare comunque).
#include "vos_berry_int.h"
#include "../core/vos_common.h"
#if VOS_WITH_BERRY && VOS_WITH_ELF
#include "../third_party/berry/src/berry.h"
#include <stdio.h>
#include <string.h>
#include "vos_hw.h"

static bool g_adminOk = false;     // chi ha lanciato lo script e l'Admin
static bool g_isApp = false;       // app: il nome del driver deve stare in use=

static const char* drvArg(bvm* vm, const char* fn) {
  char msg[64];
  if (be_top(vm) < 1 || !be_isstring(vm, 1)) { snprintf(msg, sizeof msg, "%s: serve il nome del driver", fn); be_raise(vm, "type_error", msg); }
  const char* d = be_tostring(vm, 1);
  if (g_isApp && !useHas(d)) { snprintf(msg, sizeof msg, "permesso mancante: aggiungi %s a use=", d); be_raise(vm, "value_error", msg); }
  return d;
}
static void needAdmin(bvm* vm, const char* fn) {
  char msg[48];
  if (!g_adminOk) { snprintf(msg, sizeof msg, "%s: permesso negato (serve l'Admin)", fn); be_raise(vm, "permission_error", msg); }
}
static int intArg(bvm* vm, int pos, const char* fn) {
  char msg[48];
  if (be_top(vm) < pos || !be_isint(vm, pos)) { snprintf(msg, sizeof msg, "%s: argomento %d: serve un numero intero", fn, pos); be_raise(vm, "type_error", msg); }
  return (int)be_toint(vm, pos);
}
static void check(bvm* vm, const char* fn, int r) {          // un errore di vos_hw diventa un'eccezione leggibile
  char msg[64];
  if (r >= 0) return;
  const char* why = r == HW_E_ARG ? "argomento non valido" : r == HW_E_NONTROVATO ? "il driver non ha questo bus" : r == HW_E_GRANDE ? "blocco troppo grande"
                  : r == HW_E_OCCUPATO ? "bus occupato" : "errore del bus";
  snprintf(msg, sizeof msg, "%s: %s", fn, why);
  be_raise(vm, r == HW_E_ARG || r == HW_E_GRANDE ? "value_error" : "io_error", msg);
}
static int ret(bvm* vm, int r, const char* fn) { check(vm, fn, r); be_pushint(vm, r); be_return(vm); }

static int bfGpioMode(bvm* vm)  { berryNeedFunc(vm, "gpio"); needAdmin(vm, "gpio_mode"); const char* d = drvArg(vm, "gpio_mode"); int i = intArg(vm, 2, "gpio_mode"); int m = intArg(vm, 3, "gpio_mode"); check(vm, "gpio_mode", hwGpioMode(d, i, m)); be_return_nil(vm); }
static int bfGpioRead(bvm* vm)  { berryNeedFunc(vm, "gpio"); const char* d = drvArg(vm, "gpio_read"); return ret(vm, hwGpioRead(d, intArg(vm, 2, "gpio_read")), "gpio_read"); }
static int bfGpioWrite(bvm* vm) {
  berryNeedFunc(vm, "gpio"); needAdmin(vm, "gpio_write"); const char* d = drvArg(vm, "gpio_write"); int i = intArg(vm, 2, "gpio_write"); int l = intArg(vm, 3, "gpio_write"); check(vm, "gpio_write", hwGpioWrite(d, i, l)); be_return_nil(vm); }
static int bfI2cProbe(bvm* vm)  { berryNeedFunc(vm, "i2c"); int r = hwI2cProbe(drvArg(vm, "i2c_probe")); check(vm, "i2c_probe", r); be_pushbool(vm, r == 1); be_return(vm); }
static int bfI2cRead(bvm* vm) {
  berryNeedFunc(vm, "i2c");
  const char* d = drvArg(vm, "i2c_read"); int reg = intArg(vm, 2, "i2c_read"); int n = intArg(vm, 3, "i2c_read");
  uint8_t buf[HW_I2C_MAX];
  check(vm, "i2c_read", hwI2cRead(d, reg, buf, n));
  be_pushbytes(vm, buf, (size_t)n);
  be_return(vm);
}
static int bfI2cWrite(bvm* vm) {
  berryNeedFunc(vm, "i2c");
  needAdmin(vm, "i2c_write");
  const char* d = drvArg(vm, "i2c_write"); int reg = intArg(vm, 2, "i2c_write");
  if (be_top(vm) < 3 || !be_isbytes(vm, 3)) be_raise(vm, "type_error", "i2c_write(drv, reg, dati): dati = bytes");
  size_t n = 0;
  const void* p = be_tobytes(vm, 3, &n);
  check(vm, "i2c_write", hwI2cWrite(d, reg, (const uint8_t*)p, (int)n));
  be_pushbool(vm, true);
  be_return(vm);
}
static int bfSpiTransfer(bvm* vm) {
  berryNeedFunc(vm, "spi");
  needAdmin(vm, "spi_transfer");
  const char* d = drvArg(vm, "spi_transfer");
  size_t n = 0; const void* p = nullptr;
  if (be_top(vm) < 2 || !be_isbytes(vm, 2)) be_raise(vm, "type_error", "spi_transfer(drv, dati): dati = bytes");
  p = be_tobytes(vm, 2, &n);
  uint8_t rx[HW_SPI_MAX];
  check(vm, "spi_transfer", n > (size_t)HW_SPI_MAX ? HW_E_GRANDE : hwSpiTransfer(d, (const uint8_t*)p, rx, (int)n));
  be_pushbytes(vm, rx, n);
  be_return(vm);
}
static int bfUptime(bvm* vm) {
  berryNeedFunc(vm, "time"); be_pushint(vm, (bint)hwUptime()); be_return(vm); }
static int bfDelayUs(bvm* vm) {
  berryNeedFunc(vm, "time"); be_pushint(vm, hwDelayUs(intArg(vm, 1, "delay_us"))); be_return(vm); }

void berryHwRegister(bvm* vm, bool app, bool admin) {
  g_isApp = app; g_adminOk = admin;
  if (!app || useHas("gpio")) { be_regfunc(vm, "gpio_mode", bfGpioMode); be_regfunc(vm, "gpio_read", bfGpioRead); be_regfunc(vm, "gpio_write", bfGpioWrite); }
  if (!app || useHas("i2c")) { be_regfunc(vm, "i2c_probe", bfI2cProbe); be_regfunc(vm, "i2c_read", bfI2cRead); be_regfunc(vm, "i2c_write", bfI2cWrite); }
  if (!app || useHas("spi")) be_regfunc(vm, "spi_transfer", bfSpiTransfer);
  if (!app || useHas("time")) { be_regfunc(vm, "uptime", bfUptime); be_regfunc(vm, "delay_us", bfDelayUs); }
}
#else
struct bvm;
void berryHwRegister(bvm*, bool, bool) {}
#endif
