# Cronologia delle versioni

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
