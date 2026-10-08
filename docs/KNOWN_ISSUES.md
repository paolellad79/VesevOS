# VesevOS - Problemi noti (Known Issues)

Ultimo aggiornamento: 8 ottobre 2026, versione 1.7.11. Questo file si pubblica anche su GitHub (cartella docs/) e si aggiorna a ogni rilascio.
Stato: **A** = aperto, **M** = mitigato (c'e un rimedio), **S** = da sistemare prima della vendita, **O** = in osservazione.

## Memoria (RAM)
| # | Problema | Stato | Cosa fare |
|---|---|---|---|
| M1 | Dopo molti avvia/ferma del Bluetooth la RAM si frammenta: il pezzo libero piu grande scende (da ~83 KB a ~39 KB) e puo comparire `esp-aes: Failed to allocate memory`. | M | Dalla 1.7.8f: pausa di 5 s tra due accensioni e blocco minimo 45 KB. Dalla 1.7.9a: comando `free detail` e allarme giallo "Memoria frammentata". Per recuperare: riavviare la scheda. Rimedio vero (buffer in PSRAM, Bluetooth come package) ancora da fare. |
| M2 | RAM libera a riposo bassa (circa 125 KB; con pagina HTTPS aperta molto meno). Sketch vicino al limite: serve la partizione "Huge APP". | A | Non accendere tutto insieme. Piano di riduzione (String -> buffer fissi, package con #define) nel refactoring. |
| M3 | Non c'e spazio per due slot OTA da 1,9 MB con la partizione attuale. | A | OTA (1.8.1) da ripensare. |
| M4 | Con un servizio compilato ma **spento**, la RAM che torna libera non e ancora misurata (`ram mark` / `ram diff`). I package a 0 tolgono davvero codice e RAM; lo spegnimento da pagina potrebbe liberare meno. | A | Misurare sulla scheda; vedi `claude/VesevOS-valutazione-core-app.md`. |

## Stabilita
| # | Problema | Stato | Cosa fare |
|---|---|---|---|
| S1 | "Task watchdog" dopo un reset via USB (visto a ~7 s dall'avvio). | O | Non rivisto in oltre 11 avvii dalla 1.7.8c. Il diario dei riavvii (`reboots`) registra la fase `L:...` in cui si e fermato. Se succede: mandare `reboots` e `log 30`. |
| S2 | Il 4/10 il watchdog e scattato premendo "Rigenera chiave" mentre si accendeva la rete tra schede (ESP-NOW). Non riprodotto. | O | Ripetere separando i passi (prima "Rigenera chiave", poi "Accendi") con la seriale aperta; serve il file .elf della build per leggere il backtrace. |
| S3 | La 1.7.11 (Device Manager e API `/api/dev`, package, backup cifrato, modalita ridotta del watchdog, benvenuto seriale senza ripetizioni) e provata con stub, prove sul computer (144) e pagina con finto server, **non ancora sulla scheda**. | A | Provare: Device Manager, `dev list`, Backup e ripristino (con e senza segreti), pagina Radio, apertura del monitor seriale a scheda non configurata (il benvenuto non deve scorrere). |
| S4 | Modalita ridotta del watchdog (dopo 3 riavvii in un'ora): scritta e provata solo sul codice, non ancora provocata sulla scheda. Spegnere il Bluetooth dal task del watchdog non e stato misurato. | A | Se la scheda si riavvia da sola piu volte: mandare `reboots` e il messaggio completo "Task watchdog got triggered". |

## Sicurezza (da sistemare prima della vendita)
| # | Problema | Stato | Cosa fare |
|---|---|---|---|
| X1 | Si puo usare solo HTTP (scelta dell'utente, con avviso). | S | HTTPS predefinito e HTTP spento con Wi-Fi collegata. |
| X2 | La password admin appare in chiaro sulla seriale durante il `setup`. Serve l'accesso fisico alla scheda. | S | Password diversa per scheda (etichetta/QR), cambio obbligatorio al primo accesso. |
| X3 | Certificato HTTPS autofirmato: il browser avvisa "connessione non privata". | S | Confrontare l'impronta SHA-256 (pagina Servizi > HTTPS, comando `cert`). Piano: rinnovo, impronta nel QR, CA di prodotto (claude/VesevOS-piano-giallo-verde.md). |
| X4 | Procedura CRA (segnalazione falle, aggiornamenti), marchi, classificazione export, LGPL del core Arduino-ESP32, licenza dei dati `posix_tz_db`: voci di carta ancora aperte. | S | Vedi piano "da giallo a verde". Non e consulenza legale. |
| X5 | I buffer TLS stanno in PSRAM non cifrata (dati decifrati della sessione HTTPS). Servono cifratura della flash/PSRAM o i buffer in RAM interna. | S | Prima della vendita. |
| X6 | Backup con segreti: la sicurezza dipende dalla frase scelta (minimo 10 caratteri, 20000 giri PBKDF2). Una frase debole si puo indovinare provando con un computer. Frase persa = segreti persi. | A | Scegliere una frase lunga. Avviso gia nella pagina. |
| X7 | Le app esterne (Lua, futuro) non sono ancora firmate: in sviluppo si usa SHA-256. Effetto di app di terzi sulla conformita CE/CRA da chiedere a un consulente. | S | Prima della vendita. |

## Funzioni
| # | Problema | Stato | Cosa fare |
|---|---|---|---|
| F2 | Le schede gia configurate prima della 1.7.9a con NTP spento dalla guida non hanno il segno "spento dalla guida": NTP non si riaccende da solo. (Dalla 1.7.9a vale per le guide nuove.) | M | Accendere NTP a mano in Servizi. |
| F3 | Nel sonno il pulsante BOOT non sveglia la scheda (solo timer o RESET). Wi-Fi e pagina sono spenti. | A | Limite dichiarato. |
| F4 | Le statistiche non contano pin ne consumi. | A | Limite dichiarato. |
| F5 | Seriale USB (CDC): la velocita (baud) non si cambia (e ignorata); vale solo su porta UART. Il test di velocita dura 20 s e va confermato con `serial keep`. | A | Normale per USB nativa. |
| F6 | Fine riga della seriale: in PuTTY serve "CR+LF" (predefinito) per non vedere le righe a scalino; si cambia in Sistema > Seriale o con `serial eol`. | M | |
| F7 | I pulsanti Bluetooth possono restare grigi per qualche secondo dopo un "ferma" (pausa di 5 s). | M | Attendere. |
| F8 | Riavvii "Task watchdog" visti sulla scheda (anche dopo 13 ore) con la RAM minima sotto 40 KB. La 1.7.9a toglie le cause probabili (seriale che blocca, collegamenti HTTPS fermi) e il diario ora dice di piu. | O | Mandare `reboots` e il messaggio "Task watchdog got triggered" del monitor seriale. |

## Compilazione
(Corretti nella 1.7.9a: F1 `wifi set` con spazi, C1 avviso `inList`.)

| # | Problema | Stato | Cosa fare |
|---|---|---|---|
| C2 | Servono PsychicHttp 3.1.2 e ArduinoJson 7; senza PsychicHttp non compila. | A | Installarle dal gestore librerie. |

## Come segnalare
Mandare: versione (`ver`), `reboots`, `log 30`, `free detail` e cosa si stava facendo. Non incollare password o chiavi.
