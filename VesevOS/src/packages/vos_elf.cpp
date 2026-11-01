// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_elf.cpp
// STRATO 6 (packages), LIVELLO A dei moduli ELF. Orchestra: valida -> blocco -> carica (via driver) -> esegui -> scarica.
// Un solo punto d'errore (codici VOS_ELF_E_*). Il blocco del driver protegge le funzioni NON thread-safe del caricatore (risolutore
// globale). Il contesto di chi carica (ruolo, use=, api) vale solo mentre si tiene il blocco. Nessun include di ESP-IDF.
#include "vos_elf.h"

#if VOS_WITH_ELF
#include <string.h>
#include "vos_elf_int.h"
#include "../core/vos_i18n.h"

#define LOCK_MS 5000

static bool g_init;
static volatile bool g_on;               // interruttore a runtime (servizio "elf"): spento di fabbrica
static vos_elf_ctx_t g_ctx;
static char g_ultimo[32];

vos_elf_ctx_t* vos_elf_ctx() { return &g_ctx; }

const char* vos_elf_ultimo_simbolo() { return g_ultimo; }

void vos_elf_abilita(bool on) { g_on = on; }
bool vos_elf_abilitato() { return g_on; }

int vos_elf_init() {
  if (g_init) return VOS_ELF_OK;
  if (vos_drv_elf_init(vos_elf_sym_risolvi) != 0) return VOS_ELF_E_NOINIT;
  g_init = true;
  return VOS_ELF_OK;
}

const vos_elf_caps_t* vos_elf_caps() {
  static vos_elf_caps_t c;
  vos_drv_elf_caps(&c);
  c.api_version = VOS_ELF_API_VERSION;
  return &c;
}

int vos_elf_valida(const uint8_t* p, size_t len, const vos_elf_param_t* par, vos_elf_info_t* info) {
  vos_elf_info_t tmp;
  return vos_elf_ispeziona(p, len, vos_elf_caps(), par, info ? info : &tmp);
}

static int erroreDaDriver(int r, int motivo) {
  if (r == VOS_DRV_ELF_SIMBOLO) return motivo ? motivo : VOS_ELF_E_SIMBOLO;
  return r == VOS_DRV_ELF_MEM ? VOS_ELF_E_MEM : VOS_ELF_E_CARICATORE;
}

int vos_elf_carica(const uint8_t* p, size_t len, const vos_elf_param_t* par, vos_elf_mod_t* m) {
  if (!m) return VOS_ELF_E_ARG;
  memset(m, 0, sizeof(*m));
  if (!par || (par->use && strlen(par->use) >= VOS_PRIM_USE_MAX)) return VOS_ELF_E_ARG;   // use= troppo lungo: mai troncato in silenzio
  if (!g_on) return VOS_ELF_E_SPENTO;
  if (!g_init) return VOS_ELF_E_NOINIT;
  vos_elf_info_t info;
  int e = vos_elf_valida(p, len, par, &info);
  if (e) return e;
  if (vos_drv_elf_lock(LOCK_MS) != 0) return VOS_ELF_E_OCCUPATO;
  memset(&g_ctx, 0, sizeof(g_ctx));
  g_ctx.ruolo = par->ruolo;
  g_ctx.use = par->use;
  g_ctx.api = par->api ? par->api : VOS_ELF_API_VERSION;
  vos_drv_elf_t* h = nullptr;
  vos_drv_elf_entry_t entry = nullptr;
  int r = vos_drv_elf_carica(p, len, &h, &entry);
  int motivo = g_ctx.motivo;
  if (motivo) memcpy(g_ultimo, g_ctx.simbolo, sizeof(g_ultimo));
  memset(&g_ctx, 0, sizeof(g_ctx));          // nessun contesto resta dopo il carico
  vos_drv_elf_unlock();
  if (r != VOS_DRV_ELF_OK) return erroreDaDriver(r, motivo);
  m->h = h;
  m->entry = entry;
  m->ram_stima = info.ram_stima;
  m->ruolo = par->ruolo;
  if (par->use) memcpy(m->use, par->use, strlen(par->use) + 1);   // lunghezza gia controllata
  return VOS_ELF_OK;
}

int vos_elf_esegui(vos_elf_mod_t* m, int argc, char** argv, int* ritorno) {
  if (!m || !m->h || !m->entry) return VOS_ELF_E_ARG;
  int posto = vos_prim_esec_entra(m->ruolo, m->use, m->nome);         // un posto per modulo: piu moduli girano insieme, ognuno con i suoi permessi
  if (!posto) return VOS_ELF_E_OCCUPATO;
  m->posto = posto;
  if (m->uscita) vos_prim_esec_uscita(posto, m->uscita, m->uscita_cap);
  m->in_uso = 1;
  int r = m->entry(argc, argv);
  m->in_uso = 0;
  if (m->uscita) m->uscita_len = vos_prim_esec_lunghezza(posto);
  vos_prim_esec_esce(posto);
  if (ritorno) *ritorno = r;
  return VOS_ELF_OK;
}

void vos_elf_scarica_forza(vos_elf_mod_t* m) {
  if (!m) return;
  if (m->in_uso) vos_prim_esec_esce(m->posto);   // task eliminato dentro il modulo: il contesto di esecuzione non resta acceso
  vos_drv_elf_scarica(m->h);                 // il driver accetta nullptr; lo scarico lavora su un oggetto solo: nessun blocco
  memset(m, 0, sizeof(*m));
}

int vos_elf_scarica(vos_elf_mod_t* m) {
  if (!m) return VOS_ELF_OK;
  if (m->in_uso) return VOS_ELF_E_OCCUPATO;
  vos_elf_scarica_forza(m);
  return VOS_ELF_OK;
}

void vos_elf_dettagli(const vos_elf_mod_t* m, vos_drv_elf_info_t* i) {
  vos_drv_elf_info(m ? m->h : nullptr, i);
}

const char* vos_elf_err_testo(int err) {
  switch (err) {
    case VOS_ELF_OK:           return tr("ok");
    case VOS_ELF_E_ARG:        return tr("argomento non valido");
    case VOS_ELF_E_NOINIT:     return tr("moduli ELF non avviati");
    case VOS_ELF_E_CORTO:      return tr("file troncato");
    case VOS_ELF_E_MAGIC:      return tr("non e un file ELF");
    case VOS_ELF_E_CLASSE:     return tr("classe ELF non adatta");
    case VOS_ELF_E_MACCHINA:   return tr("macchina diversa dal chip");
    case VOS_ELF_E_TIPO:       return tr("tipo ELF non adatto");
    case VOS_ELF_E_GRANDE:     return tr("file troppo grande");
    case VOS_ELF_E_ALLINEA:    return tr("allineamento non valido");
    case VOS_ELF_E_STRUTTURA:  return tr("struttura ELF incoerente");
    case VOS_ELF_E_RAM:        return tr("RAM richiesta oltre il tetto");
    case VOS_ELF_E_ARCH:       return tr("architettura non adatta");
    case VOS_ELF_E_CAPS:       return tr("la scheda non offre cio che serve");
    case VOS_ELF_E_API:        return tr("versione API non adatta");
    case VOS_ELF_E_RUOLO:      return tr("funzione negata dal ruolo");
    case VOS_ELF_E_USE:        return tr("funzione non dichiarata in use=");
    case VOS_ELF_E_SIMBOLO:    return tr("funzione inesistente");
    case VOS_ELF_E_MEM:        return tr("memoria esaurita");
    case VOS_ELF_E_CARICATORE: return tr("errore del caricatore");
    case VOS_ELF_E_OCCUPATO:   return tr("occupato");
    case VOS_ELF_E_INGRESSO:   return tr("punto di ingresso non valido");
    case VOS_ELF_E_ABI:        return tr("ABI non adatta");
    case VOS_ELF_E_SPENTO:     return tr("sottosistema vde spento: accendilo dai Task");
    case VOS_ELF_E_PIENO:      return tr("troppi moduli in esecuzione");
    case VOS_ELF_E_GIA:        return tr("modulo gia in esecuzione");
    case VOS_ELF_E_QUARANTENA: return tr("modulo in quarantena");
    case VOS_ELF_E_FUNZ:       return tr("funzionalita spenta (Sistema > Funzionalita)");
    case VOS_ELF_E_NOME:       return tr("modulo non trovato o nome non valido");
    case VOS_ELF_E_FILE:       return tr("file non leggibile o vuoto");
    case VOS_ELF_E_PERCORSO:   return tr("percorso non ammesso per i moduli");
    default:                   return tr("errore sconosciuto");
  }
}
#endif
