# Cronologia delle versioni

## 1.7.45 (ottobre 2026) - aggiornamento firmware, passo 4: "Aggiorna firmware" nella pagina

- Nuova voce **Aggiorna firmware** nel menu utente (solo Amministratore): controlla che il recovery ci sia, poi riavvia la scheda nel recovery e mostra l'indirizzo da aprire (`http://IP:80/`, finestra privata) e cosa succede se la rete di casa non c'e (hotspot `VesevOS-recovery`, password sulla seriale).
- Nuove rotte API `GET /api/recovery` (present, kb) e `POST /api/recovery` (riavvia nel recovery, risponde con l'IP); solo Amministratore, la seconda lascia una riga nel registro. Stesso lavoro del comando `recovery now`.
- Recovery 0.3.0 invariato. Testi it/en/es/de, API.md/API.en.md rigenerati, prova della pagina aggiunta a smoke172.js.

## 1.7.44 + Recovery 0.3.0 (ottobre 2026) - aggiornamento firmware, passo 3: controlli di sicurezza

- Prima di attivare un firmware il recovery controlla: intestazione valida per ESP32-S3, immagine Arduino-ESP32, **marchio VesevOS** dentro il file, dimensione, SHA-256 (se lo scrivi nella pagina, deve coincidere) e immagine completa (`esp_ota_end`). Solo dopo imposta l'avvio.
- Se l'intestazione e sbagliata il vecchio firmware **non viene toccato**. Se l'errore arriva dopo l'inizio della scrittura, lo slot viene reso non valido (il recovery resta e non avvia un'immagine a meta).
- La pagina fa gli stessi controlli sul telefono/computer prima di inviare il file e mostra lo SHA-256 calcolato dalla scheda.
- Il firmware 1.7.44 aggiunge solo il marchio `{FW:VesevOS}` (12 byte). Il marchio nel recovery e mascherato: il recovery non puo essere caricato al posto del firmware.
- Nuove prove: `tools/hosttest/test_recheck.cpp`; stub `esp_ota_ops.h`.

## Recovery 0.2.0 (ottobre 2026) - aggiornamento firmware, passo 2: il recovery con Wi-Fi

- Il recovery (strada B2) ora si collega alla rete di casa (impostazioni lette dal file di configurazione di VesevOS) e, se non ci riesce in 30 s, apre un hotspot `VesevOS-recovery` con password casuale mostrata solo sulla seriale. Pagina minima: login con utente e password di un amministratore di VesevOS (stessa formula della pagina del firmware, la password non viaggia), poi carichi il `.bin` e il recovery lo scrive nello slot app0.
- Nessun amministratore (flash vuota): serve il codice a 8 cifre mostrato sulla seriale. Cinque errori = attesa di 60 s. Dopo 15 minuti senza accessi torna da solo a VesevOS.
- Nuova tabella partizioni: recovery 1 MB, app0 2,31 MB (indirizzo 0x110000), file invariati (stesso indirizzo, la configurazione resta). Gli script controllano che i file entrino nelle partizioni.
- Il firmware 1.7.43 non cambia. Nuovo `tools/mkrecpage.py` (genera la pagina del recovery copiando SHA-256/HMAC dalla pagina del firmware).


## 1.7.43 (ottobre 2026) - Recovery: `recovery now` ora resta nel recovery

- Prova sulla scheda del passo 1: `recovery now` entrava nel recovery ma questo tornava subito a VesevOS (sembrava un riavvio). Ora il comando lascia un segno in NVS ("vosrec/stay") e il recovery 0.1.2 resta fermo finche non scrivi `b` (avvia VesevOS) sulla seriale.
- Solo `vos_drv_ota.cpp` (firmware) e lo sketch del recovery. Nessun altro cambiamento.


## 1.7.42 (ottobre 2026) - Aggiornamento firmware, passo 1: il recovery

- Strada scelta per l'OTA senza scheda SD: un piccolo programma di **recupero** (partizione factory) piu un solo slot grande per VesevOS. Il firmware non deve piu stare in due slot, quindi niente dieta e il Bluetooth resta.
- Nuovo comando `recovery` (solo Amministratore): senza argomenti dice se il recovery c'e; `recovery now` riavvia nel recovery. Nuovo driver `vos_drv_ota` (unico file con `esp_partition_*`) e regola `ota` in checklayers.
- Nuova cartella `recovery/`: sketch `VesevOS_Recovery` 0.1.0 (per ora solo passa il comando a VesevOS), `partitions.csv` (4 MB: recovery 832 KB, app0 2,5 MB, file 640 KB), `installa.sh` (prima installazione con `esptool`).
- Il firmware vero e proprio non cambia comportamento. Con la vecchia tabella partizioni `recovery` risponde "non presente".


## 1.7.41 (ottobre 2026) - RAM: le regole occupano memoria solo se ci sono

- Misura sulla scheda (1.7.40, `ram audit`): a riposo 55% occupata (121 KB liberi); il login alla pagina costa solo 7 KB di RAM interna (il TLS va in PSRAM: funziona); il Bluetooth acceso costa circa 62 KB; dopo `ble off` la RAM torna quasi uguale (114 KB liberi, pezzo piu grande 59 KB).
- **Regole**: il task `rules` (stack da 6 KB) e le tre tabelle delle regole (circa 9 KB) esistevano sempre, anche con 0 regole. Ora il task parte solo quando c'e almeno una regola, e le tabelle stanno in memoria dinamica (in PSRAM se sono grandi) solo mentre servono. Con 0 regole si recuperano circa 15 KB di RAM interna.
- `ram audit`: ogni chiamata lascia una riga "audit" nello storico, cosi la variazione tra due audit e quella vera.
- Non toccati (decisione): gli stack di `wd`, `net`, `time`, `monitor`, `led`: il margine misurato e a riposo e non garantisce lo scenario peggiore.

## 1.7.40 (ottobre 2026) - RAM: `ram audit`, lo storico che dice dove va la memoria

- Nuovo comando `ram audit`: RAM interna con percentuale occupata, stato del TLS in PSRAM e uno **storico** automatico delle ultime 14 misure (avvio finito, Bluetooth acceso/spento, accesso e uscita dalla pagina) con la variazione della RAM libera tra una e l'altra. Cosi si vede quanto costa davvero il Bluetooth e quanto costa una pagina aperta.
- Corretta la tabella degli stack: il task `wd` e di 6144 byte (era scritto 3072), quindi il margine recuperabile del comando `ram` era sbagliato per quel task.
- Costo: 14 voci fisse da 28 byte (circa 0,4 KB, senza heap). Nessun cambio di comportamento.
- Prima parte del passo 4 (RAM sotto il 75%): piano in `claude/funzioni/VesevOS-piano-ram-autonomo.md`.

## 1.7.39 (ottobre 2026) - Bluetooth: meno frammentazione della RAM (prova)

- Misura sulla scheda (1.7.38): a riposo RAM libera 121 KB e pezzo piu grande 79 KB; col Bluetooth acceso libera 59 KB (costo reale ~62 KB, la stima nel codice era 45 KB: corretta) e pezzo piu grande 30 KB; DOPO lo spegnimento libera 121 KB (nessuna perdita) ma il pezzo piu grande resta 47 KB (frammentazione 61%): NimBLE lascia 2 piccoli blocchi nel mezzo della RAM.
- Prova: al boot, prima di rete e pagina, un avvio a vuoto + spegnimento del Bluetooth (senza annunci, senza emettere radio; saltato in modo aereo). Cosi i 2 blocchi restano in fondo alla RAM e il pezzo piu grande non cala piu al primo uso. Riga nel registro: "BLE: memoria preparata al boot, blocco piu grande X KB prima, Y KB dopo". Si spegne con `#define VOS_BLE_PRIME 0`.
- Da misurare sulla scheda: `free detail` dopo il boot (era 79 KB), dopo `ble on` e dopo `ble off`.

## 1.7.38 (ottobre 2026) - Event Bus: i package pubblicano i loro eventi

- Nuovi eventi (solo annunci: nessun cambiamento di comportamento, nessuna RAM in piu):
  - `mqtt.link` (valore 1 collegato al broker / 0 scollegato), solo quando cambia;
  - `ble.link` (1 telefono collegato / 0 scollegato);
  - `mesh.node` (una scheda vicina nuova o ricomparsa nella rete tra schede);
  - `time.sync` (ora sincronizzata e accettata dal server NTP).
- Visibili con `events` e `GET /api/events`. Con questi, la pagina potra smettere di interrogare la scheda a intervalli e aggiornarsi quando succede qualcosa (passo successivo).

## 1.7.37 (ottobre 2026) - Il nome Bluetooth segue il nome della scheda anche a Bluetooth acceso

- Se cambi il nome della scheda (hostname) mentre il Bluetooth e acceso, il Bluetooth si spegne e si riaccende da solo col nome nuovo (circa 6 secondi), tenendo il tempo che restava se c'era un limite. Se un telefono e collegato aspetta che si scolleghi, per non interromperlo. Se lo spegni tu nel frattempo, non si riaccende.
- Nota: il codice di accoppiamento cambia dopo la riaccensione (come a ogni accensione).

## 1.7.36 (ottobre 2026) - Il nome Bluetooth e il nome della scheda

- Il nome con cui il Bluetooth si annuncia non e piu fisso ("VesevOS-setup"): e il **nome della scheda** (hostname, lo stesso della rete e di mDNS). Per cambiarlo si cambia il nome della scheda; vale alla prossima accensione del Bluetooth. Le app riconoscono la scheda dall'UUID del servizio, non dal nome.
- Pagina, shell e `/api/ble` (nuovo campo `name`) mostrano il nome vero. Dalla pagina il Bluetooth si accende e si spegne senza scadenza (il tempo si da solo da shell o API).

## 1.7.35 (ottobre 2026) - Bluetooth senza timer di 10 minuti (barra Home sul limite scelto)

- Il Bluetooth non si spegne piu da solo dopo 10 minuti: resta acceso finche non lo spegne la pagina, la shell (`ble off`) o un'applicazione. Il limite di tempo e ora facoltativo: `ble on 30` (shell) o `POST /api/ble` con `min=30` (da 1 a 1440); senza `min` nessun limite. Resta spento di fabbrica e solo l'amministratore lo accende.
- La protezione non cambia: accoppiamento con codice casuale a 6 cifre, nuovo a ogni accensione, visibile solo in pagina o nella shell (mai nel registro).
- Pagina: testo del Bluetooth aggiornato ("per telefono e applicazioni"); acceso senza limite mostra "Acceso" e il codice, nella Home il widget dice "Acceso" senza barra. Stato (`/api/ble`, `/api/status`) con i nuovi campi `lim`, `bleLim` e `bleTot`. Con un limite di tempo la Home mostra il tempo che resta e la barra (calcolata sul limite scelto); senza limite dice solo "Acceso".
- Comando `wifi` via Bluetooth: tolto dall'elenco del lavoro (la configurazione dal Bluetooth non serve per ora).

## 1.7.33 (ottobre 2026) - Event Bus: rete, blocco IP, allarmi

- Nuovi eventi sull'Event Bus, tutti senza dati personali (l'indirizzo IP non compare mai: lo vede l'Operatore):
  - `net.state` (valore = stato rete: 0 avvio, 1 hotspot, 2 prova client, 3 client collegato, 4 modalita aereo), solo quando cambia;
  - `sec.ban` (valore = secondi di blocco) quando un IP viene bloccato; `sec.unban` (argomento `all` se si sbloccano tutti) quando si sblocca;
  - `audit.new` (argomento = codice dell'allarme, valore 1 giallo / 2 rosso) per un nuovo allarme; `audit.clear` quando rientra.
- Visibili con `events` (shell) e `GET /api/events`. Nessun cambiamento di comportamento. Costo RAM: nessuno (stesso anello da 24 eventi).

## 1.7.32 (ottobre 2026) - Event Bus (nucleo)

- **Nuovo `EventBus`** (`core/vos_eventbus.*`, classe PascalCase): chi cambia stato lo annuncia con `EventBus::publish(topic, arg, val)`; chi vuole saperlo si iscrive con `subscribe(prefisso, gestore)` oppure chiede "cosa e successo dopo il numero N" con `since`. Anello fisso di 24 eventi in RAM (circa 1,5 KB, nessun heap); i piu vecchi vengono sovrascritti e la risposta dice quanti se ne sono persi (`lost`). Fino a 8 ascoltatori; un gestore che pubblica a sua volta non fa ripartire altri gestori (niente ricorsione). Testo degli eventi ripulito (niente virgolette ne caratteri di controllo).
- Primo evento: `svc.state` (id del servizio, nuovo stato) ogni volta che un servizio viene avviato, fermato o riavviato (Bluetooth oggi).
- Comando shell `events [n]` (anche Operatore) e rotta `GET /api/events?since=N&n=K` (Operatore): ultimi eventi in JSON (`last`, `lost`, `ev[]`). La pagina non cambia: continua a leggere a intervalli.
- Prove sul computer: 27 nuove (anello pieno, numero dal futuro dopo un riavvio, ascoltatori, ricorsione, testo pericoloso).

## 1.7.31 (ottobre 2026) - API: documento per ogni rotta e prova sulla scheda

- **Documento API generato dal codice**: `docs/API.md` (italiano) e `docs/API.en.md` (inglese) con tutte le 126 rotte: metodo, percorso, livello, cosa fa, parametri letti e codici di errore usati, piu le regole generali (accesso, formato degli errori, codici HTTP, limiti). Lo scrive `tools/mkapidoc.py` dal codice e da `tools/api_notes.txt` (descrizioni).
- `tools/checkapi.py` ora controlla anche che il documento sia allineato al codice e che ogni rotta abbia la descrizione: una rotta nuova senza descrizione, o un parametro cambiato senza rigenerare il documento, e un errore.
- **Nuova prova sulla scheda vera** `tools/apitest.py` (si lancia dal computer: `--user admin`, chiede la password, entra e esce da solo): ogni rotta GET risponde 200 con JSON valido, `apiVersion` giusta in `/api/common` e `/api/status`, sette prove di errore che non cambiano nulla (file o id inesistenti: devono dare 4xx, `ok:false` e un `code`), rotta inesistente = 404; opzionale la prova "senza sessione = 401".
- Correzione: `POST /api/ban/unban` con un indirizzo che non e nell'elenco rispondeva `ok` senza fare nulla; ora risponde 404 `notfound` ("Indirizzo non trovato"), come gia fa il comando `unban` della shell.
- Annotato nel documento: la parola AZZERA del ripristino di fabbrica e controllata solo dalla pagina (la rotta e comunque solo per admin).

## 1.7.30 (ottobre 2026) - Home: temperatura sotto il grafico arancione

- Nel widget CPU della Home la temperatura (es. `51.3 °C`) passa **sotto il grafico arancione**, con la scritta "Temp." e il pallino arancione come C0 e C1. Sotto "CPU" resta solo la velocita (es. `80 MHz`).
- Solo pagina: il firmware cambia solo nella versione.

## 1.7.29 (ottobre 2026) - API: versione in /api/common e liste con limite

- `/api/common` ora riporta anche `apiVersion` (letta da `VOS_API_VERSION` quando si genera il file con `tools/mkcommon.py`): la versione API e visibile sia in `/api/status` sia in `/api/common` (regola API 15).
- Liste con limite e campo `more` (regola API 13): `/api/fs/list` mostra al massimo 200 voci, `/api/wifi/scan` al massimo 40 reti; se ce ne sono di piu rispondono `more:true`. Prima una cartella con migliaia di file poteva riempire la RAM.
- La pagina File avvisa "Elenco parziale: mostrate le prime N voci" quando l'elenco e troncato.
- Le altre liste (registro, autodiagnosi, utenti, sessioni) hanno gia un tetto fisso nel codice.

## 1.7.28 (ottobre 2026) - File piu piccoli: la pagina web divisa in pezzi

- `web/index.html` (2155 righe) ora si scrive a pezzi in `web/src/` (15 file: stile, parti della pagina, 8 file di programma per argomento: base, automazioni, avvio e home, file, pin e accesso, periferiche e rete, servizi, guida). L'ordine e in `web/src/ORDINE.txt`.
- Nuovo `tools/mkweb.py` assembla i pezzi in `web/index.html` (`--check` controlla che siano allineati). Ordine di lavoro: `mkweb.py`, `mklang.py`, `mkpage.py`.
- La pagina prodotta e **identica byte per byte** alla 1.7.27: nessun cambio di comportamento, stessa dimensione nel firmware.

## 1.7.27 (ottobre 2026) - File piu piccoli: la configurazione divisa

- `vos_config.cpp` (570 righe) diviso: `vos_config.cpp` (203: valori iniziali, salva, carica, ripristino), `vos_config_export.cpp` (155: scrittura del file di testo), `vos_config_import.cpp` (236: lettura e controllo delle chiavi).
- Solo spostamenti. Formato del file `vesevos.conf` invariato. Il test sull'host compila i tre file.

## 1.7.26 (ottobre 2026) - File piu piccoli: la shell divisa per gruppi

- `vos_shell.cpp` (948 righe) diviso: `vos_shell.cpp` (405: aiuti, smistamento, benvenuto, seriale, guida), `vos_shell_sys.cpp` (help, stato, file, utenti...), `vos_shell_net.cpp` (wifi, led, pin, servizi...), `vos_shell_admin.cpp` (config, mesh, mqtt, energia, reboot).
- Aiuti condivisi in `vos_shell_int.h` (interno). Solo spostamenti, comandi invariati.

## 1.7.25 (ottobre 2026) - File piu piccoli: il server web diviso per gruppi

- `vos_web.cpp` (983 righe) diviso: `vos_web.cpp` (279: avvio dei server, aiuti, pagina, lingue), `vos_web_auth.cpp` (accesso, MFA, utenti), `vos_web_dev.cpp` (periferiche, pin, LED, automazioni), `vos_web_sys.cpp` (MQTT, autodiagnosi, ora, localizzazione), `vos_web_net.cpp` (configurazione, filtro IP, watchdog, servizi di rete, HTTPS), `vos_web_files.cpp` (rete tra schede, Bluetooth, file).
- Aiuti e dati condivisi in `vos_web_int.h` (interno: gli altri moduli usano solo `vos_web.h`).
- Solo spostamenti: stesse 126 rotte, stessi livelli (`tools/checkapi.py` lo conferma). Comportamento invariato.

## 1.7.24 (ottobre 2026) - API: codici HTTP giusti

- Gli errori non rispondono piu 200: il codice HTTP segue il campo `code`. `error` = 400 (dato mancante o non valido), `bad_login` = 403 (nome/password/codice errati), `forbidden` = 403, `notfound` = 404, `state` = 409 (stato non adatto: es. spegnere l'hotspot senza Wi-Fi di casa), `toobig` = 413, `blocked` = 429 (troppi errori), `off` = 503. 401 resta solo per "sessione assente" (la pagina lo usa per rimostrare il login: mai per password errata).
- Il corpo ha sempre `ok:false`, `err` (testo) e `code`: la pagina legge come prima.
- Comportamento invariato per l'utente.

## 1.7.23 (ottobre 2026) - API: la chiave della rete schede non esce piu con GET

- `/api/mesh/key` (mostra la chiave ESP-NOW) passa da GET a POST (solo admin) e scrive una riga nel registro (chi l'ha vista). La pagina e adeguata.
- Verificate le altre GET sospette: `wifi/test`, `selftest`, `wifi/scan`, `pintest` sono sola lettura (l'avvio e gia in POST). Resta GET solo `/api/config/download` (e un scaricamento di file, admin).
- Comportamento invariato per l'utente.

## 1.7.22 (ottobre 2026) - API: versione, codici errore, elenco rotte

- `GET /api/status` ha il campo `apiVersion` (oggi 1; cambia solo per rotture dell'API).
- Gli errori hanno il campo `code` (nome breve stabile, non tradotto): `error` (generico), `blocked` (troppi errori), `unauthorized` (401), `forbidden` (403), `https_only` (403). `err` resta il testo per la pagina. Campi in piu: compatibile.
- Nuovo `tools/checkapi.py` + `tools/api_routes.txt`: elenco di tutte le 126 rotte con metodo e livello (pubblico/ospite/operatore/admin); una rotta nuova o con livello cambiato fa fallire il controllo.
- Comportamento invariato.

## 1.7.21 (ottobre 2026) - Strati: il driver dei pin

- Nuovo `src/drivers/vos_drv_gpio.{h,cpp}`: unico file con pinMode / digitalWrite / digitalRead. Lo usano prova pin, regole, tasto BOOT (avvio, autotest, Device Manager).
- Controllo strati: tutte le famiglie a 0 accessi fuori dai driver (Bluetooth, ESP-NOW, MQTT, Wi-Fi, file, pin).
- Comportamento invariato.

## 1.7.20 (ottobre 2026) - Strati: il driver dei file

- Nuovo `src/drivers/vos_drv_fs.{h,cpp}`: unico file che usa LittleFS (apri, esiste, cancella, rinomina, cartelle, spazio usato/totale). Config, diario, lingue, file, regole, MQTT, autotest, sistema, shell, web e avvio lo usano.
- Controllo strati: file 99 -> 0 accessi fuori dal driver (restano solo i pin: 12).
- Comportamento invariato.

## 1.7.19 (ottobre 2026) - Wi-Fi: ultimi accessi nel driver

- Paese/canali/potenza, risparmio energia, spegnimento prima del sonno, segnale e client dell'hotspot passano da `vos_drv_wifi`. Fuori dal driver non resta nessun accesso a `WiFi.*` (debito 40 -> 0).
- Wi-Fi iscritto nel registro dei servizi (`wifi`, sola lettura: non si ferma da li, c'e il modo aereo).
- Comportamento invariato.
- 1.7.19a: rimessa `#include <esp_wifi.h>` nel driver (errore di compilazione).

## 1.7.18 (ottobre 2026) - DHCP segue l'hotspot

- Hotspot spento = server DHCP spento in automatico (evita un secondo DHCP sulla rete di casa).
- Hotspot acceso da pagina o shell = DHCP acceso. Fine guida: hotspot e DHCP spenti.
- Senza rete di casa l'hotspot forzato distribuisce comunque gli indirizzi.
- Schede gia configurate: al primo avvio il DHCP viene spento se hotspot spento e rete di casa impostata.

## 1.7.17 (ottobre 2026) - Strati: il driver Wi-Fi
Sesto passo del riordino a strati. **Nessuna funzione nuova, comportamento invariato.**
- Nuovo `vos_drv_wifi` (strato 1, driver): l'unico file che usa la libreria Wi-Fi di Arduino (`WiFi.*`) per la rete: modo (spento/client/hotspot/entrambi), collegamento alla rete di casa, IP statico o DHCP, hotspot, cambio canale, scansione, segnale, canale, indirizzi.
- `vos_net` (servizio di rete) ora chiama solo il driver; la logica (quando collegarsi, quando tornare in hotspot, modo aereo, prova della rete di casa) e rimasta uguale, nello stesso ordine di chiamate.
- Controllo strati: accessi Wi-Fi fuori dai driver da 89 a 40 (restano nei file region, selftest, power, shell, stats, fw, sys, mqtt, dev, web, wd, time: prossimo passo).
- **Prove**: stub su tutti i file con package accesi e spenti, 181 prove sul computer, pagina con finto server.

## 1.7.16 (ottobre 2026) - Strati: MQTT diviso in driver e servizio
Quinto passo del riordino a strati. **Nessuna funzione nuova, comportamento invariato.**
- `vos_drv_mqtt` (strato 1, driver): l'unico file che conosce il client MQTT di sistema e le autorita dei certificati. Sa aprire/chiudere il collegamento al broker, pubblicare, iscriversi e avvisare (collegato, scollegato, dati, errore).
- `vos_mqtt` (servizio): argomenti, comandi in arrivo, stato, Home Assistant, coda dei comandi. Non chiama piu `esp_mqtt_client_*`.
- MQTT e iscritto nel registro dei servizi (`vos_service`) come terzo servizio; pagina, shell e registro periferiche passano dal contratto. Stato: collegato = acceso, avviato ma non collegato = acceso con problema.
- Controllo strati: accessi MQTT fuori dai driver da 11 a 0. Restano Wi-Fi 89, file 99, pin 12.
- **Prove**: stub su tutti i file con package accesi e spenti, 181 prove sul computer, pagina con finto server.

## 1.7.15 (ottobre 2026) - Strati: ESP-NOW diviso in driver e servizio
Quarto passo del riordino a strati. **Nessuna funzione nuova, comportamento invariato.**
- `vos_drv_espnow` (strato 1, driver): l'unico file che conosce ESP-NOW. Sa accendere/spegnere, mandare un pacchetto a tutti, dire canale e MAC, avvisare quando arriva un pacchetto. Sceglie da solo l'interfaccia (STA o hotspot).
- `vos_mesh` (servizio): formato dei messaggi, firma, ruoli, ripetizione, nodi. Non chiama piu `esp_now_*` ne `esp_wifi_*` per canale e MAC.
- ESP-NOW e iscritto nel registro dei servizi (`vos_service`) come secondo servizio; pagina, shell e registro periferiche passano dal contratto.
- Controllo strati: accessi ESP-NOW fuori dai driver da 11 a 0; Wi-Fi da 95 a 89.
- **Prove**: stub su tutti i file con package accesi e spenti, 181 prove sul computer, pagina con finto server.

## 1.7.14 (ottobre 2026) - Strati: il contratto dei servizi
Terzo passo del riordino a strati. **Nessuna funzione nuova, comportamento invariato.**
- Nuovo `vos_service` (`src/core/`): un solo modo per avviare, fermare, riavviare e leggere lo stato di un servizio (`serviceStart/Stop/Restart/State/Ram`). Ogni servizio si descrive con una `ServiceOps` (id, nome, ruoli, stato, avvia, ferma, dettagli, RAM usata) e si iscrive con `serviceRegister()`. Stati: spento, acceso, acceso con problema, in avvio, errore.
- Il Bluetooth e il primo servizio iscritto (`bleServiceInit()` all'avvio). Pagina, shell (`ble on/off`) e registro periferiche ora passano dal contratto; le funzioni `bleStart/bleStop` restano.
- Nuova azione `restart` nella pagina Bluetooth (`/api/ble`, `a=restart`): ferma e riaccende; per la pausa di 5 secondi tra due accensioni puo chiedere di riprovare.
- **Prove**: 181 prove sul computer (19 nuove sul registro dei servizi con un servizio finto), stub su tutti i file con package accesi e spenti, pagina con finto server, controllo strati (debito hardware invariato).

## 1.7.13 (ottobre 2026) - Strati: i file in cartelle
Secondo passo del riordino a strati. **Nessuna funzione nuova, nessun cambio di codice: si spostano solo i file.**
- `VesevOS/` contiene solo `VesevOS.ino` e la cartella `src/` (Arduino IDE compila `src/` e le sue sottocartelle). Dentro `src/`, una cartella per strato:
  - `core/` nucleo sempre presente (configurazione, log, testi, ora, watchdog, file, firmware, autotest, dati generati)
  - `security/` accessi, HTTPS, cifratura
  - `net/` rete e contatori di traffico
  - `devices/` Device Manager, pin, LED
  - `drivers/` unico posto che tocca la radio o l'hardware (oggi `vos_drv_ble`)
  - `packages/` MQTT, ESP-NOW, Bluetooth, MFA, statistiche, automazioni, risparmio energia
  - `interfaces/` pagina web, shell
- Gli `#include` dei file usano percorsi relativi al file (`../core/vos_util.h`), cosi funzionano senza impostazioni particolari.
- Gli script (`mkpage.py`, `mkcommon.py`, `mklang.py`, `mklicense.py`, `checklayers.py`) e le prove sul computer cercano i file nelle nuove cartelle; i file generati vanno in `src/core/` e `src/interfaces/`.
- **Prove**: stub su tutti i file con i package accesi e spenti, 162 prove sul computer, pagina con finto server. La compilazione vera con Arduino IDE la fai tu.

## 1.7.12 (ottobre 2026) - Strati: il Bluetooth diviso in driver e servizio
Primo passo del riordino a strati (schema nel Progetto: `VesevOS-schema-strati.md`). **Nessuna funzione nuova, comportamento invariato.**
- `vos_drv_ble` (strato 1, driver): l'unico file che conosce NimBLE. Sa accendere, spegnere, annunciarsi, ricevere e inviare testo, dire se un telefono e collegato.
- `vos_ble` (ciclo di vita): controlli di memoria, 10 minuti, pausa tra due accensioni, codice di accoppiamento, stato per pagina e shell. Le funzioni pubbliche non cambiano.
- `vos_ble_cfg` (servizio): i comandi di prima configurazione (`wifi`, `country`, `name`, `mesh`, `status`, `done`). Non conosce la radio: riceve un testo e risponde.
- Nuovo `tools/checklayers.py`: conta gli accessi diretti all'hardware fuori dai driver; il debito di partenza (`tools/layers_baseline.json`) puo solo calare. Bluetooth: zero accessi fuori dal driver. Resto del debito: Wi-Fi 95, file 99, GPIO 12, ESP-NOW 11, MQTT 11.
- **Prove**: 18 prove nuove sui comandi del telefono (162 in tutto), senza radio.

## 1.7.11a (ottobre 2026) - Accessibilita (WCAG 2.2 AA)
- Colori del testo piu contrastati in tema chiaro e scuro (rapporto almeno 4,5:1), bottoni rossi e link compresi.
- Tutte le caselle hanno un'etichetta collegata; i campi senza etichetta prendono il nome dal testo guida; icone decorative nascoste ai lettori di schermo.
- Widget e anelli cliccabili usabili da tastiera (Tab, Invio, Spazio); terminale con nome e focus da tastiera.
- Bottoni piccoli piu alti (almeno 24 px); intestazione che va a capo su schermi stretti (320 px senza scroll orizzontale).
- Nuovo test automatico `tools/webtest/a11y172.js` (contrasto, nomi, bersagli, focus, dialoghi, 320 px). Resta da provare con un lettore di schermo vero.

## 1.7.11 (ottobre 2026) - Device Manager, package, backup cifrato
- **Bluetooth con NimBLE** (libreria NimBLE-Arduino 2.5.1, Apache-2.0, da installare dal gestore librerie): circa 11 KB di RAM interna in meno rispetto a prima. Si accende e si spegne anche ripetutamente.
- **Device Manager** (un solo gruppo di menu) con tutte le periferiche: virtuali (MQTT, rete tra schede, Bluetooth, MFA, statistiche) e hardware (pin, LED, tasto BOOT, USB, temperatura, memoria, processore, radio). Tutte rispondono alle stesse regole: elenco, stato, accendi/spegni (Admin), dettagli, azioni. Nuove API `GET /api/dev`, `POST /api/dev/set` (Admin), `POST /api/dev/act` e comando `dev list|status|on|off|act`. Il menu "Stato" non c'e piu: i dati sono nei widget della Home e nelle schede Memoria, Processore e Radio.
- **Package a compilazione** (`vos_common.h`): `VOS_WITH_BLE`, `VOS_WITH_MQTT`, `VOS_WITH_MESH`, `VOS_WITH_MFA`, `VOS_WITH_STATS` (1 = dentro, 0 = fuori). Con 0 il codice sparisce, le schede spariscono dalla pagina e `diag` elenca i package presenti. Misura con tutti a 0: sketch 1.653.667 byte (52%), RAM libera 151 KB invece di 127 KB.
- **Home**: grafici nei widget (CPU, RAM, PSRAM, file), grafici In/Out del Wi-Fi, selettore di aggiornamento (1/3/5/10/30 s, fermo) e "Aggiorna ora".
- **Memoria**: i buffer di HTTPS (TLS) vanno in PSRAM (soglia 256 byte); la RAM interna libera resta stabile con pagina HTTPS e Bluetooth insieme. Comandi `ram`, `ram mark`, `ram diff`, `free detail`. L'allarme "Memoria frammentata" non chiede piu il riavvio.
- **Backup e ripristino** (Sistema, Admin): scarica senza segreti (di fabbrica) oppure **con i segreti cifrati con una frase tua** (almeno 10 caratteri; PBKDF2-HMAC-SHA256 20000 giri + AES-256-GCM), con avviso da accettare e voce nel registro. Il ripristino dice quante righe sono cambiate; una frase sbagliata conta come errore di accesso. Reset di fabbrica: si conferma scrivendo AZZERA.
- **Watchdog: modalita ridotta**. Dopo 3 riavvii automatici in un'ora la scheda non si riavvia piu e non resta ferma: spegne il servizio che si e bloccato (MQTT, rete tra schede, LED) oppure, se non lo sa, MQTT, rete tra schede e Bluetooth. Pagina web, log e recupero restano. La pagina Watchdog mostra cosa e stato spento.
- **Pagina Radio** divisa in **Impostazioni** (le scegli tu) e **Informazioni** (calcolate dalla scheda); il testo non dice piu che il paese basta per la conformita: dipende anche da hardware, antenna e montaggio.
- **Statistiche locali** (nome chiaro: i dati restano nella scheda, non vengono mai inviati fuori).
- **Seriale**: il benvenuto non scorre piu all'infinito quando il segnale USB cade e risale (monitor che si riapre, velocita diversa): conta come nuova apertura solo dopo 2 secondi di chiusura, e mai prima di 5 secondi dall'ultimo benvenuto. Backspace/Ctrl+U/Ctrl+C e tasti freccia gestiti.
- **Diario dei riavvii piu ricco** e chiusura dei collegamenti HTTPS fermi quando la RAM e poca; uscita seriale che non blocca il ciclo principale.
- **NTP piu rispettoso**: tolto Google dai server di fabbrica, avviso su cosa vede il server, ora non credibile scartata.
- Correzioni: `wifi set "Casa mia" password`, NTP che si riaccende se la guida e finita senza Wi-Fi, codice di accoppiamento Bluetooth fuori dal registro, livello del registro deciso dall'inizio della riga.
- **Prove**: 144 prove sul computer (`tools/hosttest/run.sh`, comprese quelle del backup cifrato, verificato anche con Python) e pagina provata con finto server e Playwright.
- Documenti: manuali it/en (capitoli 20 e 21), `docs/KNOWN_ISSUES` e `docs/RELEASE_DEVELOPER` in italiano e inglese.

## 1.7.9a (ottobre 2026) - RAM frammentata: si vede e si misura
- Comando **`free detail`**: RAM interna (libera, pezzo piu grande, minima), blocchi occupati e liberi, percentuale di frammentazione e PSRAM.
- **Avviso giallo** in pagina (campanella/Home) quando il pezzo di RAM libero piu grande scende sotto 45 KB; sparisce quando risale sopra 55 KB.
- Il log del Bluetooth scrive gia (1.7.8f) il pezzo piu grande a ogni accensione e spegnimento.
- **Tre correzioni dalla lista dei problemi noti**:
  - `wifi set` accetta il nome della rete con spazi tra virgolette: `wifi set "Casa mia" password` (anche la password puo stare tra virgolette). Senza virgolette funziona come prima. Vale per seriale, shell della pagina e Bluetooth resta `rete|password`.
  - **NTP si riaccende da solo** se la guida e finita senza Wi-Fi (la guida lo spegne) e il Wi-Fi di casa viene impostato dopo (shell, pagina o telefono). Se lo hai spento tu, resta spento. Nuova voce di configurazione `ntpauto` (segna "spento dalla guida").
  - Tolto l'avviso del compilatore "`inList` non usata" in vos_sys.cpp.
- **Contro i riavvii "Task watchdog"** (dal diario della scheda, 7-8 ottobre):
  - **Seriale che non blocca**: se il PC non legge in tempo (monitor aperto ma fermo), l'uscita seriale si butta via per 2 secondi senza altre attese, poi riprova. Prima ogni riga aspettava il tempo massimo e il ciclo principale poteva fermarsi fino allo scatto del watchdog.
  - **Diario piu utile**: per ogni riavvio salva il pezzo di RAM libero piu grande, quante volte la seriale non e stata letta e il servizio con il battito piu vecchio (es. "web 12s"). Il vecchio diario resta e si completa (nessuna perdita).
  - **Meno RAM per HTTPS**: se il pezzo di RAM libero piu grande scende sotto 60 KB e la pagina e ferma da 20 secondi, la scheda chiude i collegamenti HTTPS rimasti aperti (circa 40 KB l'uno); il browser li riapre da solo.
- **Seriale: si puo correggere cio che scrivi** (segnalato dalla prova): il tasto Backspace toglieva il carattere dalla riga ma sullo schermo la lettera restava; ora lo schermo la cancella davvero. In piu **Ctrl+U** e **Ctrl+C** svuotano la riga, e i tasti freccia / Canc / Home non mettono piu lettere strane (`[D`, `[3~`) nella riga.
- **Misura della RAM** (passo R0 del piano RAM, claude/VesevOS-piano-ram.md): nuovo modulo `vos_ram` e comando **`ram`** (stack libero minimo e totale di ogni task e quanto si puo togliere tenendo il 25% libero) piu **`ram mark`** / **`ram diff`** (quanto costa in RAM interna, pezzo piu grande e PSRAM accendere o spegnere un servizio). Nessun cambiamento al comportamento della scheda.
- **NTP piu rispettoso (privacy e sicurezza)**:
  - **Tolto Google**: il secondo server NTP non e piu `time.google.com` fisso. Di fabbrica c'e solo `pool.ntp.org`; un secondo server e a scelta (pagina Servizi > NTP, oppure `ntp server2 <nome|off>`; `ntp server <nome>` cambia il primo). Chiave di configurazione `server2`.
  - **Avviso chiaro**: la scheda NTP dice a quali server parla e che ogni richiesta mostra il tuo IP pubblico; anche la riga di log quando NTP si accende da solo nomina i server.
  - **Ora non credibile = scartata**: l'NTP semplice non e autenticato. Un'ora prima del 2026, o un salto di oltre un giorno dopo una sincronizzazione buona, viene scartata (torna l'ora attesa) con avviso giallo; alla quarta volta di fila si accetta (l'ora vera e cambiata). Mettere l'ora a mano azzera il confronto.
  - Non fatti (da provare sulla scheda o decisioni commerciali): NTP dal router (DHCP opzione 42) e vendor zone di ntppool.org (serve prima della vendita).
- **Documenti per GitHub** (cartella `docs/`, italiano e inglese): `KNOWN_ISSUES` (problemi noti) e `RELEASE_DEVELOPER` (guida per chi sviluppa e rilascia).
- **Correzioni dalla verifica approfondita** (revisione indipendente del codice):
  - **Sicurezza**: il codice di accoppiamento del Bluetooth non va piu nel registro (lo leggeva anche un ospite). Resta solo in pagina (Admin) e nella shell.
  - Seriale: scrittura a pezzi (non un carattere alla volta) per non bloccare `loop` quando il PC non legge.
  - Seriale: i valori degli interruttori devono essere on/off/1/0 (prima "niente" o "no" spegnevano la voce); la velocita in prova non finisce nella configurazione e una seconda prova non cancella la velocita di partenza; la pagina si aggiorna quando la prova scade.
  - Bluetooth: accensione, spegnimento e ciclo principale non si calpestano piu (blocco comune); `conn` torna a zero allo spegnimento.
  - Diario: `diaryCount` non sbaglia con file vuoti o di altro formato.
  - Guida da seriale: usa lo stesso a-capo scelto.
  - Diario: l'istantanea e protetta da un blocco brevissimo (piu task la scrivono), niente piu checksum che non torna per una sovrapposizione.
  - Registro: il livello della riga si decide dall'inizio del testo (ERRORE / ATTENZIONE / AUDIT:), non da una parola a meta riga: un nome scheda o una rete Wi-Fi con "ERRORE" dentro non alza piu il livello.
- **Prove sul computer** (passo 1 del rifacimento, nessun cambio al comportamento della scheda): cartella `tools/hosttest` con 132 prove su utilita, configurazione, registro e seriale; si lanciano con `tools/hosttest/run.sh`. Correzione del finto `Arduino.h` (le funzioni `isAlphaNumeric` ecc. rispondevano sempre si).

## 1.7.9 (ottobre 2026) - Seriale configurabile
- **Nuova scheda "Seriale (cavo USB)"** in Sistema > Sicurezza e nuovo comando `serial`: velocita (baud, con prova di 20 secondi e conferma), a capo (CR+LF / LF / CR), eco dei tasti, accetta comandi, scrive il registro, benvenuto all'apertura, attesa massima di scrittura.
- Tutta l'uscita della seriale (comandi, registro, guida) passa da un solo punto che rispetta queste impostazioni.
- Seriale senza comandi: si riaccende dalla pagina o con il tasto BOOT (8-19 s). Per spegnerla da shell serve `serial input off yes`.
- Il diario dei riavvii registra anche il passo `L:serial` di `loop`.

## 1.7.8f (ottobre 2026) - Bluetooth: accendi e spegni ripetuti
- Prova: con la pagina HTTPS aperta, tre accensioni/spegnimenti del Bluetooth di fila hanno dato "esp-aes: Failed to allocate memory" (RAM frammentata).
- **Pausa di 5 secondi** tra lo spegnimento e una nuova accensione (messaggio chiaro, vale per pagina, shell e telefono).
- **Controllo prima di accendere**: se il blocco di RAM piu grande e sotto 45 KB il Bluetooth non parte e dice di riavviare la scheda.
- Le righe del log "BLE: prima di accendere..." e "BLE: spento..." scrivono RAM libera e blocco piu grande, per vedere quanto cala a ogni giro.
- Gli oggetti di risposta del Bluetooth sono fissi (prima ne restavano due in RAM a ogni accensione).

## 1.7.8e (ottobre 2026) - a-capo giusti per PuTTY
- La seriale manda sempre CR+LF: programmi come PuTTY non mostrano piu le righe "a scalinata" (prima serviva spuntare "Implicit CR in every LF"). Vale per i comandi, `log`, `reboots` e la guida.

## 1.7.8d (ottobre 2026) - seriale: niente testo perso
- Prova 1.7.8c: nessun watchdog nei reset da USB, ma con attesa 0 la seriale perdeva pezzi di testo (riquadro iniziale, `log`, `reboots`). Ora attesa massima 10 ms per scrittura. Le righe "AVVIO ... USB" nel diario sono i reset fatti aprendo il monitor, non blocchi.

## 1.7.8c (ottobre 2026) - seriale che non blocca e diario piu chiaro
- **Seriale USB con attesa breve**: ogni scrittura aspetta al massimo 10 ms il PC (prima 100 ms; con 0 si perdevano pezzi di testo lunghi, visto nella prova). Sospetto per il "Task watchdog" 7 secondi dopo un reset da USB: durante la riconnessione del PC la scrittura sulla seriale poteva fermare `loop` per piu di 5 secondi.
- Il diario dice anche **quale passo di `loop`** era in corso (L:shell, L:ble, L:power, L:stats, L:pin, L:stable...).
- Il file del diario ha un'**intestazione con versione**: un file di formato diverso viene scartato (niente piu righe senza senso). Nuovo file /boots3.bin; i vecchi si cancellano da soli.

## 1.7.8b (ottobre 2026) - watchdog all'avvio e RAM
- **Task watchdog dopo un reset da USB** (provato: 7 secondi dopo l'avvio con la CPU in automatico, mai con la CPU fissa a 240 MHz): nel primo minuto dopo l'avvio la scheda non cambia piu la frequenza della CPU da sola. Poi la scalatura automatica riparte come prima.
- Il diario dei riavvii salva anche i MHz al momento del blocco.
- "Ultima riga grave": non conta piu l'allarme della seriale (che c'e a ogni avvio) ne le righe sul riavvio anomalo, che coprivano la riga vera.
- RAM: il report dell'Autotest e il diario usano la PSRAM; stack del task di prova 5120 B (era 6144). Minima vista in prova: 22 KB.

## 1.7.8a (ottobre 2026) - diario dei riavvii piu preciso
- Dopo un caricamento da Arduino IDE la scheda si e riavviata una volta per "Task watchdog" entro 10 secondi (visto due volte, 7/10 20:04 e 20:59). Il diario non aveva i dettagli perche la prima istantanea arrivava dopo 10 s.
- Istantanea ogni secondo nei primi 15 secondi (poi ogni 10).
- Nuova **fase di avvio** nell'istantanea (pins, fs, cfg, lang, i servizi uno per uno, avviato, loop): se la scheda si blocca all'avvio il diario dice dove.
- Il diario usa un nuovo file (/boots2.bin); il vecchio si cancella da solo.

## 1.7.8 (ottobre 2026) - "LOG COMPLETO E DIARIO DEI RIAVVII"
- **Diario dei riavvii**: gli ultimi 20 avvii restano salvati anche dopo lo spegnimento (file piccolo in memoria file). Per ognuno: numero, data e ora, motivo (accensione, watchdog, crash, brownout...), da quanto tempo era accesa la scheda, RAM minima, task piu attivo e l'ultima riga grave. Si vede in Sistema > Log (scheda "Diario dei riavvii"), nel report dell'Autotest e dalla seriale con `reboots`.
- **Istantanea prima del riavvio**: ogni 10 secondi la scheda salva in una memoria che sopravvive a watchdog e crash (non allo spegnimento) il tempo acceso, la RAM minima, il task piu attivo e l'ultima riga grave. Serve a capire chi ha causato un watchdog.
- **Riga di avvio completa** nel registro: "AVVIO n.N: motivo..., RAM, MHz". Se il riavvio e anomalo compaiono righe ATTENZIONE con i dettagli.
- **Livelli di log**: ogni riga porta [E] errore, [W] attenzione, [I] info, [D] dettaglio. In Sistema > Log si sceglie cosa mostrare e (Admin) fino a che livello registrare; di fabbrica Info. Dettaglio (spento di fabbrica) aggiunge ogni minuto il task piu attivo, la CPU e la RAM. Seriale: `log level [0-3]`.
- Correzioni dell'Autotest (ex 1.7.7a): righe lunghe non piu tagliate; secondi della prova CPU realmente misurati; la pagina riprova da sola se una richiesta si perde (non resta piu ferma su "prova 3 di 16").
- Solo dati tecnici nel diario (niente IP, MAC, nomi Wi-Fi o utenti); azzerabile da pagina e seriale.

## 1.7.7 (ottobre 2026) - "AUTOTEST"
- **Autotest** (Sistema > Autotest, Admin): 16 prove non distruttive (anche CPU e temperatura: media su 2 secondi, avviso oltre 60%, errore oltre 85%; temperatura avviso oltre 70 C, errore oltre 85 C), una per volta, ognuna con esito chiaro (OK / Avviso / Errore / Saltata) e tempo. Alla fine il **report di testo** si scarica o si copia, per mandarlo a mano a Claude.
- Il report non contiene mai password, chiavi o token. MAC, nome Wi-Fi, IP e registro sono oscurati, salvo la casella "nomi reali". Le prove attive (LED, messaggio MQTT) partono solo se spunti la casella.
- Comando seriale `selftest [active] [names]`. Nulla esce dalla scheda; il report sta in RAM (circa 2,7 KB) finche non lo chiudi.
- **Pagina piu leggera**: aggiornamento ogni 3 s (era 2 s), nessuna richiesta nuova finche la precedente non e finita, e nessuna richiesta quando la scheda del browser e nascosta o in secondo piano (prima la CPU della scheda poteva restare al 100% con la pagina lasciata aperta).
- **CPU misurata meglio**: il carico di ogni core ora si calcola dal tempo dei task IDLE (stessa fonte del Task manager). Prima la taratura a conteggio, dopo i cambi di frequenza, poteva mostrare 100% con la scheda quasi ferma (Task: IDLE0 97%, IDLE1 83%).
- **Task manager**: icone Avvia / Ferma / Riavvia alte quanto il lucchetto (26 px), righe tutte della stessa altezza.

## 1.7.6 (ottobre 2026) - "TASK E HOME RIFINITI"
- **Task manager**: stesse icone Avvia / Ferma / Riavvia dei servizi, con il fondo colorato per lo stato (Riavvia grigio se il task e fermo). Un solo componente per tutti (nessun codice doppio).
- **Home**: HTTP e HTTPS sono due widget separati, ognuno con la sua porta.
- **Tempi leggibili** in tutta la pagina (statistiche, widget, Bluetooth, nodi vicini...): i secondi diventano minuti, ore e giorni (es. 3725 s = 1 h 2 min, 93784 s = 1 g 2 h).
- Solo pagina e documenti: il firmware non cambia (versione 1.7.6).

## 1.7.5 (ottobre 2026) - "HOME E SERVIZI COERENTI"
- **Testata**: ora e data al posto dell'indirizzo IP. L'IP sta sotto il widget Wi-Fi. Il riquadro Orologio e le schede "Azioni rapide" e "Posizione" sono tolti dalla Home (la Posizione si vede cliccando la bandiera; Modo aereo e Terminale restano in Rete e nella testata).
- **Widget dei servizi** in una sola fila sotto gli anelli, con icone nostre generiche (niente loghi di marchi). Titolo "Sistema" al posto di "Stai usando". Icona Periferiche tipo USB.
- **Controlli dei servizi, un solo componente**: banner Avviato (verde) / Fermo (rosso) e icone Avvia / Ferma / Riavvia / Applica con fondo colorato per lo stato. Tolte le scritte "MQTT acceso" ecc. Riavvia grigio quando il servizio e fermo o non si puo riavviare. HTTP e HTTPS: **Applica** (riavvia la scheda con conferma) al posto di "Riavvia ora".
- Vale per: Punto di accesso, DHCP, DNS, mDNS, HTTP, HTTPS, MQTT, Rete tra schede, Statistiche, NTP, Bluetooth, Risparmio energia e MFA (Avvia = accende, Ferma = spegne; Riavvia solo dove ha senso).
- Solo pagina e documenti: il firmware non cambia (versione 1.7.5).

## 1.7.4 (ottobre 2026) - "GUIDA E HOME PIU CHIARE"
- **Mappa del mondo tolta**: la scelta del paese e un elenco con ricerca (meno RAM e meno flash; restano solo i nomi dei paesi).
- **Guida a 6 passi**: Lingua, Paese, Nome, Antenna, Wi-Fi di casa o hotspot, Fine. Il passo Wi-Fi **prova subito a collegarsi** (fino a 20 s) e dice se ha funzionato; se fallisce non si perde nulla. In modalita hotspot (senza Wi-Fi di casa) compare il passo **Ora** con fuso orario, data e ora (preimpostati dal dispositivo); con la Wi-Fi di casa l'ora arriva da NTP. A fine installazione, al primo accesso dalla Wi-Fi di casa, hotspot, portale automatico e HTTP si spengono e restano spenti (li riaccende l'utente quando vuole); la guida lo dice chiaramente.
- **Password dell'hotspot piu semplice**: 12 caratteri, solo minuscole e cifre (senza simboli e senza caratteri ambigui), sempre casuale per scheda.
- **Home**: anelli CPU (con velocita e temperatura), RAM, PSRAM, file, Wi-Fi (modo, segnale o client); orologio con data; un **widget per ogni servizio acceso** (HTTP/S, MQTT, ESP-NOW, mDNS, Bluetooth, NTP, DHCP, hotspot, risparmio energia, statistiche), con stato a parole; riga Core 0 / Core 1. Tolti il grafico della temperatura, l'indirizzo IP e il QR (resta solo quello dell'MFA). Nuova richiesta `/api/home`.
- **Password e QR dell'hotspot** solo in Rete > Punto di accesso (si vedono subito aprendo la scheda, Admin). L'avviso "la Wi-Fi di casa non si e collegata" e ora nelle notifiche.
- **MQTT e Rete tra schede**: interruttore Acceso/Spento (come HTTP) e, di fianco, pulsante **Riavvia**, spento quando il servizio e fermo.
- **Sistema > Ora**: scelta del fuso orario.
- Novita tecniche: `meshNodeCount`, `timeLastSync`, `bleLeftSec`, `cpu0`/`cpu1` nello stato.

## 1.7.3 (ottobre 2026) - "SICUREZZA ED ENERGIA"
- **Accesso in due passi (MFA, TOTP RFC 6238)**: per utente, con QR (solo da HTTPS o dall'hotspot), 8 codici di recupero, codice 6 cifre a 30 s (finestra +-1, ogni codice una sola volta). Senza ora valida: si chiede l'ora al browser (solo dopo password e codice giusti, con audit giallo) oppure solo recupero/blocco, a scelta. BOOT 8 s spegne l'MFA degli Admin (allarme giallo).
- **Anti-bot senza servizi esterni**: prova di lavoro (SHA-256, 0/12/14/16/18 bit, 14 di fabbrica), campo-trappola nascosto, blocco IP dopo i fallimenti.
- **Scheda Certificato** (Sistema): scadenza (da/a), impronta SHA-256, scarica, rigenera, importa.
- **Risparmio energia** (Servizi): modi Normale / Risparmio Wi-Fi / Sonno programmato (sveglia a tempo, da 1 min a 7 giorni), comando "Dormi adesso". La scheda CPU e passata qui dentro. Il tasto BOOT non e fonte di risveglio (solo timer o RESET).
- **Statistiche d'uso** (Sistema > Diagnostica): spente di fabbrica, anonime (niente MAC/IP), contatori Wi-Fi/Bluetooth/avvii/LED, CSV scaricabile, azzera.
- Comandi seriali: `power`, `sleep [min]`, `stats`.
- Bluetooth: scheda in Servizi, messaggio chiaro se manca RAM.

## 1.7.2 (ottobre 2026) - "BASE + SERVIZI"
- **Primo avvio**: il banner con la password dell'hotspot resta finche la guida non e finita (ad ogni apertura del monitor seriale e ogni 30 s);
  il Wi-Fi salvato da solo non fa piu risultare la scheda "configurata". Comando seriale `ap`.
- **Menu riordinati**: Periferiche (ex Hardware: Pin, LED); Servizi (Punto di accesso, HTTP, HTTPS, MQTT, Rete tra schede, Automazioni, Task,
  Watchdog, Avvio, Terminale); Sistema (Stato, File, Ora, Localizzazione con la lingua, Log, Config, Accessibilita, Note legali, Aiuto);
  Sicurezza (Password con seriale, Utenti, Filtro IP, Compliant, ex Controlli).
- **Pagina di accesso con data, ora e fuso** della scheda e avviso se l'ora e diversa da quella del dispositivo.
- **QR in Home**: hotspot = QR Wi-Fi con stampa; modo cliente = QR con l'indirizzo. Tolto dalla guida. Interruttore Admin per nasconderlo.
- **Servizio Punto di accesso, HTTP e porte** (tab nuovi): ogni servizio acceso/spento, porte HTTP/HTTPS configurabili (dal riavvio), almeno un
  protocollo web sempre acceso, HTTP-solo permesso con avviso, recupero con BOOT 8 s. Portale automatico solo con HTTP sulla porta 80.
- **Guida**: nuovo ordine (... hotspot, Wi-Fi di casa, ora). Senza Wi-Fi: NTP spento e ora a mano (avviso: niente batteria per l'orologio).
  Con Wi-Fi: indirizzo nuovo con QR e, al primo accesso dalla rete di casa, si spengono hotspot, portale automatico e HTTP. Avviso in Home
  se il collegamento fallisce.
- **Configurazione solo da seriale**: comando `setup` guidato, `wifi set|off`, `ntp on|off`, `svc ...`.
- **Pin**: schema fronte/retro ridisegnato dalla foto, funzioni e avvisi per pin, **inventario** "collegato a" salvato nella configurazione;
  GP33-37 ora ammessi.
- **RAM e spazio**: registro (18 KB) in PSRAM, HTTP con 3 collegamenti, licenze lunghe e dizionario inglese compressi (gzip) nel firmware (decompressore nostro, vos_inflate.cpp; circa 65 KB in meno).
- **Legale**: tolto l'impegno di supporto di 2 anni (progetto gratuito, senza date garantite); riferimenti normativi in NOTICE, Note legali,
  manuale, SECURITY.md; semaforo CRA verde finche gratuito.
- Manuale it/en: capitoli 12-15.
- **Menu v2**: Rete = Wi-Fi, Punto di accesso, Radio (con il Modo aereo dentro Wi-Fi); Servizi = HTTP, HTTPS, MQTT, Rete tra schede, **NTP, DHCP, DNS, mDNS**, Bluetooth (non supportato), Automazioni, Avvio; Sistema = Stato, gruppo **Diagnostica** (Task, Watchdog, Log, Terminale), File, Ora, Localizzazione, Config, Accessibilita, Note legali, Aiuto.
- **Servizi configurabili e spegnibili**: NTP, DHCP dell'hotspot (durata 10-1440 min; si spegne solo con la Wi-Fi di casa collegata), DNS del portale automatico, mDNS. DHCP, mDNS e portale valgono subito. Comando seriale `svc dhcp|mdns on|off`.
- **Home**: azioni rapide (Modo aereo con conferma, Terminale, Stampa).
- **Bluetooth**: scheda propria in Servizi (stato, codice, accendi 10 minuti); se manca RAM il messaggio dice di chiudere la pagina web.
- **Guida**: pulsante "La scheda e gia configurata" e comando seriale `setup done` (prima "Piu tardi" la lasciava riapparire a ogni avvio). Avvisi in Home e nei controlli se la guida non e finita o se il salvataggio fallisce; messaggio di avvio veritiero; comando `diag`.
- **RAM**: buffer grandi (TLS) in PSRAM, richieste della pagina in coda (2 alla volta), HTTPS con 2 collegamenti e scarto del piu vecchio, log HTTPS piu quieti; NTP nel log una volta ogni 24 h; fuso orario applicato prima della prima riga di log.

## 1.7.1 (4 ottobre 2026)
- **Prima configurazione guidata** (LED arcobaleno lento): lingua, password del pannello, paese sulla mappa, nome, ora, antenna,
  password dell'hotspot con QR ed etichetta da stampare, Wi-Fi di casa. Alla fine i servizi restano spenti.
- **Password dell'hotspot casuale** e diversa per ogni scheda (niente piu `vesevos123`), mostrata sulla seriale con la
  **schermata di benvenuto** (lingua con i tasti 1 e 2) e nella pagina (Rete > Punto di accesso, con QR). Spiegato il motivo legale
  (UE RED/EN 18031, CRA; UK PSTI). Nessuna password di fabbrica per il pannello: si sceglie al primo accesso.
- **Accesso senza password in chiaro**: prova HMAC con numero casuale. **Utenti** (fino a 8) con ruoli Amministratore, Operatore, Ospite,
  controllati dalla scheda (anche nel terminale).
- **HTTPS** (server PsychicHttp): certificato unico creato dalla scheda (ECDSA), impronta in pagina e seriale, certificato tuo facoltativo.
  Dalla rete di casa la pagina passa da sola a https; dall'hotspot resta http (rete gia cifrata).
- **Filtro IP** con regola in prova per 2 minuti e conferma; uscite di emergenza (`firewall off`, BOOT 2-7 s).
- **Controlli della configurazione** (Sicurezza > Controlli): allarmi rossi (corretti subito) e gialli, registro di chi ha cambiato cosa,
  anche le modifiche fatte fuori dal pannello.
- **Localizzazione** (Sistema): paese con mappa del mondo SVG con zoom, canali e potenza della radio secondo il paese, antenna interna/esterna
  con guadagno, fuso, server dell'ora, formati, separatore decimale, primo giorno della settimana. Dati comuni (249 paesi) dentro il firmware.
- **Servizi** (nuova categoria): MQTT (spostato qui, ora anche **mqtts** con certificato del broker), **rete tra schede ESP-NOW**
  (messaggi firmati, fino a 3 salti, ruoli nodo/gateway/sensore), **Bluetooth** per configurare dal telefono (solo a mano, 10 minuti, codice).
  Tutti spenti di fabbrica: la prima volta si accendono a mano.
- **Watchdog**: servizi bloccati, rete assente, RAM bassa, riavvio programmato; massimo 3 riavvii automatici in un'ora.
- **Tasto BOOT** quando lo lasci: <2 s modo aereo, 2-7 s filtro IP spento (azzurro), 8-19 s password azzerate (giallo), 20 s fabbrica (rosso).
- **Lingue**: inglese dentro il firmware con l'italiano; ogni lingua ha la sua bandiera (SVG) e il codice locale; menu delle lingue con bandiere.
- **Note legali** (Sistema): licenze, elenco del software usato (SBOM), dati salvati, sicurezza, regole radio. Comando `legal`.
- Manuale nuovo (`docs/manuale.md`, `docs/manual.en.md`) e pagina Aiuto (`#aiuto`). Nuovi file SECURITY.md (2 anni di aggiornamenti
  di sicurezza), SBOM.spdx.json; COMMERCIAL.md con il kit LGPL; CONTRIBUTING.md con l'accordo per i contributi (CLA).
- Il menu in alto a destra dice **Logout**. Corretto il simbolo `&#183;` che compariva come testo: la pagina ora e compressa (gzip).
- Librerie: **PsychicHttp** e **ArduinoJson** al posto di ESPAsyncWebServer e AsyncTCP.
- Strumenti: `tools/stub/compila.sh` (controllo di tutti i file senza scheda), `tools/mkcommon.py` (dati dei paesi), prova della pagina `tools/webtest/smoke171.js`.

## 1.7.0 (3 ottobre 2026)
- **MQTT** (Rete > MQTT): la scheda invia il suo stato (CPU, temperatura, RAM, segnale, IP...) a un broker e riceve comandi
  sull'argomento `<prefisso>/cmd` (le stesse azioni delle Automazioni: `led-color ff0000`, `gpio 4 1`, `reboot`...), con risposta su `cmd/result`.
  Messaggio online/offline, Home Assistant la riconosce da solo (sensori e pulsante Riavvia). E un servizio: avvio automatico o a mano,
  compare in Ordine di avvio e nel tab Task (Ferma / Riavvia). Shell `mqtt [status|start|stop|restart|pub]`, azione "MQTT: invia un messaggio" nelle Automazioni.
  Usa il client MQTT gia dentro il core ESP32: nessuna libreria in piu.
- **Modo aereo** (Rete > Modo aereo): spegne il Wi-Fi, le Automazioni continuano. Si sceglie come riaccendere la rete: al prossimo avvio,
  dopo un tempo, a un orario oppure solo a mano. Sempre: `airplane off` dalla seriale o pressione breve del tasto BOOT. LED viola. Anche nelle Automazioni.
- **Portale automatico**: in modalita hotspot, collegandoti alla rete della scheda la pagina si apre da sola (Android, iPhone, Windows, Mac).
- **Blocco degli accessi** (Sicurezza): dopo N password sbagliate dallo stesso indirizzo IP la scheda lo blocca; ogni blocco dura il doppio (max 1 ora).
  Troppe richieste senza accesso contano come errore. Elenco degli indirizzi, sblocco dalla pagina o dalla seriale (`ban`, `unban all`).
- **Pin**: rilevamento automatico del chip (modello, revisione, numero di GPIO) e schema SVG della scheda vista dall'alto, con pin colorati e cliccabili,
  linee verso chi usa un pin e pulsante Scarica SVG.
- **Task**: pulsante Riavvia accanto a Ferma; un task fermato resta in elenco con il pulsante Avvia. Shell `start <nome>`.
- **Registro (Log)**: scheda propria in Sistema, aggiornamento automatico, filtro, righe colorate, Scarica e Svuota. Fino a 150 righe con data e ora vere.
- **Ora**: data e ora a mano (anche `date set AAAA-MM-GG HH:MM`) e frequenza della sincronizzazione NTP (15 minuti ... ogni settimana, oppure solo all'avvio).
- **Menu del tasto in alto a destra**: Esci, Riavvia, Sleep (sonno profondo: si riaccende con RESET), con icone. Shell `sleep`.
- **File**: grafico a torta dello spazio (file della cartella, sistema, libero); le cartelle mostrano la loro dimensione.
- Tolto il LED aggiuntivo (pagina, shell, configurazione); le regole vecchie con `led2` vengono ignorate senza errore.
- Il MAC si vede senza pulsante copia.
- Corretto: nella 1.6.1 lo stile del pulsante copia rovinava il pannello del terminale.

## 1.6.1 (3 ottobre 2026)
- **Indirizzo MAC**: si vedono il MAC Wi-Fi (client) e quello del punto di accesso nella Home, in Rete > Indirizzo IP e in Rete > Punto di accesso.
  Un tocco lo copia. Il MAC si legge dal chip, quindi c'e anche con il Wi-Fi spento. Nuovi comandi shell `net` (rete leggibile) e `info` (riassunto della scheda).
- **Tab File rifatto**: percorso cliccabile, icone per cartelle e tipi di file (testo, JSON, immagine, binario), barra dello spazio usato.
  Su ogni voce un menu (pulsante con tre puntini, tasto destro o tocco lungo sul telefono) con Apri/Modifica, Scarica, Rinomina, Duplica, Sposta, Elimina.
  Finestre di conferma vere al posto dei vecchi riquadri del browser; avviso prima di sostituire un file che esiste gia.
- **Editor di file**: nuovo file con scelta della cartella, editor a tutto schermo con numeri di riga, riga/colonna, dimensione e spazio libero.
  Ctrl+S salva, Tab inserisce spazi (Ctrl+M per cambiare), Salva come, Incolla, Svuota, Scarica. Avviso se chiudi senza salvare.
  Controllo dei file .json prima di salvare (dice la riga dell'errore). Avviso sui file di sistema (`/rules.txt`, `/lang/`). Limite 32 KB.
- **Firmware dei file**: salvataggio sicuro (prima un file temporaneo, poi lo scambio: un calo di corrente non rovina il file), controllo dello spazio libero,
  lettura in UTF-8 (gli accenti non diventano piu "?"), i file binari non si aprono nell'editor. Nuove API `/api/fs/copy` e `/api/fs/dirs`.
- **Accessibilita** (Config > Accessibilita, oppure dal menu del tema): testo grande e molto grande, contrasto alto, colori adatti ai daltonici,
  riduci animazioni (contrasto e animazioni possono seguire il dispositivo). Link "Vai al contenuto", contorno ben visibile quando usi la tastiera,
  Esc chiude menu e finestre, pulsanti con nome per i lettori di schermo, messaggi annunciati, pulsanti piu grandi sui telefoni.
- **Icone nuove**: termometro (temperatura), banco di memoria (RAM e PSRAM), dischi sovrapposti (file), ruota dentata (Sistema), scheda SD (pronta per il futuro).
  Nel tab File un'icona per ogni tipo: cartella, testo, JSON, codice (html, js, cpp...), immagine, binario.
- Corretto: aprire un file in modifica non mostrava il titolo (errore JavaScript).

## 1.6.0 (3 ottobre 2026)
- **Terminale a pannello**: la shell web esce dalla scheda e diventa un pannello in basso, sempre disponibile da ogni scheda,
  ridimensionabile (si trascina la barra; pulsanti piccolo / medio / grande) e ricordato nel browser. Si apre dal pulsante in alto o con Ctrl + `.
  Su telefono sta sopra la barra delle icone. La scheda "Terminale" resta come scorciatoia.
- **Automazioni** (nuova scheda in Sistema): regole "Quando... Se... Allora..." che la scheda esegue da sola, anche senza rete.
  Quando: orario con giorni (sveglia), ogni N tempo, dopo N dall'avvio, all'avvio, Wi-Fi collegato/perso, temperatura sopra una soglia.
  Se: sempre, fascia oraria, certi giorni. Allora: colore/modo/luminosita del LED, LED aggiuntivo, pin acceso/spento, attesa,
  nota nel registro, aggiorna l'ora, riavvia. Editor a menu con frase di anteprima, modelli pronti (Sveglia, Dopo l'avvio, Allarme temperatura),
  "Prova ora", interruttore per ogni regola, protezione antiloop (oltre 10 esecuzioni al minuto la regola si ferma).
  Le regole stanno in `/rules.txt` (una riga per regola). Nuovo comando shell `rules [run <n>]`.
- **Ordine di avvio** (nuova scheda in Sistema): si sceglie in che ordine partono i servizi (Sistema, LED, Rete, Ora, Pagina web, Automazioni),
  trascinando o con le frecce. Ogni servizio dichiara cosa richiede e resta attaccato ad esso (vista ad albero); il firmware corregge sempre
  l'ordine perche nessun servizio parta prima di quelli che richiede. I servizi fissi (registro, pin, file e configurazione, lingua) partono sempre per primi.
  Se l'avvio con l'ordine scelto non riesce per 2 volte, la scheda torna da sola a quello predefinito. Comando shell `boot-order`.
- **Menu del tema** con scelta chiara e scritta del tema in uso (Automatico, Chiaro, Scuro), al posto del pulsante che girava tra i tre.
- **Scheda Ora**: nuovo pulsante "Aggiorna ora dalla rete" (richiede NTP acceso e Wi-Fi collegato).
- **Tema scuro corretto**: alcune variabili di colore (campi, bordi, riquadri dei messaggi) non erano definite e ora hanno i colori giusti.
- Pagina: link legali e GitHub in fondo, con versione e copyright.
- Nuove traduzioni (en, es, de) per tutte le novita; il controllo `mklang.py` ora verifica anche le liste di testi nei menu.

## 1.5.0 (3 ottobre 2026)
- README: tolta la sezione "Problemi noti" (nota sul LED del pin 38).
- **Scheda Task dinamica**: tabella che si aggiorna da sola ogni 2 secondi, ordinabile cliccando i titoli
  (nome, tipo, stato, priorita, stack libero, CPU se disponibile). Il tipo distingue Sistema, VesevOS e App.
  Si possono fermare solo i task consentiti (led, time, monitor), con conferma; gli altri hanno il lucchetto.
  Il task fermato riparte al riavvio. Nuovo comando shell `kill <nome>`. L'azione finisce nel log.

- **Nuova interfaccia a 5 tab con icone**: Home, Rete, Hardware, Sistema, Sicurezza. Barra laterale sul computer,
  barra fissa in basso sul telefono, sotto-schede dentro Hardware (Pin, LED) e Sistema (Task, Stato, File, Ora, Shell, Config).
- **Home control room**: anelli colorati verde/giallo/rosso (CPU, RAM, PSRAM, File, Wi-Fi, Temperatura) cliccabili,
  carta d'identita della scheda (chip, core, memoria, ID unico dal MAC, ore di vita), risorse, rete, grafici CPU e temperatura,
  campanella con gli allarmi, pulsante "Trova questa scheda" (il LED fa l'arcobaleno per 10 secondi).
- **Tema** automatico / chiaro / scuro, ricordato nel browser.
- **Tab Sicurezza**: cambio password (spostato da Config) e **interruttore per la password sulla seriale**
  (predefinito acceso; comando `serial-auth on|off`; BOOT 8 s e ripristino di fabbrica la riaccendono).
- Sul telefono restano fissi la barra in alto, la barra in basso e le sotto-schede.
- I messaggi verdi di conferma (Salvato, Copiato...) compaiono con una dissolvenza e spariscono da soli dopo 3 secondi; gli errori restano.
- Il firmware ora fornisce alla pagina chip, revisione, core, flash, canale Wi-Fi e dispositivi collegati all'AP.

## 1.4.3 (3 ottobre 2026)
- README rinnovato: banner, missione, visione, filosofia, schermate, mappa del progetto.
- README in inglese (`README.en.md`), banner con il Vesuvio, `assets/vesuvio.svg`.
- Pagina: logo a chip in alto a sinistra (intestazione e login), Vesuvio nel login, icona SVG della scheda del browser.
- Corretto il numero di versione scritto nel firmware (era rimasto 1.4.0).
- Link al GitHub del progetto: in fondo alla pagina, nel saluto sulla seriale e nel nuovo comando `about`.
- **Shell web migliorata** (solo pagina): aspetto da terminale con colori (errori rossi, avvisi gialli),
  cronologia con le frecce su/giu salvata nel browser (`passwd` mai salvata), completamento con Tab
  (comandi e opzioni), filtri `| grep`, `| head`, `| tail`, comandi `clear` e `history`, Ctrl+L e Ctrl+C,
  incolla su piu righe con conferma, pulsante "Copia tutto", tasti rapidi per il telefono,
  conferma prima di `reboot` e `factory-reset`.
- **Scheda Rete** (prima "Wi-Fi") con quattro sotto-schede: Wi-Fi (cerca reti, SSID, password), Indirizzo IP,
  Nome (nome host e dominio, spostati qui da Config), Punto di accesso.
  I campi IP, mask, gateway e DNS sono sempre scritti ma disattivati quando non si possono cambiare
  (DHCP: valori assegnati dal router; punto di accesso: indirizzo fisso; non connesso: vuoti).
  Con "Statico" si attivano gia precompilati con i valori in uso.
  Il firmware ora fornisce davvero mask, gateway, DNS 1 e DNS 2 e lo stato della rete.
  Corretto: durante la scansione in modo AP l'indirizzo mostrato poteva essere 0.0.0.0.
- **Contaore di vita** (come un contachilometri): ore totali di accensione della scheda, salvate ogni 10 minuti
  in memoria NVS. Non si azzera col ripristino di fabbrica. Visibile nel Riepilogo e nel comando `uptime`.

## 1.4.2 (3 ottobre 2026)
- Titolare del software: Domenico Paolella. Nome inserito nelle note legali, in `NOTICE.txt`
  e nelle intestazioni di copyright di tutti i sorgenti.

## 1.4.1 (3 ottobre 2026)
- **Prova dei pin**: nella scheda Pin i pin sono cliccabili (verde = provabile, blu = in uso, grigio = vietato).
  Si puo mettere un pin Alto, Basso, farlo Lampeggiare o Leggerlo (con resistenza verso 3,3 V o massa).
  La prova si spegne da sola (20 s le uscite, 60 s la lettura). Un popup avvisa dei rischi per la scheda.
  Pin vietati: USB, LED RGB, BOOT, pin di avvio, flash/PSRAM, pin gia usati dal sistema.
- Comando shell `pin <n> [high|low|blink|read [up|down]|off]`; API `/api/pinmap` e `/api/pintest`.

## 1.4.0 (3 ottobre 2026)
- **Traduzioni separate dal firmware**: italiano nel firmware, altre lingue come file `lang/<codice>.json`
  caricabili dalla pagina (Config > Lingue). Incluse inglese, spagnolo e tedesco. Valgono per pagina,
  shell e messaggi. Selettore di lingua nella pagina (anche al login), comando shell `lang`.
- **Licenze e note legali nel firmware**: GPL v3, LGPL v3, LGPL v2.1, Apache 2.0 e nota legale con
  titolarita e riferimenti normativi. Si leggono da Config > Licenze, dal piede di pagina e con il comando
  shell `license`. File `NOTICE.txt`.
- Doppia licenza GPL v3 + commerciale (vedi `COMMERCIAL.md`).
- Strumenti: `tools/mklang.py` (genera e controlla le lingue), `tools/mklicense.py` (testi legali).

## Licenza
- Doppia licenza: GPL v3 o successiva + licenza commerciale. Intestazione SPDX nei sorgenti.

## 1.3.6 (3 ottobre 2026)
- LED aggiuntivo comandato solo acceso/spento (digitalWrite), senza PWM. Pin predefinito 38.
- Comando shell `led2-invert on|off`; `led2` senza argomenti mostra lo stato.

## 1.3.5
- Nome host e dominio configurabili (pagina, shell, configurazione).
- Uptime senza valori a zero iniziali.

## 1.3.4
- Correzioni: fine riga nella shell web, riconnessione Wi-Fi in AP, motivo del reset.

## 1.3.0 - 1.3.3 (fase 2)
- Ora e NTP (fuso, formati data/ora, server NTP), scheda File, velocita CPU automatica o fissa,
  allarme temperatura, LED aggiuntivo, nuovi comandi shell.

## 1.2.0 (fase 1)
- Base: Wi-Fi AP + client, pagina web, shell web e seriale, password (SHA-256), LED RGB,
  scheda Pin, configurazione in stile OpenWrt, log.
