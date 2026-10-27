# Sicurezza / Security

## Segnalare un problema
Hai trovato un problema di sicurezza in VesevOS? **Non aprire una Issue pubblica.**
Scrivi a **paolellad79@gmail.com** con: versione, cosa succede, come riprodurlo.

- Risposta entro **7 giorni**.
- Correzione e avviso pubblico appena possibile, citando chi ha segnalato (se vuole).
- Le vulnerabilita sfruttate attivamente vengono gestite con priorita, anche per gli obblighi di
  notifica del Cyber Resilience Act (Regolamento UE 2024/2847) quando applicabili.

## Aggiornamenti
VesevOS e un progetto **gratuito**, senza garanzia e senza impegno di supporto: gli aggiornamenti di sicurezza non hanno date garantite.
Le segnalazioni sono comunque benvenute e vengono lette. Un periodo di supporto verra dichiarato solo se VesevOS diventera un prodotto a pagamento.
Si consiglia sempre l'ultima versione.

| Versione | Note |
|---|---|
| 1.8.x (1.8.0b) | corrente |
| 1.7.x e precedenti | aggiorna alla 1.8.x |

## Misure gia presenti (1.8.0b)
Password dell'hotspot casuale e diversa per ogni scheda, nessuna password di fabbrica per il pannello,
accesso con prova HMAC (la password non viaggia), HTTPS con certificato unico, utenti con ruoli controllati
dalla scheda, blocco dopo troppi errori, filtro IP, controllo della configurazione con allarmi, servizi di rete
spenti di fabbrica, messaggi tra schede firmati, Bluetooth solo a mano con codice e per 10 minuti.
Dalla 1.8.0: accesso alla seriale con utente, password e codice MFA, ruoli per ogni sottocomando, password mai nella cronologia,
`/api/sleep`, sospensione e spegnimento solo per l'Admin con conferma.

## Aggiornamenti: cosa protegge e cosa no (onesto)
- I file in `firmware/` hanno uno **SHA-256**: dice che il file non e rotto. **Non c'e una firma digitale** (per ora non si fa).
- L'elenco `firmware/indice.json` porta `publisher` e `channel` ("official").
- Non c'e aggiornamento automatico. L'aggiornamento online e sempre manuale (dai tu l'indirizzo e lo SHA-256) o, in futuro, solo se lo accendi tu. Scarica solo da questo repository.
- La pagina di prima installazione (`flash/`) e il recovery 0.5.1 non sono firmati; il recovery usa solo HTTP (nessun TLS) e va usato su una rete di cui ti fidi. Secure Boot non e attivo.
- Valutazione preliminare, non consulenza legale; le norme (CRA, RED/EN 18031, PSTI) potrebbero essere cambiate. Per chi vende un prodotto con VesevOS servono firma, procedura di segnalazione e periodo di supporto dichiarati.

---

## Reporting a problem
Found a security problem in VesevOS? **Do not open a public Issue.** Write to **paolellad79@gmail.com** with
the version, what happens and how to reproduce it. Answer within **7 days**. VesevOS is a free project without warranty or support commitment: security updates have no guaranteed dates (a support period will be declared only if it becomes a paid product). Always use the latest version. Files in `firmware/` have a SHA-256 but no digital signature: "official" in `indice.json` is only an unsigned label, so download only from this repository.
