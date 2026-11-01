# Indice del catalogo delle periferiche

Elenco dei file. L'impronta SHA-256 serve a controllare che il file non sia cambiato (oggi si controlla a mano; in futuro lo fara la scheda).
Se cambi un file, ricalcola l'impronta: `shasum -a 256 nomefile.vdb`. Il caricamento automatico da GitHub non esiste ancora (vedi LEGGIMI.md).
Aggiornato per VesevOS 1.7.61 (classe i2c, driver BMP280, sezione `[pins]` con i colori dei fili).

- ldr.vdb: Sensore di luce LDR - stato: FUNZIONA - licenza CC0 - versione 1.1
  SHA-256: ad604c3fa41c38b0e64056414175f3a0ad7bcda7d04c7192275e52d0371c1f07
- dht11.vdb: Sensore DHT11 (solo schema) - stato: SOLO SCHEMA (manca il driver DHT) - licenza CC0 - versione 1.1
  SHA-256: 5563aceb0b7780edfc7515070506f275081746e05f38bc8239f31ca1590150a0
- bmp280.vdb: Sensore BMP280 (indirizzo 0x76) - stato: FUNZIONA dalla 1.7.61, da provare su un modulo vero - licenza CC0 - versione 2.0
  SHA-256: de484a4d3f5d1c104737797464838ecc7a1d1f25ab93c4626b8a18877b3c8f50
- bmp280-77.vdb: Sensore BMP280 (indirizzo 0x77) - stato: FUNZIONA dalla 1.7.61, da provare su un modulo vero - licenza CC0 - versione 2.0
  SHA-256: 451e00adf6a95bf4f7b79722b1c70d2d18c729db7140894cdd44791320dde496
Dal 1.8.11 l'estensione dei file e .vdb (il firmware legge ancora i vecchi .vdp).
