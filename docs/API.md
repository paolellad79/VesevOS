# API di VesevOS (generato da `tools/mkapidoc.py`: non modificare a mano)

Versione API: **1** (campo `apiVersion` in `/api/status` e `/api/common`). Cambia solo per rotture; aggiungere campi e sempre permesso.

## Regole generali

- **Accesso:** cookie di sessione dopo `POST /api/login`. Livelli: `pubblico` (nessuna sessione), `ospite`, `operatore`, `admin` (ogni livello include i precedenti). Senza sessione: 401 `unauthorized`; ruolo insufficiente: 403 `forbidden`.
- **Risposta:** JSON. In errore sempre `{"ok":false,"err":"testo tradotto","code":"nome"}`; `code` e stabile (usalo nelle app), `err` cambia con la lingua.
- **Codici di errore** (il codice HTTP segue `code`):

| code | HTTP | quando |
|---|---|---|
| `error` | 400 | dato mancante o non valido (nessun cambiamento fatto) |
| `bad_login` | 403 | nome, password o codice errati |
| `forbidden` | 403 | ruolo insufficiente o azione non permessa |
| `https_only` | 403 | dalla rete di casa serve HTTPS |
| `notfound` | 404 | elemento non trovato |
| `state` | 409 | stato non adatto (es. spegnere l'hotspot senza Wi-Fi di casa) |
| `toobig` | 413 | dato troppo grande |
| `blocked` | 429 | troppi errori: IP bloccato |
| `off` | 503 | servizio spento o pacchetto assente |
| `unauthorized` | 401 | manca la sessione (solo questo caso usa 401) |

- **Limiti:** corpo massimo 48 KB; elenco file 200 voci, ricerca reti 40 reti (oltre: `more:true`). Date in secondi Unix (UTC), durate in secondi, dimensioni in byte.
- **Parametri:** `application/x-www-form-urlencoded`. Regola API 12: un valore non valido non deve cambiare nulla.
- La colonna **Parametri** e la **Errori** sono lette dal codice (indicative: gli errori elencati sono quelli esplicitamente dati dalla rotta, oltre a quelli generali sopra).

## Rotte (127)

| Metodo | Percorso | Livello | Cosa fa | Parametri | Errori |
|---|---|---|---|---|---|
| GET | `/` | pubblico | La pagina web (compressa) | - | - |
| POST | `/api/airplane` | operatore | Modo aereo: spegne o riaccende la radio (on, exit) | `exit`, `on`, `param` | `error` |
| GET | `/api/ap` | ospite | Hotspot: nome, stato, client | - | - |
| POST | `/api/ap` | admin | Cambia nome/password/QR dell'hotspot (new = nuova password casuale) | `new`, `pass`, `qr`, `ssid` | `error` |
| GET | `/api/audit` | ospite | Allarmi di configurazione e storico | - | - |
| POST | `/api/audit/ack` | operatore | Segna come letto un allarme | `n` | - |
| GET | `/api/auth` | pubblico | Stato dell'accesso: utente, ruolo, pacchetti presenti, impronta del certificato | - | - |
| GET | `/api/ban` | admin | Elenco IP bloccati e regole di blocco | - | - |
| POST | `/api/ban/set` | admin | Imposta tentativi e secondi di blocco | `fails`, `secs` | `error` |
| POST | `/api/ban/unban` | admin | Sblocca un IP | `ip` | `notfound` |
| GET | `/api/ble` | admin | Stato del Bluetooth | - | - |
| POST | `/api/ble` | admin | Comando Bluetooth (a = start, restart, stop; min = limite di tempo facoltativo in minuti, 0 o assente = nessun limite) | `a`, `min` | `error` |
| GET | `/api/boot` | operatore | Ordine di avvio dei servizi | - | - |
| POST | `/api/boot` | admin | Salva l'ordine di avvio | `order` | `error` |
| POST | `/api/boot/reset` | admin | Ordine di avvio predefinito | - | - |
| GET | `/api/boots` | operatore | Diario dei riavvii | - | - |
| POST | `/api/boots/clear` | admin | Cancella il diario dei riavvii | - | - |
| GET | `/api/common` | pubblico | Dati comuni (paesi, regole radio, librerie, apiVersion), in cache 24 ore | - | - |
| GET | `/api/config/download` | admin | Scarica la configurazione preparata | - | - |
| POST | `/api/config/export` | admin | Prepara la configurazione da scaricare (acc = accetto avviso, pw = frase per i segreti) | `acc`, `pw` | `error` |
| POST | `/api/config/restore` | admin | Ripristina la configurazione da file (t = testo, pw = frase) | `pw`, `t` | `error`, `toobig` |
| POST | `/api/cpu` | admin | Modo CPU (automatico o fisso) | `mode` | `error` |
| GET | `/api/dev` | operatore | Elenco periferiche del Device Manager, o una sola (id) | `id` | - |
| POST | `/api/dev/act` | operatore | Azione di una periferica (a, arg) | `a`, `arg`, `id` | `error` |
| POST | `/api/dev/set` | admin | Accende o spegne una periferica | `id`, `on` | `error` |
| GET | `/api/events` | operatore | Ultimi eventi del sistema (Event Bus; argomenti: svc.state, net.state, mqtt.link, ble.link, mesh.node, time.sync, sec.ban, sec.unban, audit.new, audit.clear): since = ultimo numero visto, n = quanti (1-40, 20 di fabbrica); lost = persi | `n`, `since` | - |
| POST | `/api/factory` | admin | Ripristino di fabbrica e riavvio (la parola AZZERA e controllata solo dalla pagina) | - | - |
| POST | `/api/firstpass` | pubblico | Crea la password del primo amministratore | `p` | `error`, `forbidden`, `state` |
| POST | `/api/fs/copy` | admin | Copia file | `from`, `to` | `error` |
| POST | `/api/fs/del` | admin | Cancella file o cartella | `path` | `error` |
| GET | `/api/fs/dirs` | operatore | Elenco delle sole cartelle | `dir` | `error` |
| GET | `/api/fs/get` | operatore | Scarica un file | `path` | - |
| GET | `/api/fs/list` | operatore | Elenco di una cartella (max 200 voci, more:true se tagliato) | `path` | - |
| POST | `/api/fs/mkdir` | admin | Crea cartella | `path` | `error` |
| POST | `/api/fs/ren` | admin | Rinomina o sposta | `from`, `to` | `error` |
| POST | `/api/fs/save` | admin | Salva un file di testo (new = file nuovo) | `new`, `path`, `text` | `error` |
| GET | `/api/fs/text` | operatore | Legge un file di testo | `path` | - |
| GET | `/api/fw` | admin | Regole del firewall (filtro IP) | - | - |
| POST | `/api/fw` | admin | Salva il filtro IP (mode, regole, ntp) | `mode`, `ntp`, `rules` | `error` |
| POST | `/api/fw/confirm` | admin | Conferma il filtro IP dopo la prova | - | - |
| GET | `/api/home` | ospite | Dati dei widget della Home (un solo pezzo) | - | - |
| POST | `/api/identify` | operatore | Fa lampeggiare il LED per riconoscere la scheda | - | - |
| POST | `/api/lang` | admin | Lingua della scheda | `code` | `error` |
| GET | `/api/langfile` | pubblico | File di lingua da scaricare | `code` | - |
| GET | `/api/langs` | pubblico | Lingue disponibili | - | - |
| POST | `/api/led` | operatore | Colore, modo, luminosita del LED | `br`, `color`, `mode` | `error` |
| GET | `/api/license` | pubblico | Testo di una licenza (id) o elenco | `id` | - |
| GET | `/api/log` | ospite | Registro (testo) | - | - |
| POST | `/api/log/clear` | admin | Svuota il registro | - | - |
| GET | `/api/log/level` | ospite | Livello del registro | - | - |
| POST | `/api/log/level` | admin | Cambia livello del registro (lv) | `lv` | - |
| POST | `/api/login` | pubblico | Accesso con nome e password (prova HMAC: nonce, hp, mac) | `hp`, `mac`, `nonce`, `pw`, `u` | `bad_login`, `blocked`, `error` |
| POST | `/api/login/mfa` | pubblico | Secondo passaggio con codice MFA | `code`, `now`, `tok` | `bad_login`, `blocked`, `error` |
| POST | `/api/login/start` | pubblico | Inizio accesso: chiede nonce e prova di lavoro | `u` | `blocked` |
| POST | `/api/logout` | pubblico | Esce e chiude la sessione | - | - |
| GET | `/api/mesh` | ospite | Stato della rete tra schede (ESP-NOW) | - | - |
| POST | `/api/mesh` | admin | Impostazioni della rete tra schede | `auto`, `ch`, `key`, `newkey`, `role` | `error` |
| POST | `/api/mesh/key` | admin | Genera una nuova chiave | - | - |
| POST | `/api/mesh/run` | admin | Accende, spegne, riavvia (a) | `a` | `error` |
| POST | `/api/mesh/send` | operatore | Invia testo o comando a una scheda | `cmd`, `text`, `to` | `error` |
| GET | `/api/mfa` | ospite | Stato MFA dell'utente | - | - |
| POST | `/api/mfa/begin` | ospite | Inizia l'attivazione MFA | - | `error`, `forbidden` |
| POST | `/api/mfa/confirm` | ospite | Conferma con un codice | `code` | `error` |
| POST | `/api/mfa/off` | ospite | Spegne MFA | `i` | `forbidden` |
| POST | `/api/mfa/recovery` | ospite | Nuovi codici di recupero | - | `forbidden`, `state` |
| POST | `/api/mfa/set` | admin | Impostazioni MFA (admin) | `nt`, `pow` | `error` |
| GET | `/api/mqtt` | operatore | Stato MQTT | - | - |
| POST | `/api/mqtt` | admin | Impostazioni MQTT | `auto`, `clearpass`, `every`, `ha`, `host`, `pass`, `port`, `prefix`, `tls`, `user` | `error`, `toobig` |
| POST | `/api/mqtt/ca` | admin | Carica il certificato CA del broker | `pem` | `error` |
| POST | `/api/mqtt/run` | operatore | Accende, spegne, riavvia (a) | `a` | `error`, `state` |
| POST | `/api/passwd` | ospite | Cambia la propria password (o = vecchia, n = nuova) | `n`, `o` | `bad_login`, `error` |
| GET | `/api/pinmap` | ospite | Mappa dei pin | - | - |
| GET | `/api/pinnotes` | ospite | Note dei pin | - | - |
| POST | `/api/pinnotes` | admin | Salva le note di un pin | `g`, `name` | `error` |
| GET | `/api/pins` | ospite | Stato dei pin | - | - |
| GET | `/api/pins/info` | ospite | Informazioni sui pin | - | - |
| GET | `/api/pintest` | ospite | Stato della prova pin | - | - |
| POST | `/api/pintest` | operatore | Prova un pin (action, gpio, pull) | `action`, `gpio`, `pull` | `error` |
| GET | `/api/power` | operatore | Risparmio energia: modo e tempi | - | - |
| POST | `/api/power` | admin | Cambia modo e tempi | `awake`, `mode`, `sleep` | `error` |
| POST | `/api/reboot` | operatore | Riavvia la scheda | - | - |
| GET | `/api/region` | ospite | Paese, fuso, lingua, radio | - | - |
| POST | `/api/region` | admin | Cambia paese, fuso, formati, antenna, potenza | `antenna`, `country`, `datefmt`, `decsep`, `gain`, `ntp`, `tempunit`, `timefmt`, `txpower`, `tz`, `tzname`, `weekstart` | `error` |
| GET | `/api/rules` | operatore | Testo delle automazioni | - | - |
| POST | `/api/rules` | operatore | Salva le automazioni | `text` | `error` |
| POST | `/api/rules/run` | operatore | Esegue una regola (i) | `i` | `error` |
| GET | `/api/selftest` | admin | Stato dell'autotest | - | - |
| POST | `/api/selftest` | admin | Avvia autotest, cancella (active, clear, names) | `active`, `clear`, `names` | `error` |
| GET | `/api/selftest/report` | admin | Rapporto dell'autotest | - | `notfound` |
| GET | `/api/serial` | admin | Impostazioni della seriale | - | - |
| POST | `/api/serial` | admin | Cambia una impostazione (k, v) | `k`, `v` | `error` |
| POST | `/api/serialauth` | admin | Password sulla seriale acceso/spento (on) | `on` | - |
| GET | `/api/settings` | ospite | Impostazioni generali (per la pagina) | - | - |
| POST | `/api/setup/done` | admin | Chiude la guida (ntp = tieni NTP) | `ntp` | - |
| POST | `/api/shell` | operatore | Esegue un comando della shell (c) | `c` | - |
| POST | `/api/sleep` | operatore | Mette la scheda in sonno per min minuti | `min` | `error` |
| GET | `/api/stats` | operatore | Statistiche d'uso (JSON) | - | - |
| POST | `/api/stats` | admin | Accende, spegne, azzera (on, reset) | `on`, `reset` | - |
| GET | `/api/stats.csv` | operatore | Statistiche in CSV | - | - |
| GET | `/api/status` | ospite | Stato completo (versione, apiVersion, RAM, rete, CPU) | - | - |
| GET | `/api/svc` | ospite | Servizi di rete: hotspot, HTTP, HTTPS, porte, DHCP, mDNS | - | - |
| POST | `/api/svc` | admin | Cambia i servizi di rete | `apOn`, `captive`, `dhcpOn`, `httpOn`, `httpPort`, `httpsOn`, `httpsPort`, `lease`, `mdnsOn` | `error`, `state` |
| POST | `/api/system` | admin | Nome della scheda (hostname) e dominio | `domain`, `hostname` | `error` |
| POST | `/api/taskkill` | admin | Ferma un task (name) | `name` | `error` |
| GET | `/api/tasklist` | operatore | Task in JSON | - | - |
| POST | `/api/taskrestart` | admin | Riavvia un task (name) | `name` | `error` |
| GET | `/api/tasks` | operatore | Task in testo | - | - |
| GET | `/api/time` | ospite | Ora e impostazioni NTP | - | - |
| POST | `/api/time` | admin | Impostazioni NTP | `every`, `ntp`, `serve`, `server`, `server2` | `error` |
| POST | `/api/time/set` | admin | Imposta l'ora a mano (epoch o local) | `epoch`, `local` | `error` |
| POST | `/api/time/sync` | operatore | Sincronizza ora con NTP | - | `state` |
| GET | `/api/tls` | ospite | Stato HTTPS e certificato | - | - |
| POST | `/api/tls` | admin | HTTPS: acceso, certificato, chiave, rigenera | `cert`, `key`, `on`, `regen` | `error` |
| GET | `/api/tls/cert` | ospite | Scarica il certificato | - | `state` |
| GET | `/api/users` | admin | Utenti | - | - |
| POST | `/api/users/add` | admin | Aggiunge un utente | `name`, `pass`, `role` | `error` |
| POST | `/api/users/del` | admin | Elimina un utente (i) | `i` | `error` |
| POST | `/api/users/pass` | admin | Cambia la password di un utente | `i`, `pass` | `error` |
| POST | `/api/users/set` | admin | Cambia ruolo o attivo | `i`, `on`, `role` | `error` |
| GET | `/api/wd` | operatore | Watchdog: regole e stato | - | - |
| POST | `/api/wd` | admin | Salva le regole del watchdog | `at`, `days`, `net`, `netmin`, `ram`, `ramkb`, `task`, `updays` | - |
| POST | `/api/wifi/ap` | admin | Passa a solo hotspot (spegne il Wi-Fi di casa) | - | - |
| POST | `/api/wifi/save` | admin | Salva il Wi-Fi di casa (ssid, pass, IP) | `cleanup`, `d1`, `d2`, `dhcp`, `gw`, `ip`, `mask`, `pass`, `ssid` | `error` |
| GET | `/api/wifi/scan` | operatore | Risultato della ricerca reti (max 40, more:true se tagliato) | - | - |
| POST | `/api/wifi/scan` | operatore | Avvia la ricerca reti | - | - |
| GET | `/api/wifi/test` | admin | Esito dell'ultima prova di connessione | - | - |
| POST | `/api/wifi/test` | admin | Prova la connessione a una rete senza salvare | `pass`, `ssid` | `error` |
