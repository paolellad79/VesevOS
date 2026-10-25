// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_cmdtab.cpp
// La tabella unica dei comandi (vedi vos_shell_cmdtab.h). UNA riga per comando, in ordine alfabetico del nome ufficiale (inglese).
//   alias : i vecchi nomi che fanno la stessa cosa (restano validi ma nascosti: canonicalName li trasforma nel nome ufficiale)
//   uso   : la sintassi, scritta cosi com'e (placeholder in italiano)
//   breve : la descrizione per l'elenco di help (si traduce quando si stampa)
//   Tab   : le parole che il Tab propone subito dopo il comando; piu a fondo (terza parola in poi) vedi vos_shell_complete_data.cpp
//   A     : alias VISIBILE: riga a parte con il suo nome in help e nel Tab; il campo alias e il nome ufficiale a cui rimanda (nessun codice suo)
//   H     : comando NASCOSTO (variante o vecchio nome che le regole e gli script usano ancora): valido, ma non in help ne' nel Tab
// Per aggiungere un comando: una riga qui, una riga in vos_shell_roles.cpp (senza riga e solo Admin), il codice del comando, e se serve il
// testo lungo di man in vos_shell_man.cpp.
#include "vos_shell_cmdtab.h"
#include "../core/vos_common.h"
#include <string.h>

namespace {

const uint8_t H = shx::CMD_HIDDEN;
const uint8_t A = shx::CMD_ALIAS_ROW;

const shx::CmdInfo kCmds[] = {
  // nome, alias, uso, breve, Tab, flag
  {"about", "", "about", TRK("chi siamo, licenza e indirizzo GitHub"), "", 0},
  {"airplane", "aereo", "airplane [on <come>|off]", TRK("modalita aereo (radio spenta)"), "on off", 0},
  {"ap", "", "ap [show|new]", TRK("hotspot: mostra o rinnova la password"), "show new", 0},
  {"app", "", "app [list|info|start|stop|set|out]", TRK("app salvate in /apps (.vap)"), "list info start stop set out", 0},
  {"audit", "", "audit [ack [n]]", TRK("allarmi e registro delle modifiche"), "ack", 0},
  {"ban", "", "ban", TRK("indirizzi bloccati o sospetti"), "", 0},
  {"benchsha", "", "benchsha", TRK("misura SHA-256 e formula password (diagnostica)"), "", 0},
  {"berry", "", "berry [codice]", TRK("prova codice Berry al volo"), "", 0},
  {"ble", "", "ble [on [min]|off]", TRK("Bluetooth per telefoni e app"), "on off status", 0},
  {"boot-order", "", "boot-order [list|reset]", TRK("ordine di avvio dei servizi"), "list reset", 0},
  {"btn", "", "btn [1|2|3 none|airplane|wps|find]", TRK("azioni dei click del tasto BOOT"), "1 2 3", 0},
  {"cat", "", "cat <file>", TRK("mostra un file"), "", 0},
  {"cd", "", "cd [percorso]", TRK("cambia cartella"), "", 0},
  {"cert", "", "cert [new]", TRK("certificato HTTPS: impronta o nuovo"), "new", 0},
  {"config", "", "config", TRK("mostra la configurazione (senza password)"), "", 0},
  {"cp", "", "cp <da> <a>", TRK("copia un file"), "", 0},
  {"cpu", "", "cpu [info|governor|freq|why|set]", TRK("processore: governor, frequenza, temperatura"), "info governor freq why set 80 160 240 ondemand conservative schedutil", 0},
  {"crash", "", "crash [clear|test]", TRK("rapporti degli ultimi crash"), "clear test", 0},
  {"date", "", "date [set AAAA-MM-GG HH:MM]", TRK("data e ora"), "set", 0},
  {"dev", "", "dev [status <nome>|on <nome>|off <nome>|act <nome> <azione> [arg]]", TRK("periferiche (MQTT, Bluetooth, pin, LED...)"), "status on off act", 0},
  {"df", "", "df [-h]", TRK("spazio su flash e microSD"), "-h", 0},
  {"diag", "", "diag", TRK("diagnosi: guida, rete, servizi, RAM, flash"), "", 0},
  {"domain", "", "domain [nome|-]", TRK("mostra, cambia o toglie il dominio"), "", 0},
  {"echo", "", "echo [-n] [testo]", TRK("scrive il testo"), "-n", 0},
  {"events", "", "events [n]", TRK("ultimi eventi del sistema (Event Bus)"), "", 0},
  {"exit", "logout", "exit", TRK("chiude la sessione (seriale)"), "", 0},
  {"factory-reset", "", "factory-reset", TRK("azzera tutto e riavvia (chiede conferma)"), "yes", 0},
  {"firewall", "", "firewall [off|confirm|mode <0-3>|add <ip|a-b|rete/nn> [nome]|del <n>|test <ip>]", TRK("filtro degli indirizzi IP"), "off confirm mode add del test", 0},
  {"free", "", "free [detail]", TRK("memoria RAM e PSRAM"), "detail", 0},
  {"gunzip", "", "gunzip [-c] [-k] [-f] file.gz ...", TRK("decomprime file .gz"), "-c -k -f", 0},
  {"gzip", "", "gzip [-d] [-c] [-k] [-f] [-1..-9] file ...", TRK("comprime file (.gz) o li decomprime con -d"), "-d -c -k -f -1 -9", 0},
  {"help", "?", "help [comando]", TRK("elenco dei comandi, o aiuto su uno"), "@commands", 0},
  {"hostname", "", "hostname [nome]", TRK("mostra o cambia il nome host"), "", 0},
  {"id", "", "id", TRK("utente, ruolo e identificativi simulati"), "", 0},
  {"info", "", "info", TRK("riassunto: versione, chip, memoria, rete"), "", 0},
  {"io", "", "io [list|add <classe> <pin> [nome]|del <nome>|rename <nome> <nuovo>|set <nome> <0|1>|get <nome>]", TRK("periferiche a classi (relay, led, pin, input...)"), "list add del rename set get", 0},
  {"ip", "", "ip", TRK("rete e indirizzo"), "", 0},
  {"kill", "", "kill <nome>", TRK("ferma un task"), "", 0},
  {"lang", "", "lang [codice]", TRK("mostra o cambia la lingua"), "", 0},
  {"led", "", "led [trigger [nome]|color [sorgente] <RRGGBB>|brightness <0-255>|threshold <10-95>|state|heartbeat|fixed|off]", TRK("LED: trigger, colori, luminosita"), "trigger color brightness threshold state heartbeat fixed off", 0},
  {"led-bright", "", "led-bright <0-255>", TRK("vecchio nome di 'led brightness'"), "", H},
  {"led-color", "", "led-color <RRGGBB>", TRK("vecchio nome di 'led color'"), "", H},
  {"legal", "", "legal", TRK("note legali: licenze, dati salvati, sicurezza"), "", 0},
  {"license", "licenza", "license [id]", TRK("licenze e note legali"), "notice gpl3 apache2 mit bsd3", 0},
  {"log", "", "log [n|clear|level [0-3]]", TRK("registro: ultime righe, svuota, livello"), "clear level", 0},
  {"ls", "", "ls [-l] [-a] [-h] [-1] [percorso ...]", TRK("elenco dei file"), "", 0},
  {"lscpu", "", "lscpu [-v]", TRK("come 'cpu info'"), "-v", H},
  {"man", "", "man <comando>", TRK("manuale di un comando"), "@commands", 0},
  {"mesh", "", "mesh [start|stop|send <scheda|*> <testo>|cmd <scheda> <azione>|key new|role <0|1|2>]", TRK("rete tra schede"), "start stop send cmd key role", 0},
  {"mkdir", "", "mkdir <nome>", TRK("crea una cartella"), "", 0},
  {"mod", "", "mod [list|info <nome>|install <nome> [prefisso]|del <nome>]", TRK("schede modulo .vdp da installare sui pin"), "list info install del", 0},
  {"mqtt", "", "mqtt [start|stop|restart|pub <argomento> <testo>]", TRK("servizio MQTT"), "start stop restart pub", 0},
  {"mv", "", "mv <da> <a>", TRK("sposta o rinomina"), "", 0},
  {"net", "", "net", TRK("rete: IP, MAC, segnale"), "", 0},
  {"ntp", "", "ntp [on|off|sync|every <min>]", TRK("ora da internet (NTP)"), "on off sync every", 0},
  {"passwd", "", "passwd <utente>", TRK("cambia la password di un utente"), "", 0},
  {"pin", "", "pin <n> [high|low|blink|read [up|down]|off]", TRK("prova un pin (si spegne da solo)"), "", 0},
  {"ping", "", "ping [-c n] [-i secondi] [-W secondi] [-s byte] [-t ttl] host", TRK("prova se un indirizzo risponde (ICMP echo)"), "-c -i -W -s -t", 0},
  {"pins", "", "pins", TRK("pin usati"), "", 0},
  {"power", "", "power [off|wifi|cycle <sveglia> <sonno>]", TRK("risparmio energia"), "off wifi cycle", 0},
  {"poweroff", "", "poweroff [yes]", TRK("sonno profondo senza sveglia (solo RESET)"), "yes", 0},
  {"ps", "", "ps", TRK("elenco dei task"), "", 0},
  {"pwd", "", "pwd", TRK("cartella corrente"), "", 0},
  {"ram", "", "ram [mark|diff|audit]", TRK("stack dei task e costo dei servizi"), "mark diff audit", 0},
  {"reboot", "", "reboot", TRK("riavvia la scheda"), "", 0},
  {"reboots", "", "reboots [clear]", TRK("diario degli ultimi 20 riavvii"), "clear", 0},
  {"recovery", "", "recovery [now|update <file>]", TRK("recovery: stato, avvio, aggiornamento"), "now update", 0},
  {"region", "locale", "region [country XX|tx <dBm>|antenna int|ext <dBi>]", TRK("paese, potenza e antenna della radio"), "country tx antenna", 0},
  {"rm", "", "rm <nome>", TRK("elimina un file o una cartella"), "", 0},
  {"rules", "", "rules [run <n>]", TRK("automazioni: elenco o esegui una regola"), "run", 0},
  {"script", "", "script [list|run <nome>|stop|out]", TRK("script salvati in /scripts (.vsc)"), "list run stop out", 0},
#if VOS_WITH_SD
  {"sd", "", "sd [status|mount|unmount|test|ls [cartella]|read <file>|write <file> <testo>|append <file> <testo>|rm <file>|mkdir <cartella>]", TRK("scheda microSD"), "status mount unmount test ls read write append rm mkdir", 0},
#endif
  {"selftest", "", "selftest [active] [names]", TRK("autodiagnosi con rapporto"), "active names", 0},
  {"serial", "", "serial [baud <n>|eol crlf|lf|cr|echo|input|log|banner on|off|tx <ms>|idle <min>|keep]", TRK("impostazioni della seriale"), "baud eol echo input log banner tx keep idle", 0},
  {"serial-auth", "", "serial-auth [on|off]", TRK("login sulla seriale: mostra o cambia"), "on off", 0},
  {"setup", "", "setup [done]", TRK("configurazione guidata da seriale"), "done", 0},
  {"sleep", "", "sleep [minuti]", TRK("vecchio nome di 'suspend'"), "", H},
  {"start", "restart", "start <nome>", TRK("avvia o riavvia un task fermato"), "", 0},
  {"stats", "", "stats [on|off|reset]", TRK("statistiche locali (spente di fabbrica)"), "on off reset", 0},
  {"suspend", "", "suspend [minuti]", TRK("sonno profondo (chiede conferma)"), "yes", 0},
  {"svc", "", "svc [ap|captive|http|https|redirect|dhcp|mdns on|off] [http-port|https-port <n>]", TRK("servizi di rete e porte"), "ap captive http https redirect dhcp mdns http-port https-port", 0},
  {"tar", "", "tar -c|-t|-x [-v] -f archivio [-C cartella] [--overwrite] [file ...]", TRK("archivi tar: crea, elenca, estrae"), "-c -t -x -v -f -C --overwrite", 0},
  {"temp", "", "temp", TRK("temperatura della CPU"), "", 0},
  {"time", "", "time <comando ...>", TRK("misura quanto dura un comando"), "@commands", 0},
  {"top", "", "top", TRK("CPU, RAM, temperatura"), "", 0},
  {"uname", "", "uname [-a|-s|-n|-r|-v|-m]", TRK("nome e versione del sistema"), "-a -s -n -r -v -m", 0},
  {"unban", "", "unban <ip|all>", TRK("sblocca un indirizzo (o tutti)"), "all", 0},
  {"unzip", "", "unzip [-l] [-o] [-d cartella] archivio.zip [file ...]", TRK("elenca o estrae un archivio .zip"), "-l -o -d", 0},
  {"uptime", "", "uptime", TRK("da quanto e acceso (+ motivo reset, avvii)"), "", 0},
  {"user", "users", "user [add <nome> <admin|oper|guest>|del <nome>|role <nome> <ruolo>|on <nome>|off <nome>]", TRK("utenti: elenco e modifiche"), "add del role on off", 0},
  {"vesevos-backup", "", "vesevos-backup [status|list|create [nome]|restore <nome>|delete <nome>]", TRK("backup cifrato della configurazione (con frase) in /backup"), "status list create restore delete", 0},
  {"vesevos-bugreport", "", "vesevos-bugreport [-f] [-n <righe>]", TRK("rapporto per l'assistenza (senza segreti)"), "-f -n", 0},
  {"vesevos-cleaner", "", "vesevos-cleaner [-y] [tmp|crash|log|reboots|cache ...]", TRK("pulizia sicura: prova a secco, poi con -y e conferma"), "-y tmp crash log reboots cache", 0},
  {"vesevos-config", "", "vesevos-config [setup|system|network|region|time|services|users|security|led|cpu] [comando ...]", TRK("configurazione per aree: riepilogo, dettagli e modifiche"), "setup system network region time services users security led cpu", 0},
  {"vesevos-cpuinfo", "", "vesevos-cpuinfo [-v]", TRK("riepilogo di cpu e cpu info in una vista"), "-v", 0},
  {"vesevos-drive_manager", "", "vesevos-drive_manager [list|status|mount|unmount|test]", TRK("volumi: memoria interna e microSD (stato, spazio, monta, smonta, prova)"), "list status mount unmount test", 0},
  {"vesevos-led_control", "led", "vesevos-led_control [come led]", TRK("alias di 'led' (trigger, colori, luminosita)"), "trigger color brightness threshold state heartbeat fixed off", A},
  {"vesevos-network", "", "vesevos-network [info] [-f] | scan | ping <host>", TRK("riepilogo di rete in testo (senza segreti)"), "info scan ping -f", 0},
  {"vesevos-services", "", "vesevos-services [<servizio> [start|stop|restart|status]]", TRK("servizi con stato, porta e RAM"), "http https redirect ap captive dhcp mdns wifi wps mqtt mesh ble ntp rules script wd", 0},
  {"vesevos-update", "", "vesevos-update [status|file [ip-del-computer]|net <url-https> <sha256> [versione]]", TRK("aggiornamento tramite recovery: stato, da file, da indirizzo https"), "status file net", 0},
  {"wd", "watchdog", "wd [status|set twdt <s>|set <servizio> <s>|defaults]", TRK("watchdog: stato e tempi"), "status set defaults", 0},
  {"welcome", "", "welcome", TRK("schermata di benvenuto"), "", 0},
  {"who", "", "who", TRK("chi e collegato (web e seriale)"), "", 0},
  {"whoami", "", "whoami", TRK("nome dell'utente della sessione"), "", 0},
  {"wifi", "", "wifi [scan|set <rete>|off]", TRK("stato della Wi-Fi, ricerca, collega, scollega"), "scan set off", 0},
  {"wifi-scan", "", "wifi-scan", TRK("vecchio nome di 'wifi scan'"), "", H},
  {"wps", "", "wps [on|off|pin on|off|start [pin]|stop]", TRK("collegamento alla Wi-Fi di casa con il WPS"), "on off pin start stop", 0},
  {"write", "", "write <file> <testo>", TRK("scrive un file di testo"), "", 0},
  {"zip", "", "zip [-r] [-0..-9] [-q] [-f] [-j] archivio.zip file ...", TRK("crea un archivio .zip"), "-r -q -f -j -0 -1 -9", 0},
  {"zcat", "", "zcat file.gz ...", TRK("mostra un file .gz (solo testo)"), "", 0},
};

const size_t kCmdsN = sizeof(kCmds) / sizeof(kCmds[0]);

// La parola w e una di quelle della lista (separate da spazi)?
bool inList(const char* list, const char* w) {
  size_t wl = strlen(w);
  while (*list) {
    while (*list == ' ') list++;
    const char* b = list;
    while (*list && *list != ' ') list++;
    if ((size_t)(list - b) == wl && strncmp(b, w, wl) == 0) return true;
  }
  return false;
}

}  // namespace

size_t shx::cmdCount() { return kCmdsN; }

const shx::CmdInfo& shx::cmdAt(size_t i) { return kCmds[i < kCmdsN ? i : 0]; }

const shx::CmdInfo* shx::cmdFind(const char* name) {
  if (!name || !*name) return nullptr;
  for (const CmdInfo& c : kCmds)
    if (strcmp(c.name, name) == 0) return &c;
  for (const CmdInfo& c : kCmds)
    if (c.alias[0] && inList(c.alias, name)) return &c;
  return nullptr;
}

void shx::canonicalName(Word& cmd) {
  const CmdInfo* c = cmdFind(cmd.c_str());
  if (c && (c->flags & CMD_ALIAS_ROW)) { cmd = Word(c->alias, strlen(c->alias)); return; }     // alias visibile (vesevos-led_control): il comando vero e c->alias
  if (c && strcmp(c->name, cmd.c_str()) != 0) cmd = Word(c->name, strlen(c->name));
}
