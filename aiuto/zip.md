# Aiuto del comando `zip`

Crea un archivio .zip nuovo (formato standard, lo leggono anche i comuni programmi unzip) da file e cartelle della memoria interna. -r scende nelle cartelle (3 livelli), -j mette solo il nome del file senza le cartelle, -q non scrive i nomi. I file si archiviano sempre senza compressione (-0 si puo scrivere, -1..-9 sono rifiutati).

Non aggiorna archivi che ci sono gia (niente -u, -d, -g): senza -f rifiuta se l'archivio esiste, con -f lo ricrea da zero. Si scrive in un file temporaneo e si scambia solo alla fine. LittleFS non salva la data dei file: tutte le voci portano l'ora di sistema se e valida (NTP o impostata), altrimenti 1980-01-01 00:00.

Non entrano mai i file protetti (configurazione, registro, chiavi, certificati, copie .vbk): nominati a mano il comando fallisce, trovati dentro una cartella si saltano. Si saltano anche i nomi nascosti (.xxx, .tmp~) dentro le cartelle, e l'archivio stesso. Solo nomi relativi, ASCII, al massimo 30 voci e 1 MB per file.

Dalla pagina web 128 KB in tutto e 10 s, dalla seriale 1 MB e 120 s (Ctrl+C ferma); l'archivio deve stare nello spazio libero (meno 16 KB). Funziona uguale su tutte le schede, senza PSRAM. Niente ZIP64, cifratura o commenti. Solo Admin.

## Esempi

```
zip -r bk.zip scripts
zip -qj lingue.zip lang/en.vld lang/en.vlp
zip -f -0 tutto.zip notes.txt
```
