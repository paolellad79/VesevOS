# Aiuto del comando `unzip`

Elenca (-l) o estrae un archivio .zip che sta nella memoria interna. Solo lettura: estrae solo voci senza compressione (store), CRC32 controllato; con -l elenca anche quelle compresse ma non le estrae. Niente ZIP64, cifratura, piu volumi o collegamenti. Per creare archivi usa zip o tar.

Estrae nella cartella corrente o in quella data con -d, mai nella radice '/'. Senza -o un file che c'e gia si salta, con -o si sostituisce. Dopo l'archivio si possono dire i nomi (o le cartelle) da estrarre.

Prima si controlla TUTTO l'archivio e se una voce e pericolosa lo si rifiuta per intero: nomi assoluti, '..' o con la barra rovesciata (zip slip), file protetti come /vesevos.conf, piu di 30 voci, piu di 1 MB per voce, 2 MB in tutto o oltre lo spazio libero, voci compresse (deflate).

Tempo e dimensione: 10 s e 128 KB dalla pagina web, 120 s e 1 MB dalla seriale (Ctrl+C ferma). Solo Admin.

## Esempi

```
unzip -l dati.zip
unzip -d /lang dati.zip
unzip -o dati.zip config.txt
```
