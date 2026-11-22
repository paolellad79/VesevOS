// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_admin.cpp
// Comandi della shell: configurazione, utenti, sicurezza, rete schede, servizi, energia, riavvio. Aiuti e include: vos_shell_int.h.
#include "vos_shell_int.h"
#include "vos_shell_ask.h"
#include "../packages/vos_prog_kind.h"
#include "../packages/vos_rec_file.h"
#include "../drivers/vos_drv_time.h"
#include "../drivers/vos_drv_heap.h"
#include "../core/vos_clamp.h"
#include "../core/vos_wdset.h"
#include <stdlib.h>

// Avviso: la password scritta nella riga si vede sullo schermo e (nella pagina) finirebbe nella cronologia. La sintassi resta solo
// perche gli script gia scritti continuano a funzionare; la pagina in ogni caso non salva queste righe.
void shx::shWarnPassInLine(OutSink& o) {
  o.println(tr("Attenzione: la password scritta nella riga resta visibile. Meglio senza: scrivi solo il nome e te la chiedo io, senza mostrarla."));
}

// passwd: la password di un utente
void shx::shPasswdApply(const char* user, const char* pass, OutSink& o) {
  ErrBuf err;
  int u = userFind(user);
  if (u < 0) { o.println(tr("Utente non trovato")); return; }
  bool onSerial = serIsOut(o);
  bool stays = onSerial && (!authIsSet() || serialUser() == u);   // primo avvio, oppure cambia la propria password: la seriale resta aperta
  if (!userSetPassword(u, pass, err)) { o.println(err.c_str()); return; }
  cfgSave();
  if (stays) serialOpen(u);                                         // userSetPassword chiude le sessioni dell'utente, seriale compresa
  o.println(tr("Password cambiata"));
}

// user add: crea l'utente e mostra l'elenco
void shx::shUserAddApply(const char* name, int role, const char* pass, OutSink& o) {
  ErrBuf err;
  if (!userAdd(name, role < 0 ? 255 : (uint8_t)role, pass, err)) { o.println(err.c_str()); return; }
  cfgSave();
  HeapBuf t(AUTH_TEXT_MAX); usersText(t); o.print(t.c_str());
}

// Azioni che non si annullano: sulla seriale chiedono y/n, nella pagina serve "yes" in fondo alla riga (la pagina lo chiede con una finestra).
// Ritorna true se ha fatto la domanda (il comando deve fermarsi qui).
static bool needConfirm(const char* line, OutSink& o, const char* question) {
  if (askConfirmed(line)) return false;
  askConfirm(o, line, question);
  return true;
}

bool shCmdAdmin(const Word& c, const Word& a1, const char* line, OutSink& o, int role) {
  if (c == "config") { HeapBuf t(CFG_TEXT_MAX); if (cfgExport(t, false)) o.print(t.c_str()); else o.println(tr("Memoria insufficiente")); }
  else if (c == "passwd") {
    // passwd <utente>: la password si chiede (seriale: domanda senza eco; pagina: campo a parte), non si scrive nella riga.
    if (userFind(a1.c_str()) < 0) { o.println(tr("Uso: passwd <utente>   (la password te la chiedo io, senza mostrarla)")); return true; }
    const char* inLine = restFrom(line, 2);
    if (*inLine) { shWarnPassInLine(o); shPasswdApply(a1.c_str(), inLine, o); }          // vecchia sintassi, per gli script
    else if (const char* given = askSecretGiven(o)) shPasswdApply(a1.c_str(), given, o);
    else askSecret(o, SECRET_PASSWD, a1.c_str(), 0, true);
  }
  else if (c == "user") {
    Word a2 = argAt(line, 2), a3 = argAt(line, 3);
    ErrBuf err;
    if (a1 == "" || a1 == "list") { HeapBuf t(AUTH_TEXT_MAX); usersText(t); o.print(t.c_str()); return true; }
    if (a1 == "add") {
      if (a2.empty() || a2.cut()) { o.println(tr("Uso: user add <nome> <admin|oper|guest>   (la password te la chiedo io)")); return true; }
      if (roleArg(a3) < 0) { o.println(tr("Ruoli: admin, oper, guest")); return true; }
      const char* inLine = restFrom(line, 4);
      if (*inLine) { shWarnPassInLine(o); shUserAddApply(a2.c_str(), roleArg(a3), inLine, o); }   // vecchia sintassi, per gli script
      else if (const char* given = askSecretGiven(o)) shUserAddApply(a2.c_str(), roleArg(a3), given, o);
      else askSecret(o, SECRET_USER_ADD, a2.c_str(), roleArg(a3), true);
      return true;
    }
    if (a1 == "del") { if (!userDel(userFind(a2.c_str()), err)) { o.println(err.c_str()); return true; } }
    else if (a1 == "role") { if (roleArg(a3) < 0) { o.println(tr("Ruoli: admin, oper, guest")); return true; } if (!userSet(userFind(a2.c_str()), roleArg(a3), -1, err)) { o.println(err.c_str()); return true; } }
    else if (a1 == "on" || a1 == "off") { if (!userSet(userFind(a2.c_str()), -1, a1 == "on" ? 1 : 0, err)) { o.println(err.c_str()); return true; } }
    else { o.println(tr("Uso: user [add <nome> <admin|oper|guest>|del <nome>|role <nome> <ruolo>|on <nome>|off <nome>]")); return true; }
    cfgSave();
    HeapBuf t(AUTH_TEXT_MAX); usersText(t); o.print(t.c_str());
  }
  else if (c == "ap") {
    if (a1 == "new") { cfg.apPass = cfgNewApPass().c_str(); cfgSave(); netReconfigure(); }
    else if (a1 != "" && a1 != "show") { o.println(tr("Uso: ap [show|new]")); return true; }
    o.println(trf("Hotspot: %s  password: %s", cfg.apSsid.c_str(), cfg.apPass.c_str()));
  }
  else if (c == "region") {                                    // locale e il vecchio nome (canonicalName)
    Word a2 = argAt(line, 2), a3 = argAt(line, 3);
    if (a1 == "country") { a2.upper(); if (!regionValid(a2.c_str())) { o.println(tr("Paese sconosciuto")); return true; } cfg.country = a2.c_str(); cfgSave(); regionApplyRadio(); }
    else if (a1 == "tx") { int t = a2.toInt(); if (t < 0 || t > 20) { o.println(tr("Potenza: da 2 a 20 dBm (0 = massima consentita)")); return true; } cfg.txDbm = t; cfgSave(); regionApplyRadio(); }
    else if (a1 == "antenna") { if (a2 == "int") { cfg.antExt = 0; } else if (a2 == "ext") { int g = a3.toInt(); if (g < 0 || g > 15) { o.println(tr("Guadagno dell'antenna: da 0 a 15 dBi")); return true; } cfg.antExt = 1; cfg.antGain = g; } else { o.println(tr("Uso: region antenna int | region antenna ext <dBi>")); return true; } cfgSave(); regionApplyRadio(); }
    else if (a1 != "") { o.println(tr("Uso: region [country XX|tx <dBm>|antenna int|ext <dBi>]")); return true; }
    o.print(regionText().c_str());
  }
  else if (c == "firewall") {
    Word a2 = argAt(line, 2);
    ErrBuf err;
    if (a1 == "off") { fwOff("seriale"); }
    else if (a1 == "confirm") fwConfirm();
    else if (a1 == "mode") { int m = a2.toInt(); if (a2 == "" || m < 0 || m > 3) { o.println(tr("Modi: 0 spento, 1 solo la mia rete, 2 lista consentita, 3 lista bloccati")); return true; } fwTryStart(0); cfg.fwMode = m; cfgSave(); }
    else if (a1 == "add") { uint32_t a, b; if (cfg.fwN >= VOS_FW_MAX || !cfgParseRange(a2.c_str(), a, b)) { o.println(tr("Indirizzo non valido o lista piena")); return true; } cfg.fw[cfg.fwN].a = a; cfg.fw[cfg.fwN].b = b; cfg.fw[cfg.fwN].on = true; fwRuleSetName(cfg.fw[cfg.fwN], restFrom(line, 3)); cfg.fwN++; cfgSave(); }
    else if (a1 == "del") { int n = a2.toInt(); if (n < 1 || n > cfg.fwN) { o.println(tr("Voce non trovata")); return true; } for (int i = n - 1; i < cfg.fwN - 1; i++) cfg.fw[i] = cfg.fw[i + 1]; cfg.fwN--; cfgSave(); }
    else if (a1 == "test") { uint32_t ip; if (!ipParse(a2.c_str(), ip)) { o.println(tr("IP non valido")); return true; } o.println(fwAllow(ip, false) ? tr("ammesso") : tr("rifiutato")); return true; }
    else if (a1 != "" && a1 != "status") { o.println(tr("Uso: firewall [off|confirm|mode <0-3>|add <ip|a-b|rete/nn> [nome]|del <n>|test <ip>]")); return true; }
    o.print(fwText().c_str());
  }
  else if (c == "audit") {
    if (a1 == "ack") { auditAck((int)argAt(line, 2).toInt()); }
    HeapBuf t(AUDIT_JSON_MAX); auditText(t); o.print(t.c_str());
  }
  else if (c == "wd") {
    ErrBuf err;
    if (a1 == "set") {
      Word a2 = argAt(line, 2), a3 = argAt(line, 3);
      char* end = nullptr; long v = strtol(a3.c_str(), &end, 10);
      if (a2.empty() || a3.empty() || *end) { o.println(tr("Uso: wd set twdt <secondi> | wd set <net|monitor|mesh|mqtt|led> <secondi>")); return true; }
      if (!wdSetOne(a2.c_str(), v, err)) { o.println(err.c_str()); return true; }
    }
    else if (a1 == "defaults") { if (!wdDefaults(err)) { o.println(err.c_str()); return true; } }
    else if (a1 != "" && a1 != "status") { o.println(tr("Uso: wd [status|set twdt <s>|set <servizio> <s>|defaults]")); return true; }
    o.print(wdText().c_str());
  }
  else if (c == "mesh") {
    Word a2 = argAt(line, 2);
    ErrBuf err;
    if (a1 == "start") { if (!serviceStart("mesh", err)) { o.println(err.c_str()); return true; } }
    else if (a1 == "stop") serviceStop("mesh", err);
    else if (a1 == "send") { if (!meshSendText(a2.c_str(), restFrom(line, 3), err)) { o.println(err.c_str()); return true; } o.println(tr("Inviato")); return true; }
    else if (a1 == "cmd") { if (!meshSendCmd(a2.c_str(), restFrom(line, 3), err)) { o.println(err.c_str()); return true; } o.println(tr("Inviato")); return true; }
    else if (a1 == "key") { if (a2 == "new") { char nk[MESH_KEY_CHARS + 1]; meshNewKey(nk); cfg.meshKey = nk; cfgSave(); } if (!cfg.meshKey.empty()) o.println(cfg.meshKey.c_str()); else o.println(tr("(nessuna chiave)"));
      return true; }
    else if (a1 == "role") { int r = a2.toInt(); if (a2 == "" || r < 0 || r > 2) { o.println(tr("Ruoli: 0 nodo, 1 gateway, 2 sensore")); return true; } cfg.meshRole = r; cfgSave(); }
    else if (a1 != "" && a1 != "status") { o.println(tr("Uso: mesh [start|stop|send <scheda|*> <testo>|cmd <scheda> <azione>|key new|role <0|1|2>]")); return true; }
    o.print(meshText().c_str());
  }
  else if (c == "cert") {
    if (a1 == "new") { tlsRegenerate(); o.println(tr("Nuovo certificato pronto: vale dal prossimo riavvio (reboot)")); return true; }
    o.println(trf("HTTPS: %s", cfg.https ? (webHttpsUp() ? tr("attivo") : tr("non partito")) : tr("spento")));
    o.println(trf("Impronta SHA-256: %s", tlsFingerprint()));
  }
  else if (c == "legal") {
    o.printf("%s %s - GPL-3.0-or-later / %s\r\n", VOS_NAME, VOS_VERSION, tr("licenza commerciale"));
    o.println(tr("Software di terzi e testi delle licenze: comando license, pagina Informazioni > Note legali."));
    o.println(tr("Dati salvati sulla scheda: configurazione, utenti (solo impronta della password), registro (ultime righe, con indirizzi IP), allarmi. Nessun dato va fuori dalla scheda se non attivi tu MQTT o la rete tra schede."));
    o.println(tr("Cancellare tutto: factory-reset oppure tasto fisico 20 secondi."));
    o.println(tr("Sicurezza: segnala i problemi a paolellad79@gmail.com (risposta entro 7 giorni). Progetto gratuito: gli aggiornamenti non hanno date garantite."));
    o.println(tr("Radio: comando region. Non e consulenza legale."));
    o.println(tr("Software libero gratuito, fornito fuori da attivita commerciale e senza garanzia (GPL-3.0 sezioni 15-16). Riferimenti normativi: comando license notice."));
  }
  else if (c == "welcome") shellWelcome(o, !cfg.setupDone && serIsOut(o));
  else if (c == "log") {
    if (a1 == "clear") { logClear(); o.println(tr("Registro svuotato")); }
    else if (a1 == "level") {
      Word a2 = argAt(line, 2);
      if (!a2.empty()) { int lv = vosClamp((int)a2.toInt(), 0, 3); cfg.logLevel = (uint8_t)lv; logSetLevel(lv); cfgSave(); }
      o.println(trf("Livello del registro: %d (0 errori, 1 + attenzioni, 2 + info, 3 + dettagli)", (int)logLevel()));
    }
    else o.print(logGet(!a1.empty() ? vosClamp((int)a1.toInt(), 1, 150) : 30).c_str());
  }
  else if (c == "reboots") {
    if (a1 == "clear") { diaryClear(); o.println(tr("Diario dei riavvii azzerato")); }
    else { std::string d = diaryText(20, true); if (d.empty()) { o.print(tr("Nessun avvio registrato")); o.print("\n"); } else o.print(d.c_str()); }
  }
  else if (c == "factory-reset") {
    // Dal terminale della pagina non si azzera: la via web e una sola, /api/factory, che vuole la password.
    if (!serIsOut(o)) { o.println(tr("Dal terminale della pagina non si azzera: usa Sistema > Configurazione, che chiede la password.")); return true; }
    if (needConfirm(line, o, tr("Azzero TUTTO (impostazioni, utenti, password) e riavvio. Non si annulla. Procedo?"))) return true;
    o.println(tr("Azzero tutto e riavvio..."));
    vlog("SISTEMA: ripristino di fabbrica dalla shell");
    cfgFactoryReset(); sysReboot("ripristino di fabbrica dalla shell", 300);
  }
  else if (c == "airplane") {
    if (a1 == "off") { netAirplaneOff("dalla shell"); o.println(tr("Modalita aereo spenta: il Wi-Fi riparte")); }
    else if (a1 == "on") {
      Word m = argAt(line, 2); m.lower();
      const char* colon = strchr(m.c_str(), ':');
      int ex = 0; uint32_t par = 0;
      if (m == "" || m == "boot") ex = 0;
      else if (m == "fisso" || m == "fixed" || m == "manual") ex = 3;
      else if (colon && colon > m.c_str()) { int h = (int)m.toInt(), mi = atoi(colon + 1); ex = 2; par = (h >= 0 && h < 24 && mi >= 0 && mi < 60) ? h * 60 + mi : 9999; }
      else { long n = m.toInt(); char u = m.last(); ex = 1; par = (u == 'm') ? n * 60 : (u == 'h') ? n * 3600 : n; }
      ErrBuf err;
      if (netAirplaneOn(ex, par, err)) o.println(netAirplaneText().c_str()); else o.println(err.c_str());
    }
    else if (a1 == "") o.println(netAirplaneText().c_str());
    else o.println(tr("Uso: airplane on [boot|<N>s|<N>m|<N>h|HH:MM|fixed]  /  airplane off"));
  }
  else if (c == "mqtt") {
    if (a1 == "start" || a1 == "restart") { ErrBuf e; if (!(a1 == "start" ? serviceStart("mqtt", e) : serviceRestart("mqtt", e))) { o.println(e.c_str()); return true; } o.println(tr("MQTT avviato")); }
    else if (a1 == "stop") { ErrBuf e; serviceStop("mqtt", e); o.println(tr("MQTT fermato")); }
    else if (a1 == "pub") {
      Word tp = argAt(line, 2);
      const char* msg = restFrom(line, 3);
      if (tp.empty() || tp.cut()) { o.println(tr("Uso: mqtt pub <argomento> <testo>")); return true; }
      o.println(mqttPublishRel(tp.c_str(), msg) ? tr("Inviato") : tr("MQTT non collegato"));
    }
    else if (a1 == "" || a1 == "status") o.print(mqttStatusText().c_str());
    else o.println(tr("Uso: mqtt [status|start|stop|restart|pub <argomento> <testo>]"));
  }
  else if (c == "ban") { HeapBuf t(AUTH_TEXT_MAX); authBanText(t); o.print(t.c_str()); }
  else if (c == "unban") {
    if (a1.empty()) { o.println(tr("Uso: unban <indirizzo IP> | unban all")); return true; }
    o.println(authUnban(a1.c_str()) ? tr("Sbloccato") : tr("Indirizzo non trovato"));
  }
  else if (c == "suspend" || c == "sleep" || c == "poweroff") {
    // In una shell classica "sleep N" e una pausa: qui il comando e suspend. "sleep" resta come vecchio nome, ma chiede conferma come gli altri.
    // poweroff: l'ESP32-S3 non si spegne davvero; il piu vicino e il sonno profondo senza sveglia (consumo minimo, solo RESET).
    bool off = (c == "poweroff");
    bool arg = !a1.empty() && a1 != "yes";                                  // un numero di minuti (o altro)
    int m = arg ? (int)a1.toInt() : 0;
    if (off && arg) { o.println(tr("Uso: poweroff   (nessun argomento: si riaccende solo con RESET)")); return true; }
    if (arg && (m < 1 || m > 10080)) { o.println(tr("Sonno: da 1 minuto a 7 giorni")); return true; }
    bool asked = m ? needConfirm(line, o, trf("Sonno profondo per %d minuti: la scheda si spegne e poi riparte da sola. Procedo?", m))
                   : needConfirm(line, o, tr("Spegnimento (sonno profondo): la scheda si riaccende solo con il tasto RESET. Procedo?"));
    if (asked) return true;
    if (c == "sleep") o.println(tr("(sleep e il vecchio nome di suspend)"));
    if (m) { o.println(trf("Sonno profondo per %d minuti: poi la scheda riparte da sola (o con RESET)", m)); drvDelayMs(300); powerSleepNow((uint32_t)m * 60UL); }
    else { o.println(tr("Sonno profondo: si riaccende con il tasto RESET")); drvDelayMs(300); sysSleep(); }
  }
  else if (c == "standby") {
    // standby <leggero|profondo> [tempo <secondi> | alle <HH:MM>]: il sonno con risveglio a tempo e/o da un piedino.
    // Il risveglio dal tasto non si scrive qui: se e installato il driver "ripresa", lo standby la trova e la usa da solo.
    bool profondo = (a1 == "profondo");
    if (!profondo && a1 != "leggero") { o.println(tr("Uso: standby leggero|profondo [tempo <secondi> | alle <HH:MM>]")); return true; }
    long sec = 0;
    int alle = -1;
    for (int i = 2; ; i++) {
      Word w = argAt(line, i);
      if (w.empty()) break;
      if (w == "yes") continue;
      if (w == "tempo") { sec = argAt(line, ++i).toInt(); continue; }
      if (w == "alle") {
        int hh = -1, mm = -1;
        Word hm = argAt(line, ++i);
        if (sscanf(hm.c_str(), "%d:%d", &hh, &mm) != 2 || hh < 0 || hh > 23 || mm < 0 || mm > 59) { o.println(tr("Orario non valido")); return true; }
        alle = hh * 60 + mm;
        continue;
      }
      o.println(tr("Uso: standby leggero|profondo [tempo <secondi> | alle <HH:MM>]"));
      return true;
    }
    if (sec < 0) { o.println(tr("Tempo: fino a 7 giorni")); return true; }
    if (alle >= 0) {
      if (sec) { o.println(tr("Scegli tempo oppure alle, non tutti e due")); return true; }
      sec = (long)powerSecondiAllOra((unsigned)alle);
      if (!sec) { o.println(tr("L'ora non e valida: serve la rete o l'ora impostata")); return true; }
    }
    if (!profondo && (sec == 0 || sec > POWER_LEGGERO_MAX_S)) {
      profondo = true;
      o.println(tr("Il sonno leggero e sospeso: lo standby usa il sonno profondo e la scheda riparte da capo al risveglio"));
    }
    bool conRipresa = powerRipresaPresente();                         // lo standby sa solo se c'e un tasto di ripresa, non il suo piedino
    ErrBuf e;
    if (!powerStandbyControlla(profondo, (uint32_t)sec, conRipresa, e)) { o.println(e.c_str()); return true; }   // prima i controlli, senza dormire
    bool asked = needConfirm(line, o, profondo ? tr("Standby profondo: la scheda si spegne e riparte da capo al risveglio. Procedo?")
                                                : tr("Standby leggero: la rete si ferma e riparte al risveglio. Procedo?"));
    if (asked) return true;
    if (conRipresa) o.println(tr("Si risveglia anche con il tasto di ripresa"));
    ErrBuf e2;
    if (!powerStandby(profondo, (uint32_t)sec, e2)) { o.println(e2.c_str()); return true; }
    o.println(profondo ? tr("Standby profondo: parte tra un secondo, la scheda riparte da capo al risveglio") : tr("Standby leggero: parte tra un secondo, la rete torna da sola al risveglio"));
  }
  else if (c == "selftest") {
    ErrBuf e;
    bool act = strstr(line, "active") != nullptr, nm = strstr(line, "names") != nullptr;
    if (!selftestStart(act, nm, e)) { o.println(e.c_str()); return true; }
    int shown = 0;
    while (selftestRunning() || shown < selftestTotal()) {
      std::string l = selftestLine(shown);
      if (!l.empty()) { o.println(l.c_str()); shown++; } else if (!selftestRunning()) break; else drvDelayMs(100);
    }
    { HeapBuf rp(SELFTEST_REPORT_MAX); selftestReport(rp); o.print(rp.c_str()); }
  }
  else if (c == "benchsha") benchShaRun(o);   // diagnostica: misura SHA-256 hardware/software (solo Admin: non e nell'elenco dell'Operatore)
  else if (c == "stats") {
    if (a1 == "on") { cfg.statOn = true; statsReset(); cfgSave(); }
    else if (a1 == "off") { cfg.statOn = false; cfgSave(); }
    else if (a1 == "reset") statsReset();
    else if (a1 != "" && a1 != "status") { o.println(tr("Uso: stats [on|off|reset]")); return true; }
    o.print(statsText().c_str());
  }
  else if (c == "power") {
    ErrBuf e;
    if (a1 == "off") powerSet(0, 0, 0, e);
    else if (a1 == "wifi") powerSet(1, 0, 0, e);
    else if (a1 == "cycle") powerSet(2, (uint32_t)argAt(line, 2).toInt(), (uint32_t)argAt(line, 3).toInt(), e);
    else if (a1 != "" && a1 != "status") { o.println(tr("Uso: power [off|wifi|cycle <minuti sveglia> <minuti sonno>]")); return true; }
    if (e.length()) { o.println(e.c_str()); return true; }
    if (a1 != "" && a1 != "status") { cfgSave(); o.println(tr("Salvato e applicato.")); }
    o.print(powerText().c_str());
  }
#if VOS_WITH_REC
  else if (c == "rec") {
    // rec [status] | azioni | on | off | clear | set <interval|where|max|limite|ogni|azioni|evmax|temp|ram|dev> <valore>
    // (dev vuole anche il numero 1-4 e il nome, "-" = nessuna; ogni vuole la fonte 1-4, temp o ram e i secondi, 0 = predefinito)
    RecSettings v;
    recSettingsGet(v);
    ErrBuf e;
    bool change = false;
    if (a1 == "on") { v.on = true; change = true; }
    else if (a1 == "off") { v.on = false; change = true; }
    else if (a1 == "azioni") { o.print(recAzioniText().c_str()); return true; }
    else if (a1 == "clear") { recClear(e); o.println(e.length() ? e.c_str() : tr("Dati del registratore cancellati.")); return true; }
    else if (a1 == "set") {
      Word k = argAt(line, 2), val = argAt(line, 3);
      k.lower(); val.lower();
      if (k == "interval") v.sec = (uint16_t)val.toInt();
      else if (k == "where") v.where = val == "sd" ? 1 : (val == "flash" ? 0 : 9);
      else if (k == "max") v.maxKb = (uint16_t)val.toInt();
      else if (k == "limite") {
        if (val != "totale" && val != "file") { o.println(tr("Uso: rec set limite <totale|file>")); return true; }
        v.limite = val == "file" ? 1 : 0;
      }
      else if (k == "nome") {
        Word nm = argAt(line, 3);                        // il nome tiene maiuscole e minuscole: val e gia in minuscolo
        if (nm.length() > REC_NOME_MAX) { o.println(tr("Uso: rec set nome <nome del file, 1-24 caratteri: lettere, numeri, _ e ->")); return true; }
        snprintf(v.nome, sizeof v.nome, "%s", nm.c_str());
      }
      else if (k == "evmax") v.evMax = (uint16_t)val.toInt();
      else if (k == "ogni") {
        long s = argAt(line, 4).toInt();
        if (s < 0 || s > 86400) { o.println(tr("Uso: rec set ogni <1-4|temp|ram> <secondi, 0 = predefinito>")); return true; }
        if (val == "temp") v.secT = (uint16_t)s;
        else if (val == "ram") v.secR = (uint16_t)s;
        else if (val.toInt() >= 1 && val.toInt() <= REC_DEV_MAX) v.secDev[val.toInt() - 1] = (uint16_t)s;
        else { o.println(tr("Uso: rec set ogni <1-4|temp|ram> <secondi, 0 = predefinito>")); return true; }
      }
      else if (k == "azioni") {
        // elenco di categorie separate da virgola (uscita,pwm,led,contatore,programma,riavvio,sonno,avvio,rete,servizio,sd), "tutte" o "nessuna"
        uint16_t m = 0;
        if (val == "tutte") m = 0x7FF;
        else if (val != "nessuna") {
          const char* p = val.c_str();
          while (*p) {
            const char* q = strchr(p, ',');
            size_t n = q ? (size_t)(q - p) : strlen(p);
            int found = -1;
            for (int c2 = 0; c2 < AZ_N; c2++) if (strlen(azioneNomeCat(c2)) == n && strncmp(azioneNomeCat(c2), p, n) == 0) found = c2;
            if (found < 0) { o.println(tr("Uso: rec set azioni <uscita,pwm,led,contatore,programma,riavvio,sonno,avvio,rete,servizio,sd|tutte|nessuna>")); return true; }
            m |= (uint16_t)(1u << found);
            p += n + (q ? 1 : 0);
          }
        }
        v.azioni = m;
      }
      else if (k == "temp") v.t = (val == "on");
      else if (k == "ram") v.r = (val == "on");
      else if (k == "dev") {
        long n = val.toInt();
        Word nm = argAt(line, 4);
        if (n < 1 || n > REC_DEV_MAX || nm.length() > 15) { o.println(tr("Uso: rec set dev <1-4> <nome periferica|->")); return true; }
        snprintf(v.dev[n - 1], sizeof v.dev[n - 1], "%s", nm == "-" ? "" : nm.c_str());
      }
      else { o.println(tr("Uso: rec set interval <secondi> | where <flash|sd> | max <KB> | nome <nome> | limite <totale|file> | ogni <fonte> <secondi> | azioni <elenco> | evmax <righe> | temp <on|off> | ram <on|off> | dev <1-4> <nome|->")); return true; }
      change = true;
    }
    else if (a1 != "" && a1 != "status") { o.println(tr("Uso: rec [status|azioni|on|off|clear|set ...]")); return true; }
    if (change) {
      recSet(v, e);
      if (e.length()) { o.println(e.c_str()); return true; }
      cfgSetOrigin("registratore");
      cfgSave();
      o.println(tr("Salvato e applicato."));
    }
    o.print(recText().c_str());
  }
#endif
  else if (c == "reboot") { o.println(tr("Riavvio...")); sysReboot("comando reboot", 300); }
  else if (c == "script" || c == "vsh") {
    // script: elenco, avvio, stop, uscita degli script salvati in /sbin (.vsb).  vsh <codice>: prova al volo.
    // (vsh list|run|stop|out funzionano ancora come scorciatoie dei comandi script)
    const char* code = restFrom(line, 1);
    ErrBuf err;
    bool scr = (c == "script");
    HeapBuf txt(FUNZIONI_TESTO_MAX);                                       // i testi delle funzioni VesevOS si scrivono qui (heap), non sullo stack
    if (!*code) {
      if (scr) { txt.add(tr("Uso: script list | script run <nome> | script stop | script out")); txt.add('\n'); funzioniElenca(txt); }
      else funzioniTestoStato(txt);
      o.print(txt.c_str()); return true;
    }
    if (a1 == "list" && !strcmp(code, "list")) { funzioniElenca(txt); o.print(txt.c_str()); return true; }
    if (a1 == "out" && !strcmp(code, "out")) { funzioniTestoUscita(txt); o.print(txt.c_str()); return true; }
    if (a1 == "stop" && !strcmp(code, "stop")) { if (funzioniFerma(err)) o.println(tr("Script fermato")); else o.println(err.c_str()); return true; }
    if (a1 == "run") {
      Word nm = argAt(line, 2);
      if (nm.empty()) { o.println(tr("Uso: script run <nome>  (file /sbin/<nome>.vsb)")); return true; }
      shStartScript(nm.c_str(), o);
      return true;
    }
    if (scr) { o.println(tr("Uso: script list | script run <nome> | script stop | script out")); return true; }
    HeapBuf out(FUNZIONI_USCITA_MAX + 16);                                   // uscita dello script (con "..." finale se tagliata)
    bool ok = funzioniValuta(code, out, err);
    vlog("SCRIPT: script eseguito dalla shell (%u caratteri, %s)", (unsigned)strlen(code), ok ? "ok" : "errore");
    if (out.length()) { o.print(out.c_str()); if (out.c_str()[out.length() - 1] != '\n') o.println(); }
    if (!ok) { if (err.length()) o.println(err.c_str()); else o.println(tr("Errore")); }
  }
  else if (c == "app") {
    // app: elenco, controllo, avvio, stop, valori e uscita dei file /bin/*.vab (una sola app alla volta, nel motore delle funzioni)
    Word nm = argAt(line, 2);
    ErrBuf err;
    if (a1 == "list") { HeapBuf t(APP_TEXT_MAX); appListText(t); o.print(t.c_str()); }
    else if ((a1 == "info" || a1 == "check") && !nm.empty()) { HeapBuf t(APP_TEXT_MAX); appInfoText(nm.c_str(), t); o.print(t.c_str()); }
    else if (a1 == "start" && !nm.empty()) shStartApp(nm.c_str(), o, 2);
    else if (a1 == "stop" && !nm.empty()) { if (appStop(nm.c_str(), err, 2)) o.println(tr("App fermata")); else o.println(err.c_str()); }
    else if (a1 == "set" && !nm.empty() && !argAt(line, 3).empty()) { if (appSet(nm.c_str(), argAt(line, 3).c_str(), restFrom(line, 4), err, 2)) o.println(tr("Fatto")); else o.println(err.c_str()); }
    else if (a1 == "data" && !nm.empty()) shProgData(PK_APP_B, nm.c_str(), argAt(line, 3).c_str(), o);
    else if (a1 == "out") { HeapBuf t(FUNZIONI_TESTO_MAX); appRunText(t); o.print(t.c_str()); }
    else o.println(tr("Uso: app list | app info <nome> | app start <nome> | app stop <nome> | app set <nome> <id> <valore> | app data <nome> [flash|sd] | app out"));
  }
  else if (c == "crash") {
    if (a1 == "clear") o.println(crashClear() ? tr("Rapporti dei crash cancellati") : tr("Errore"));
    else if (a1 == "test") {
      if (needConfirm(line, o, tr("Crash di prova: la scheda va in crash di proposito e si riavvia. Procedo?"))) return true;
      o.println(tr("Crash di prova: la scheda si riavvia e lascia il rapporto (comando crash)")); vlog("SISTEMA: crash di prova chiesto dalla shell"); drvDelayMs(300); abort();
    }
    else o.print(crashText().c_str());
  }
  else if (c == "who") { HeapBuf t(AUTH_TEXT_MAX); authWhoText(t); o.print(t.c_str()); shellSerialWho(o); }
  else if (c == "recovery") {
    if (!drvRecoveryPresent()) { o.println(tr("Recovery non presente: la scheda ha ancora la tabella partizioni vecchia.")); return true; }
    if (a1 == "update") {
      Word f = argAt(line, 2);
      char from[16], to[16]; ErrBuf err;
      if (f.empty()) { o.println(tr("Uso: recovery update <file.bin> <sha256>  (prima carica il file in Sistema > File)")); return true; }
      if (!drvPsramPresent() && needConfirm(line, o, tr("Questa scheda non ha PSRAM: il recovery viene scritto direttamente. Se si interrompe, il recovery resta inutilizzabile finche non lo riscrivi. Continuare?"))) return true;
      o.println(tr("Controllo e scrivo il recovery: non spegnere la scheda..."));
      Word shaW = argAt(line, 3);
      if (shaW == "yes") shaW = Word();                                 // "yes" in fondo e la conferma, non l'impronta
      if (shaW.length() != 64) { o.println(tr("L'impronta SHA-256 deve avere 64 cifre esadecimali")); return true; }   // obbligatoria (20/11/2026)
      if (recUpFromFile(f.c_str(), shaW.c_str(), "shell", from, to, sizeof from, err)) o.println(trf("Recovery aggiornato: %s -> %s", from[0] ? from : "?", to));
      else o.println(err.c_str());
      return true;
    }
    if (a1 != "now") { const char* v = recVersion(); o.println(trf("Recovery presente (versione %s). 'recovery now' = riavvia nel recovery, 'recovery update <file>' = aggiornalo.", v[0] ? v : "?")); return true; }
    if (!drvRecoveryEnter()) { o.println(tr("Non riesco a passare al recovery.")); return true; }
    o.println(tr("Riavvio nel recovery...")); sysReboot("recovery dalla shell", 300);
  }
  else return false;
  return true;
}
