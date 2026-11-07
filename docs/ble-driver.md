# Bluetooth (BLE) da programma VesevOS

Stato: SCRITTO il 26/10/2026, compilato solo con gli stub (`tools/stub/compila.sh`), NON compilato con ESP-IDF, NON provato su scheda.
Decisione dell'utente (26/10/2026): il BLE resta nel firmware SPENTO; cosa farne lo decide un programma VesevOS; senza programma VesevOS NON e utilizzabile.

## Cosa offre il firmware
- La radio Bluetooth (solo ruolo "periferica": la scheda si annuncia, il telefono si collega). Spenta di fabbrica e dopo ogni riavvio.
- Un solo servizio definito dal programma, con fino a 4 caratteristiche (lettura, scrittura, notifica). Valori fino a 128 byte.
- Abbinamento con cifratura (LE Secure Connections) con un codice a 6 cifre casuale, nuovo a ogni accensione.
- Ciclo di vita: lo accende e lo spegne chi lo possiede; si pulisce da solo quando il programma finisce.

## Cosa NON fa
- Nessun servizio, canale di testo, comando, annuncio o nome incorporato. Niente configurazione Wi-Fi dal telefono. Niente provisioning.
- Nessun comando `ble` nella shell, nessun interruttore o scheda nella pagina, nessuna rotta `/api/ble`, nessuna impostazione `ble` in `/vesevos.conf` (una vecchia `[ble] on=` si legge e si ignora).
- Niente scansione e niente ruolo "centrale" (leggere altri dispositivi): e un lavoro futuro (`drvBleScan`).
- Niente memoria dei telefoni (nessun "bonding"): a ogni collegamento ci si abbina di nuovo.

## Chi puo usarlo
- App `.vab` con `use=ble` nel manifest: tutte le funzioni `ble_*` qui sotto. Il permesso lo da l'Admin installando l'app.
- Script `.vsb` e comando `vsh`: solo `ble_status()`. Se li lancia l'Admin (`script run`, pagina Script, `vsh`) anche `ble_off()` e `ble_passkey()`. Uno script avviato da una regola, da MQTT o dalla rete tra schede NON puo vedere il codice (potrebbe mandarlo fuori).
- Un solo programma alla volta possiede il Bluetooth. Un secondo riceve l'errore "Bluetooth gia in uso dal programma X".

## Funzioni degli script (nelle app con `use=ble`)
| Funzione | Cosa fa |
|---|---|
| `ble_name("nome")` | Nome visibile, 1-20 caratteri (lettere, cifre, spazio, `.`, `_`, `-`). Prima di `ble_on`. Di fabbrica `VesevOS`. NON metterci dati personali. |
| `ble_service("uuid", "uuid1", "rn", "uuid2", "w", ...)` | Definisce il servizio e da 1 a 4 caratteristiche. Proprieta: `r` lettura, `w` scrittura, `n` notifica (senza ripetizioni). Prima di `ble_on`. |
| `ble_adv(vero/falso)` | L'annuncio parte solo se lo chiedi. Senza annuncio nessun telefono puo trovare la scheda. |
| `ble_on(minuti)` | Accende. Senza minuti (o 0) = finche l'app gira. Massimo 60. Chiede soltanto: lo fa il ciclo principale. |
| `ble_off()` | Spegne e libera. |
| `ble_status()` | Testo: `off`, `on ...` o `connected ...` con `owner=`, `left=` (secondi, -1 senza limite), `adv=`, `paired=`, `name=`. Se e spento per un motivo: `off err=ram|locked|timeout|start`. Il codice NON c'e. |
| `ble_set("uuid", dati)` | Valore che il telefono leggera (testo o numero 0-255). |
| `ble_notify("uuid", dati)` | Imposta il valore e avvisa il telefono iscritto. Errore se non e abbinato o iscritto. |
| `ble_disconnect()` | Chiude il collegamento in corso. |
| `ble_passkey()` | SOLO negli script lanciati dall'Admin: il codice, o `nil` se spento. |

Funzioni che l'app puo definire (come `on_set` e `on_tick`):
- `on_write(uuid, dati)`: il telefono ha scritto. `dati` e un testo (anche con byte qualunque).
- `on_read(uuid)`: il telefono ha letto. La lettura in corso e gia servita col valore di prima (la radio non aspetta lo script); se la funzione restituisce un testo o un numero 0-255, diventa il valore della lettura SUCCESSIVA. Per valori sempre freschi usa `ble_set` in `on_tick`.

Esempio completo: `docs/esempi/app/ble-stato.vab` (una caratteristica di sola lettura con temperatura del chip e tempo di attivita, e un "ping" che risponde "ok"). Per leggere il codice: `docs/esempi/script/ble.vsb`; per spegnere: `ble-off.vsb`.

Errori: sono eccezioni dello script `ble_error` con un testo chiaro (mai un crash). Un errore non gestito ferma l'app e quindi spegne il Bluetooth. Per le cose che possono fallire (per esempio la notifica a un telefono non iscritto) usa `try ... except .. ... end`.

## Ciclo di vita
1. Il programma configura (`ble_name`, `ble_service`, `ble_adv`) e chiama `ble_on`. Da qui il programma e il "proprietario".
2. Il ciclo principale controlla memoria (almeno 85 KB liberi e un blocco da 35 KB), modo aereo e pausa di 5 s dall'ultimo spegnimento, genera il codice e accende la radio. Se non riesce, il Bluetooth resta spento e `ble_status()` lo dice (`err=`).
3. Si spegne e si pulisce (servizio, annunci, collegamento, coda degli eventi, codice) quando: l'app finisce o fallisce, o e fermata anche con la forza; scade `ble_on(minuti)`; il programma chiama `ble_off()`; un'Admin lancia `ble-off.vsb`; modo aereo; modalita ridotta del watchdog; troppi collegamenti senza abbinamento.
4. Se l'app muore senza avvisare, il ciclo principale se ne accorge al giro successivo (rete di sicurezza).

## Sicurezza
- Spento di fabbrica; nessun servizio, nessun segreto fisso, nessun codice uguale per tutte le schede (6 cifre casuali ogni volta).
- Ogni lettura, scrittura e notifica richiede un collegamento cifrato E abbinato col codice (MITM). La libreria rifiuta gli accessi non abbinati; il driver ricontrolla e chiude il collegamento.
- Il codice si vede solo con `ble_passkey()` negli script dell'Admin: mai nel registro, nelle verifiche (audit), negli eventi, nello stato, nelle risposte web. Si cancella dalla memoria allo spegnimento.
- Una sola connessione per volta (anche nella configurazione del firmware: `CONFIG_BT_NIMBLE_MAX_CONNECTIONS=1`).
- Limiti: 10 scritture e 10 notifiche al secondo (il resto e scartato), 128 byte per valore, 4 caratteristiche, `ble_on` al massimo 60 minuti, coda di 6 eventi.
- Un collegamento non abbinato entro 30 s, o fermo da 120 s, e chiuso. Dopo 3 collegamenti chiusi senza abbinamento il Bluetooth si spegne (contro chi prova i codici).
- Rischio noto: chi e vicino e vede l'annuncio puo spegnere il Bluetooth collegandosi 3 volte senza abbinarsi (scelta di sicurezza: meglio spento che indovinabile). Per questo l'annuncio va acceso solo quando serve.
- Le callback dello script NON girano nel task Bluetooth: il driver mette gli eventi in una coda e li esegue il ciclo dell'app, con i limiti di tempo e di memoria di sempre. Un'app che non risponde non blocca la radio.
- UUID: si accettano 4 cifre esadecimali o 36 caratteri `8-4-4-4-12`; si rifiutano i 16 bit riservati (`1800`, `1801`, `2800`-`29FF`) e i doppioni.

## Come NON farlo
- Niente segreti nei valori (password, chiavi, token, indirizzi IP, nomi di reti): un telefono abbinato li legge, e il collegamento e protetto solo da un codice a 6 cifre.
- Niente "comandi liberi": non eseguire il testo ricevuto con `act()` o altro. Se serve un comando, accetta un elenco CHIUSO di parole e controlla ogni valore.
- Niente dati personali nel nome del dispositivo (e visibile a chiunque sia vicino e cerchi).
- Non lasciare l'annuncio acceso piu del necessario: usa `ble_on(minuti)`.
- Non usare UUID dei servizi standard per cose diverse.

## Come provarlo (a cura dell'utente, non provato)
1. Carica `ble-stato.vab` in `/apps` (pagina File) e avvialo dalla pagina App (serve Admin).
2. Dalla shell: `script run ble` (carica `ble.vsb` in `/scripts`): mostra lo stato e il codice di abbinamento.
3. Sul telefono apri un'app generica per BLE, cerca `VesevOS stato`, collegati e abbinati col codice.
4. Leggi la caratteristica `...1001` (temperatura e secondi); scrivi un testo in `...1002` e leggi `ok`.
5. Prova a: scollegare e ricollegare; lasciare fermo 2 minuti (si chiude); fermare l'app (il Bluetooth si spegne); `script run ble-off`.
6. Da misurare: RAM libera e blocco piu grande prima e dopo (`free detail`, vedi anche il registro: righe `BLE: spento ...`).

## Controllo legale (preliminare; non e consulenza legale; le norme potrebbero essere cambiate)
- Licenze: la libreria BLE usata e esp-nimble-cpp 2.5.0 (Apache-2.0, verificato in `idf/managed_components/h2zero__esp-nimble-cpp/LICENSE` e `idf_component.yml`; il suo `NOTICE` chiede solo di tenere l'avviso), sopra lo stack NimBLE di ESP-IDF. Compatibile con GPL-3.0 + licenza commerciale. Nessuna libreria nuova. Il marchio "Bluetooth": usare il nome solo in modo descrittivo, nessun logo.
- Cybersecurity (RED/EN 18031, CRA, PSTI): spento di fabbrica, abbinamento obbligatorio con codice casuale, nessun segreto fisso, nessuna password di default, limiti di frequenza e di tempo, nessun servizio incorporato. Da aggiornare l'analisi dei rischi al prossimo rilascio.
- Privacy (GDPR, CCPA, PIPL, LGPD): il nome del dispositivo e visibile a chi e vicino: non deve contenere dati personali (lo controlla solo la forma, non il senso). La scheda non registra gli indirizzi dei telefoni e non invia nulla fuori. Le statistiche locali contano solo accensioni e collegamenti.
- Radio: potenza di trasmissione e canali sono quelli di fabbrica del chip e dell'ESP-IDF; il firmware non li cambia ne li aumenta. Da confermare con la dichiarazione del modulo.
- Export: crittografia del collegamento gia coperta come per il resto del firmware. IA, accessibilita: non applicabili. Avvisi: la guida dice che il Bluetooth e spento e lo accende solo un programma con il permesso `ble`.

## English (short)
The firmware keeps Bluetooth LE compiled in but OFF. It cannot be used by itself: no built-in service, text channel, advertising, shell command, web switch or config setting. Only a VesevOS app with `use=ble` in its manifest can turn it on, with `ble_name`, `ble_service`, `ble_adv`, `ble_on(minutes)`, `ble_set`, `ble_notify`, `ble_disconnect`, `ble_off`, `ble_status`, and the optional `on_write(uuid, data)` / `on_read(uuid)` callbacks. One program owns it at a time; it is switched off and cleaned when the program ends, fails or times out (max 60 minutes). Every read/write/notify needs an encrypted, passkey-paired link (random 6-digit code per start, shown only to the Admin with `ble_passkey()` in a script the Admin starts), one connection at a time, 10 writes/s, 128-byte values, 30 s pairing and 120 s idle timeouts, lock after 3 unpaired connections. Scripts get only `ble_status()` (plus `ble_off()` and `ble_passkey()` when started by the Admin). Examples: `docs/esempi/app/ble-stato.vab`, `docs/esempi/script/ble.vsb`, `ble-off.vsb`. Not compiled with ESP-IDF and not tested on a board yet. This is a preliminary legal note, not legal advice; rules may have changed.
