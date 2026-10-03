# Cronologia delle versioni

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
