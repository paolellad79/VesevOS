# Cronologia delle versioni

## 1.4.3 (3 ottobre 2026)
- README rinnovato: banner, missione, visione, filosofia, schermate, mappa del progetto.
- README in inglese (`README.en.md`), banner con il Vesuvio, `assets/vesuvio.svg`.
- Pagina: logo a chip in alto a sinistra (intestazione e login), Vesuvio nel login, icona SVG della scheda del browser.
- Corretto il numero di versione scritto nel firmware (era rimasto 1.4.0).
- Link al GitHub del progetto: in fondo alla pagina, nel saluto sulla seriale e nel nuovo comando `about`.
- **Shell web migliorata** (solo pagina): aspetto da terminale con colori (errori rossi, avvisi gialli),
  cronologia con le frecce su/giu salvata nel browser (`passwd` mai salvata), completamento con Tab
  (comandi e opzioni), filtri `| grep`, `| head`, `| tail`, comandi `clear` e `history`, Ctrl+L e Ctrl+C,
  incolla su piu righe con conferma, pulsante "Copia tutto", tasti rapidi per il telefono,
  conferma prima di `reboot` e `factory-reset`.
- **Scheda Rete** (prima "Wi-Fi") con quattro sotto-schede: Wi-Fi (cerca reti, SSID, password), Indirizzo IP,
  Nome (nome host e dominio, spostati qui da Config), Punto di accesso.
  I campi IP, mask, gateway e DNS sono sempre scritti ma disattivati quando non si possono cambiare
  (DHCP: valori assegnati dal router; punto di accesso: indirizzo fisso; non connesso: vuoti).
  Con "Statico" si attivano gia precompilati con i valori in uso.
  Il firmware ora fornisce davvero mask, gateway, DNS 1 e DNS 2 e lo stato della rete.
  Corretto: durante la scansione in modo AP l'indirizzo mostrato poteva essere 0.0.0.0.
- **Contaore di vita** (come un contachilometri): ore totali di accensione della scheda, salvate ogni 10 minuti
  in memoria NVS. Non si azzera col ripristino di fabbrica. Visibile nel Riepilogo e nel comando `uptime`.

## 1.4.2 (3 ottobre 2026)
- Titolare del software: Domenico Paolella. Nome inserito nelle note legali, in `NOTICE.txt`
  e nelle intestazioni di copyright di tutti i sorgenti.

## 1.4.1 (3 ottobre 2026)
- **Prova dei pin**: nella scheda Pin i pin sono cliccabili (verde = provabile, blu = in uso, grigio = vietato).
  Si puo mettere un pin Alto, Basso, farlo Lampeggiare o Leggerlo (con resistenza verso 3,3 V o massa).
  La prova si spegne da sola (20 s le uscite, 60 s la lettura). Un popup avvisa dei rischi per la scheda.
  Pin vietati: USB, LED RGB, BOOT, pin di avvio, flash/PSRAM, pin gia usati dal sistema.
- Comando shell `pin <n> [high|low|blink|read [up|down]|off]`; API `/api/pinmap` e `/api/pintest`.

## 1.4.0 (3 ottobre 2026)
- **Traduzioni separate dal firmware**: italiano nel firmware, altre lingue come file `lang/<codice>.json`
  caricabili dalla pagina (Config > Lingue). Incluse inglese, spagnolo e tedesco. Valgono per pagina,
  shell e messaggi. Selettore di lingua nella pagina (anche al login), comando shell `lang`.
- **Licenze e note legali nel firmware**: GPL v3, LGPL v3, LGPL v2.1, Apache 2.0 e nota legale con
  titolarita e riferimenti normativi. Si leggono da Config > Licenze, dal piede di pagina e con il comando
  shell `license`. File `NOTICE.txt`.
- Doppia licenza GPL v3 + commerciale (vedi `COMMERCIAL.md`).
- Strumenti: `tools/mklang.py` (genera e controlla le lingue), `tools/mklicense.py` (testi legali).

## Licenza
- Doppia licenza: GPL v3 o successiva + licenza commerciale. Intestazione SPDX nei sorgenti.

## 1.3.6 (3 ottobre 2026)
- LED aggiuntivo comandato solo acceso/spento (digitalWrite), senza PWM. Pin predefinito 38.
- Comando shell `led2-invert on|off`; `led2` senza argomenti mostra lo stato.

## 1.3.5
- Nome host e dominio configurabili (pagina, shell, configurazione).
- Uptime senza valori a zero iniziali.

## 1.3.4
- Correzioni: fine riga nella shell web, riconnessione Wi-Fi in AP, motivo del reset.

## 1.3.0 - 1.3.3 (fase 2)
- Ora e NTP (fuso, formati data/ora, server NTP), scheda File, velocita CPU automatica o fissa,
  allarme temperatura, LED aggiuntivo, nuovi comandi shell.

## 1.2.0 (fase 1)
- Base: Wi-Fi AP + client, pagina web, shell web e seriale, password (SHA-256), LED RGB,
  scheda Pin, configurazione in stile OpenWrt, log.
