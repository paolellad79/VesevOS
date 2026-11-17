# Aiuto del comando `vesevos-driver`

Una sola famiglia di comandi per i driver delle periferiche in /devs, sia vdb sia vde: il tipo si capisce dall'estensione del file o dal nome. list elenca, info mostra i dettagli.

install <file|nome> [prefisso] [pin,pin] (Admin) mette il file in /devs, se serve, e crea le periferiche sui pin che scegli tu o, senza, su quelli proposti; il controllo dei pin (vietati, gia usati, doppi) e quello del registro dei pin. Un nome vale per un solo driver.

pins <nome> pin,pin (Admin) da' altri pin a un driver vde installato: se i nuovi non vanno restano i vecchi e dice quale pin e occupato e da chi. remove <nome> (Admin) toglie il driver: un vde libera i pin, un vdb lascia le periferiche gia create (si tolgono da Periferiche).

## Esempi

```
vesevos-driver
vesevos-driver info sd-spi
vesevos-driver install sd-spi 10,11,12,13
vesevos-driver pins bmp280 21,22
vesevos-driver remove microsd
```
