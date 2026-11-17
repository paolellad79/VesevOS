# Aiuto del comando `vesevos-config`

Fronte unico di configurazione per aree: system, network, region, time, services, users, security, led, cpu. Senza argomenti scrive un riepilogo di tutto (mai password, hash o chiavi).

vesevos-config <area> mostra i valori dell'area e i comandi che li cambiano. vesevos-config <area> <comando ...> esegue quel comando (per esempio region country IT).

Non e un modo nuovo di cambiare le impostazioni: la riga passa dal percorso normale della shell, con gli stessi ruoli, limiti della radio, conferme e domande di password. setup avvia la guida.

L'Operatore vede il riepilogo e le aree in lettura (users, security e setup sono solo per l'Admin); cambiare e solo per l'Admin.

## Esempi

```
vesevos-config
vesevos-config region
vesevos-config region country IT
vesevos-config network wifi set casa
vesevos-config setup
```
