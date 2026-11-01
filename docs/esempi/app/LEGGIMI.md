# App di esempio (.vab)

App con pagina e codice VesevOS, per `/apps` sulla scheda. Possono usare solo i permessi scritti in `use=`. Solo ASCII, titoli ed etichette fino a 24 caratteri, codice fino a 8 KB.

- `ble-stato.vab` - Bluetooth con una caratteristica di lettura (temperatura del chip e tempo di attivita) e una di scrittura "ping" (risposta "ok"). Usa `use=ble,temp`.
- `logmqtt.vab` - manda le ultime 10 righe del registro al broker MQTT (a mano o ogni ora).

Ogni file di questa cartella viene controllato dalla prova `tools/hosttest/test_app.cpp`.
Come si caricano e si provano: vedi `../LEGGIMI.md`.
