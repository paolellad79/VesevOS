# Indice del catalogo delle periferiche

Elenco dei file. L'impronta SHA-256 serve a controllare che il file non sia cambiato (oggi si controlla a mano; in futuro lo fara la scheda).
Se cambi un file, ricalcola l'impronta: `shasum -a 256 nomefile.vdp`. Il caricamento automatico da GitHub non esiste ancora (vedi LEGGIMI.md).
Aggiornato per VesevOS 1.7.61 (classe i2c, driver BMP280, sezione `[pins]` con i colori dei fili).

- ldr.vdp: Sensore di luce LDR - stato: FUNZIONA - licenza CC0 - versione 1.1
  SHA-256: ad604c3fa41c38b0e64056414175f3a0ad7bcda7d04c7192275e52d0371c1f07
- dht11.vdp: Sensore DHT11 (solo schema) - stato: SOLO SCHEMA (manca il driver DHT) - licenza CC0 - versione 1.1
  SHA-256: 5563aceb0b7780edfc7515070506f275081746e05f38bc8239f31ca1590150a0
- bmp280.vdp: Sensore BMP280 (indirizzo 0x76) - stato: FUNZIONA dalla 1.7.61, da provare su un modulo vero - licenza CC0 - versione 2.0
  SHA-256: b0b770aec4f91f4567e86c822dcff935871a83d52281eb5bb577548f07265d3d
- bmp280-77.vdp: Sensore BMP280 (indirizzo 0x77) - stato: FUNZIONA dalla 1.7.61, da provare su un modulo vero - licenza CC0 - versione 2.0
  SHA-256: ed841bfa9ea0b24dec1c34636b9192218fdd88a524f8e10b2413de10fac36ca8
