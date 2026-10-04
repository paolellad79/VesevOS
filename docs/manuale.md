# VesevOS 1.7.2 - Manuale

Manuale per chi usa la scheda. Parole semplici, niente programmazione.
English version: [manual.en.md](manual.en.md). Lo stesso aiuto e nella pagina della scheda (Sistema > Aiuto, oppure `http://192.168.4.1/#aiuto`).

> Non e consulenza legale. Le leggi citate servono a spiegare perche la scheda funziona cosi.

## 1. Primo accesso a una scheda nuova

### Perche la password del Wi-Fi e casuale (e non scritta qui)
Ogni scheda VesevOS, la prima volta che si accende, inventa da sola una password Wi-Fi diversa da tutte le altre.

- **Una password uguale per tutti non protegge nessuno.** Se tutte le schede avessero la stessa password, basterebbe leggerla
  in questo manuale per entrare in qualsiasi scheda VesevOS: quella del vicino, quella in un negozio, la tua.
- **La legge lo vieta.** In Europa (direttiva radio 2014/53/UE con la norma EN 18031, regolamento Cyber Resilience Act)
  e nel Regno Unito (legge PSTI) i dispositivi collegati non possono avere password iniziali uguali per tutti o facili da indovinare.
- **Solo chi ha la scheda in mano la conosce.** La password si legge solo con il cavo USB (o dall'etichetta che prepari tu).
- **Non e ricavata dal MAC o dal numero di serie**: quei dati si vedono via radio, chiunque potrebbe ricalcolarla.
- **Non e nel nome della rete**: il nome lo vedono tutti quelli vicini, sarebbe come attaccare la chiave alla porta.
- **Dopo la configurazione la puoi cambiare** (Servizi > Punto di accesso), anche il nome della rete.

### Metodo 1 (consigliato): cavo USB e monitor seriale
1. Carica VesevOS sulla scheda (Arduino IDE > Carica).
2. Apri Strumenti > Monitor seriale, velocita 115200, e premi RESET sulla scheda.
3. Leggi la schermata di benvenuto: nome della rete (`VesevOS`), password (es. `k7Hm-pQ4x-Tr9a`) e indirizzo `http://192.168.4.1`.
   Vuoi l'inglese? Premi `2` (senza Invio). Per tornare all'italiano premi `1`.
4. Collegati con telefono o PC alla rete `VesevOS` con quella password. La pagina si apre da sola; altrimenti apri `http://192.168.4.1`.
5. **Scegli la password del pannello** (almeno 6 caratteri). Non esiste una password di fabbrica: la scegli tu e la conosci solo tu.
6. Parte la **configurazione guidata** (il LED fa l'arcobaleno finche non hai finito). I passi: lingua, paese sulla mappa, nome della
   scheda, antenna, password dell'hotspot, **Wi-Fi di casa** (facoltativo), **ora**, fine. Vedi il capitolo 12.
7. Il **QR** per collegare il telefono (e per stamparlo) e nella pagina **Home**: in modo hotspot e il QR della Wi-Fi della scheda,
   quando la scheda e collegata alla tua Wi-Fi e il QR con l'indirizzo della scheda. Stampa l'etichetta e attaccala sotto la scheda.
   Il banner con la password dell'hotspot sulla seriale resta finche la guida non e finita (anche se riapri il monitor).

Senza Arduino IDE va bene qualsiasi programma per porta seriale (PuTTY, screen, app "Serial USB Terminal" con cavo OTG).
Il QR non viene disegnato sulla seriale (nei monitor seriali spesso non si legge): e solo nella pagina Home. La configurazione si puo fare anche solo da seriale (capitolo 13).

### Metodo 2: scheda preparata da un altro
Chi la prepara legge la password come nel Metodo 1 e consegna la scheda con l'etichetta (nome rete + password + QR).
Chi la riceve, al primo accesso, sceglie la password del pannello e, se vuole, cambia anche quella del Wi-Fi.

## 2. Password perse e tasto BOOT

| Tasto BOOT (tieni premuto, poi lascia) | Colore del LED | Cosa fa |
|---|---|---|
| meno di 2 secondi | - | esce dal modo aereo |
| da 2 a 7 secondi | azzurro | spegne il filtro IP (se ti sei chiuso fuori) |
| da 8 a 19 secondi | giallo | azzera le password degli amministratori e scrive la password Wi-Fi sulla seriale |
| 20 secondi o piu | rosso | ripristino di fabbrica: cancella tutto, nuova password Wi-Fi |

- L'azione parte quando **lasci** il tasto: guarda il colore e lascia al momento giusto.
- Password del Wi-Fi: pannello > Home (QR, solo in modo hotspot) oppure Servizi > Punto di accesso > Mostra. Oppure cavo USB + BOOT 8 secondi.
- Password del pannello: la scheda conserva solo un'impronta, non si puo rileggere. BOOT 8 secondi la azzera; entro 10 minuti
  ne scegli una nuova (dall'hotspot della scheda in qualsiasi momento).

## 3. Accesso, utenti e ruoli
- Fino a 8 utenti (Sicurezza > Utenti). **Amministratore**: tutto. **Operatore**: usa la scheda (LED, pin, automazioni, messaggi)
  ma non cambia rete, sicurezza e utenti; nel terminale ha solo comandi sicuri. **Ospite**: guarda Home, stato e registro.
- La password non viaggia mai: la pagina manda solo una prova calcolata (numero casuale + impronta). Dopo troppe password
  sbagliate lo stesso indirizzo viene bloccato per un po' (Sicurezza > Password).
- La seriale USB e protetta da password (consigliato lasciarla attiva).

## 4. HTTPS (pagina cifrata)
Dalla rete di casa la pagina si apre in `https://`. Il certificato e creato dalla scheda e unico: la prima volta il browser
avvisa "connessione non privata". E normale per i dispositivi di casa: confronta l'impronta mostrata dal browser con quella
della pagina (Servizi > HTTPS) o della seriale, poi prosegui. Dall'hotspot della scheda la pagina resta in `http://`
perche la rete e gia cifrata dalla password Wi-Fi. Puoi caricare un certificato tuo (vale dal riavvio successivo).

## 5. Paese, radio e localizzazione
Sistema > Localizzazione: paese (mappa del mondo con zoom), lingua, fuso orario, server dell'ora, formati di data e ora,
separatore dei decimali, primo giorno della settimana, gradi C/F, antenna e potenza.
- **Il paese decide i canali Wi-Fi e la potenza massima** secondo le sue regole: la scheda non li supera.
  Senza paese valgono le regole piu prudenti (canali 1-11).
- Antenna esterna: indica il guadagno (dBi) e la potenza scende da sola per restare nel limite. Montarla e responsabilita di chi la monta.

## 6. Servizi (spenti di fabbrica)
MQTT, rete tra schede, Bluetooth, server dell'ora per altri dispositivi e filtro IP **sono spenti**: la prima volta li accendi tu.
- **MQTT** (Servizi > MQTT): invia lo stato a un broker e riceve comandi. Con utente e password usa `mqtts` (cifrato).
- **Rete tra schede** (Servizi > Rete tra schede, ESP-NOW): le schede parlano senza router, fino a 3 salti. Serve la stessa
  chiave comune (64 cifre) su tutte; i messaggi senza chiave giusta vengono scartati. Ruoli: nodo, gateway (manda tutto a MQTT), sensore.
  Il contenuto non e cifrato: non mandare dati personali.
- **Bluetooth** (nella stessa pagina): solo per configurare dal telefono, si accende a mano per 10 minuti con un codice di accoppiamento.

## 7. Filtro IP
Sicurezza > Filtro IP decide chi puo parlare con la scheda (solo la mia rete, lista consentita, lista bloccati).
Una regola nuova vale 2 minuti: se la pagina risponde ancora premi **Conferma**, altrimenti torna quella di prima.
L'hotspot della scheda e la seriale sono sempre ammessi. Uscite di emergenza: `firewall off` dalla seriale o BOOT 2-7 secondi.

## 8. Watchdog e controlli
- **Watchdog** (Servizi > Watchdog): se un servizio si blocca lo riavvia, poi riavvia la scheda; massimo 3 riavvii automatici
  in un'ora, poi si ferma e avvisa. Facoltativi: rete assente, RAM bassa, riavvio programmato.
- **Compliant** (Sicurezza > Compliant): la scheda controlla la configurazione. Rosso = valore vietato (legge o sicurezza),
  corretto subito. Giallo = permesso ma rischioso. Il registro dice chi ha cambiato cosa (pagina, seriale, file).

## 9. Dati e privacy
Sulla scheda restano: configurazione, utenti (solo l'impronta della password), registro di 150 righe con gli indirizzi IP
di chi accede, allarmi e ultime modifiche. Niente va fuori se non accendi tu MQTT o la rete tra schede.
Per cancellare tutto: Config > Azzera tutto, comando `factory-reset` o BOOT 20 secondi.

## 10. Sicurezza e aggiornamenti
Problemi di sicurezza: scrivi a paolellad79@gmail.com (risposta entro 7 giorni). Progetto gratuito: gli aggiornamenti
non hanno date garantite. Dettagli in [SECURITY.md](../SECURITY.md).

VesevOS e software libero, gratuito, fornito fuori da un'attivita commerciale e senza garanzia (GPL-3.0, sezioni 15 e 16).
Riferimenti normativi (informativi, non sono consulenza legale): Cyber Resilience Act (UE 2024/2847, considerando 18-19: non si
applica al software libero fuori da attivita commerciale); Direttiva prodotti difettosi (UE 2024/2853, art. 2 par. 2);
Direttiva radio 2014/53/UE con Regolamento delegato 2022/30 (EN 18031); UK PSTI Act 2022; Codice civile art. 1229.
Se VesevOS diventa un prodotto a pagamento, questi obblighi valgono per quella versione e si dichiarera il periodo di supporto.

## 11. Terminale (seriale e pagina)
Scrivi `help`. Comandi nuovi: `welcome`, `ap`, `user`, `passwd`, `locale`, `firewall`, `audit`, `watchdog`, `mesh`, `ble`, `cert`, `legal`.
Sulla seriale, a riga vuota, i tasti `1` e `2` cambiano lingua.

## 12. Menu, hotspot, HTTP/HTTPS, porte e fine della guida (novita 1.7.2)
**Dove si trova cosa**
- **Home**: situazione e QR. **Rete**: Wi-Fi, indirizzo IP, nome, modo aereo.
- **Servizi**: Punto di accesso (AP), HTTP, HTTPS, MQTT, Rete tra schede, Automazioni, Task, Watchdog, Avvio, Terminale.
- **Periferiche** (ex Hardware): Pin (schema fronte/retro e inventario), LED.
- **Sistema**: Stato, File, Ora, Localizzazione (qui anche la lingua), Log, Config, Accessibilita, Note legali, Aiuto.
- **Sicurezza**: Password (qui anche la password della seriale), Utenti, Filtro IP, Compliant.

**L'utente sceglie sempre.** Ogni servizio si accende e si spegne. Restano tre paletti: (1) non puoi restare chiuso fuori: almeno un
protocollo web (HTTP o HTTPS) resta acceso, e BOOT 8 secondi rimette hotspot, HTTP, HTTPS e porte di fabbrica; (2) le regole radio
del paese non si superano; (3) niente password uguali per tutti.
- **Punto di accesso (AP)**: e la Wi-Fi della scheda. Si accende da solo al primo avvio e se la tua Wi-Fi non e configurata. Si puo
  spegnere solo quando la scheda e collegata alla tua Wi-Fi. Se la Wi-Fi sparisce e l'AP e spento la scheda non si vede: si recupera
  con BOOT 8 secondi o dalla seriale. Il **portale automatico** (il telefono apre da solo la pagina) funziona solo con HTTP sulla porta 80.
- **HTTP**: senza cifratura. Serve all'hotspot. Collegato alla tua Wi-Fi puoi spegnerlo: consigliato, risparmia memoria.
- **HTTPS**: cifrato (consigliato). Puoi anche spegnerlo e usare solo HTTP: scelta tua, funziona tutto, ma sessione e dati non sono cifrati
  (il login non manda la password) e Compliant mostra un avviso giallo.
- **Porte**: HTTP 80 o da 1024 a 65535, HTTPS 443 o da 1024 a 65535, mai la stessa. **Valgono dal riavvio** (la pagina avvisa). Cambiare
  porta non e sicurezza vera: la sicurezza resta password, filtro IP e cifratura. Link e QR mostrano la porta.

**Fine della configurazione guidata**
- Passo **Wi-Fi di casa**: cerca la rete, scrivi la password, oppure **Salta**.
- Passo **Ora**: con la Wi-Fi di casa l'ora arriva da Internet (NTP). Se salti la Wi-Fi il servizio NTP si spegne e ti chiede ora e data
  (preimpostate da questo dispositivo). Attenzione: **la scheda non ha una batteria per l'orologio**, se la spegni l'ora si perde.
- A fine guida, con la Wi-Fi di casa, la pagina mostra il nuovo indirizzo (con QR). Collega il telefono a quella rete e aprilo: **al primo
  accesso da li** si spengono da soli hotspot, portale automatico e HTTP. Se la scheda non riesce a collegarsi, l'hotspot torna da solo
  e in Home compare l'avviso. Senza Wi-Fi di casa niente si spegne (tranne NTP).
- **Pagina di accesso**: mostra data, ora e fuso della scheda e avvisa se l'ora e diversa da quella del tuo dispositivo.

## 13. Configurazione solo da seriale
Senza pagina web: apri il monitor seriale (115200) e scrivi `setup`. Chiede: lingua, paese, nome, antenna, hotspot (mostra la password,
`n` = nuova), password dell'amministratore (si vede mentre scrivi: fallo in un luogo sicuro), Wi-Fi di casa (elenco numerato), ora.
Invio = tieni il valore, `skip` = salta, `quit` = esci. Comandi singoli: `lang`, `locale country XX`, `hostname`, `wifi set <rete> [password]`,
`wifi off`, `ntp on|off`, `date set AAAA-MM-GG HH:MM`, `svc ap|captive|http|https on|off`, `svc http-port N`, `svc https-port N`, `ap new`.
Dopo la guida valgono le regole di sicurezza (seriale con password, se acceso).

## 14. Pin: schema e inventario
Periferiche > Pin: schema **Fronte** (pin sul bordo, USB-C in alto, BOOT/RESET, LED RGB, antenna) e **Retro** (piazzole da saldare
GP14-18, 21, 33-42, 45-48 e pad B+, B-, BOOST: BOOST solo con batteria oltre 500 mAh). Tocca un pin per vedere funzioni e avvisi
(pin di avvio 0/3/45/46, ADC2 con il Wi-Fi acceso, USB 19/20, JTAG 39-42). **Inventario**: scrivi a cosa colleghi ogni pin
(es. "sensore porta"): il nome resta nella configurazione e un avviso compare se provi un pin gia segnato. GP33-37 sono liberi con la
PSRAM da 2 MB di questa scheda (occupati sulle schede con PSRAM octal).
