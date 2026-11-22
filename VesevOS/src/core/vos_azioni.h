// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_azioni.h
// AZIONI sul dispositivo (uscite, PWM, LED, contatore, programmi, riavvii, sonno, avvio, rete, servizi, microSD).
// Chi esegue un'azione la annuncia con azionePubblica(): diventa un evento "act.<categoria>" sul bus (vos_eventbus.h).
// Il registratore di dati (packages/vos_rec_ev.cpp) li ascolta e li scrive in eventi.csv. Qui non c'e nessun nome utente ne dato personale:
// solo categoria, nome della cosa (periferica, programma...), un numero e l'origine in una parola (sistema, shell, pagina, script, regola).
#pragma once
#include <stdint.h>

enum AzioneCat {
  AZ_USCITA = 0,     // uscita o rele acceso/spento (nome = periferica, valore 0/1)
  AZ_PWM,            // PWM: duty (valore = percentuale) o frequenza (nome = periferica)
  AZ_LED,            // LED: stato, colore, modo
  AZ_CONTATORE,      // contatore di impulsi azzerato
  AZ_PROGRAMMA,      // script, app o programma ELF: 1 avviato, 0 finito o fermato, 2 finito con errore
  AZ_RIAVVIO,        // riavvio (nome = motivo)
  AZ_SONNO,          // ingresso in sonno profondo (valore = secondi, 0 = fino a RESET)
  AZ_AVVIO,          // avvio o risveglio (nome = avvio/risveglio, valore = motivo dell'ultimo reset)
  AZ_RETE,           // Wi-Fi collegato/perso, hotspot acceso/spento
  AZ_SERVIZIO,       // servizio acceso/spento
  AZ_SD,             // microSD montata/smontata/formattata
  AZ_N
};

enum AzioneOrigine { AO_SISTEMA = 0, AO_SHELL, AO_PAGINA, AO_SCRIPT, AO_REGOLA, AO_N };

void        azioniImposta(uint16_t mascheraCategorie);   // lo fa il registratore: quali categorie servono adesso (0 = nessuna: azionePubblica non fa nulla)
const char* azioneNomeCat(int cat);                   // "uscita", "pwm"... (e il nome dopo "act.")
const char* azioneNomeOrigine(int o);                 // "sistema", "shell"...
int         azioneCatDaTopic(const char* topic);      // da "act.uscita" a AZ_USCITA; -1 se non e un'azione
void        azionePubblica(int cat, const char* nome, int32_t valore);   // veloce: se il registratore non la vuole non fa nulla
void        azioneDecodifica(int32_t val, int& origine, int32_t& valore); // spacchetta il valore dell'evento

// Per un task che dura tutta la vita di un programma (script, ELF): origine fissa dall'inizio; AO_SISTEMA la toglie.
// Un task che sparisce senza passare di qui (vTaskDelete dal supervisore) va tolto con azioneOrigineLibera(handle), altrimenti la voce resta.
void azioneOrigineImposta(int origine);
void azioneOrigineLibera(void* task);

// Dichiara chi sta agendo, per il task che esegue il blocco (shell, pagina, script, regola). Fuori da un blocco l'origine e "sistema".
class AzioneDa {
 public:
  explicit AzioneDa(int origine, bool soloSeSistema = false);   // soloSeSistema: se qualcuno ha gia dichiarato l'origine (pagina, regola) resta quella
  ~AzioneDa();
  AzioneDa(const AzioneDa&) = delete;
  AzioneDa& operator=(const AzioneDa&) = delete;
 private:
  int prev_;
};
