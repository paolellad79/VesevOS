// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_sys.cpp
// Comandi della shell: stato, memoria, ora. cpu: vos_shell_cpu.cpp; help, man, df, uname e gli altri di base: vos_shell_basic.cpp. Include: vos_shell_int.h.
#include "vos_shell_int.h"
#include "../core/vos_eventbus.h"
#include "../core/vos_heapbuf.h"
#include <ctype.h>
#include <string_view>
#include "../core/vos_clamp.h"
#include "../drivers/vos_drv_cpu.h"
#include "../drivers/vos_drv_heap.h"
#include "../drivers/vos_drv_sys.h"
#include "../core/vos_config.h"

bool shCmdSys(const Word& c, const Word& a1, const char* line, OutSink& o, int role) {
  (void)role;                                   // nessun comando di questo gruppo cambia comportamento col ruolo (lo decide la tabella dei ruoli)
  if (c == "about") {
    o.printf("%s %s - %s\r\n", VOS_NAME, VOS_VERSION, tr("Una piattaforma, mille schede."));
    o.printf("GitHub: %s\r\n", VOS_GITHUB);
    o.printf("(C) 2026 Domenico Paolella - GPL-3.0-or-later / %s\r\n", tr("licenza commerciale"));
  }
  else if (c == "serial-auth") {
    Word a = a1; a.lower();
    if (a == "on" || a == "off") {
      cfg.serialAuth = (a == "on"); cfgSave();      // acceso: la seriale chiede il login (utente, password, MFA)
      vlog("SICUREZZA: password sulla seriale %s", cfg.serialAuth ? "attivata" : "disattivata");
    } else if (!a.empty()) { o.println(tr("Uso: serial-auth on|off")); return true; }
    o.println(cfg.serialAuth ? tr("Password sulla seriale: ACCESA") : tr("Password sulla seriale: SPENTA"));
  }
  else if (c == "serial") {
    Word k = a1; k.lower();
    Word v = argAt(line, 2); v.lower();
    if (k == "idle") {                                  // uscita automatica per inattivita (solo Admin: tabella dei ruoli); 0 = mai
      if (!v.empty()) {
        long m = v.toInt();
        if (m < 0 || m > 1440 || (m == 0 && v != "0")) { o.println(tr("Uso: serial idle <minuti 0-1440> (0 = mai)")); return true; }
        cfg.serialIdleMin = (uint16_t)m; cfgSave();
        vlog("SICUREZZA: uscita della seriale per inattivita: %ld minuti", m);
      }
      if (cfg.serialIdleMin) o.println(trf("Uscita per inattivita: %u minuti", (unsigned)cfg.serialIdleMin));
      else o.println(tr("Uscita per inattivita: spenta"));
      return true;
    }
    if (k == "keep") { o.println(serialKeep() ? tr("Velocita confermata") : tr("Nessuna velocita in prova")); return true; }
    if (!k.empty()) {
      if (k == "input" && (v == "off" || v == "0") && argAt(line, 3) != "yes") { o.println(tr("Attenzione: la seriale non accettera piu comandi (si riattiva da pagina web o con il tasto BOOT). Per confermare: serial input off yes")); return true; }
      ErrBuf err;
      if (!serialSet(k.c_str(), v.c_str(), err)) { o.println(err.c_str()); return true; }
    }
    o.print(serialText().c_str());
  }
  else if (c == "uptime") {
    char up[64]; uptimeTo(sysUptimeSec(), up, sizeof up);
    o.println(trf("Acceso da: %s", up));
    o.println(trf("Ultimo reset: %s", sysResetText()));
    o.println(trf("Avvii totali: %lu", (unsigned long)sysBootCount()));
    { char dur[48]; durStr(sysLifeSec(), dur, sizeof dur); o.println(trf("Ore di vita: %s", dur)); }
  }
  else if (c == "ram") {
    if (a1 == "mark") { ramMark(); o.println(tr("Memoria segnata: accendi o spegni un servizio e scrivi: ram diff")); }
    else if (a1 == "diff") { std::string d = ramDiff(); if (d.empty()) { o.print(tr("Prima scrivi: ram mark")); o.print("\n"); } else o.print(d.c_str()); }
    else if (a1 == "audit") o.print(ramAudit().c_str());
    else o.print(ramReport().c_str());
  }
  else if (c == "events") {
    int n = !a1.empty() ? vosClamp((int)a1.toInt(), 1, 40) : 10;
    uint32_t last = EventBus::lastSeq();
    if (last == 0) { o.println(tr("Nessun evento")); return true; }
    Event ev[40]; uint32_t lost = 0;
    int got = EventBus::since(last > (uint32_t)n ? last - n : 0, ev, n, &lost);
    if (got == 0) { o.println(tr("Nessun evento")); return true; }
    for (int i = 0; i < got; i++) o.printf("#%lu  %lus  %s  %s  %ld\r\n", (unsigned long)ev[i].seq, (unsigned long)(ev[i].ms / 1000), ev[i].topic, ev[i].arg, (long)ev[i].val);
  }
  else if (c == "free" && a1 == "detail") {
    multi_heap_info_t in, ps; heap_caps_get_info(&in, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); heap_caps_get_info(&ps, MALLOC_CAP_SPIRAM);
    o.println(trf("RAM interna: libera %u KB, pezzo piu grande %u KB, minima %u KB", (unsigned)(in.total_free_bytes / 1024), (unsigned)(in.largest_free_block / 1024), (unsigned)(in.minimum_free_bytes / 1024)));
    o.println(trf("  blocchi: %u occupati (%u KB), %u liberi", (unsigned)in.allocated_blocks, (unsigned)(in.total_allocated_bytes / 1024), (unsigned)in.free_blocks));
    unsigned fr = in.total_free_bytes ? (unsigned)(100UL - (100UL * in.largest_free_block) / in.total_free_bytes) : 0;
    o.println(trf("  frammentazione: %u%% (0 = tutto in un pezzo; sopra 60 la RAM e a pezzi piccoli)", fr));
    o.println(trf("PSRAM: libera %u KB, pezzo piu grande %u KB", (unsigned)(ps.total_free_bytes / 1024), (unsigned)(ps.largest_free_block / 1024)));
  }
  else if (c == "free") {
    o.println(trf("RAM   libera %u KB su %u KB", (unsigned)(drvHeapFree() / 1024), (unsigned)(drvHeapTotal() / 1024)));
    o.println(trf("PSRAM libera %u KB su %u KB", (unsigned)(drvPsramFree() / 1024), (unsigned)(drvPsramTotal() / 1024)));
    o.println(trf("RAM minima dall'avvio %u KB, blocco piu grande %u KB", (unsigned)(drvHeapMinFree() / 1024), (unsigned)(drvHeapMaxBlock() / 1024)));
  }
  else if (c == "temp") { char tt[16]; fmtTemp(sysCpuTemp(), tt, sizeof tt); o.println(trf("Temperatura CPU: %s", tt)); }
  else if (c == "top") {
    char tt[16]; fmtTemp(sysCpuTemp(), tt, sizeof tt);
    o.println(trf("CPU %d%%  Temp %s  %u MHz", sysCpuPercent(), tt, (unsigned)drvCpuMhz()));
    o.println(trf("RAM libera %u KB  PSRAM libera %u KB", (unsigned)(drvHeapFree() / 1024), (unsigned)(drvPsramFree() / 1024)));
  }
  else if (c == "ps") {
    HeapBuf txt(8192);                                               // il testo si scrive qui (heap), non sullo stack
    if (!sysTasksText(txt)) o.println(tr("Memoria insufficiente")); else o.print(txt.c_str());
  }
  else if (c == "kill") {
    char err[96];
    if (a1.empty()) o.println(tr("Uso: kill <nome task>"));
    else if (sysTaskKill(a1.c_str(), err, sizeof err)) o.println(trf("Task '%s' fermato (start %s per farlo ripartire)", a1.c_str(), a1.c_str()));
    else o.println(err);
  }
  else if (c == "start" || c == "restart") {
    char err[96];
    if (a1.empty()) o.println(tr("Uso: start <nome task>  /  restart <nome task>"));
    else if (sysTaskRestart(a1.c_str(), err, sizeof err)) o.println(trf("Task '%s' avviato", a1.c_str()));
    else o.println(err);
  }
  else if (c == "rules") {
    Word a2 = argAt(line, 2);
    if (a1 == "run") {
      ErrBuf err;
      if (rulesRunNow(a2.toInt() - 1, err)) o.println(trf("Regola %d avviata", (int)a2.toInt()));
      else o.println(trf("Errore: %s", err.c_str()));
    } else {
      HeapBuf t(RULES_READ_MAX);
      if (!rulesText(t)) { o.println(tr("Memoria insufficiente")); return true; }
      std::string_view all(t.c_str(), t.length());
      if (all.empty()) o.println(tr("Nessuna regola"));
      else {
        int n = 0;
        size_t p = 0;
        while (p < all.size()) {
          size_t e = all.find('\n', p); if (e == std::string_view::npos) e = all.size();
          std::string_view l = all.substr(p, e - p);
          p = e + 1;
          while (!l.empty() && isspace((unsigned char)l.front())) l.remove_prefix(1);      // riga senza spazi ai lati
          while (!l.empty() && isspace((unsigned char)l.back())) l.remove_suffix(1);
          if (l.empty() || l[0] == '#') continue;
          n++;
          o.printf("%d  ", n);
          o.write((const uint8_t*)l.data(), l.size());
          o.println();
        }
      }
    }
  }
  else if (c == "boot-order") {
    if (a1 == "reset") { bootResetOrder(); o.println(tr("Ordine di avvio predefinito (vale dal prossimo riavvio)")); }
    else if (a1 == "" || a1 == "list" || a1 == "lista") { HeapBuf j(BOOT_JSON_MAX); if (bootJson(j)) o.println(j.c_str()); }
    else {
      char res[96]; ErrBuf err;
      if (bootSetOrder(a1.c_str(), res, sizeof res, err)) o.println(trf("Ordine salvato: %s (vale dal prossimo riavvio)", res));
      else o.println(trf("Errore: %s", err.c_str()));
    }
  }
  // ls, cat, mkdir, rm, mv, cp, write, cd, pwd: vos_shell_files.cpp
  else if (c == "date") {
    char now[32];
    if (a1 == "set") { ErrBuf err; if (timeSetLocal(restFrom(line, 2), err)) { timeNowFmt(now, sizeof now); o.println(now); } else o.println(err.c_str()); }
    else { timeNowFmt(now, sizeof now); o.print(now); o.printf("  (%s)\r\n", cfg.tzName.c_str()); }
  }
  else if (c == "ntp") {
    if (a1 == "on" || a1 == "off") { cfg.ntpOn = (a1 == "on"); cfg.ntpAutoOff = false; cfgSave(); timeApply(); o.println(cfg.ntpOn ? tr("NTP acceso") : tr("NTP spento")); }
    else if (a1 == "sync") { timeApply(); o.println(tr("Sincronizzazione NTP richiesta")); }
    else if (a1 == "server" || a1 == "server2") {
      Word v = argAt(line, 2);
      bool two = (a1 == "server2"), off = two && (v == "off" || v == "-");
      if (v.empty() || (!off && !utilHostOk(v.c_str()))) { o.println(tr("Uso: ntp server <nome>  |  ntp server2 <nome|off>  (solo lettere, numeri, punto e trattino)")); return true; }
      if (two) cfg.ntpServer2 = off ? "" : v.c_str(); else cfg.ntpServer = v.c_str();
      cfgSave(); timeApply();
      o.println(trf("Server NTP: %s", cfg.ntpServer.c_str()));
      o.println(trf("Secondo server NTP: %s", cfg.ntpServer2.length() ? cfg.ntpServer2.c_str() : tr("nessuno")));
    }
    else if (a1 == "every") {
      Word a2 = argAt(line, 2);
      long m = a2.toInt();
      if (a2.empty() || !timeEveryValid(m)) { o.println(tr("Uso: ntp every 0|15|60|360|720|1440|10080  (minuti, 0 = solo all'avvio)")); return true; }
      cfg.ntpEvery = m; cfgSave(); timeApply(); o.println(trf("Sincronizzazione NTP ogni %ld minuti", m));
    }
    else { HeapBuf j(TIME_JSON_MAX); if (timeJson(j)) o.println(j.c_str()); }
  }
  else return false;
  return true;
}
