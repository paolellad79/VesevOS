# Aiuto del comando `vesevos-elf`

Moduli ELF (.vae): codice nativo caricato a richiesta. Lettura (status, ps, which, list, quarantine): Operatore; il resto: Admin. Funziona con il sottosistema vde acceso (di fabbrica lo e; vesevos-services vde start|stop).

run [-t secondi] [-u gruppi] <nome|file> [argomenti]: avvia il modulo in sottofondo e torna subito. Un nome semplice e il file /bin/<nome>.vae; un percorso puo stare in /bin, /apps, /devs o sulla microSD. Senza -u concede i gruppi di funzioni che il modulo chiede; con -u solo quelli indicati (per esempio log,sd); il gruppo base e sempre concesso. Le Funzionalita tolte in Sistema valgono comunque. -t ferma il modulo dopo quei secondi; senza -t gira finche non finisce o non lo fermi. Al massimo 4 moduli insieme, 128 KB di RAM ciascuno.

ps: moduli in corso e ultimi finiti. kill <numero|nome>: ferma. which <nome|file>: dove sta e se e valido. list: i .vae in /bin. install <file>: controlla e mette in /bin. quarantine / unquarantine <nome>: elenco e rimozione; un modulo che fa cadere la scheda 3 volte di fila non parte piu da solo. driver list|info|install|remove: driver in ELF (.vde) in /devs; install <file|nome> [pin,pin] da' i pin: i tuoi o, senza, quelli proposti dal firmware (stesso controllo di tutte le periferiche); remove toglie pin e file.

test [-t secondi] [-u gruppi] <file> [argomenti]: prova in primo piano (tetto 1-30 s, di solito 5) e scrive valore restituito, tempo e memoria. Se un modulo cade, la scheda si riavvia.

## Esempi

```
vesevos-elf run -u log,sd sensore
vesevos-elf driver install bmp1.vde 21,22
vesevos-elf ps
vesevos-elf kill sensore
vesevos-elf install /media/sd/sensore.vae
vesevos-elf test -t 10 -u log prova.vae uno due
```
