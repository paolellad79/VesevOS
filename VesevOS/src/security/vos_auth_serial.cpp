// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_auth_serial.cpp
// Accesso dalla seriale (cavo USB): la verifica della password e la sessione seriale (una sola alla volta).
//
// Il blocco dopo gli errori NON e qui: e lo stesso del web (authFailIp / authIpBlocked / authIpOk in vos_auth.cpp), con l'origine fissa
// AUTH_IP_SERIAL. Il codice MFA lo controlla packages/vos_mfa.h. La conversazione col terminale (utente, password, codice) e in
// interfaces/vos_shell_login.cpp: security non include verso l'alto.
#include "vos_auth.h"
#include "../core/vos_config.h"

static int g_serialUser = -1;            // l'utente della sessione seriale (-1 = nessuno)

// Controlla nome e password con la stessa formula del web (sale + hash iterato, vedi authHashPass).
// Il lavoro e uguale per un nome che non esiste (sale finto e stabile): chi prova a indovinare i nomi non vede differenze di tempo.
int serialPasswordCheck(const char* name, const char* pass) {
  int idx = userFind(name);
  char salt[33], hash[SEC_HEX_BUF];
  authSaltFor(name, salt);
  authHashPass(salt, pass, hash);
  bool same = secSameText(hash, idx >= 0 ? cfg.users[idx].hash.c_str() : "");
  bool allowed = authUserCanLogin(idx);
  memset(hash, 0, sizeof hash);
  return (same && allowed) ? idx : -1;
}

bool serialLoginRequired() { return authIsSet() && cfg.serialAuth; }

bool serialAuthed() { return g_serialUser >= 0 && authUserCanLogin(g_serialUser); }

void serialOpen(int userIdx) { g_serialUser = authUserCanLogin(userIdx) ? userIdx : -1; }

void serialClose() { g_serialUser = -1; }

int serialUser() { return g_serialUser; }

int serialRole() {
  if (!serialLoginRequired()) return ROLE_ADMIN;         // primo avvio senza password, oppure seriale "senza password" scelta dall'amministratore
  return serialAuthed() ? cfg.users[g_serialUser].role : -1;
}

void serialUserName(char* out, size_t cap) {
  if (!out || !cap) return;
  out[0] = 0;
  if (serialAuthed()) snprintf(out, cap, "%s", cfg.users[g_serialUser].name.c_str());
}
