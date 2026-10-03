<p align="center">
  <img src="assets/banner.svg" alt="VesevOS - una piattaforma, mille schede, ognuna con la sua identità" width="100%">
</p>

<p align="center">
  <img alt="Licenza" src="https://img.shields.io/badge/licenza-GPL%20v3%20%2B%20commerciale-3fa7ff">
  <img alt="Scheda" src="https://img.shields.io/badge/scheda-ESP32--S3-37e0b0">
  <img alt="Arduino" src="https://img.shields.io/badge/Arduino%20IDE-core%20esp32%203.3.x-00979d">
  <img alt="Stato" src="https://img.shields.io/badge/stato-in%20sviluppo-orange">
  <img alt="Lingue" src="https://img.shields.io/badge/lingue-it%20%C2%B7%20en%20%C2%B7%20es%20%C2%B7%20de-blueviolet">
</p>

<p align="center"><a href="README.md"><b>Italiano</b></a> · <a href="README.en.md">English</a></p>

<p align="center"><b>Un piccolo sistema operativo per ESP32-S3: semplice da usare, sicuro per impostazione, utile anche dove internet non c'è.</b></p>

---

## In due parole

Colleghi la scheda, apri una pagina nel browser e hai già un piccolo sistema operativo:
rete Wi-Fi, ora, file, LED, pin, shell e tante altre cose, tutto protetto da una password che scegli tu.
**Niente cloud, niente account, niente abbonamenti.** La pagina e le lingue vivono dentro la scheda.

<p align="center">
  <img src="assets/screenshot-pin.png" alt="La scheda Pin: pin cliccabili per fare prove" width="48%">
  <img src="assets/screenshot-stato.png" alt="La scheda Stato: CPU, memoria e temperatura" width="48%">
</p>
<p align="center"><sub>Schermate della pagina di controllo (con dati di esempio).</sub></p>

> **Stato:** in sviluppo (versione 1.5.0). Il progetto cresce a fasi: guarda "Dove stiamo andando".

## Perché si chiama VesevOS

**Vesevo** (in latino *Vesevus*) è l'antico nome del **Vesuvio**, il vulcano che da sempre veglia sul golfo di Napoli.
Ci è piaciuto perché racconta bene l'idea: un'energia sempre accesa, piccola ma affidabile, che resta al suo posto
anche quando intorno tutto tace. Da qui il vulcano nel banner.

## Missione

> **Rendere semplice e sicuro trasformare una scheda ESP32 in un dispositivo utile, anche per chi comincia adesso.**

Chi si avvicina all'elettronica spesso si ferma davanti a un muro: librerie da scegliere, configurazioni
da scrivere, sicurezza da inventare. VesevOS toglie il muro. Ti dà una base già pronta, ben fatta e
spiegata con parole semplici, su cui costruire quello che ti serve.

## Visione

> **Una piattaforma comune per mille schede, dove ognuna ha la sua identità e la sua funzione, scelta dall'utente.**

Oggi VesevOS è la **base standard**: il nucleo uguale per tutte le schede. Domani ogni scheda avrà il suo
ruolo (stazione meteo, nodo sensore, ripetitore, punto di comunicazione...) con un percorso guidato per
scegliere la funzione, collegare gli accessori "plug and play" e metterla al lavoro. Pensiamo a
schede che funzionano **anche dove la rete non arriva**: in campagna con un pannello solare, in
una situazione di emergenza, in silenzio radio quando serve, o comunicando in modi alternativi.

## Filosofia

| | Principio | Cosa significa |
|---|---|---|
| 🧭 | **Semplice prima di tutto** | Parole semplici, percorsi guidati, niente da capire per forza. Chi vuole può andare a fondo. |
| 🔐 | **Sicuro per impostazione** | La password protegge pagina, shell e API. Quello che è pericoloso chiede conferma. |
| 📴 | **Funziona senza internet** | Nessuna dipendenza dal cloud. Pagina, lingue e licenze sono dentro la scheda. |
| 🔍 | **Aperto e trasparente** | Codice leggibile, nessun tracciamento nascosto. Se un giorno servirà una statistica, sarà **solo con il tuo consenso**. |
| 🧩 | **Modulare** | Si accende solo ciò che serve: ogni servizio ha il suo interruttore e consuma solo se attivo. |
| 🌍 | **Per tutti** | Italiano, inglese, spagnolo e tedesco, e si può aggiungere una lingua con un solo file. |
| 🔋 | **Affidabile e frugale** | Pensato per reggere i blackout e per consumare poco, anche con la batteria. |

## Cosa fa oggi

- **Rete**: modo AP (la scheda crea la sua rete Wi-Fi) e modo client, IP automatico o fisso, scansione reti, nome host e dominio configurabili (`nome.local` con mDNS).
- **Pagina web** con schede: Riepilogo, Stato, Wi-Fi, Shell, Task, Ora, File, LED, Pin, Config.
- **Prova dei pin**: tocca un pin per mettere un'uscita alta o bassa, farla lampeggiare o leggerla, con avvisi sui rischi. La prova si spegne da sola.
- **Shell** web e seriale con molti comandi (digita `help`).
- **Sicurezza**: password con SHA-256 salato e ripetuto, blocco dopo 5 errori, sessioni a tempo. La password protegge pagina, API e shell.
- **Ora**: NTP, fusi orari, formati data/ora, temperatura in C o F, server NTP locale.
- **File**: memoria interna (LittleFS) con cartelle, carica/scarica/modifica.
- **CPU**: velocità automatica o fissa (80/160/240 MHz), temperatura interna, allarme se troppo calda.
- **LED**: LED RGB WS2812 (stato del sistema, battito legato al carico CPU, colore fisso) e un LED aggiuntivo.
- **Configurazione** in stile OpenWrt (`/vesevos.conf`), scaricabile e ripristinabile.
- **Lingue**: italiano nel firmware; inglese, spagnolo e tedesco come file separati (`lang/`), caricabili dalla pagina senza ricompilare.
- **Licenze e note legali** consultabili dalla pagina e dalla shell (`license`).

## Dove stiamo andando

Queste sono idee e progetti, non promesse: l'ordine può cambiare.

**Prossimi rilasci**
- [ ] **Home a "sala di controllo"**: riquadri con CPU, memoria, disco, temperatura, segnale Wi-Fi e allarmi; un clic porta alla scheda giusta. Grafica curata con icone SVG leggere, tema chiaro, scuro o automatico.
- [ ] **Scheda Rete** unica (Wi-Fi, IP, nome, punto di accesso) e **Task** dinamico e ordinabile.
- [ ] **Firewall** semplice: blocco dei tentativi sbagliati, regole per indirizzi e limiti alle connessioni.
- [ ] **Pin avanzati**: misure di tensione e frequenza, PWM, correzione dei valori con il tester, animazione dei pin.
- [ ] **Shell web** più comoda (cronologia, completamento, colori).
- [ ] **Aggiornamento del firmware dalla pagina** (OTA).

**Più avanti**
- [ ] **Prima configurazione guidata** (lingua, password, paese, Wi-Fi, ora).
- [ ] **Scheda SD** e **app Lua** protette in una "gabbia" sulla SD.
- [ ] **Catalogo di accessori plug and play**: scegli il sensore dall'elenco e VesevOS ti mostra come collegarlo e installa il necessario.
- [ ] **Comunicazione tra schede** (ESP-NOW, reti mesh) e modi alternativi per le emergenze.
- [ ] **Modalità silenzio radio** e **risparmio energetico** per l'uso con pannello solare.
- [ ] Server **SSH**, MQTT, HTTPS.

## Requisiti

- Scheda **ESP32-S3 SuperMini** (ESP32-S3FH4R2: 4 MB flash, 2 MB PSRAM).
- Arduino IDE con core **esp32 3.3.x** o piu recente.
- Librerie: **ESP32Async ESPAsyncWebServer** e **AsyncTCP** (ESP32Async).

## Impostazioni Arduino IDE

| Voce | Valore |
|---|---|
| Scheda | ESP32S3 Dev Module |
| PSRAM | QSPI PSRAM |
| USB CDC On Boot | Enabled |
| Partition Scheme | Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS) |

## Installazione

1. Scarica o clona il repository.
2. Apri la cartella `VesevOS/` (deve contenere `VesevOS.ino`) con Arduino IDE.
3. Imposta la scheda come nella tabella, poi carica lo sketch.
4. Apri il monitor seriale a 115200 per vedere i messaggi di avvio.

## Primo accesso

1. Collegati alla rete Wi-Fi **VesevOS** (password iniziale `vesevos123`).
2. Apri `http://192.168.4.1`.
3. Scegli la password della scheda (minimo 6 caratteri). Vale anche per la shell seriale.

**Password dimenticata:** tieni premuto il pulsante **BOOT** per 8 secondi a scheda gia accesa.
Non tenerlo premuto all'accensione: la scheda entrerebbe in modo download.

## Colori del LED RGB (modo "Stato")

| Colore | Significato |
|---|---|
| Arancio | Avvio |
| Blu | Modo AP (rete propria) |
| Giallo | Sto collegandomi al Wi-Fi |
| Verde | Collegata al Wi-Fi |
| Rosso lampeggiante | Allarme (temperatura alta) |

## Organizzazione del repository

```
VesevOS/      sketch Arduino (VesevOS.ino + file .h/.cpp)
web/          pagina web in HTML (sorgente di vos_page.h)
assets/       banner e schermate del README
lang/         file di lingua pronti da caricare (en, es, de) - GENERATI da tools/lang_src.json
licenses/     testi delle licenze (GPL, LGPL, Apache) e modello della nota legale
tools/        mkpage.py (pagina), mklang.py (lingue), mklicense.py (testi legali)
NOTICE.txt    note legali: titolarita, licenze, riferimenti normativi
CHANGELOG.md  cronologia delle versioni
```

Se modifichi `web/index.html`, rigenera la pagina con:

```
python3 tools/mkpage.py
```

I sorgenti usano solo caratteri ASCII (le lettere accentate nella pagina sono entita HTML).

## Lingue

L'italiano e dentro il firmware. Per le altre lingue:

1. Apri la pagina, scheda **Config > Lingue**.
2. Scegli il file (`en.json`, `es.json` o `de.json` dalla cartella `lang/`) e premi **Carica lingua**.
3. Scegli la lingua dal menu in alto a destra. La scheda la ricorda; da shell: `lang en`.

Ogni lingua occupa circa 17 KB della memoria interna. **Per aggiungere una lingua**: copia una
voce di `tools/lang_src.json`, aggiungi la colonna con il nuovo codice in `langs` e nelle voci,
poi esegui `python3 tools/mklang.py`. Il controllo avvisa se manca qualche testo.

## Contribuire

Le idee, le prove sulla scheda e le correzioni sono benvenute. Anche una traduzione: basta un file in `lang/`.
Per il codice leggi `CONTRIBUTING.md`.

## Licenza

Doppia licenza:

- **GPL v3 o successiva** (file `LICENSE`): gratuita. Chi distribuisce VesevOS modificato, o dentro un
  prodotto, deve pubblicare il proprio codice con la stessa licenza.
- **Licenza commerciale**: per usare VesevOS in prodotti chiusi. Vedi `COMMERCIAL.md`.

Titolarità, licenze delle librerie di terzi e riferimenti normativi: vedi `NOTICE.txt` (anche nella
pagina, scheda Config > Licenze, e nella shell con `license`).

Copyright (C) 2026 Domenico Paolella.
