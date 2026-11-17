# Aiuto del comando `lspin`

Elenca i pin come lspci elenca le periferiche: una riga per pin occupato con la periferica, il segnale e il tipo (periferica, driver, riservato dal sistema). Sola lettura: non cambia nessun pin.

-v aggiunge il livello letto adesso, le funzioni possibili e la nota; -vv mostra anche i pin liberi; -s <pin> mostra solo quel pin; -k <nome> solo i pin di una periferica o di un driver. Usa gli stessi dati della pagina Pin; 'pins' resta come elenco semplice.

## Esempi

```
lspin
lspin -v
lspin -vv
lspin -s 12
lspin -k sd1
```
