# VesevOS - pagina per la prima installazione

Pagina statica (solo HTML, CSS, JavaScript) da pubblicare su GitHub Pages.
Scrive il firmware VesevOS su una scheda ESP32-S3 dal browser, con la porta seriale del browser (Web Serial) e `esptool-js`.
Progetto separato dal firmware. Licenza: GPL-3.0-or-later (licenza commerciale alternativa: vedi `COMMERCIAL.md` del repo). Copyright 2026 Domenico Paolella.

**STATO: NON PROVATA con una scheda reale.** Controllata solo la sintassi JavaScript (`node --check`).
Da provare con una scheda sacrificabile (vedi "Cosa va provato").

## Cosa fa
- Elenco dei file ufficiali in `firmware.json` (versione, indirizzo, dimensione, SHA-256).
- Prima di abilitare "Scrivi", per OGNI file: (a) primo byte 0xE9 (o contenuto giusto per tabella, memoria vuota, selezione avvio),
  (b) marchio `{FW:VesevOS}` nel firmware (nel recovery: marchio "VesevOS Recovery" e NON quello del firmware),
  (c) dimensione nello slot (firmware al massimo 2.424.832 byte, recovery al massimo 1.048.576 byte, bootloader 32 KiB, ecc.),
  (d) SHA-256 (con `crypto.subtle`) uguale a quello dell'elenco.
- Si scrivono SOLO i file scaricati da `bin/` e presenti in `firmware.json`. Non c'e scelta di file dal disco.
- Indirizzi e dimensioni massime stanno nel codice (`controlli.js`), non nell'elenco: un elenco sbagliato non puo mettere un file nel posto sbagliato.
- Se il browser non ha la porta seriale: la pagina mostra solo "Browser non compatibile".
- Lingue it/en/es/de (testi in `lang.js`), tema chiaro/scuro con `prefers-color-scheme`, barra di avanzamento, registro delle fasi con `aria-live`.
- Nessun upload, nessun tracker, nessun cookie. Tutto con `Content-Security-Policy` che ammette solo file dello stesso sito.
- Nota in pagina: gli aggiornamenti successivi si fanno dalla scheda (recovery); la pagina serve solo per la prima installazione o il recupero.

## File
| File | Scopo |
|---|---|
| `index.html`, `stile.css` | pagina e stile |
| `app.js` | interfaccia, elenco, controlli, avvio della scrittura |
| `controlli.js` | regole (indirizzi, dimensioni, marchi) e SHA-256 |
| `scrittura.js` | collegamento alla scheda e scrittura con esptool-js |
| `lang.js` | testi in 4 lingue |
| `firmware.json` | elenco dei file ufficiali con SHA-256 |
| `bin/` | COPIE dei file ufficiali (gli originali restano in `consegna/`) |
| `vendor/` | `esptool-js` 0.5.7 (bundle) e la sua licenza Apache-2.0 |

## Pacchetti
La scheda ha la tabella: `nvs` 0x9000 (20 KiB), `otadata` 0xe000 (8 KiB), recovery 0x10000 (max 1 MiB), `app0` 0x110000 (max 2.424.832 byte), spazio file 0x360000.

- **Installazione completa (scheda nuova)**, scelta predefinita. Scrive 6 file:
  | Indirizzo | File | Note |
  |---|---|---|
  | 0x0 | `bootloader-1.8.0.bin` | dal build del firmware 1.8.0 (con memoria RTC riservata) |
  | 0x8000 | `partition-table-1.8.0.bin` | uguale nei build di firmware e recovery |
  | 0x9000 | `nvs-vuoto.bin` | 20 KiB di 0xFF: NIENTE dati di nessuna scheda |
  | 0xe000 | `otadata-app0.bin` | primo avvio direttamente su `app0` (stesso contenuto che ha una scheda in uso, solo numero di sequenza e CRC) |
  | 0x10000 | `VesevOS-Recovery-0.5.1.bin` | recovery |
  | 0x110000 | `VesevOS-1.8.0.bin` | firmware |
- **Solo firmware** (0x110000) e **Solo recovery** (0x10000): per recuperare una scheda con tabella e avvio gia buoni.
- Scelta fatta: file separati, uno per parte, ognuno con il suo SHA-256 e i suoi controlli, al posto di una immagine unica da 4 MB.
  Motivi: si riusano i binari ufficiali senza copie "fuse"; ogni controllo (marchio, slot) e chiaro; niente 4 MB quasi tutti vuoti; il file con i dati di una scheda non esiste.
- NON si scrive: lo spazio file (0x360000, LittleFS). Una scheda usata puo quindi tenere `/vesevos.conf` e utenti; una scheda nuova non ha nulla.
  Il pacchetto completo cancella invece la memoria `nvs` (impostazioni Wi-Fi ecc.).
- Mai usare un backup (`backup-*.bin`) come origine: contiene dati personali (Wi-Fi, utenti, chiavi, certificati).

## Come pubblicare su GitHub Pages
1. Crea un repository separato (es. `vesevos-installa`) e copia dentro il CONTENUTO di questa cartella (non la cartella stessa): `index.html`, `*.js`, `stile.css`, `firmware.json`, `bin/`, `vendor/`, `README.md`.
2. Su GitHub: Settings > Pages > "Deploy from a branch" > ramo `main`, cartella `/ (root)`.
3. Aspetta qualche minuto e apri l'indirizzo `https://<utente>.github.io/<repo>/`. Serve HTTPS (anche per `crypto.subtle` e per la porta seriale).
4. Apri la pagina: deve scaricare `firmware.json` e i file senza errori; i controlli devono diventare tutti OK.
5. Non aprire `index.html` dal disco (`file://`): il browser blocca il download dell'elenco.

## Come aggiornare `firmware.json` e gli SHA (nuova versione)
1. Copia i nuovi binari in `bin/` con il numero di versione nel nome (esempio `VesevOS-1.8.1.bin`). Per un'installazione completa servono anche bootloader e tabella dello stesso build.
2. Calcola gli SHA-256 e le dimensioni: `shasum -a 256 bin/*` e `ls -l bin/`. Devono coincidere con i file `.sha256` accanto ai binari in `consegna/`.
3. In `firmware.json`: aggiungi le voci in `parti` (tipo, file, indirizzo, dimensione, sha256) e un nuovo pacchetto in `pacchetti`.
   Le voci vecchie possono restare: il downgrade e possibile (decisione del 22/10/2026, da rivedere prima del rilascio ufficiale).
   Gli indirizzi devono essere quelli di `controlli.js` (se non lo sono, la pagina rifiuta il file).
4. Cambia `aggiornato` e fai `git push` (a cura del proprietario).
5. Pulizia: togli dal repo della pagina le versioni che non vuoi piu offrire.
Script rapido per rigenerare dimensioni e SHA: vedi il commento in fondo a questo file.

## Dubbi aperti
- Bootloader: scelto quello del build del firmware 1.8.0 (con RTC riservata, `FLASHMODE` QIO nella configurazione ma intestazione DIO nel file); quello del recovery e diverso (1.200 byte meno). Da confermare su scheda.
- Reset automatico dell'USB nativa: `esptool-js` usa il reset dedicato (PID 0x1001), ma il riavvio finale (`hard_reset`) potrebbe non partire da solo: la pagina dice di ricollegare il cavo o premere reset.
- Velocita: per l'USB nativa il valore della velocita non conta, ma `esptool-js` cambia comunque velocita dopo il collegamento (460800): da vedere se serve abbassarla.
- Scrittura con compressione e "keep" per modo/frequenza/dimensione flash: l'intestazione resta quella del file.
- Il controllo MD5 di `esptool-js` dopo la scrittura NON e attivo (manca la funzione MD5): il controllo e lo SHA-256 prima di scrivere.
- Il bundle di `esptool-js` e Apache-2.0 (non MIT), con dentro pako (MIT AND Zlib). Altre parti incluse: da verificare nel `package.json` dello strumento.

## Cosa va provato (prima di dire che funziona)
Con una scheda SACRIFICABILE (non quelle in uso) e con il backup a portata di mano:
1. Collegamento e reset automatico su tre sistemi (Windows, Mac, Linux) con Chrome/Edge: scheda in uso, scheda spenta e scheda che non parte.
2. Installazione completa su scheda nuova, poi avvio: la scheda deve partire su VesevOS 1.8.0; recovery raggiungibile.
3. Solo firmware e solo recovery su scheda gia installata.
4. Rifiuti: file modificato di 1 byte, elenco con SHA sbagliato, firmware messo come recovery, sito non raggiungibile, pagina aperta da disco.
5. Browser senza porta seriale: deve comparire solo "Browser non compatibile".
6. Lettori di schermo e tastiera; telefono (la porta seriale su telefono spesso non esiste: atteso "Browser non compatibile").

## Controllo legale (valutazione preliminare, NON consulenza legale; le norme potrebbero essere cambiate)
- **Licenze**: `esptool-js` 0.5.7 e Apache-2.0 (verificato nel `package.json` della versione scaricata), non MIT. La licenza e in `vendor/LICENSE-esptool-js-Apache-2.0.txt`.
  Apache-2.0 e compatibile con GPL-3.0; compatibile anche con la licenza commerciale alternativa del progetto (da riverificare). Il bundle include pako (MIT AND Zlib).
  Citare esptool-js in `NOTICE.txt` e nell'SBOM del progetto. Nessun codice da CDN: tutto locale.
- **Privacy**: nessun dato esce dal browser; nessun cookie, tracker o upload. Con GitHub Pages il server di GitHub vede comunque indirizzo IP e richieste di pagina (GDPR e altre leggi): dirlo nell'informativa del progetto. Non c'e nessun identificativo della scheda inviato.
  I file in `bin/` non contengono dati personali (memoria `nvs` vuota; `otadata` senza dati).
- **Sicurezza** (RED/EN 18031, CRA): lo SHA-256 e preso dallo STESSO sito dei file: protegge da file rovinati, NON da un sito compromesso. L'autenticita vera richiede la firma Ed25519 dell'elenco (fase futura, `SIC-002`/`SIC-011`). Secure Boot non e toccato.
  "Solo file ufficiali" e una scelta di questa pagina, non un divieto tecnico: chi vuole puo usare `esptool` a mano.
- **Marchi**: i testi della pagina non nominano marchi di terzi (browser, sistemi, produttori). Nel codice restano solo i nomi tecnici necessari ai controlli. Il marchio VesevOS va usato solo per i file ufficiali.
- Export, IA, radio: non applicabile. Accessibilita: etichette, focus visibile, contrasto, `aria-live`; non verificata con lettori di schermo.

## Rigenerare dimensioni e SHA (comando)
```
python3 - <<'PY'
import json, hashlib
j = json.load(open('firmware.json'))
for p in j['parti'].values():
    d = open(p['file'], 'rb').read()
    p['dimensione'], p['sha256'] = len(d), hashlib.sha256(d).hexdigest()
open('firmware.json', 'w').write(json.dumps(j, indent=1) + '\n')
PY
```
