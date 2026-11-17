# Aiuto del comando `vesevos-backup`

Solo Admin. Salva e ripristina la CONFIGURAZIONE (rete, utenti, servizi, periferiche) in /var/backup, sempre CIFRATA con una frase che scegli tu. Senza argomenti mostra i backup, lo spazio libero e l'avvertenza.

create [nome] crea /var/backup/<nome>.vbk (la frase si chiede due volte). restore <nome> chiede la frase, dice quante righe cambierebbero (mai i valori) e chiede conferma prima di applicare. delete <nome> cancella dopo conferma. Al massimo 5 backup.

Il backup contiene password e chiavi: la frase non si scrive mai nella riga (la chiedo io, senza mostrarla) e non resta in cronologia. Se la perdi non si recupera. Tienilo dove decidi tu: e un tuo dato personale.

Stesso formato del backup con i segreti della pagina: si ripristina anche da li (scaricalo da Sistema > File) e viceversa (caricalo in /var/backup con il nome che finisce in .vbk). Nomi: lettere, numeri, - e _.

## Esempi

```
vesevos-backup
vesevos-backup create prima-della-prova
vesevos-backup restore prima-della-prova
vesevos-backup delete prima-della-prova
```
