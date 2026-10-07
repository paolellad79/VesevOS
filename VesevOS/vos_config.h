// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_config.h
// Configurazione unica in stile OpenWrt, salvata in /flash/vesevos.conf
#pragma once
#include <Arduino.h>
#include "vos_common.h"

struct VosUser {
  String name, salt, hash;       // password mai in chiaro (hash con sale)
  uint8_t role;                  // UserRole
  bool on;                       // attivo
  String mfa;                    // segreto MFA (40 cifre esadecimali), vuoto = MFA mai attivato
  bool mfaOn;                    // MFA attivo
  uint32_t mfaLast;              // ultimo contatore TOTP accettato (mai in calo)
  String rec;                    // codici di recupero: hash corti separati da virgola
};

#define VOS_FW_MAX 16
struct FwRule {
  uint32_t a, b;                 // intervallo di indirizzi (ordine "umano": primo numero nel byte alto)
  String name;
  bool on;
};

struct VosConfig {
  String hostname, domain;
  bool   setupDone;              // prima configurazione completata
  String apSsid, apPass;         // apPass: unica e casuale per ogni scheda (creata al primo avvio)
  bool   staEnabled;
  bool     mqttAuto;             // MQTT: parte da solo all'avvio (solo dopo la prima attivazione a mano)
  String   mqttHost, mqttUser, mqttPass, mqttPrefix;
  uint16_t mqttPort;
  uint16_t mqttEvery;            // secondi tra due invii dello stato
  bool     mqttHa;               // Home Assistant: presentazione automatica
  bool     mqttTls;              // mqtts:// (cifrato)
  uint8_t  powBits;              // difficolta della prova di lavoro al login (0 = spenta)
  uint8_t  mfaNoTime;            // se l'ora non e valida: 0 chiedi l'ora, 1 solo codici di recupero, 2 blocca
  uint8_t  banFails;             // password sbagliate prima del blocco (3-20)
  uint32_t banSecs;              // durata del primo blocco in secondi (poi raddoppia, max 1 ora)
  uint8_t  airOn;                // modalita aereo: 1 = radio spenta
  uint8_t  airExit;              // come si riattiva: 0 al prossimo avvio, 1 dopo un tempo, 2 a un orario, 3 solo a mano
  uint32_t airUntil;             // airExit 1: epoch di fine (se l'ora c'era) oppure secondi dall'avvio
  uint16_t airAt;                // airExit 2: minuti dalla mezzanotte (HH*60+MM)
  String staSsid, staPass;
  bool   staDhcp;
  String ip, mask, gw, dns1, dns2;
  String authSalt, authHash;     // SOLO per leggere i file vecchi (1.7.0): diventano l'utente "admin"
  VosUser users[VOS_MAX_USERS];
  String pinNote[49];            // inventario pin: "collegato a ..." scritto dall'utente (indice = GPIO)
  bool   apQr;                   // true = il QR Wi-Fi dell'hotspot si vede in Home a chi e entrato
  uint32_t serBaud;              // seriale: velocita (solo UART; con USB nativa non conta). Di fabbrica 115200
  uint8_t  serEol, serTx;        // a-capo: 0 CR+LF, 1 LF, 2 CR; attesa massima di scrittura in ms
  bool     serEcho, serIn, serLogOut, serBanner;   // eco dei tasti, accetta comandi, scrive il log, mostra il benvenuto all'apertura
  bool   serialAuth;             // true = la shell seriale chiede la password (predefinito)
  uint8_t  ledMode;
  uint32_t ledColor;             // 0xRRGGBB
  uint8_t  ledBrightness;        // 0..255
  uint8_t  ledPin;
  bool   ntpOn, ntpServe;        // client NTP, server NTP per altri dispositivi
  uint32_t ntpEvery;             // minuti tra due sincronizzazioni NTP (0 = solo all'avvio)
  String ntpServer, tz, tzName;  // server, fuso (formato POSIX), nome
  uint8_t dateFmt, timeFmt, tempUnit;   // data: 0 GG/MM/AAAA 1 AAAA-MM-GG 2 MM/GG/AAAA; ora: 0=24h 1=12h; temp: 0=C 1=F
  uint8_t weekStart, decSep;     // 0 = lunedi / 1 = domenica; 0 = virgola / 1 = punto
  String  country;               // paese (ISO 3166, es. "IT"); vuoto = non scelto (regole prudenti)
  uint8_t antExt;                // 0 = antenna interna, 1 = antenna esterna
  int8_t  antGain;               // guadagno antenna esterna in dBi (0-15)
  int8_t  txDbm;                 // potenza scelta (dBm); 0 = la massima consentita
  uint8_t  logLevel;             // 0 errori, 1 + attenzioni, 2 + info (di fabbrica), 3 + dettagli
  bool     statOn;               // statistiche d'uso (spente di fabbrica)
  uint8_t  pwMode;               // risparmio energia: 0 spento, 1 Wi-Fi a risparmio massimo, 2 sonno profondo a cicli
  uint16_t pwAwake, pwSleep;     // minuti sveglia / minuti di sonno (modo 2)
  uint16_t cpuMhz;               // 0 = automatico, altrimenti 80/160/240
  String lang;                   // codice lingua: "it" (predefinita), "en" (interna) o file /lang/<codice>.json
  bool   sdEnabled;
  uint8_t sdCs, sdSck, sdMiso, sdMosi;
  // Filtro IP
  uint8_t fwMode;                // 0 spento, 1 solo la mia rete, 2 lista consentita, 3 lista bloccati
  bool    fwNtp;                 // filtra anche il server NTP
  FwRule  fw[VOS_FW_MAX];
  uint8_t fwN;
  // Rete tra schede (ESP-NOW)
  bool    meshAuto;              // parte all'avvio (solo dopo la prima attivazione a mano)
  uint8_t meshRole;              // 0 nodo, 1 gateway, 2 sensore
  String  meshKey;               // chiave comune (64 cifre esadecimali)
  uint8_t meshCh;                // canale di partenza (1-13)
  // HTTPS
  bool    https;                 // pagina e API cifrate (predefinito acceso)
  bool    apOn;                  // servizio Punto di accesso: se spento e la Wi-Fi di casa e configurata, l'hotspot non riparte da solo
  bool    apCaptive;             // portale automatico (DNS) dell'hotspot
  bool    dhcpOn;                // server DHCP dell'hotspot (assegna gli indirizzi ai telefoni)
  uint16_t dhcpLease;            // durata dell'indirizzo assegnato, in minuti (10-1440)
  bool    mdnsOn;                // annuncio nome.local (mDNS)
  bool    httpOn;                // server HTTP acceso (predefinito si)
  uint16_t httpPort, httpsPort;  // porte (predefinite 80 e 443)
  // Watchdog
  bool    wdTask, wdNet, wdRam;  // controlli: task bloccati, rete assente, RAM bassa
  uint16_t wdNetMin;             // minuti senza rete prima del riavvio
  uint16_t wdRamKb;              // soglia RAM libera
  uint16_t wdUpDays;             // riavvio dopo N giorni di accensione (0 = mai)
  int16_t  wdAt;                 // riavvio programmato: minuti dalla mezzanotte (-1 = spento)
  uint8_t  wdDays;               // giorni del riavvio programmato (bit 0 = lunedi ... bit 6 = domenica)
};

extern VosConfig cfg;

void   cfgDefaults();
bool   cfgLoad();                       // da file (true se trovato)
bool   cfgSave();                       // salvataggio sicuro (.tmp/.bak) + controllo (allarmi) + registro delle modifiche
String cfgExport(bool withSecrets);     // testo OpenWrt
bool   cfgImport(const String& text, String& err);   // ripristino: chiavi assenti restano
void   cfgFactoryReset();
bool   cfgLastSaveOk();                // l'ultimo salvataggio della configurazione e riuscito?
bool   cfgSvcRecover();                 // riporta hotspot, HTTP, HTTPS e porte ai valori di fabbrica (true se qualcosa e cambiato)
String cfgSvcCheck(bool apOn, bool httpOn, int httpPort, bool httpsOn, int httpsPort);   // "" = ok, altrimenti il motivo del rifiuto
void   cfgSetOrigin(const String& who); // chi sta cambiando la configurazione (per il registro): "web admin 192.168.1.5"
String cfgOrigin();
bool   cfgFileChanged();                // il file e stato cambiato fuori dal pannello?
String cfgNewApPass();                  // password casuale per l'hotspot (12 caratteri a gruppi)
bool   cfgApPassWeak(const String& p);  // vuota, troppo corta o di fabbrica
bool   cfgParseRange(const String& txt, uint32_t& a, uint32_t& b);   // "1.2.3.4", "1.2.3.4-1.2.3.9", "1.2.3.0/24"
