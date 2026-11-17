# Aiuto del comando `vesevos-drive_manager`

Elenca i volumi: memoria interna (LittleFS, su /) e microSD se la periferica 'sd' e configurata. Per ognuno stato (montata, non montata, non presente, formato non supportato), formato, dimensione, usato, libero e, per la microSD, la frequenza SPI.

mount, unmount e test (solo Admin) passano dal comando 'sd' del driver microSD, con gli stessi controlli; senza il driver dicono che manca. unmount rifiuta se la scheda e in uso. status e come l'elenco (l'Admin vede anche 'sd status').

format microsd FORMATTA [--attendi] (solo Admin) formatta la microSD in FAT32 e CANCELLA TUTTO; senza la parola dice cosa succede e non fa niente. La formattazione parte in un task a parte e il comando torna subito: si segue con 'sd status' o con la riga della microSD in 'vesevos-drive_manager'. Con --attendi il comando aspetta (al massimo 60 secondi) e scrive il tempo trascorso. Funziona anche con una scheda che non si monta (exFAT, vuota). La memoria interna non si formatta. Se la microSD non e configurata, installa il driver microSD dalla pagina Driver.

## Esempi

```
vesevos-drive_manager
vesevos-drive_manager mount
vesevos-drive_manager unmount
vesevos-drive_manager format microsd FORMATTA [--attendi]
```
