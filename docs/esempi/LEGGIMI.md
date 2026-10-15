# Esempi (demo) per VesevOS

Programmi Berry pronti da leggere, copiare e provare. Sono DEMO: servono a capire come si scrive un programma per VesevOS.

## Due tipi di programma
- **Script** (`.vsc`): solo codice Berry, senza pagina. Vive in `/scripts` sulla scheda. Ha tutte le funzioni. Lo gestisce l'Admin.
- **App** (`.vap`): un file con una pagina (pulsanti, interruttori, testi...) e il suo codice. Vive in `/apps`. Puo usare solo le funzioni dei permessi scritti in `use=`.

## Cartelle
- `script/` - esempi di script (`.vsc`).
- `app/` - esempi di app (`.vap`).

## Esempi
- `app/logmqtt.vap` - un pulsante (e un interruttore "ogni ora") che manda le ultime 10 righe del registro al broker MQTT, su `<prefisso>/app/log`. Usa i permessi `logread`, `mqtt`, `time`.
- `script/torcia.vsc` - un click del tasto BOOT accende il LED bianco, il click dopo lo spegne.

## Come si caricano sulla scheda
1. Dalla pagina: Sistema > File, cartella `/scripts` (script) oppure `/apps` (app), poi carica il file. Poi si avviano dalla pagina (le app compaiono tra le app della scheda; gli script nella sezione degli script).
2. Oppure con l'API (solo Admin): `POST /api/fs/save` con il percorso `/scripts/nome.vsc` o `/apps/nome.vap`.

## Come si provano
- Prima su una scheda di prova, mai su una scheda che non puoi recuperare.
- Gli script si avviano dalla pagina o dalla shell. Se qualcosa non va, l'errore compare con la riga del problema.
- Le app si avviano dalla pagina; un'app senza permesso per una funzione si ferma con "undeclared".
- Sul computer: `tools/hosttest/run.sh` controlla che ogni app di `app/` sia valida.

## Regole
- Solo ASCII (niente lettere accentate, niente emoji).
- Titoli ed etichette delle app: da 1 a 24 caratteri.
- Codice di un'app: massimo 8 KB. File `.vap`: massimo 16 KB, 12 controlli.
- Un ciclo senza `sleep` si ferma dopo 3 secondi: metti sempre una pausa.
- Se aggiungi un esempio nuovo, aggiungilo anche all'elenco qui sopra.

## Sicurezza
- Un'app con `mqtt` manda testi FUORI dalla scheda, al broker. Chi la usa deve sapere cosa invia (per esempio le righe del registro) e deve essere d'accordo.
- Non mettere password o dati personali nei testi mandati fuori.

## Controllo legale
- Non applicabile per le demo che restano sulla scheda.
- Privacy: le demo che mandano dati fuori (`logmqtt`) possono inviare dati personali (indirizzi IP, nomi utente nel registro). Serve il consenso di chi le usa e un broker sotto il tuo controllo. Le norme (GDPR e altre) possono essere cambiate; questo non e una consulenza legale.
