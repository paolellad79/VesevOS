# Catalogo delle periferiche VesevOS

Questa cartella e il catalogo delle periferiche: sensori e moduli che si collegano alla scheda.
Un giorno la scheda potra scaricare da GitHub solo quella che ti serve. Oggi NON e ancora cosi (vedi "Stato").

## Idea
- Una periferica = un file, per esempio `dht11.vdp`.
- Il file dice come collegarla: i pin con il loro nome vero (VDD, DATA, SDA, SCL, GND...) e il colore dei fili.
- Nella pagina di VesevOS si sceglie la periferica, si vedono i fili da collegare e la scheda propone i pin liberi.

## Formato dei file
- Si usa lo stesso formato delle schede modulo `.vdp` che esistono gia (cartella `docs/devs`). Sono solo dati, niente codice.
- Sezioni: `[module]`, `[signals]`, `[pins]` (facoltativa), `[icon]`, `[wiring]` (facoltativa). Il file deve avere lo stesso nome del campo `name=`.
- Chiavi di `[module]` usate: `name`, `title`, `class` (rele, led, pin, ingresso, analogico, i2c), `power`, `level`, `pull`, `part`, `author`, `version`, `license`, `warn`.
- Solo per `class=i2c`: `driver=` (oggi solo `bmp280`), `addr=` (0x76 o 0x77) e `freq=` (100000 o 400000, facoltativa).
  I segnali sono esattamente `SDA` e poi `SCL`. Si puo avere un solo sensore I2C per volta (un solo bus).
- In `[signals]` ci sono solo i pin che vanno su un GPIO della scheda (massimo 8). Alimentazione e massa non sono segnali.
- Le righe che iniziano con `#` sono commenti: il firmware le salta. Li usiamo per i FILI.

### Fili e colori (sezione `[pins]`)
- Ogni riga e `ETICHETTA=destinazione,colore`. L'etichetta e il nome vero del pin sul modulo (VDD, SDA, DATA...).
  La destinazione e `vcc`, `gnd` oppure uno dei segnali di `[signals]`. Massimo 8 fili; ogni segnale deve avere il suo filo.
- Il colore e un nome (`red`, `black`, `blue`, `green`, `yellow`, `orange`, `white`, `brown`, `purple`, `gray`, `pink`, `cyan`, o i nomi italiani
  come `rosso`, `nero`...) oppure `#rrggbb`. Un colore non valido fa rifiutare il file.
- La pagina disegna i fili con quei colori e scrive anche il nome del colore (nel disegno e nella tabella), per chi non vede i colori.
- Se manca `[pins]` la pagina usa i colori di sempre: i file vecchi funzionano senza cambiare nulla.
- Gli stessi fili restano anche nei commenti `#` in cima al file, per chi legge il file a mano.
- Rosso = alimentazione (3V3 o 5V). Nero = massa (GND).
- Giallo, verde, azzurro, viola = segnali (un colore per segnale, come nel disegno di esempio con DHT11, LDR, MQ-135 e BMP280).

### Pin della scheda (ESP32-S3)
- Il chip ha i GPIO 0-21 e 33-48. La SuperMini ne porta fuori meno: controlla la tua scheda.
- Da non usare: 19 e 20 (USB), 48 (LED della scheda), 0 (tasto BOOT), 3, 45, 46 (pin di avvio).
- Analogico: solo i GPIO 1-10 (gli altri disturbano il Wi-Fi).
- I numeri dei GPIO NON si scrivono nei file del catalogo: li sceglie la scheda tra quelli liberi.

### Programma Berry del driver (idea)
- Il formato `.vdp` oggi non contiene codice. Un driver potrebbe essere un file Berry accanto (`<nome>.vsc` o `.vap`).
- Non basta: il bus I2C (BMP280) ha il suo driver nel firmware (dalla 1.7.61), il DHT11 (tempi in microsecondi) ne vuole ancora uno.

## Stato (cosa funziona davvero)
- Funziona: `ldr` (classe analogico, gia supportata).
- Funziona dalla 1.7.61, ma da provare su un modulo vero: `bmp280` (indirizzo 0x76) e `bmp280-77` (indirizzo 0x77). Leggono temperatura e pressione
  (nelle app: `dev_get("nome")` = centesimi di grado, `dev_get("nome.p")` = pascal). Il chip BME280 (id 0x60) viene accettato ma misura solo temperatura e pressione, non l'umidita.
- Solo schema, NON legge niente: `dht11`. Manca nel firmware il driver DHT (un filo).
  Se lo installi dalla pagina, la scheda crea solo un pin normale: non e un errore, ma non misura. Lo dice anche l'avviso (`warn`) dentro il file.
- Il download da GitHub NON esiste ancora. Oggi i file si caricano a mano dalla pagina Periferiche, come i `.vdp` di `docs/devs`.
- Quando si fara serviranno almeno:
  - firma Ed25519 (ancora da fare) e controllo SHA-256 dell'indice;
  - consenso esplicito prima del primo scarico (privacy);
  - solo l'Admin puo installare, con conferma;
  - catalogo spegnibile e uso offline (file a mano).

## Come si aggiunge una periferica
1. Copia un file simile (`ldr.vdp` come modello) e dagli il nome nuovo: minuscole, numeri, `_` e `-`, massimo 24 caratteri.
2. Scrivi `name=` uguale al nome del file, il titolo (massimo 32 caratteri) e un avviso `warn=` (massimo 80 caratteri).
3. Metti nella sezione `[pins]` i fili con i nomi veri dei pin e i colori (e, se vuoi, anche nei commenti). Scrivi se e "FUNZIONA" o "SOLO SCHEMA".
4. Scrivi i dati con parole tue e con i dati dal datasheet. Non copiare testi o disegni protetti.
5. Licenza dei dati: CC0 (come gli esempi), salvo accordi diversi. Il codice resta GPL-3.0-or-later o commerciale.
6. Controlla il file con le prove sul computer (`tools/hosttest/run.sh`, parte `test_mod`; legge `docs/devs`: per questa cartella
   si puo provare con una copia della prova che punta a `periferiche/`).
7. Aggiungi la riga in `INDICE.md` con il suo SHA-256 (comando: `shasum -a 256 nomefile.vdp`). Se cambi il file, aggiorna l'impronta.

## Controllo legale (valutazione preliminare, non e consulenza legale; le norme possono essere cambiate)
- **Licenze:** i file del catalogo sono dati nostri con licenza CC0, compatibile con GPL-3.0-or-later e con la licenza commerciale.
  Un file di terzi entra solo con licenza chiara e compatibile (non copiare datasheet, immagini o librerie senza permesso).
- **Marchi:** i nomi dei produttori e dei componenti (DHT11, BMP280 ecc.) sono usati solo per dire a cosa serve il file, senza loghi e senza far credere in un accordo.
- **Sicurezza (RED/EN 18031, CRA):** un pacchetto scaricato e codice o dati di terzi dentro il prodotto. Ci vogliono firma, controllo SHA-256,
  installazione solo da Admin, nessun codice che parte da solo e un modo per ritirare un pacchetto sbagliato. Oggi i file sono solo dati.
- **Privacy (GDPR e simili):** scaricando da GitHub, GitHub vede l'indirizzo IP della scheda. Serve il consenso prima del primo scarico e il catalogo deve poter restare spento.
- **Accessibilita:** i colori dei fili non sono mai l'unica informazione: il disegno e la tabella scrivono anche il nome del colore e il nome del pin.
- **Sicurezza elettrica:** un collegamento sbagliato (5 V su un pin a 3,3 V) puo rovinare la scheda. Ogni file ha `warn=` e il livello del segnale (`level`). I moduli a 230 V non fanno parte di questo catalogo.
- **Brevetti, export, IA, accessibilita:** non applicabile a questi file. L'accessibilita vale per la pagina che li mostrera (testo dei fili, non solo colori).
