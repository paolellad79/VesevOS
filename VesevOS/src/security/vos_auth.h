// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_auth.h
// Utenti (fino a 8, ruoli Amministratore / Operatore / Ospite), password con hash e sale,
// accesso senza password in chiaro (numero casuale + HMAC), sessioni, blocco dopo troppi errori.
#pragma once
#include "../core/vos_errbuf.h"
#include "../core/vos_heapbuf.h"
#include "../core/vos_common.h"
#include "vos_sechash.h"

// Dimensioni: i testi JSON e di elenco si scrivono in un HeapBuf del chiamante (tetto AUTH_TEXT_MAX), mai troncati in silenzio.
#define AUTH_TEXT_MAX   4096                      // tetto per utenti, blocchi, chi e collegato
#define AUTH_TOKEN_LEN  32                        // il gettone di sessione ha sempre 32 caratteri esadecimali
#define AUTH_TOKEN_BUF  (AUTH_TOKEN_LEN + 1)

bool   authIsSet();                               // esiste almeno un amministratore con password
// stessa formula usata dalla pagina: out = 64 caratteri esadecimali + zero
void   authHashPass(const char* salt, const char* pass, char out[SEC_HEX_BUF]);
// ---- utenti ----
int    userFind(const char* name);              // indice o -1
bool   userNameValid(const char* name);         // 3-20: lettere, numeri, punto, trattino, sottolineato
bool   userAdd(const char* name, uint8_t role, const char* pass, ErrBuf& err);
bool   userSet(int idx, int role, int on, ErrBuf& err);   // -1 = non cambiare
bool   userDel(int idx, ErrBuf& err);
bool   userSetPassword(int idx, const char* pass, ErrBuf& err);   // non salva: chiamare cfgSave
bool   userCheckPassword(int idx, const char* pass);
void   usersJson(HeapBuf& j);
void   usersText(HeapBuf& t);
const char* roleName(uint8_t role);
// ---- accesso dalla pagina ----
// 1) authLoginStart: scrive sale e numero casuale (anche per nomi inesistenti: non si scopre chi esiste)
void   authLoginStart(const char* name, HeapBuf& j);   // {"salt":"..","nonce":"..","iter":3000}
// 2) authLoginFinish: mac = HMAC-SHA256(chiave = hash della password, messaggio = nonce)
//    ritorna 0 ok (idx = utente), 1 errato, 2 IP bloccato (waitSec)
int    authLoginFinish(uint32_t ip, const char* name, const char* nonce, const char* mac, int& idx, uint32_t& waitSec);
bool   authIpBlocked(uint32_t ip, uint32_t& waitSec);
bool   authFailIp(uint32_t ip, const char* why);   // conta un errore (password, MFA, trappola); true = ora bloccato
void   authIpOk(uint32_t ip);                     // accesso riuscito da quell'origine: azzera errori e livello di blocco
// Origine fissa della seriale nella tabella dei blocchi (la seriale non ha un IP): stesso meccanismo, stesso tetto di errori e stessa
// durata del web, ma una riga a parte, cosi un cavo USB non si confonde con un indirizzo di rete. Nell'elenco si legge "seriale".
#define AUTH_IP_SERIAL 0xFFFFFFFEu
bool   authUserCanLogin(int idx);                 // utente esistente, con password, attivo (e, finche non c'e un amministratore, solo lui)
void   authSaltFor(const char* name, char salt[33]);   // sale dell'utente; per un nome che non esiste un sale finto ma stabile
void   authNoteDenied(uint32_t ip);               // richiesta senza sessione valida (limite richieste)
void   authBanJson(HeapBuf& j);
void   authBanText(HeapBuf& t);
bool   authUnban(const char* ip);                 // "all" o un indirizzo
// ---- sessioni ----
void   authNewSession(char token[AUTH_TOKEN_BUF], int userIdx, uint32_t ip = 0, bool secure = false);   // scrive il gettone nuovo
int    authSessionUser(const char* token);        // indice utente o -1
// "chi e collegato": ogni richiesta con sessione aggiorna indirizzo, HTTP/HTTPS e l'ultima azione (le letture periodiche della pagina non contano)
void   authSessionNote(const char* token, uint32_t ip, bool secure, bool post, const char* path);
int    authSessionCount(int via);                 // sessioni attive negli ultimi 2 minuti: 0 = HTTP, 1 = HTTPS, -1 = tutte
void   authWhoJson(HeapBuf& j);                   // {"list":[{user,role,ip,via,age,idle,act,actAge}...]}
void   authWhoText(HeapBuf& t);
void   authLogout(const char* token);
void   authLogoutUser(int idx);                   // chiude tutte le sessioni di un utente
// Estrae il valore di "vos=" dall'intestazione Cookie in out (vuoto se manca o se non ci sta)
void   authCookieFromHeader(const char* cookieHeader, char* out, size_t cap);
// ---- shell seriale: login come in una shell classica (utente, password e, se acceso, codice MFA), con utente e ruolo veri ----
// La verifica della password e qui (vos_auth_serial.cpp); il codice MFA lo controlla packages/vos_mfa.h con le stesse funzioni del web;
// la conversazione col terminale e in interfaces/vos_shell_login.cpp. Una sola sessione seriale alla volta.
int    serialPasswordCheck(const char* name, const char* pass);   // indice dell'utente se nome e password sono giusti, altrimenti -1 (stesso tempo anche per nomi che non esistono)
bool   serialLoginRequired();                     // c'e un amministratore con password e l'interruttore "seriale con password" e acceso
bool   serialAuthed();                            // una sessione seriale e aperta (e il suo utente c'e ancora ed e attivo)
void   serialOpen(int userIdx);                   // apre la sessione seriale per quell'utente (dopo password e MFA giusti)
void   serialClose();                             // chiude la sessione (logout, exit, inattivita, ruolo o password cambiati)
int    serialUser();                              // indice dell'utente della sessione, -1 se nessuno
int    serialRole();                              // ruolo con cui girano i comandi: quello dell'utente; amministratore se il login non serve; -1 se serve e manca
// Nome dell'utente con cui si e entrati dalla seriale. Vuoto se non entrato o se il login non serve.
void   serialUserName(char* out, size_t cap);
// ---- tasto BOOT 8 s: azzera la password dell'amministratore (il prossimo accesso la crea di nuovo) ----
void   authResetAdmin();
int    authFirstAdmin();                          // primo accesso: indice dell'amministratore a cui dare la password
