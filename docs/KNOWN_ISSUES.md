# VesevOS - Problemi noti (Known Issues)

Ultimo aggiornamento: 8 ottobre 2026, versione 1.7.39. Questo file si pubblica anche su GitHub (cartella docs/) e si aggiorna a ogni rilascio.
Stato: **A** = aperto, **M** = mitigato (c'e un rimedio), **S** = in programma per la versione commerciale, **O** = in osservazione.

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
| S5 | La 1.7.12 divide il Bluetooth in tre file (driver `vos_drv_ble`, ciclo di vita `vos_ble`, comandi `vos_ble_cfg`). Provata con stub e prove sul computer (comandi), **non sulla scheda**. | A | Provare: `ble on`, accoppiamento col codice, comandi `status`, `wifi`, `done` da un telefono, spegnimento a 10 minuti, `ble off` e riaccensione dopo 5 secondi. RAM libera dopo lo spegnimento uguale a prima. |
| S6 | La 1.7.13 sposta i file in `VesevOS/src/<strato>/`. Provata con stub e prove sul computer, **non con Arduino IDE**. | A | Compilare lo sketch dalla cartella `VesevOS/` come sempre. Se l'IDE dice "No such file" o "was not declared" mandami l'errore intero (puo servire un percorso di include). Controllare anche che la dimensione dello sketch sia uguale alla 1.7.12 (nessun codice cambiato). |
| S7 | La 1.7.14 introduce il registro dei servizi (`vos_service`) e iscrive il Bluetooth. Provata con stub e prove sul computer; **non sulla scheda**. | A | Provare `ble on`, `ble off`, `ble on` dopo 5 secondi, accensione/spegnimento dalla pagina e da `dev`. Comportamento atteso: identico alla 1.7.13. |
| S8 | La 1.7.15 divide ESP-NOW in driver (`vos_drv_espnow`) e servizio (`vos_mesh`). Provata con stub, **non sulla scheda** (serve una seconda scheda per vedere i messaggi). | A | Con 2 schede: `mesh start` su entrambe, controllare che si vedano (`mesh`), mandare `mesh send <nome> ciao`, provare `mesh stop`/`start` e lo stesso dalla pagina. Atteso: identico alla 1.7.14. |
| S9 | La 1.7.16 divide MQTT in driver (`vos_drv_mqtt`) e servizio (`vos_mqtt`). Provata con stub, **non sulla scheda**. | A | Con un broker: avvio/stop/riavvio dalla pagina e da `mqtt start|stop|restart`, controllare `state` e `cmd` (es. `led-color ff0000`), prova con TLS e senza, test dopo cambio impostazioni mentre e acceso. Atteso: identico alla 1.7.15. |
| S10 | La 1.7.17 sposta tutte le chiamate Wi-Fi di `vos_net` nel driver `vos_drv_wifi`. Provata con stub, **non sulla scheda**: e la parte piu delicata (se sbagliata la scheda non si collega o non fa l'hotspot). | A | Provare: avvio con rete di casa (si collega, IP giusto); avvio senza rete (hotspot, password dall'etichetta); IP statico; scansione reti dalla pagina; prova rete di casa della guida; cambio canale hotspot con ESP-NOW; modo aereo on/off; staccare il router e riaccenderlo. Se qualcosa non va: BOOT 8 s (recupero) e mandami il log. |
| S11 | La 1.7.18 lega il DHCP all'hotspot. Provata con stub, **non sulla scheda**. | B | Provare: finire la guida con rete di casa (hotspot e DHCP spenti); riaccendere l'hotspot da Rete (DHCP si accende); senza rete di casa l'hotspot da' indirizzi; scheda gia configurata: dopo l'avvio DHCP spento. |
| S12 | La 1.7.19 sposta nel driver Wi-Fi paese/potenza, risparmio energia e letture di segnale. Provata con stub, **non sulla scheda**. | B | Provare: `region` / pagina Radio (paese e potenza applicati, log RADIO), modo risparmio energia, segnale nella Home, Autotest (prova Wi-Fi), sonno profondo. |
| S13 | La 1.7.20 sposta tutte le operazioni sui file nel driver `vos_drv_fs`. Provata con stub, **non sulla scheda**: tocca la configurazione e il diario. | A | Provare: avvio normale (config caricata); salvare una impostazione e riavviare; Sistema > File (elenco, nuova cartella, scrivi, rinomina, copia, cancella, scarica, carica); cambio lingua; diario dopo riavvio; Autotest (prova memoria); certificato MQTT. Se la config sparisce: BOOT 8 s e mandami il log. |
| S14 | La 1.7.21 sposta pinMode/digitalWrite/digitalRead nel driver `vos_drv_gpio`. Provata con stub, **non sulla scheda**. | B | Provare: tasto BOOT (premuto adesso in Device Manager; BOOT 8 s = recupero); Pin > prova di un pin libero (alto, basso, lampeggio, lettura con pull); una regola che accende un pin; Autotest (prova tasto). |
| S15 | La 1.7.22 aggiunge `apiVersion` e il campo `code` negli errori. Provata con stub e pagina finta, **non sulla scheda**. | B | Provare: la pagina funziona come prima (accesso, errori mostrati con il testo); `/api/status` mostra `apiVersion:1`; password sbagliata e ruolo insufficiente mostrano il messaggio e hanno `code`. |
| S16 | La 1.7.23 porta `/api/mesh/key` da GET a POST. Provata con stub e pagina finta, **non sulla scheda**. | B | Provare: Rete schede > mostra la chiave (appare); riga "MESH: chiave mostrata" nel registro; altre schede ancora collegate. |
| S17 | La 1.7.24 fa rispondere gli errori con il codice HTTP giusto (400/403/404/409/413/429). Provata con stub e pagina finta, **non sulla scheda**: la pagina deve mostrare i messaggi come prima. | A | Provare: password sbagliata (messaggio rosso, NON torna alla schermata di accesso); troppi tentativi (blocco); valore non valido in una scheda (IP sbagliato); spegnere l'hotspot senza Wi-Fi di casa (messaggio); file troppo grande; guida: password debole. Mai deve comparire un errore "vuoto". |
| S18 | La 1.7.25 divide `vos_web.cpp` in sei file. Solo spostamenti, provata con stub e pagina finta, **non sulla scheda**. | B | Provare: si compila; la pagina funziona come prima (accesso, Home, Rete, Periferiche, File, Autotest). La dimensione dello sketch deve essere quasi uguale alla 1.7.24 (scrivimela). |
| S19 | La 1.7.26 divide `vos_shell.cpp` in quattro file. Solo spostamenti, provata con stub, **non sulla scheda**. | B | Provare da seriale e shell web: help, status, ls/cat, wifi, led, pin, svc, user, mesh, mqtt, power, reboot. Scrivimi la dimensione dello sketch. |
| S20 | La 1.7.27 divide `vos_config.cpp` in tre file. Solo spostamenti, provata con stub e test sull'host, **non sulla scheda**. | B | Provare: si compila; dopo il riavvio le impostazioni restano; Sistema > esporta/importa configurazione funziona. Scrivimi la dimensione dello sketch. |
| S21 | La 1.7.28 spezza il sorgente della pagina in `web/src/`. La pagina finale e identica alla 1.7.27 (confrontata byte per byte), provata con stub e pagina finta. | B | Provare: si compila; la pagina funziona come prima. Dimensione dello sketch uguale alla 1.7.27 (2015923). |
| S22 | La 1.7.29 limita l'elenco file (200 voci) e le reti trovate (40) e aggiunge `apiVersion` a `/api/common`. Provata con stub e pagina finta, **non sulla scheda**. | B | Provare: File > cartella normale si vede come prima; Rete > cerca reti funziona. Se hai una cartella con piu di 200 file compare l'avviso "Elenco parziale". Scrivimi la dimensione dello sketch. |
| S23 | La 1.7.30 sposta la temperatura nel widget CPU sotto il grafico arancione. Provata con la pagina finta (immagine controllata), **non sulla scheda**. | B | Guardare la Home: sotto CPU solo i MHz; sotto il grafico arancione "Temp. xx.x °C". |
| S25 | L'Event Bus (1.7.32-1.7.33) e confermato sulla scheda per `svc.state`, `net.state`, `sec.ban`, `sec.unban`. Manca solo da vedere `audit.new` / `audit.clear`. | B | Al primo allarme giallo (es. `frag`, memoria frammentata dopo il Bluetooth) scrivi `events`: deve comparire `audit.new` con il codice dell'allarme; quando rientra, `audit.clear`. |
| S28 | La 1.7.38 aggiunge gli eventi `mqtt.link`, `ble.link`, `mesh.node`, `time.sync`. Provata solo con stub, **non sulla scheda**. | B | Shell `events` dopo: collegamento del telefono al Bluetooth (`ble.link 1`, poi 0 allo scollegamento); accensione/spegnimento MQTT con broker raggiungibile (`mqtt.link`); sincronizzazione NTP (`time.sync`, anche con `ntp sync`). Se hai due schede: `mesh.node` all'arrivo di una vicina. |
| S29 | La 1.7.39 fa un avvio a vuoto del Bluetooth al boot per evitare che dopo il primo `ble on`/`ble off` il pezzo di RAM piu grande scenda da 79 a 47 KB. Provata solo con stub, **non sulla scheda**. | A | Nel log di avvio cerca "BLE: memoria preparata al boot" (prima/dopo). Poi `free detail` a riposo (prima: 121 KB liberi, pezzo piu grande 79 KB), `ble on` + `free detail`, `ble off` + `free detail`: il pezzo piu grande dopo lo spegnimento deve restare vicino a quello a riposo (prima 47 KB). Controlla che il boot non sia piu lento di piu di 1 secondo e che il Bluetooth si accenda come prima. Se la scheda si riavvia da sola al boot: rimetti `VOS_BLE_PRIME 0`. |

## Sicurezza (in programma per la versione commerciale)
| # | Problema | Stato | Cosa fare |
|---|---|---|---|
| X1 | Si puo usare solo HTTP (scelta dell'utente, con avviso). | S | HTTPS predefinito e HTTP spento con Wi-Fi collegata. |
| X2 | La password admin appare in chiaro sulla seriale durante il `setup`. Serve l'accesso fisico alla scheda. | S | Password diversa per scheda (etichetta/QR), cambio obbligatorio al primo accesso. |
| X3 | Certificato HTTPS autofirmato: il browser avvisa "connessione non privata". | S | Confrontare l'impronta SHA-256 (pagina Servizi > HTTPS, comando `cert`). Piano: rinnovo, impronta nel QR, CA di prodotto (claude/VesevOS-piano-giallo-verde.md). |
| X4 | Procedura CRA (segnalazione falle, aggiornamenti), marchi, classificazione export, LGPL del core Arduino-ESP32, licenza dei dati `posix_tz_db`: voci di carta ancora aperte. | S | Vedi piano "da giallo a verde". Non e consulenza legale. |
| X5 | I buffer TLS stanno in PSRAM non cifrata (dati decifrati della sessione HTTPS). Servono cifratura della flash/PSRAM o i buffer in RAM interna. | S | In programma. |
| X6 | Backup con segreti: la sicurezza dipende dalla frase scelta (minimo 10 caratteri, 20000 giri PBKDF2). Una frase debole si puo indovinare provando con un computer. Frase persa = segreti persi. | A | Scegliere una frase lunga. Avviso gia nella pagina. |
| X7 | Le app esterne (Lua, futuro) non sono ancora firmate: in sviluppo si usa SHA-256. Effetto di app di terzi sulla conformita CE/CRA da chiedere a un consulente. | S | In programma. |

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
