// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_vde.h
// STRATO 6 (packages): DRIVER IN ELF (.vde). UN file in /devs/<nome>.vde: una parte di testo (come un driver .vdb) e, dopo la riga "[elf] N",
// N byte di ELF. Formato completo: docs/progetto/formato-vde.md. Le regole dei driver sono le stesse del .vdb (regole-driver.md, 9-14):
// contenitore unico (icona, widget, comandi, logica), nel firmware solo primitive, il driver PROPONE i pin e l'UTENTE li sceglie e li applica,
// controllo dei pin unico (vos_pins), il driver non ha diritti propri. Le funzioni del driver sono nel suo ELF (main(argc, argv)):
//   argv[0] = nome del driver, argv[1] = sottocomando ("widget" per il widget), poi gli argomenti. L'uscita si scrive con vos_print(), una riga per chiamata.
// Parte di testo: [module] [signals] [icon] [widget] [commands]. Niente [page]/[code]/[cmd] (la logica e l'ELF). Chiavi di [module]:
//   name= (uguale al nome del file) title= bus=gpio|i2c|spi|sd addr= freq= khz= power= level= author= version= license= warn= use= who=
//   Segnali: i2c SDA SCL; spi CS MOSI MISO SCK; sd CS MOSI SCK MISO; gpio da 1 a 4 etichette libere. use= (al massimo 32 caratteri): gruppi di primitive
//   come per una app; il nome del driver e sempre concesso a chi lo esegue. [widget]: title= alt= who= e, facoltativo, source=sd = i valori li tiene
//   il FIRMWARE (non gira nessun codice a ogni richiesta); senza source= il widget chiama il driver con argv[1] = "widget" (al massimo ogni 10 s).
#pragma once
#include "../core/vos_common.h"
#include "../core/vos_errbuf.h"
#include "../core/vos_heapbuf.h"
#include "../devices/vos_mod.h"
#include "../devices/vos_mod_line.h"
#include "../devices/vos_mod_cmd.h"

#if VOS_WITH_ELF
#define VDE_DIR       "/devs"
#define VDE_EXT       ".vde"
#define VDE_TEXT_MAX  8192
#define VDE_USE_MAX   32
#define VDE_SIG_MAX   4
#define VDE_LIST_MAX  8
#define VDE_LIST_JSON_MAX 24576   // JSON dell'elenco: fino a 4 driver installati con icona (2 KB con le sequenze di escape)

enum VdeBus : uint8_t { VB_GPIO = 1, VB_I2C = 2, VB_SPI = 3, VB_SD = 4 };

// Grande (icona 2 KB): sempre sull'heap.
struct VdeInfo {
  char name[25], title[33], power[6], author[25], ver[11], lic[25], warn[81], use[VDE_USE_MAX + 1], who[6];
  uint8_t bus;
  int addr;                          // solo i2c
  int khz;                           // i2c: 100 o 400 (freq=100000|400000); spi e sd: kHz
  char level;                        // '3' o '5'
  char sig[VDE_SIG_MAX][9]; int nsig;
  char icon[MOD_ICON_MAX + 16];
  ModWidget widget; char source[13]; // sorgente dei valori del widget tenuta dal firmware ("" = chiama il driver)
  int ncmd; ModCmd cmd[MOD_CMD_MAX]; char cmdWord[13];
  size_t textLen, elfOff, elfLen;    // dove stanno testo ed ELF nel file
};

// Controlla il file intero (testo + ELF, struttura e simboli). false = err "Riga N: motivo" o il motivo dell'ELF.
// elf (facoltativo): se non e nullptr riceve una copia dell'ELF in un blocco allineato (malloc) da liberare con free().
bool vdeParse(const uint8_t* file, size_t len, const char* fileName, VdeInfo& v, uint8_t** elf, ErrBuf& err);
bool vdeSalva(const char* nome, const uint8_t* file, size_t len, ErrBuf& err);          // controlla e scrive /devs/<nome>.vde (Admin)
bool vdeLeggi(const char* nome, VdeInfo& v, uint8_t** elf, ErrBuf& err);                  // legge e controlla; *elf = copia dell'ELF (malloc, allineata) da liberare con free()
bool vdeEsiste(const char* nome);
int  vdeElenco(char (*nomi)[MOD_NAME_BUF], int mx);                                       // nomi dei file /devs/*.vde (senza estensione); ritorna quanti ce ne sono
// Dare i pin e togliere il driver. pinsCsv "21,22" (tanti numeri quanti i segnali) o vuoto = i pin proposti dal firmware.
bool vdeInstalla(const char* nome, const char* pinsCsv, ErrBuf& err);
bool vdeCambiaPin(const char* nome, const char* pinsCsv, ErrBuf& err);                    // da' altri pin a un driver gia installato (se i nuovi non vanno, restano i vecchi)
int  vdePinDati(const char* nome, int* pin, int mx);                                       // i pin del driver (da drvres o, per sd, dalla periferica sd): quanti; 0 = nessuno
bool vdeRimuovi(const char* nome, bool conFile, ErrBuf& err);                              // toglie i pin (e, con conFile, il file); ferma i moduli del driver
bool vdeInstallato(const char* nome);                                                      // il driver ha i suoi pin (o, per sd, la periferica)
bool vdeElencoJson(HeapBuf& out);                                                          // GET /api/vde  {"ok":true,"list":[{n,t,b,ok,e,i,p,s,by,pw,w,ic}]}; false = manca la memoria
void vdeRinfresca();
// ---- esecuzione (vos_vde_run.cpp) ----
// Lancia il driver: argv = {nome, sub, args...}. admin = chi lo lancia e Admin E il sottocomando e "admin": solo allora il modulo ha il ruolo Admin (scrive su pin e bus).
// out = quello che il driver ha scritto con vos_print(); true = il driver ha finito con 0 entro 3 secondi. Un comando alla volta per driver.
bool vdeComando(const char* nome, const char* sub, bool admin, int argc, const char* const* args, HeapBuf& out, ErrBuf& err);
int  vdeComandiLista(ModCmdEntry* out, int mx);                    // le righe [commands] dei driver .vde installati, per il registro dei comandi della shell
int  vdeWidgetLista(ModWidgetEntry* out, int mx);                  // i driver .vde con [widget] (per la Home)
bool vdeWidgetValori(const char* nome, const char* source, ModWidgetVals& vals);   // source=sd: la fotografia del firmware; altrimenti chiama il driver (argv[1] = "widget", tetto 600 ms)                                                                       // un driver e cambiato: comandi e widget si rileggono
#endif
