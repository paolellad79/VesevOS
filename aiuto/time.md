# Aiuto del comando `time`

Esegue il comando e poi scrive quanto e durato, come nelle shell classiche: real 0m1.234s.

Il tempo e misurato in millisecondi. 'user' e 'sys' non ci sono: FreeRTOS qui non li misura.

Il comando gira con la tua sessione e il tuo ruolo: 'time' non da' nessun permesso in piu.

Non si annida ('time time ls' e un errore). Senza comando scrive l'uso: per l'ora usa 'date'.

## Esempi

```
time ls /sbin
time wifi-scan
```
