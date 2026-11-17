# Aiuto del comando `run`

Avvia uno script (.vsb) o un'app (.vab) per nome o per percorso, come nelle vecchie shell a riga di comando. L'estensione si puo omettere: si prova .vsb e poi .vab.

Un nome senza barre si cerca nella cartella corrente, poi in /bin, in /sbin e nelle vecchie /apps e /scripts. Con una barra (prova.vsb, ./prova, /sbin/prova) si prova solo quel percorso.

Si puo anche scrivere solo il nome ('prova'): se non e un comando interno parte il programma. I comandi interni vincono sempre; con 'which' vedi cosa partirebbe.

Con '?' dopo il nome ('run prova ?') il programma non parte: mostra nome, versione, autore e descrizione. Il '?' e riservato a questo.

Per ora gli script partono da /sbin e le app da /bin, e gli argomenti dopo il nome non vengono ancora passati. Stessi permessi di 'script run' e 'app start': solo Admin.

## Esempi

```
run prova
run prova.vsb
run /sbin/prova
prova
run prova ?
```
