# Aiuto del comando `vesevos-update`

Solo Admin. Senza argomenti mostra versione del firmware e del recovery, le partizioni, lo spazio e l'esito dell'ultimo ordine. Non cerca aggiornamenti in rete da solo.

file [ip]: la scheda si riavvia nel recovery e accetta UN file di firmware dal computer. Stampa una sola volta l'indirizzo http://<ip>/#g=<codice>: il codice vale 15 minuti ed e legato a quel computer, non condividerlo. Dalla seriale scrivi l'IP del computer.

net <url-https> <sha256> [versione]: il recovery scarica da un indirizzo https consentito (le Release ufficiali o i prefissi di /updurl.txt) e controlla lunghezza e SHA-256. Sovrascrive il firmware prima di verificare lo SHA completo.

Chiede sempre conferma (y/n, o yes dalla pagina). Sono gli stessi ordini della pagina Aggiornamento: nessun permesso in piu. Il recovery lavora in HTTP sulla rete locale.

## Esempi

```
vesevos-update
vesevos-update file 192.168.1.20
vesevos-update net https://github.com/paolellad79/VesevOS/releases/download/v1.7.71/VesevOS-1.7.71.bin <sha256>
```
