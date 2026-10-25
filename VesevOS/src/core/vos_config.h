// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_config.h
// Configurazione unica in stile OpenWrt, salvata in /flash/vesevos.conf
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string_view>
#include "vos_errbuf.h"
#include "vos_fixedstr.h"
#include "vos_heapbuf.h"
#include "vos_common.h"
#include "vos_wd_limits.h"

struct VosUser {
  FixedStr<21>  name;            // 3-20 caratteri
  FixedStr<65>  salt, hash;      // password mai in chiaro (hash con sale; hash = 64 cifre esadecimali)
  uint8_t role;                  // UserRole
  bool on;                       // attivo
  FixedStr<41> mfa;              // segreto MFA (40 cifre esadecimali), vuoto = MFA mai attivato
  bool mfaOn;                    // MFA attivo
  uint32_t mfaLast;              // ultimo contatore TOTP accettato (mai in calo)
  FixedStr<137> rec;             // codici di recupero: hash corti separati da virgola (8 x 17)
};

#define VOS_FW_MAX 16
#define FW_NAME_MAX 24
struct FwRule {
  uint32_t a, b;                 // intervallo di indirizzi (ordine "umano": primo numero nel byte alto)
  char name[FW_NAME_MAX + 1] = "";
  bool on;
};
// Mette il nome della regola: via gli a-capo di Windows, i caratteri non ASCII diventano '?', al massimo FW_NAME_MAX caratteri.
void fwRuleSetName(FwRule& r, const char* text, size_t len);
inline void fwRuleSetName(FwRule& r, const char* text) { fwRuleSetName(r, text, strlen(text)); }

// LED: valori di fabbrica dei colori per sorgente (azzurro e arancio per la flash, bianco e magenta per il Wi-Fi, rosso per la CPU),
// nomi delle chiavi nel file di configurazione e soglia di carico della CPU. Servono a cfgDefaults, import ed export.
constexpr uint32_t kLedSrcDefault[LEDSRC_COUNT] = {0x00A0FF, 0xFF8000, 0xFFFFFF, 0xFF00FF, 0xFF0000};
constexpr const char* kLedSrcKey[LEDSRC_COUNT] = {"colfr", "colfw", "colrx", "coltx", "colcpu"};
constexpr uint8_t LED_CPU_THR_DEFAULT = 70;

struct VosConfig {
  FixedStr<64> hostname;         // 1-32 caratteri
  FixedStr<64> domain;           // fino a 60
  bool   setupDone;              // prima configurazione completata
  FixedStr<33> apSsid;           // max 32
  FixedStr<65> apPass;           // max 63; apPass: unica e casuale per ogni scheda (creata al primo avvio)
  bool   staEnabled;
  bool     mqttAuto;             // MQTT: parte da solo all'avvio (solo dopo la prima attivazione a mano)
  FixedStr<81>  mqttHost;        // max 80
  FixedStr<61>  mqttUser, mqttPrefix;   // max 60
  FixedStr<128> mqttPass;
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
  FixedStr<33> staSsid;          // max 32
  FixedStr<65> staPass;          // max 63
  bool   staDhcp;
  FixedStr<20> ip, mask, gw, dns1, dns2;   // indirizzi IPv4 in testo
  FixedStr<65> authSalt, authHash;     // SOLO per leggere i file vecchi (1.7.0): diventano l'utente "admin"
  VosUser users[VOS_MAX_USERS];
  FixedStr<25> pinNote[VOS_GPIO_COUNT];            // inventario pin: "collegato a ..." scritto dall'utente (indice = GPIO)
  bool   apQr;                   // true = il QR Wi-Fi dell'hotspot si vede in Home a chi e entrato
  bool   wpsOn, wpsPin;         // WPS: servizio acceso (di fabbrica spento), metodo PIN consentito
  uint8_t btnAct[3];            // tasto BOOT: azione di 1, 2 e 3 click (vedi packages/vos_btn.h)
  uint32_t serBaud;              // seriale: velocita (solo UART; con USB nativa non conta). Di fabbrica 115200
  uint8_t  serEol, serTx;        // a-capo: 0 CR+LF, 1 LF, 2 CR; attesa massima di scrittura in ms
  bool     serEcho, serIn, serLogOut, serBanner;   // eco dei tasti, accetta comandi, scrive il log, mostra il benvenuto all'apertura
  bool   serialAuth;             // true = la shell seriale chiede il login: utente, password e, se acceso, codice MFA (predefinito)
  uint16_t serialIdleMin;        // minuti senza tasti dopo i quali la seriale esce da sola (0 = mai, predefinito; massimo 1440)
  uint8_t  ledMode;
  uint32_t ledColor;             // 0xRRGGBB
  uint8_t  ledBrightness;        // 0..255
  uint8_t  ledPin;
  uint32_t ledSrcColor[LEDSRC_COUNT];   // colori per sorgente dei trigger flash, wifi e cpu (0xRRGGBB)
  uint8_t  ledCpuThr;            // soglia di carico CPU (percento) oltre la quale il trigger cpu cambia colore
  bool   ntpOn, ntpServe;        // client NTP, server NTP per altri dispositivi
  bool   ntpAutoOff;             // NTP spento DALLA GUIDA (nessun Wi-Fi): si riaccende da solo quando arriva il Wi-Fi
  uint32_t ntpEvery;             // minuti tra due sincronizzazioni NTP (0 = solo all'avvio)
  FixedStr<64> ntpServer, ntpServer2, tz;   // max 60
  FixedStr<64> tzName;           // server, secondo server (vuoto = nessuno), fuso (formato POSIX), nome
  uint8_t dateFmt, timeFmt, tempUnit;   // data: 0 GG/MM/AAAA 1 AAAA-MM-GG 2 MM/GG/AAAA; ora: 0=24h 1=12h; temp: 0=C 1=F
  uint8_t weekStart, decSep;     // 0 = lunedi / 1 = domenica; 0 = virgola / 1 = punto
  FixedStr<4> country;             // paese (ISO 3166, es. "IT"); vuoto = non scelto (regole prudenti)
  uint8_t antExt;                // 0 = antenna interna, 1 = antenna esterna
  int8_t  antGain;               // guadagno antenna esterna in dBi (0-15)
  int8_t  txDbm;                 // potenza scelta (dBm); 0 = la massima consentita
  uint8_t  logLevel;             // 0 errori, 1 + attenzioni, 2 + info (di fabbrica), 3 + dettagli
  bool     statOn;               // statistiche d'uso (spente di fabbrica)
  uint8_t  pwMode;               // risparmio energia: 0 spento, 1 Wi-Fi a risparmio massimo, 2 sonno profondo a cicli
  uint16_t pwAwake, pwSleep;     // minuti sveglia / minuti di sonno (modo 2)
  uint16_t cpuMhz;               // 0 = governor (cpuGov), altrimenti velocita fissa 80/160/240
  uint8_t  cpuGov;               // governor quando cpuMhz = 0: 0 ondemand, 1 conservative, 2 schedutil (vedi vos_cpugov.h)
  uint8_t  cpuUp, cpuDown;       // soglie di salita e discesa (%) dei governor
  uint16_t cpuHold;              // secondi minimi prima di scendere
  FixedStr<16> lang;                // codice lingua: "it" (predefinita), "en" (interna) o file /lang/<codice>.json
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
  FixedStr<65> meshKey;             // chiave comune (64 cifre esadecimali)
  uint8_t meshCh;                // canale di partenza (1-13)
  // Bluetooth (BLE): spento di fabbrica; true = si accende all'avvio (impostazione dell'utente, vedi vos_ble.h)
  bool    bleOn;
  // HTTPS
  bool    https;                 // pagina e API cifrate (predefinito acceso)
  bool    apOn;                  // servizio Punto di accesso: se spento e la Wi-Fi di casa e configurata, l'hotspot non riparte da solo
  bool    apCaptive;             // portale automatico (DNS) dell'hotspot
  bool    dhcpOn;                // server DHCP dell'hotspot (assegna gli indirizzi ai telefoni)
  uint16_t dhcpLease;            // durata dell'indirizzo assegnato, in minuti (10-1440)
  bool    mdnsOn;                // annuncio nome.local (mDNS)
  bool    httpOn;                // server HTTP acceso (predefinito si)
  uint16_t httpPort, httpsPort;  // porte (predefinite 80 e 443)
  bool    httpRedirect;          // HTTP porta a HTTPS (307) per chi non e sull'hotspot; conta solo con HTTP e HTTPS accesi (predefinito no)
  // Watchdog
  bool    wdTask, wdNet, wdRam;  // controlli: task bloccati, rete assente, RAM bassa
  uint16_t wdNetMin;             // minuti senza rete prima del riavvio
  uint16_t wdRamKb;              // soglia RAM libera
  uint16_t wdUpDays;             // riavvio dopo N giorni di accensione (0 = mai)
  int16_t  wdAt;                 // riavvio programmato: minuti dalla mezzanotte (-1 = spento)
  uint8_t  wdTwdt;               // Task watchdog di ESP-IDF, secondi (5-60)
  uint16_t wdLim[WD_LIM_N];      // limiti del nostro watchdog per servizio (vedi vos_wd_limits.h)
  uint8_t  wdDays;               // giorni del riavvio programmato (bit 0 = lunedi ... bit 6 = domenica)
};

extern VosConfig cfg;

void   cfgDefaults();
bool   cfgLoad();                       // da file (true se trovato)
bool   cfgNtpAfterWifi();             // chiamare quando si imposta il Wi-Fi di casa: riaccende NTP se lo aveva spento la guida; true se e cambiato
bool   cfgSave();                       // salvataggio sicuro (.tmp/.bak) + controllo (allarmi) + registro delle modifiche
#define CFG_TEXT_MAX 32768              // tetto del testo della configurazione (esportazione)
bool   cfgExport(HeapBuf& out, bool withSecrets);    // testo OpenWrt in coda a out; false se manca la memoria o si supera il tetto
bool   cfgImport(const char* text, size_t len, ErrBuf& err);   // ripristino: chiavi assenti restano
void   cfgFactoryReset();
bool   cfgLastSaveOk();                // l'ultimo salvataggio della configurazione e riuscito?
bool   cfgSvcRecover();                 // riporta hotspot, HTTP, HTTPS e porte ai valori di fabbrica (true se qualcosa e cambiato)
const char* cfgSvcCheck(bool apOn, bool httpOn, int httpPort, bool httpsOn, int httpsPort);   // nullptr = ok, altrimenti il motivo del rifiuto (testo tradotto)
void   cfgSetOrigin(const char* who); //
void   cfgSetOriginf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));   // lo stesso con printf: cfgSetOriginf("web admin %s", ip)
// chi sta cambiando la configurazione (per il registro): "web admin 192.168.1.5"
bool   cfgFileChanged();                // il file e stato cambiato fuori dal pannello?
FixedStr<16> cfgNewApPass();            // password casuale per l'hotspot (12 caratteri a gruppi)
bool   cfgApPassWeak(const char* p);  // vuota, troppo corta o di fabbrica
bool   cfgParseRange(std::string_view txt, uint32_t& a, uint32_t& b);   // "1.2.3.4", "1.2.3.4-1.2.3.9", "1.2.3.0/24"
