// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_mod.h
// Schede modulo (.vdp): UN file di testo in /devs che descrive un modulo (rele a 2 canali, pulsante, sensore...): classe, segnali,
// icona e schema SVG. Sono SOLO DATI (nessun codice). Il firmware controlla il file, propone i pin liberi e crea le periferiche (vos_io).
// Formato: vedi il manuale (schede modulo .vdp)
//   [module]   name= title= class= active= pull= power= level= part= author= version= license= warn=
//              level = livello del segnale del modulo (3V3 o 5V); part = componente da aggiungere: serie|gnd|vcc,valore,segnale (max 4)
//   [signals]  una riga per segnale (etichetta 1-8 caratteri: IN1, OUT, DATA...), massimo 8
//   [icon]     un solo <svg> (max 2 KB, forme semplici)
//   [wiring]   un solo <svg> con lo schema del modulo (max 4 KB, forme semplici)
//   [pins]     (facoltativa) un filo per riga: ETICHETTA=destinazione,colore. Etichetta = nome vero del pin sulla periferica (VCC, GND, SDA...),
//              destinazione = vcc | gnd | un segnale di [signals], colore = nome (red, nero...) oppure #rrggbb. Max 8 righe. Serve a disegnare i fili.
//   Classe i2c: driver=bmp280 addr=0x76|0x77 [freq=100000|400000]; segnali esattamente SDA e poi SCL
#pragma once
#include "../core/vos_errbuf.h"
#include "../core/vos_heapbuf.h"
#include <string_view>

#define MOD_DIR        "/devs"
#define MOD_EXT        ".vdp"
#define MOD_FILE_MAX   8192
#define MOD_ICON_MAX   2048
#define MOD_WIRING_MAX 4096
#define MOD_SIG_MAX    8
#define MOD_PART_MAX   4
#define MOD_LIST_MAX   16
#define MOD_PINS_MAX   8

// Tetti dei testi e dei JSON della shell e della pagina
#define MOD_TEXT_MAX   4096     // testi della shell (mod list, mod info)
#define MOD_INFO_MAX   16384    // JSON di una scheda (icona 2 KB + schema 4 KB, con le sequenze di escape)
#define MOD_LIST_JSON_MAX 81920 // JSON dell'elenco (fino a 16 schede con icona)

// Tutto a buffer fissi (l'oggetto sta sempre sull'heap: ~7 KB).
struct ModInfo {
  char name[25], title[33], cls[16], power[6], author[25], ver[11], lic[25], warn[81];
  char icon[MOD_ICON_MAX + 16];
  char wiring[MOD_WIRING_MAX + 16];
  bool low;                          // attivo basso
  char pull;                         // 'n' nessuno, 'u' pull-up, 'd' pull-down (solo ingresso)
  char sig[MOD_SIG_MAX][9]; int nsig;
  char level;                        // livello del segnale: '3' = 3,3 V (predefinito), '5' = 5 V
  struct Part { char pos; char val[13]; char sig[9]; } part[MOD_PART_MAX]; int npart;   // pos: 's' in serie al segnale, 'g' verso GND, 'v' verso l'alimentazione
  char drv[12]; int addr; bool fast;   // solo classe i2c: driver, indirizzo, 400 kHz
  struct WirePin { char lab[9]; char dst[9]; char hex[8]; char cname[8]; } wp[MOD_PINS_MAX]; int nwp;   // fili: etichetta del pin, destinazione, colore (#rrggbb e nome base)
  size_t size;
};

bool   modColor(const char* v, char* hex, char* cname);   // nome (red, rosso...) o #rrggbb -> #rrggbb; cname = nome base inglese o vuoto
bool   modParse(std::string_view text, const char* fileName, ModInfo& m, ErrBuf& err);   // false = err "Riga N: motivo"
bool   modListJson(HeapBuf& out);                              // GET /api/mods  {"ok":true,"list":[{n,ok,t,c,p,ns,v,a,l,w,icon,e}]}. false = manca la memoria o il tetto (out da buttare)
bool   modInfoJson(const char* name, HeapBuf& out);            // GET /api/mod?name=  dettagli + wiring + pin proposti (stesso significato di false)
bool   modInstall(const char* name, const char* prefix, HeapBuf& outJson, ErrBuf& err);   // crea le periferiche sui pin proposti
bool   modSave(const char* name, std::string_view text, ErrBuf& err);      // controlla e salva /devs/<name>.vdp
bool   modDelete(const char* name, ErrBuf& err);
void   modListText(HeapBuf& out);                              // comando "mod list"
void   modInfoText(const char* name, HeapBuf& out);            // comando "mod info <nome>"
