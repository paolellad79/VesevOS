# VesevOS - REGISTRO LEGALE, continuazione: sezione L (1.7.40 - 1.7.45, 10 ottobre 2026) e sezione M (1.7.46 - 1.8.0b, 25 ottobre 2026)
(Continua il registro legale precedente, che arriva alla 1.7.9a; il semaforo completo della 1.7.45 e nei documenti interni. Non e consulenza legale.)

## A bis. Componenti nuovi o usati in modo nuovo (aggiunta all'inventario A)
| Componente | Uso | Licenza | Obblighi | Semaforo |
|---|---|---|---|---|
| Recovery 0.3.0 (nostro, recovery/VesevOS_Recovery) | programma di aggiornamento nella partizione factory | GPL-3.0-or-later OR commerciale | SPDX, NOTICE | 🟢 |
| Arduino-ESP32 core: WebServer, WiFi, ESPmDNS, LittleFS, Preferences (usati nel recovery) | pagina di caricamento, rete, file di configurazione in sola lettura | LGPL-2.1+ | come il core: kit di ricollegamento per il commerciale (COMMERCIAL.md) | 🟡 (solo commerciale) |
| ESP-IDF esp_ota_ops / esp_partition | scrittura e attivazione dell'immagine | Apache-2.0 | testo + NOTICE (gia presente) | 🟢 |
| Mbed TLS (SHA-256, HMAC) nel recovery | controllo dello SHA-256 e login a sfida e risposta | Apache-2.0 | testo (gia presente) | 🟢 |
| esptool 4.7 (Espressif) | strumento che l'utente installa (pip) per la prima installazione e il backup | GPL-2.0-or-later (PyPI, 10/10/2026) | NON incluso nel repo ne nel firmware: solo richiamato dagli script; nessun obbligo di distribuzione | 🟢 |
| Stub di prova esp_ota_ops.h, esp_partition.h, WebServer.h, WiFi.h, Update.h | solo prova di compilazione, scritti da noi | nostra | - | 🟢 |

## L. Pacchetti 1.7.40 - 1.7.45
| Voce | Cosa | Semaforo |
|---|---|---|
| 1.7.40-1.7.41 RAM: `ram audit`, task e tabelle delle regole solo se servono | solo comportamento interno e diagnostica; nessun dato personale, nessuna libreria nuova | 🟢 |
| 1.7.42-1.7.45 Comando `recovery` e voce di menu "Aggiorna firmware" | solo Amministratore; scrive una riga nel registro; nessun dato esce dalla scheda | 🟢 |
| Rotte GET/POST /api/recovery | Admin; rotte API 129 controllate da checkapi | 🟢 |
| Recovery: rete di casa poi hotspot | usa il paese della configurazione (radio invariata); hotspot VesevOS-recovery con password casuale di 12 caratteri scritta solo sulla seriale, canale 1, 2 client; senza amministratori codice a 8 cifre casuale | 🟢 |
| Recovery: login amministratore | stessa formula della pagina (password mai in chiaro); 5 errori = blocco 60 s; token di sessione 10 min | 🟢 |
| Recovery: controlli sul file | intestazione ESP32-S3, marchio {FW:VesevOS}, dimensione, SHA-256 facoltativo, esp_ota_end; file sbagliato = vecchio firmware intatto | 🟢 |
| Recovery solo HTTP | niente TLS (RAM/spazio): il token e il file si vedono nella LAN; la password non viaggia | 🟡 prima della vendita (TLS nel recovery) |
| Aggiornamenti senza firma digitale | integrita si, autenticita no | 🟡 CRA/EN 18031 per chi vende: firma Ed25519 + chiave dell'editore |
| GPLv3 sezione 6 | nessun secure boot ne firma obbligatoria: chi compila una sua versione la installa dalla pagina | 🟢 (da rivedere se si introduce la firma: chiavi per l'utente o commerciale) |
| Recovery solo in italiano | accessibilita/EAA | 🟡 prima della vendita |
| Nessuna partizione core dump | nessuna fuga di dati; meno diagnostica | 🟢 |
| Guida "Preparare la scheda" it/en | solo testi nostri; cita esptool e Arduino IDE in modo descrittivo; nessun logo | 🟢 |
| Nessun dato personale nuovo, nessun invio fuori, nessuna icona o servizio esterno | | 🟢 |

Date verificate sul web il 10/10/2026: CRA = segnalazione di vulnerabilita sfruttate e incidenti gravi (ENISA, 24 h / 72 h) dall'11/9/2026, tutto il resto dall'11/12/2027 (developer.espressif.com, jonesday.com); RED cybersecurity (EN 18031) dal 1/8/2025; esptool GPL-2.0+ (pypi.org). Da riverificare prima della vendita: PSTI (29/4/2024), PLD (9/12/2026), considerando CRA su EUR-Lex.
Controllo legale: 🟡 (🟢 12 / 🟡 8 / 🔴 0; nessun rosso, i gialli sono da sistemare prima della vendita). NOTICE.txt invariato (nessuna libreria nuova rispetto all'elenco: core Arduino-ESP32, ESP-IDF, Mbed TLS gia citati).

---

## M. Pacchetti 1.7.46 - 1.8.0b (sintesi, 25 ottobre 2026)
Righe sintetiche raggruppate per gruppi di versioni, scritte leggendo CHANGELOG.md. Non e consulenza legale; le norme citate potrebbero essere cambiate. Semaforo completo: `docs/progetto/semaforo-legale-1.8.0.md` (valido anche per la 1.8.0b, che aggiunge solo testi senza nomi di terzi e la correzione dei ruoli nella pagina LED/QR).

| Gruppo di versioni | Cosa | Semaforo |
|---|---|---|
| 1.7.46 - 1.7.55 | Servizi, Berry (MIT, script `.vsc`/app `.vap` con permessi), pagina Script, Device Manager, periferiche (`vos_io`), schede modulo `.vdp` | 🟢 (Berry MIT, nessun dato esce) |
| 1.7.56 - 1.7.57a | WPS (collegarsi al router senza scrivere la password; WPS di ESP-IDF), tasto BOOT per gli script, schema dei collegamenti | 🟢 (WPS: protocollo noto debole, spento finche non lo usi; non provato su router veri) |
| 1.7.58 - 1.7.60a | Aggiornamento del recovery dalla pagina (Admin, controlli del file), `who`, 10 sessioni, rapporto crash, inglese come file, `logtail`/`mqtt` per Berry | 🟢 |
| 1.7.61 - 1.7.66a | Sensori I2C (BMP280), file di scheda, controlli degli strati, microSD come periferica (solo FAT/FAT32 con nomi lunghi; exFAT rifiutato) | 🟢 (nessun exFAT: divieto fisso rispettato) |
| 1.7.67 - 1.7.70h | ESP-IDF puro senza Arduino (niente LGPL), PSRAM, BLE spento di fabbrica, redirect HTTP->HTTPS, aggiornamento tramite recovery (ordine nella NVS, modo 2 file dal computer con gettone monouso, modo 1 URL + SHA-256 a mano), governor CPU, ticket di sessione TLS (4 h), watchdog, server HTTP minimo, correzioni | 🟢 / 🟡 (vedi gialli 3 e 4) |
| 1.8.0 - 1.8.0b | Shell con comandi classici (pwd/cd/ls, ping, gzip/unzip/tar/zip, vesevos-*), login seriale con utente/password/MFA e ruoli, `/api/sleep` solo Admin, LED con trigger, testi senza nomi di terzi | 🟢 (accessi per ruolo; nessun dato personale nuovo; codice di gzip/zip/tar scritto da noi, giudizio limitato: non confrontato riga per riga con altri progetti) |
| Recovery 0.5.0 - 0.5.1 | Recovery ESP-IDF: ordini di aggiornamento modo 1/2, gettone monouso (vale anche per l'avvio), solo HTTP, solo italiano | 🟡 (gialli 3 e 4) |
| Pagina di prima installazione (`flash/`, esptool-js 0.5.7 Apache-2.0 con pako 2.1.0 MIT AND Zlib, tutto locale, nessun CDN) | scrive la scheda dal browser (Web Serial); controlli SHA-256, marchio, dimensione; NON PROVATA con una scheda reale | 🟡 (giallo 6: privacy GitHub Pages; informativa breve scritta) |
| Cartella `firmware/` (indice.json con `publisher` e `channel`) | file compilati con SHA-256; origine/canale sono scritte non firmate | 🟡 (giallo 3) |

### Componenti nuovi o chiariti (aggiunta all'inventario A)
| Componente | Uso | Licenza | Esito |
|---|---|---|---|
| esptool-js 0.5.7 (Espressif) | pagina `flash/` | Apache-2.0 (testo in `flash/vendor/`) | 🟢, in NOTICE e SBOM |
| pako 2.1.0 (dentro il bundle di esptool-js) | decompressione nel browser | MIT AND Zlib | 🟢, in NOTICE e SBOM |
| miniz (tinfl/tdefl nella ROM del chip e header di ESP-IDF) | gzip/zip/unzip della shell | MIT | 🟢, in NOTICE e SBOM |
| Recovery 0.5.1 (nostro) | programma di aggiornamento | GPL-3.0-or-later OR commerciale | 🟢, in NOTICE e SBOM |
| `esp_ping` (ESP-IDF) | comando `ping` | Apache-2.0 | 🟢 |
| newlib 4.3.0 (ESP-IDF) | libreria C | BSD misto; nel `COPYING.NEWLIB` di ESP-IDF 5.5.5 le due voci LGPL (21 e 22) valgono solo per target linux e i386, non Xtensa | 🟡 letto il file COPYING, NON controllato il codice collegato file per file |
| Bundle certificati (dati Mozilla, copia curl.se del 25/2/2025) | HTTPS verso l'esterno (firmware completo, recovery ridotto) | il file di ESP-IDF cita l'origine e non porta il testo della licenza; in genere MPL-2.0 (dati) | DA VERIFICARE (alla fonte non verificata) |
| posix_tz_db | fusi orari in formato POSIX | indicata MIT; testo non nei sorgenti locali | DA VERIFICARE |

### Gialli accettati come scelte consapevoli (semaforo 1.8.0: 🟢 14 / 🟡 8 / 🔴 0)
1. NOTICE e SBOM: aggiunti esptool-js, pako, miniz e Recovery 0.5.1 in questo rilascio (sistemato).
2. Voci "DA VERIFICARE" aperte: bundle certificati (MPL-2.0 non verificata), posix_tz_db (MIT non verificata), newlib (letto COPYING, non controllato file per file).
3. Aggiornamenti senza firma: scelta dell'utente del 25/10/2026, niente firma Ed25519 per ora. Solo SHA-256 + `publisher`/`channel` come scritte non firmate; integrita si, autenticita no. Nessun aggiornamento automatico. Prima della vendita: firma (CRA, EN 18031).
4. Recovery solo HTTP e solo italiano (nessun TLS, nessuna traduzione; accessibilita/EAA): prima della vendita.
5. CRA / PSTI per chi vende: SECURITY.md c'e; canale ENISA e periodo di supporto da dichiarare. Progetto gratuito fuori da attivita commerciale: in genere non soggetto, cambia se diventa a pagamento.
6. Privacy della pagina GitHub: GitHub Pages vede IP e richieste; nessun cookie, tracker o upload; informativa breve nella pagina e nel README di `flash/`.
7. Marchi: i testi del prodotto non nominano piu marchi di terzi (tolti "Debian", "Linux", "GNU" nella 1.8.0b); il nome VesevOS non e registrato; restano da valutare i nomi nei commenti dei sorgenti.
8. Marchio VesevOS non registrato; CE/RAEE/RoHS e classificazione export: a carico di chi vende, prima della vendita.

Date delle norme: lette solo come scritte nei documenti del progetto (RED/EN 18031 dal 1/8/2025, CRA segnalazioni dall'11/9/2026 e obblighi principali dall'11/12/2027, PSTI dal 29/4/2024); NON riverificate sul web in questa occasione.
Controllo legale: 🟡 (🟢 14 / 🟡 8 / 🔴 0; nessun rosso; i gialli sono scelte accettate o da sistemare prima della vendita). NOTICE.txt, SBOM.spdx.json e registro aggiornati il 25/10/2026.
