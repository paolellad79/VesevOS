# Cronologia delle versioni

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
