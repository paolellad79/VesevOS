# App di esempio (.vap)

App con pagina e codice Berry, per `/apps` sulla scheda. Possono usare solo i permessi scritti in `use=`. Solo ASCII, titoli ed etichette fino a 24 caratteri, codice fino a 8 KB.

- `logmqtt.vap` - manda le ultime 10 righe del registro al broker MQTT (a mano o ogni ora).

Ogni file di questa cartella viene controllato dalla prova `tools/hosttest/test_app.cpp`.
Come si caricano e si provano: vedi `../LEGGIMI.md`.
