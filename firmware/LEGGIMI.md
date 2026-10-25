# VesevOS - cartella firmware

Qui ci sono i file gia compilati, pronti all'uso, per la scheda ESP32-S3 SuperMini.
Valutazione e avvisi: questo testo NON e consulenza legale; le norme citate potrebbero essere cambiate.

## Cosa c'e
| Cartella | Contenuto |
|---|---|
| `1.8.0b/` | firmware VesevOS 1.8.0b (`.bin` + `.sha256`) |
| `recovery/0.5.1/` | Recovery 0.5.1 (`.bin` + `.sha256`) |
| `recovery/0.5.0/` | Recovery 0.5.0 (`.bin` + `.sha256`) |
| `indice.json` | elenco leggibile da programmi: versione, data, dimensione, SHA-256, indirizzo del file |

Sono pubblicate solo versioni costruite con ESP-IDF 5.5 (senza Arduino). Il file `.elf` non si pubblica.
Il programma di avvio (bootloader), la tabella delle partizioni e i file vuoti della prima installazione stanno nella pagina `flash/`.

## Elenco `indice.json`
Per ogni voce: `id`, `type` (`firmware` o `recovery`), `version`, `channel`, `publisher`, `date`, `size`, `sha256`,
`file` (percorso dentro `firmware/`), `url` (indirizzo "raw" su GitHub), `board`, `min_recovery` (se noto), `notes`.
Indirizzo dell'elenco: `https://raw.githubusercontent.com/paolellad79/VesevOS/main/firmware/indice.json`

IMPORTANTE (onesta): `channel: "official"` e `publisher: "Domenico Paolella"` sono SOLO scritte nell'elenco. Non c'e una firma digitale (per ora non si fa).
Chi controllasse il sito, o ne facesse una copia, potrebbe scrivere la stessa cosa su un altro file. Lo SHA-256 dice che il file non e rotto,
NON dice chi lo ha fatto. Scarica solo da questo repository, controlla l'indirizzo e non fidarti di copie.

## Controllare lo SHA-256 prima di usare un file
Dalla cartella del file:
- macOS: `shasum -a 256 VesevOS-1.8.0b.bin`
- Altri sistemi: `sha256sum VesevOS-1.8.0b.bin`
- Controllo automatico: `sha256sum -c VesevOS-1.8.0b.bin.sha256` (su macOS: `shasum -a 256 -c VesevOS-1.8.0b.bin.sha256`)
Il numero deve essere uguale a quello del file `.sha256` e a quello di `indice.json`. Se e diverso, NON usare il file.

## Come caricare
Prima di tutto: fai un BACKUP dei dati della scheda (pagina Sistema, oppure copia dei file). Una scheda usata ha password, Wi-Fi e chiavi.

### A) Scheda nuova: prima installazione dalla pagina
- Apri la pagina `flash/` (GitHub Pages, vedi `README.md`), con un browser che ha la porta seriale (Web Serial), collega la scheda e scegli "Installazione completa".
- La pagina controlla ogni file (marchio, dimensione, SHA-256) prima di scrivere.
- La pagina e NON PROVATA con una scheda reale: usa prima una scheda di prova.

### B) Scheda gia con VesevOS: aggiornamento dal recovery
- Menu "Aggiorna tramite recovery" (solo Admin). Il recovery fa il lavoro, nessuna password passa al recovery.
- Modo 2: file dal computer (`.bin` scaricato da qui), con gettone monouso.
- Modo 1: da internet, a mano: dai l'indirizzo (`url` di `indice.json`) e lo SHA-256; il recovery scarica e controlla. Serve il Wi-Fi di casa. L'aggiornamento automatico NON c'e: l'online e sempre una tua scelta.
- Le richieste a GitHub rivelano il tuo indirizzo IP a chi ospita i file.

### C) Con esptool (dal computer, cavo USB)
Gli indirizzi sono questi (tabella della scheda):
- Recovery: `0x10000` (massimo 1 MiB)
- Firmware: `0x110000` (massimo 2.424.832 byte)
Esempio (installa prima esptool, vedi la guida `docs/GUIDA-PREPARARE-LA-SCHEDA.md`):
`esptool.py --chip esp32s3 write_flash 0x110000 VesevOS-1.8.0b.bin`
Non cancellare tutta la memoria se vuoi tenere le impostazioni. Per una scheda nuova servono anche bootloader (0x0), tabella (0x8000), nvs (0x9000) e otadata (0xe000): usa la pagina `flash/`.

### D) Ripristino
- Scheda che non parte ma con il recovery vivo: carica di nuovo il firmware dal recovery (B).
- Scheda che non parte e senza recovery: A) o C) con un file di questa cartella.
- Ripristino dei dati: dal backup che hai fatto prima. Non usare mai un backup completo preso da un'altra scheda: contiene dati personali.

## Avvisi
- Il downgrade (mettere una versione piu vecchia) e possibile e non e bloccato: puo non leggere le impostazioni nuove.
- Il recovery e solo in italiano e solo HTTP (nessun TLS): usalo in una rete di casa di cui ti fidi.
- Nessuna garanzia (GPL-3.0, sezioni 15-16). Licenza: GPL-3.0-or-later oppure licenza commerciale (`COMMERCIAL.md`). Note legali: `NOTICE.txt`.
- Aggiornamenti senza firma digitale: integrita si (SHA-256), autenticita no. Vedi `SECURITY.md`.
