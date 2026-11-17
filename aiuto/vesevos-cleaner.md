# Aiuto del comando `vesevos-cleaner`

Pulizia SICURA, solo Admin. Senza argomenti e una prova a secco: elenca cosa si potrebbe pulire e quanto spazio si libera, senza cancellare niente. Con -y pulisce, dopo conferma (y/n dalla seriale, yes dalla pagina).

Categorie: tmp (residui .tmp~ di salvataggi interrotti, solo se il file completo esiste), crash (rapporti dei crash), log (registro in RAM: non libera flash), reboots (diario dei riavvii), cache (file di prova /selftest.tmp). Senza nomi vale per tutte.

Non tocca MAI: configurazione, periferiche (io.cfg), utenti, certificati e chiavi, script, app, regole, lingue, registro di audit (si svuota solo con 'audit'), file protetti, cartelle intere. Non scende nelle sottocartelle.

Rapporti e registri possono contenere dati che identificano l'uso della scheda: cancellarli e un tuo diritto. Si registra una riga nel registro con le categorie e i byte liberati.

## Esempi

```
vesevos-cleaner
vesevos-cleaner tmp crash
vesevos-cleaner -y tmp
```
