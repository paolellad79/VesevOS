// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_sys.cpp
// Comandi della shell: stato, memoria, file, ora, CPU. Aiuti e include: vos_shell_int.h.
#include "vos_shell_int.h"
#include "../core/vos_eventbus.h"

bool shCmdSys(String c, String a1, const String& line, Print& o, int role) {
  if (c == "help" || c == "?") cmdHelp(o);
  else if (c == "uname") o.println(String(VOS_NAME) + " " + VOS_VERSION + " (ESP32-S3, core " + ESP.getSdkVersion() + ")");
  else if (c == "about") {
    o.println(String(VOS_NAME) + " " + VOS_VERSION + " - " + tr("Una piattaforma, mille schede."));
    o.println(String("GitHub: ") + VOS_GITHUB);
    o.println(String("(C) 2026 Domenico Paolella - GPL-3.0-or-later / ") + tr("licenza commerciale"));
  }
  else if (c == "serial-auth") {
    String a = a1; a.toLowerCase();
    if (a == "on" || a == "off") {
      cfg.serialAuth = (a == "on"); cfgSave();
      vlog("SICUREZZA: password sulla seriale %s", cfg.serialAuth ? "attivata" : "disattivata");
    } else if (a.length()) { o.println(tr("Uso: serial-auth on|off")); return true; }
    o.println(cfg.serialAuth ? tr("Password sulla seriale: ACCESA") : tr("Password sulla seriale: SPENTA"));
  }
  else if (c == "serial") {
    String k = a1; k.toLowerCase();
    String v = argAt(line, 2); v.toLowerCase();
    if (k == "keep") { o.println(serialKeep() ? tr("Velocita confermata") : tr("Nessuna velocita in prova")); return true; }
    if (k.length()) {
      if (k == "input" && (v == "off" || v == "0") && argAt(line, 3) != "yes") { o.println(tr("Attenzione: la seriale non accettera piu comandi (si riattiva da pagina web o con il tasto BOOT). Per confermare: serial input off yes")); return true; }
      String err;
      if (!serialSet(k, v, err)) { o.println(err); return true; }
    }
    o.print(serialText());
  }
  else if (c == "uptime") {
    o.println(trf("Acceso da: %s", uptimeStr(sysUptimeSec()).c_str()));
    o.println(trf("Ultimo reset: %s", sysResetReason().c_str()));
    o.println(trf("Avvii totali: %lu", (unsigned long)sysBootCount()));
    o.println(trf("Ore di vita: %lu h %lu min", (unsigned long)(sysLifeSec() / 3600UL), (unsigned long)((sysLifeSec() / 60UL) % 60UL)));
  }
  else if (c == "ram") {
    if (a1 == "mark") { ramMark(); o.println(tr("Memoria segnata: accendi o spegni un servizio e scrivi: ram diff")); }
    else if (a1 == "diff") { String d = ramDiff(); o.print(d.length() ? d : String(tr("Prima scrivi: ram mark")) + "\n"); }
    else o.print(ramReport());
  }
  else if (c == "events") {
    int n = a1.length() ? constrain((int)a1.toInt(), 1, 40) : 10;
    uint32_t last = EventBus::lastSeq();
    if (last == 0) { o.println(tr("Nessun evento")); return true; }
    Event ev[40]; uint32_t lost = 0;
    int got = EventBus::since(last > (uint32_t)n ? last - n : 0, ev, n, &lost);
    if (got == 0) { o.println(tr("Nessun evento")); return true; }
    for (int i = 0; i < got; i++) o.println(String("#") + ev[i].seq + "  " + String((unsigned long)(ev[i].ms / 1000)) + "s  " + ev[i].topic + "  " + ev[i].arg + "  " + ev[i].val);
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
    o.println(trf("RAM   libera %u KB su %u KB", (unsigned)(ESP.getFreeHeap() / 1024), (unsigned)(ESP.getHeapSize() / 1024)));
    o.println(trf("PSRAM libera %u KB su %u KB", (unsigned)(ESP.getFreePsram() / 1024), (unsigned)(ESP.getPsramSize() / 1024)));
    o.println(trf("RAM minima dall'avvio %u KB, blocco piu grande %u KB", (unsigned)(ESP.getMinFreeHeap() / 1024), (unsigned)(ESP.getMaxAllocHeap() / 1024)));
  }
  else if (c == "df") {
    o.println(trf("/flash  usati %u KB su %u KB", (unsigned)(drvFsUsed() / 1024), (unsigned)(drvFsTotal() / 1024)));
  }
  else if (c == "temp") o.println(trf("Temperatura CPU: %s", fmtTemp(sysCpuTemp()).c_str()));
  else if (c == "top") {
    o.println(trf("CPU %d%%  Temp %s  %u MHz", sysCpuPercent(), fmtTemp(sysCpuTemp()).c_str(), (unsigned)getCpuFrequencyMhz()));
    o.println(trf("RAM libera %u KB  PSRAM libera %u KB", (unsigned)(ESP.getFreeHeap() / 1024), (unsigned)(ESP.getFreePsram() / 1024)));
  }
  else if (c == "ps") o.print(sysTasksText());
  else if (c == "kill") {
    String err;
    if (a1.length() == 0) o.println(tr("Uso: kill <nome task>"));
    else if (sysTaskKill(a1, err)) o.println(trf("Task '%s' fermato (start %s per farlo ripartire)", a1.c_str(), a1.c_str()));
    else o.println(err);
  }
  else if (c == "start" || c == "restart") {
    String err;
    if (a1.length() == 0) o.println(tr("Uso: start <nome task>  /  restart <nome task>"));
    else if (sysTaskRestart(a1, err)) o.println(trf("Task '%s' avviato", a1.c_str()));
    else o.println(err);
  }
  else if (c == "rules") {
    String a2 = argAt(line, 2);
    if (a1 == "run") {
      String err;
      if (rulesRunNow(a2.toInt() - 1, err)) o.println(trf("Regola %d avviata", (int)a2.toInt()));
      else o.println(trf("Errore: %s", err.c_str()));
    } else {
      String t = rulesText();
      if (t.length() == 0) o.println(tr("Nessuna regola"));
      else {
        int n = 0, p = 0;
        while (p < (int)t.length()) {
          int e = t.indexOf('\n', p); if (e < 0) e = t.length();
          String l = t.substring(p, e); l.trim(); p = e + 1;
          if (!l.length() || l[0] == '#') continue;
          n++;
          o.println(String(n) + "  " + l);
        }
      }
    }
  }
  else if (c == "boot-order") {
    if (a1 == "reset") { bootResetOrder(); o.println(tr("Ordine di avvio predefinito (vale dal prossimo riavvio)")); }
    else if (a1 == "" || a1 == "lista") o.println(bootJson());
    else {
      String res, err;
      if (bootSetOrder(a1, res, err)) o.println(trf("Ordine salvato: %s (vale dal prossimo riavvio)", res.c_str()));
      else o.println(trf("Errore: %s", err.c_str()));
    }
  }
  else if (c == "ls") {
    String j = fsListJson(a1.length() ? a1 : "/");
    if (j.indexOf("\"ok\":true") < 0) { o.println(tr("Cartella non trovata")); }
    else {
      // lista semplice: scorro il JSON {"n":"..","d":bool,"s":N}
      int pos = 0;
      while ((pos = j.indexOf("{\"n\":\"", pos)) >= 0) {
        int e = j.indexOf("\",\"d\":", pos);
        String nm = j.substring(pos + 6, e);
        bool dir = j.startsWith("true", e + 7);
        int sp = j.indexOf("\"s\":", e); int se = j.indexOf('}', sp);
        o.println(String(dir ? "[D] " : "    ") + nm + (dir ? "" : "  " + j.substring(sp + 4, se) + " B"));
        pos = se;
      }
    }
  }
  else if (c == "cat") {
    if (!a1.length()) { o.println(tr("Uso: cat <file>")); return true; }
    String t, err;
    if (!fsReadText(a1, t, err)) { o.println(err); return true; }
    o.println(t);
  }
  else if (c == "mkdir") {
    String err; if (!a1.length()) { o.println(tr("Uso: mkdir <cartella>")); return true; }
    o.println(fsMkdir(a1, err) ? String(tr("Creata")) : err);
  }
  else if (c == "rm") {
    String err; if (!a1.length()) { o.println(tr("Uso: rm <file o cartella>")); return true; }
    o.println(fsRemove(a1, err) ? String(tr("Eliminato")) : err);
  }
  else if (c == "mv" || c == "cp") {
    String a2 = argAt(line, 2), err;
    if (!a1.length() || !a2.length()) { o.println(trf("Uso: %s <da> <a>", c.c_str())); return true; }
    bool r = (c == "mv") ? fsRename(a1, a2, err) : fsCopy(a1, a2, err);
    o.println(r ? String(tr("Fatto")) : err);
  }
  else if (c == "write") {
    String txt = restFrom(line, 2), err;
    if (!a1.length()) { o.println(tr("Uso: write <file> <testo>")); return true; }
    o.println(fsWriteText(a1, txt, err) ? String(tr("Scritto")) : err);
  }
  else if (c == "date") {
    if (a1 == "set") { String err; if (timeSetLocal(restFrom(line, 2), err)) o.println(timeNowStr()); else o.println(err); }
    else o.println(timeNowStr() + "  (" + cfg.tzName + ")");
  }
  else if (c == "ntp") {
    if (a1 == "on" || a1 == "off") { cfg.ntpOn = (a1 == "on"); cfg.ntpAutoOff = false; cfgSave(); timeApply(); o.println(cfg.ntpOn ? tr("NTP acceso") : tr("NTP spento")); }
    else if (a1 == "sync") { timeApply(); o.println(tr("Sincronizzazione NTP richiesta")); }
    else if (a1 == "server" || a1 == "server2") {
      String v = argAt(line, 2);
      bool two = (a1 == "server2"), off = two && (v == "off" || v == "-");
      if (!v.length() || (!off && !utilHostOk(v))) { o.println(tr("Uso: ntp server <nome>  |  ntp server2 <nome|off>  (solo lettere, numeri, punto e trattino)")); return true; }
      if (two) cfg.ntpServer2 = off ? String("") : v; else cfg.ntpServer = v;
      cfgSave(); timeApply();
      o.println(trf("Server NTP: %s", cfg.ntpServer.c_str()) + "\n" + trf("Secondo server NTP: %s", cfg.ntpServer2.length() ? cfg.ntpServer2.c_str() : tr("nessuno")));
    }
    else if (a1 == "every") {
      long m = argAt(line, 2).toInt();
      if (!argAt(line, 2).length() || !timeEveryValid(m)) { o.println(tr("Uso: ntp every 0|15|60|360|720|1440|10080  (minuti, 0 = solo all'avvio)")); return true; }
      cfg.ntpEvery = m; cfgSave(); timeApply(); o.println(trf("Sincronizzazione NTP ogni %ld minuti", m));
    }
    else o.println(timeJson());
  }
  else if (c == "cpu") {
    if (!a1.length()) { o.println(cfg.cpuMhz ? trf("CPU a %u MHz, modo %u fisso", (unsigned)getCpuFrequencyMhz(), (unsigned)cfg.cpuMhz) : trf("CPU a %u MHz, modo automatico", (unsigned)getCpuFrequencyMhz())); return true; }
    int m = (a1 == "auto") ? 0 : a1.toInt();
    if (m != 0 && m != 80 && m != 160 && m != 240) { o.println(tr("Uso: cpu auto|80|160|240")); return true; }
    cfg.cpuMhz = m; cfgSave(); sysApplyCpuMode(); o.println(trf("Modo CPU: %s", a1.c_str()));
  }
  else return false;
  return true;
}
