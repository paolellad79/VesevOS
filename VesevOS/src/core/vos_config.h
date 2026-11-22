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
#include "vos_datefmt.h"

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

// Programma con avvio automatico: kind 0 script, 1 app, 2 app ELF (come in packages/vos_prog.h).
#define VOS_BOOTPROG_MAX 8
#define VOS_BOOTPROG_NAME 24
struct BootProg {
  uint8_t kind;
  char name[VOS_BOOTPROG_NAME + 1] = "";
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
// LED a colori: tonalita dei sei stati di rete (stessi numeri di NetState: avvio, hotspot, collegamento, collegato, aereo, attesa)
// e guadagno dei tre canali (rosso, verde, blu: 255 = nessuna correzione). Servono a cfgDefaults, import ed export e alla pagina.
constexpr int LED_STATI = 6;
constexpr uint32_t kLedStatoDefault[LED_STATI] = {0xFF7800, 0x0000FF, 0xFFFF00, 0x00FF00, 0xAA00FF, 0xFF7800};
constexpr const char* kLedStatoKey[LED_STATI] = {"stavvio", "stap", "stcoll", "stok", "staereo", "statt"};
constexpr const char* kLedGuadKey[3] = {"guadr", "guadv", "guadb"};

struct VosConfig {
  FixedStr<64> hostname;         // 1-32 caratteri
  FixedStr<64> domain;           // fino a 60
  FixedStr<201> messaggioLogin;  // messaggio della pagina di accesso, pubblico, fino a 200 caratteri (vedi vos_msglogin.h)
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
  bool   ip6On;                  // IPv6 sulla Wi-Fi di casa (di fabbrica spento)
  bool   ip6Fisso;               // true = oltre agli indirizzi del router, un indirizzo fisso (ip6Ind, prefisso /64)
  FixedStr<40> ip6Ind;           // indirizzo IPv6 fisso in testo
  FixedStr<40> ip6Gw, ip6Dns;    // gateway e DNS IPv6 a mano (vuoto = automatico, dal router)
  FixedStr<65> authSalt, authHash;     // SOLO per leggere i file vecchi (1.7.0): diventano l'utente "admin"
  VosUser users[VOS_MAX_USERS];
  FixedStr<25> pinNote[VOS_GPIO_COUNT];            // inventario pin: "collegato a ..." scritto dall'utente (indice = GPIO)
  bool   apQr;                   // true = il QR Wi-Fi dell'hotspot si vede in Home a chi e entrato
  bool   wpsOn, wpsPin;         // WPS: servizio acceso (di fabbrica spento), metodo PIN consentito
  uint8_t btnAct[3];            // tasto fisico: azione di 1, 2 e 3 click (vedi packages/vos_btn.h)
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
  uint8_t  ledGain[3];           // calibrazione: guadagno 0..255 di rosso, verde e blu (255 = nessuna correzione)
  uint32_t ledStato[LED_STATI];  // tonalita (0xRRGGBB) degli stati di rete mostrati dal LED a colori
  uint8_t  led2Mode;             // LED2 (un solo colore): stesso elenco dei modi del LED1 (LedMode), senza colori
  uint8_t  led2CpuThr;           // LED2: soglia di carico CPU (percento) del modo cpu
  uint8_t  led2Brightness;       // LED2: luminosita 0..255 (PWM), di fabbrica 255 = come prima che l'avesse
  bool   ntpOn, ntpServe;        // client NTP, server NTP per altri dispositivi
  bool   ntpAutoOff;             // NTP spento DALLA GUIDA (nessun Wi-Fi): si riaccende da solo quando arriva il Wi-Fi
  uint32_t ntpEvery;             // minuti tra due sincronizzazioni NTP (0 = solo all'avvio)
  FixedStr<64> ntpServer, ntpServer2, tz;   // max 60
  FixedStr<64> tzName;           // server, secondo server (vuoto = nessuno), fuso (formato POSIX), nome
  uint8_t dateFmt, timeFmt, tempUnit;   // data corta: 0 GG/MM/AAAA 1 AAAA-MM-GG 2 MM/GG/AAAA 3 scritta a mano (dataCortaTesto); ora: 0=24h 1=12h; temp: 0=C 1=F
  uint8_t dataLungaFmt;          // data lunga: 0 abbreviata ("Mar. 3 Nov. 2026"), 1 completa ("Martedi 3 novembre 2026"), 2 scritta a mano (dataLungaTesto)
  uint8_t oraLungaFmt;           // ora lunga: 0 pronta, 1 scritta a mano (oraLungaTesto)
  bool    oraLungaSec;           // l'ora lunga ha i secondi (l'ora corta non li ha mai)
  FixedStr<DF_TESTO_MAX + 1> dataCortaTesto, dataLungaTesto, oraLungaTesto;   // formati scritti a mano (sintassi in vos_datefmt.h), al massimo 32 caratteri
  int16_t tempTarSoc, tempTarExt;   // taratura della temperatura in decimi di grado Celsius (-200..200): sensore del chip, sensore esterno
  uint8_t weekStart, decSep;     // 0 = lunedi / 1 = domenica; 0 = virgola / 1 = punto
  FixedStr<4> country;             // paese (ISO 3166, es. "IT"); vuoto = non scelto (regole prudenti)
  uint8_t antExt;                // 0 = antenna interna, 1 = antenna esterna
  int8_t  antGain;               // guadagno antenna esterna in dBi (0-15)
  int8_t  txDbm;                 // potenza scelta (dBm); 0 = la massima consentita
  uint8_t  logLevel;             // 0 errori, 1 + attenzioni, 2 + info (di fabbrica), 3 + dettagli
  bool     statOn;               // statistiche d'uso (spente di fabbrica)
  uint8_t  pwMode;               // risparmio energia: 0 spento, 1 Wi-Fi a risparmio massimo, 2 sonno profondo a cicli
  uint16_t pwAwake, pwSleep;     // minuti sveglia / minuti di sonno (modo 2)
  bool     recOn;                // registratore di dati: spento di fabbrica
  uint16_t recSec;               // secondi tra due campioni (1-86400; sulla flash almeno 10)
  uint8_t  recWhere;             // dove salva: 0 memoria interna (/var/lib/rec), 1 microSD (/media/sd/rec)
  uint16_t recMaxKb;             // tetto di un file CSV in KB (flash 4-1024, microSD 4-65535)
  bool     recT, recR;           // fonti: temperatura del chip, RAM libera
  FixedStr<16> recDev[4];        // fonti: nomi di periferiche (vuoto = nessuna)
  FixedStr<25> recNome;          // nome dei file del registratore (a-z A-Z 0-9 _ -, 1-24 caratteri; di fabbrica "rec")
  uint8_t  recLimite;            // spazio massimo: 0 = totale di tutti i file della cartella, 1 = per ogni singolo file
  uint16_t recSecDev[4];         // secondi tra due letture di ogni periferica (0 = usa recSec)
  uint16_t recSecT, recSecR;     // lo stesso per temperatura del chip e RAM libera
  uint16_t recAzioni;            // categorie di azioni tracciate (un bit per categoria, vedi core/vos_azioni.h)
  uint16_t recEvMax;             // righe di azioni al giorno al massimo (0 = nessun limite)
  uint16_t cpuMhz;               // 0 = governor (cpuGov), altrimenti velocita fissa 80/160/240
  uint8_t  cpuGov;               // governor quando cpuMhz = 0: 0 ondemand, 1 conservative, 2 schedutil (vedi vos_cpugov.h)
  uint8_t  cpuUp, cpuDown;       // soglie di salita e discesa (%) dei governor
  uint16_t cpuHold;              // secondi minimi prima di scendere
  FixedStr<16> lang;                // codice lingua: "it" (predefinita), "en" (interna) o file /lang/<codice>.vld
  bool   sdEnabled;
#if VOS_WITH_ELF
  bool   elfOn;                  // sottosistema vde (ELF): di fabbrica ACCESO dal 31/10/2026; solo Admin lo cambia (dai Task); codice nativo senza protezione di memoria
#endif
  bool   funzioniAttive;                // sottosistema vdb (funzioni VesevOS): di fabbrica acceso; spento = nessuno script, app o comando di driver parte
  uint16_t funcOff;              // Funzionalita tolte dall'utente (Sistema > Funzionalita): un bit per id di vos_func_ids.h; 0 = tutte concesse
  uint8_t  bootN;                // programmi con avvio automatico (di fabbrica nessuno)
  BootProg boot[VOS_BOOTPROG_MAX];   // l'ordine e l'ordine di avvio, dopo i servizi
  uint8_t  dataSdN;              // app che salvano i dati sulla microSD (scelta dell'utente; le altre usano la flash)
  BootProg dataSd[VOS_BOOTPROG_MAX];   // kind 1 app, 2 app ELF (come boot)
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
  uint8_t  wdRamPerc;            // soglia CRITICA: % di RAM interna USATA (di fabbrica 93; limiti 80-99). Prima era wdRamKb (KB liberi)
  uint16_t wdUpDays;             // riavvio dopo N giorni di accensione (0 = mai)
  int16_t  wdAt;                 // riavvio programmato: minuti dalla mezzanotte (-1 = spento)
  uint8_t  wdTwdt;               // Task watchdog di ESP-IDF, secondi (5-60)
  uint16_t wdLim[WD_LIM_N];      // limiti del nostro watchdog per servizio (vedi vos_wd_limits.h)
  uint8_t  wdDays;               // giorni del riavvio programmato (bit 0 = lunedi ... bit 6 = domenica)
  // Aggiornamenti (spenti di fabbrica): vedi packages/vos_aggiorna.h
  uint8_t  aggModo;              // 0 spento, 1 solo notifica, 2 installa da solo
  uint8_t  aggFreq;              // 0 ogni giorno, 1 ogni settimana, 2 ogni mese
  uint8_t  aggGiorno;            // settimana: 1 lunedi ... 7 domenica; mese: 1-28
  uint16_t aggOra;               // minuti dalla mezzanotte, ora locale
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
void   cfgSetRamScarsa(bool on);         // il watchdog della RAM avvisa: RAM scarsa (il registro delle modifiche non tiene copie)
void   cfgRilasciaCopia();             // libera la copia in RAM del registro delle modifiche (RAM scarsa)
bool   cfgLastSaveOk();                // l'ultimo salvataggio della configurazione e riuscito?
bool   cfgSvcRecover();                 // riporta hotspot, HTTP, HTTPS e porte ai valori di fabbrica (true se qualcosa e cambiato)
const char* cfgSvcCheck(bool apOn, bool httpOn, int httpPort, bool httpsOn, int httpsPort);   // nullptr = ok, altrimenti il motivo del rifiuto (testo tradotto)
void   cfgSetOrigin(const char* who); //
void   cfgSetOriginf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));   // lo stesso con printf: cfgSetOriginf("web admin %s", ip)
// chi sta cambiando la configurazione (per il registro): "web admin 192.168.1.5"
bool   cfgFileChanged();                // il file e stato cambiato fuori dal pannello?
FixedStr<16> cfgNewApPass();            // password casuale per l'hotspot (12 caratteri a gruppi)
bool   cfgApPassWeak(const char* p);  // vuota, troppo corta o di fabbrica
bool   cfgSetupServices();            // fine guida: con la Wi-Fi di casa configurata spegne hotspot, portale e DHCP (senza salvare)
bool   cfgEnsureApPass();             // se manca o e debole ne crea una (senza salvare); true = cambiata
bool   cfgParseRange(std::string_view txt, uint32_t& a, uint32_t& b);   // "1.2.3.4", "1.2.3.4-1.2.3.9", "1.2.3.0/24"
