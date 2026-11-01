// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_mod.h
// Driver / schede modulo (.vdb): UN file di testo in /devs che descrive un modulo (rele a 2 canali, pulsante, sensore, microSD...): classe,
// segnali, icona, schema SVG e, facoltativi, pagina, codice Berry e dati iniziali (il driver completo: vedi interfaces/vos_mod_app.h).
// Il firmware controlla il file, propone i pin liberi (l'utente puo cambiarli) e crea le periferiche (vos_io, che controlla i pin).
// Formato: vedi il manuale (schede modulo .vdb)
//   [module]   name= title= class= active= pull= power= level= part= author= version= license= warn= khz= use= who=
//              khz = solo classe sd (frequenza SPI, 400-20000); use= e who= = permessi e chi usa l'app del driver (come nel manifest di una app)
//              level = livello del segnale del modulo (3V3 o 5V); part = componente da aggiungere: serie|gnd|vcc,valore,segnale (max 4)
//   [signals]  una riga per segnale (etichetta 1-8 caratteri: IN1, OUT, DATA...), massimo 8
//   [icon]     un solo <svg> (max 2 KB, forme semplici)
//   [wiring]   un solo <svg> con lo schema del modulo (max 4 KB, forme semplici)
//   [pins]     (facoltativa) un filo per riga: ETICHETTA=destinazione,colore. Etichetta = nome vero del pin sulla periferica (VCC, GND, SDA...),
//              destinazione = vcc | gnd | un segnale di [signals], colore = nome (red, nero...) oppure #rrggbb. Max 8 righe. Serve a disegnare i fili.
//   Classe i2c: driver=bmp280 addr=0x76|0x77 [freq=100000|400000]; segnali esattamente SDA e poi SCL
//   Classe sd: segnali esattamente CS, MOSI, SCK, MISO (in questo ordine); khz= facoltativo
//   [widget]   (facoltativa) title= alt= who=  il riquadro del driver nella Home. I valori vivi li stampa on_widget() nel codice [cmd] (vedi vos_mod_cmd.h)
//   [page] [code] [data]  (facoltative, insieme) come in una app .vab: controlli, codice Berry (max 8 KB), valori iniziali. Le controlla vos_mod_app
#pragma once
#include "../core/vos_errbuf.h"
#include "../core/vos_heapbuf.h"
#include <string_view>
#include "vos_mod_line.h"

#define MOD_DIR        "/devs"
#define MOD_EXT        ".vdb"      // driver in Berry (dal 1.8.11)
#define MOD_EXT_OLD    ".vdp"      // vecchia estensione: i file gia installati si leggono ancora
#define MOD_FILE_MAX   16384    // come APP_FILE_MAX: il driver puo portare pagina e codice
#define MOD_CODE_MAX   8192
#define MOD_PAGE_MAX   12
#define MOD_ICON_MAX   2048
#define MOD_WIRING_MAX 4096
#define MOD_SIG_MAX    8
#define MOD_PART_MAX   4
#define MOD_LIST_MAX   16
#define MOD_PINS_MAX   8
#define MOD_CMDCODE_MAX 4096    // codice dei comandi del driver ([cmd])
#define MOD_NAME_BUF   48

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
  int khz;                           // solo classe sd: frequenza SPI
  char use[100], who[6], autostart[9]; bool hasApp;   // app del driver: permessi, chi la usa, ha [code]
  ModWidget widget;                  // widget della Home (sezione [widget]): titolo, testo alternativo, chi lo vede; i valori vivi li da' on_widget()
  bool hasCmd; int ncmd; char cmdWord[13];   // comandi del driver: ha [commands] e [cmd], quante righe, la parola del comando (vedi vos_mod_cmd.h)
  struct WirePin { char lab[9]; char dst[9]; char hex[8]; char cname[8]; } wp[MOD_PINS_MAX]; int nwp;   // fili: etichetta del pin, destinazione, colore (#rrggbb e nome base)
  size_t size;
};

int    modScan(char (*names)[MOD_NAME_BUF], int mx);          // nomi dei file /devs/*.vdb e *.vdp (senza estensione, senza doppioni) nei primi mx posti; ritorna quanti ce ne sono in tutto
bool   modSectionKnown(std::string_view name);                    // name (senza parentesi) e una sezione di un driver? (module, signals, icon, wiring, pins, page, code, data, commands, cmd, widget)
bool   modColor(const char* v, char* hex, char* cname);   // nome (red, rosso...) o #rrggbb -> #rrggbb; cname = nome base inglese o vuoto
bool   modParse(std::string_view text, const char* fileName, ModInfo& m, ErrBuf& err);   // false = err "Riga N: motivo"
bool   modListJson(HeapBuf& out);                              // GET /api/mods  {"ok":true,"list":[{n,ok,t,c,p,ns,v,a,l,w,icon,e}]}. false = manca la memoria o il tetto (out da buttare)
bool   modInfoJson(const char* name, HeapBuf& out);            // GET /api/mod?name=  dettagli + wiring + pin proposti (stesso significato di false)
bool   modInstall(const char* name, const char* prefix, const char* pinsCsv, HeapBuf& outJson, ErrBuf& err);   // crea le periferiche sui pin scelti dall'utente (pinsCsv "10,11,12,13": tanti numeri quanti i segnali) o, se vuoto, su quelli proposti
bool   modSave(const char* name, std::string_view text, ErrBuf& err);      // controlla e salva /devs/<name>.vdb (toglie il vecchio .vdp con lo stesso nome)
void   modFilePath(char* out, size_t cap, const char* name);            // percorso del file del driver: .vdb, o il vecchio .vdp se c'e solo quello
bool   modDelete(const char* name, ErrBuf& err);
void   modListText(HeapBuf& out);                              // comando "mod list"
void   modInfoText(const char* name, HeapBuf& out);            // comando "mod info <nome>"
