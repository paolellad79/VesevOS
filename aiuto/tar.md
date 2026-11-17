# Aiuto del comando `tar`

Crea (-c), elenca (-t) o estrae (-x) un archivio tar ustar nella memoria interna. -f indica l'archivio (obbligatorio), -C la cartella, -v scrive i nomi. Anche unito: tar -xf a.tar, tar xvf a.tar.

Solo il contenitore, senza compressione (i .tar.gz non si aprono: decomprimili sul computer). Solo file e cartelle, nomi fino a 100 caratteri, permessi fissi, nessuna data dentro l'archivio.

Stesse protezioni e tetti di unzip: nomi pericolosi rifiutati, file protetti, 64 voci, 1 MB per voce. Estraendo un file che c'e gia si salta: --overwrite lo sostituisce (il tar classico lo sostituisce sempre).

Creando: 3 livelli di cartelle e 128 KB in tutto dalla pagina web (1 MB dalla seriale; Ctrl+C ferma). Solo Admin.

## Esempi

```
tar -cf bk.tar scripts
tar -tvf bk.tar
tar -xf bk.tar -C /ripristino
```
