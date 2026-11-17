# Aiuto del comando `df`

Mostra lo spazio dei dischi: la memoria interna (LittleFS, montata su /) e, se montata, la microSD.

Colonne come df: filesystem, dimensione in blocchi da 1K, usato, disponibile, percentuale d'uso e dove e montato.

-h scrive le dimensioni in K, M o G. La microSD non e dentro l'albero delle cartelle: la gestisce il driver microSD (comando 'sd').

## Esempi

```
df
df -h
```
