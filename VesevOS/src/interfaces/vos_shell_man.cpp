// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_man.cpp
// Aiuto della shell: l'elenco "help", "help <comando>" e "man <comando>". Tutto viene dalla tabella unica dei comandi
// (vos_shell_cmdtab.cpp) e dai ruoli (vos_shell_roles.cpp): niente e copiato. Qui stanno anche i testi lunghi di man.
// MANCANO i testi lunghi (resta sintassi + descrizione breve, alias, ruolo e parole) dei comandi non elencati qui sotto:
// per aggiungerne uno basta una riga nella tabella kMan (al massimo 4 righe di testo, poi gli esempi con \n tra l'uno e l'altro).
#include "vos_shell_cmdtab.h"
#include "vos_shell_roles.h"
#include "../core/vos_common.h"
#include "../core/vos_i18n.h"
#include <string.h>

namespace {

using shx::CmdInfo;

struct ManText {
  const char* name;
  const char* lines[5];      // righe di descrizione (chiavi tr), chiuse da nullptr
  const char* examples;      // esempi di comandi, separati da '\n' (non si traducono)
};

const ManText kMan[] = {
  {"man",
    {TRK("Mostra il manuale di un comando: descrizione, uso, opzioni, alias e ruolo minimo."),
      TRK("Vedi solo i comandi che il tuo ruolo puo usare. Con 'help' senza nome trovi l'elenco."), nullptr},
    "man ls\nman cpu"},
  {"help",
    {TRK("Senza nome elenca i comandi che il tuo ruolo puo usare, in ordine alfabetico."),
      TRK("Con un nome mostra sintassi, descrizione breve, alias e ruolo minimo; 'man' da' di piu."), nullptr},
    "help\nhelp df"},
  {"time",
    {TRK("Esegue il comando e poi scrive quanto e durato, come il 'time' di bash: real 0m1.234s."),
      TRK("Il tempo e misurato in millisecondi. 'user' e 'sys' non ci sono: FreeRTOS qui non li misura."),
      TRK("Il comando gira con la tua sessione e il tuo ruolo: 'time' non da' nessun permesso in piu."),
      TRK("Non si annida ('time time ls' e un errore). Senza comando scrive l'uso: per l'ora usa 'date'."), nullptr},
    "time ls /scripts\ntime wifi-scan"},
  {"ping",
    {TRK("Invia pacchetti ICMP echo a un indirizzo IPv4 (o a un nome) e scrive le risposte come il ping classico: tempo, TTL, perdita."),
      TRK("-c pacchetti (4, al massimo 20), -i secondi tra i pacchetti (1, minimo 0.2), -W attesa della risposta (2, al massimo 5), -s byte di dati (56, da 16 a 1024), -t TTL (64)."),
      TRK("Durata massima 30 s dalla seriale (Ctrl+C ferma) e 12 s dalla pagina web. Un solo host, solo IPv4; broadcast, multicast e 127.x non ammessi."),
      TRK("I nomi .local usano mDNS (se il servizio e acceso), gli altri il DNS. I tempi hanno la precisione di 1 ms. Solo Admin: genera traffico di rete."), nullptr},
    "ping 192.168.1.1\nping -c 3 -i 0.5 stanza.local\ntime ping -c 2 192.168.1.1"},
  {"gzip",
    {TRK("Comprime ogni file in 'file.gz' (gzip, leggibile dai programmi gzip comuni) e a successo toglie l'originale, come di solito. -k lo tiene, -f sovrascrive un .gz che c'e gia, da -1 (veloce) a -9 (compressione massima, 6 di norma)."),
      TRK("-d decomprime (come gunzip). -c con -d scrive a schermo, ma solo se il risultato e testo ASCII di al massimo 16 KB: i dati compressi sono binari e la shell e testuale."),
      TRK("Controlla magic, metodo, CRC32 e dimensione. Serve la PSRAM (il compressore usa circa 168 KB): senza, il comando si rifiuta. Una sola compressione alla volta, anche tra seriale e pagina web."),
      TRK("Limiti: dalla seriale 120 s e 1 MB (Ctrl+C ferma); dalla pagina web e dagli script 10 s e 128 KB, per file piu grandi usa la seriale. Solo Admin."), nullptr},
    "gzip notes.txt\ngzip -k -9 notes.txt\ngzip -d notes.txt.gz"},
  {"gunzip",
    {TRK("Decomprime i file .gz (come gzip -d): crea il file senza .gz e a successo toglie il .gz. -k lo tiene, -f sovrascrive il file che c'e gia."),
      TRK("-c scrive a schermo, solo se e testo ASCII di al massimo 16 KB. Controlla CRC32 e dimensione; stessi limiti di gzip. Solo Admin."), nullptr},
    "gunzip notes.txt.gz\ngunzip -k notes.txt.gz"},
  {"zcat",
    {TRK("Scrive a schermo il contenuto di un file .gz (come gzip -dc). Solo testo ASCII fino a 16 KB: per i file binari o piu grandi usa gunzip. Solo Admin."), nullptr},
    "zcat notes.txt.gz"},
  {"unzip",
    {TRK("Elenca (-l) o estrae un archivio .zip che sta nella memoria interna. Solo lettura: metodi store e deflate, CRC32 controllato; niente ZIP64, cifratura, piu volumi o collegamenti. Per creare archivi usa zip o tar."),
      TRK("Estrae nella cartella corrente o in quella data con -d, mai nella radice '/'. Senza -o un file che c'e gia si salta, con -o si sostituisce. Dopo l'archivio si possono dire i nomi (o le cartelle) da estrarre."),
      TRK("Prima si controlla TUTTO l'archivio e se una voce e pericolosa lo si rifiuta per intero: nomi assoluti, '..' o con la barra rovesciata (zip slip), file protetti come /vesevos.conf, piu di 64 voci, piu di 1 MB per voce, 2 MB in tutto o oltre lo spazio libero, espansione oltre 100 a 1 (zip bomb)."),
      TRK("Tempo e dimensione come gzip: 10 s e 128 KB dalla pagina web, 120 s e 1 MB dalla seriale (Ctrl+C ferma). Solo Admin."), nullptr},
    "unzip -l dati.zip\nunzip -d /lang dati.zip\nunzip -o dati.zip config.txt"},
  {"zip",
    {TRK("Crea un archivio .zip nuovo (formato standard, lo leggono anche i comuni programmi unzip) da file e cartelle della memoria interna. -r scende nelle cartelle (3 livelli), -j mette solo il nome del file senza le cartelle, -q non scrive i nomi, da -0 (senza compressione) a -9 (massima), 6 di norma."),
      TRK("Non aggiorna archivi che ci sono gia (niente -u, -d, -g): senza -f rifiuta se l'archivio esiste, con -f lo ricrea da zero. Si scrive in un file temporaneo e si scambia solo alla fine. LittleFS non salva la data dei file: tutte le voci portano l'ora di sistema se e valida (NTP o impostata), altrimenti 1980-01-01 00:00."),
      TRK("Non entrano mai i file protetti (configurazione, registro, chiavi, certificati, copie .vbk): nominati a mano il comando fallisce, trovati dentro una cartella si saltano. Si saltano anche i nomi nascosti (.xxx, .tmp~) dentro le cartelle, e l'archivio stesso. Solo nomi relativi, ASCII, al massimo 64 voci e 1 MB per file."),
      TRK("Dalla pagina web 128 KB in tutto e 10 s, dalla seriale 1 MB e 120 s (Ctrl+C ferma); l'archivio deve stare nello spazio libero (meno 16 KB). Serve la PSRAM per comprimere (-0 no). Niente ZIP64, cifratura o commenti. Solo Admin."), nullptr},
    "zip -r bk.zip scripts\nzip -9qj lingue.zip lang/it.json lang/en.json\nzip -f -0 tutto.zip notes.txt"},
  {"tar",
    {TRK("Crea (-c), elenca (-t) o estrae (-x) un archivio tar ustar nella memoria interna. -f indica l'archivio (obbligatorio), -C la cartella, -v scrive i nomi. Anche unito: tar -xf a.tar, tar xvf a.tar."),
      TRK("Solo il contenitore, senza compressione (per un .tar.gz: prima gunzip, poi tar). Solo file e cartelle, nomi fino a 100 caratteri, permessi fissi, nessuna data dentro l'archivio."),
      TRK("Stesse protezioni e tetti di unzip: nomi pericolosi rifiutati, file protetti, 64 voci, 1 MB per voce. Estraendo un file che c'e gia si salta: --overwrite lo sostituisce (il tar classico lo sostituisce sempre)."),
      TRK("Creando: 3 livelli di cartelle e 128 KB in tutto dalla pagina web (1 MB dalla seriale; Ctrl+C ferma). Solo Admin."), nullptr},
    "tar -cf bk.tar scripts\ntar -tvf bk.tar\ntar -xf bk.tar -C /ripristino"},
  {"whoami",
    {TRK("Scrive il nome dell'utente con cui hai fatto login in questa sessione."),
      TRK("Senza utente (regole, script, primo avvio senza password) scrive 'sistema'."), nullptr},
    "whoami"},
  {"id",
    {TRK("Scrive utente, gruppo e gruppi nella forma classica: uid=1000(mario) gid=1002(admin) groups=1002(admin)."),
      TRK("I numeri sono SIMULATI (VesevOS non ha uid veri): uid = 1000 + posto dell'utente, gid = 1000 + ruolo."),
      TRK("Il gruppo e il ruolo: guest (0), oper (1), admin (2). Non mostra niente che tu non sappia gia."), nullptr},
    "id"},
  {"echo",
    {TRK("Scrive il testo che segue, cosi com'e. Con -n non va a capo alla fine."),
      TRK("Solo testo: niente variabili ($NOME), niente apici o jolly, niente redirezione. Per scrivere un file usa 'write'."), nullptr},
    "echo ciao a tutti\necho -n senza a capo"},
  {"df",
    {TRK("Mostra lo spazio dei dischi: la memoria interna (LittleFS, montata su /) e, se montata, la microSD."),
      TRK("Colonne come df: filesystem, dimensione in blocchi da 1K, usato, disponibile, percentuale d'uso e dove e montato."),
      TRK("-h scrive le dimensioni in K, M o G. La microSD non e dentro l'albero delle cartelle: si usa con il comando 'sd'."), nullptr},
    "df\ndf -h"},
  {"uname",
    {TRK("Senza opzioni scrive nome, versione e core del sistema. Con le opzioni, come il comando uname classico."),
      TRK("-s nome del sistema, -n nome host, -r versione di VesevOS, -v versione di ESP-IDF, -m chip, -a tutto insieme."), nullptr},
    "uname\nuname -a\nuname -r"},
  {"exit",
    {TRK("Chiude la sessione della seriale: poi serve di nuovo il login. Anche Ctrl+D a riga vuota fa lo stesso. logout e uguale."),
      TRK("Dalla pagina web non chiude niente: usa il menu utente e Logout."), nullptr},
    "exit"},
  {"ls",
    {TRK("Elenca file e cartelle. Senza percorso usa la cartella corrente; i percorsi possono essere relativi."),
      TRK("-l formato lungo (senza data), -a anche i nomi nascosti, -h dimensioni in K o M, -1 una voce per riga."), nullptr},
    "ls\nls -l /scripts\nls -lah .."},
  {"cd",
    {TRK("Cambia la cartella corrente. Senza nome torna alla radice (~). '..' sale di un livello, '-' torna alla cartella di prima."),
      TRK("I percorsi senza / iniziale partono dalla cartella corrente. La cartella e per sessione e si perde con il logout."), nullptr},
    "cd /scripts\ncd ..\ncd -"},
  {"cat",
    {TRK("Mostra il contenuto di un file di testo. Il percorso puo essere relativo alla cartella corrente."),
      TRK("Non mostra i file binari ne' quelli sopra 32 KB; i file protetti del sistema non si aprono."), nullptr},
    "cat notes.txt"},
  {"led",
    {TRK("led mostra lo stato del LED. led trigger [nome] mostra o cambia che cosa lo accende: none, state (colore dello stato di rete), heartbeat, flash (un lampo a ogni accesso alla flash o alla microSD), wifi (un lampo a ogni pacchetto), cpu (il colore vira con il carico alto), default-on (colore fisso)."),
      TRK("led color RRGGBB e led brightness 0-255 cambiano colore e luminosita. led color <sorgente> RRGGBB cambia il colore di flash-read, flash-write, wifi-rx, wifi-tx o cpu; led threshold 10-95 e la soglia di carico del trigger cpu. Cambiare trigger, sorgenti e soglia e solo per l'Admin."),
      TRK("I lampi sono al massimo 3 al secondo. Di fabbrica il trigger e state, come sempre. Le vecchie forme led state|heartbeat|fixed|off e i nomi led-color e led-bright restano validi (le regole li usano)."),
      TRK("vesevos-led_control e un alias di led: stesso comando, stessi ruoli, stessi argomenti (vesevos-led_control trigger flash).")},
    "led\nled trigger flash\nled color flash-write ff8800\nled threshold 80"},
  {"region",
    {TRK("Mostra o cambia paese, potenza di trasmissione e antenna della radio Wi-Fi. Cambiare e solo per l'Admin."),
      TRK("Il paese fissa canali e potenza ammessi dalla legge: la scheda non va oltre. 'locale' e il vecchio nome e resta valido."), nullptr},
    "region\nregion country IT\nregion antenna ext 3"},
  {"vesevos-config",
    {TRK("Fronte unico di configurazione per aree: system, network, region, time, services, users, security, led, cpu. Senza argomenti scrive un riepilogo di tutto (mai password, hash o chiavi)."),
      TRK("vesevos-config <area> mostra i valori dell'area e i comandi che li cambiano. vesevos-config <area> <comando ...> esegue quel comando (per esempio region country IT)."),
      TRK("Non e un modo nuovo di cambiare le impostazioni: la riga passa dal percorso normale della shell, con gli stessi ruoli, limiti della radio, conferme e domande di password. setup avvia la guida."),
      TRK("L'Operatore vede il riepilogo e le aree in lettura (users, security e setup sono solo per l'Admin); cambiare e solo per l'Admin."), nullptr},
    "vesevos-config\nvesevos-config region\nvesevos-config region country IT\nvesevos-config network wifi set casa\nvesevos-config setup"},
  {"vesevos-led_control",
    {TRK("Alias di 'led': fa esattamente la stessa cosa, con gli stessi argomenti e gli stessi ruoli (lettura all'Operatore, cambi all'Admin tranne cio che le regole usano). Il manuale completo e quello di 'led': man led."), nullptr},
    "vesevos-led_control\nvesevos-led_control trigger heartbeat"},
  {"vesevos-cleaner",
    {TRK("Pulizia SICURA, solo Admin. Senza argomenti e una prova a secco: elenca cosa si potrebbe pulire e quanto spazio si libera, senza cancellare niente. Con -y pulisce, dopo conferma (y/n dalla seriale, yes dalla pagina)."),
      TRK("Categorie: tmp (residui .tmp~ di salvataggi interrotti, solo se il file completo esiste), crash (rapporti dei crash), log (registro in RAM: non libera flash), reboots (diario dei riavvii), cache (file di prova /selftest.tmp). Senza nomi vale per tutte."),
      TRK("Non tocca MAI: configurazione, periferiche (io.cfg), utenti, certificati e chiavi, script, app, regole, lingue, registro di audit (si svuota solo con 'audit'), file protetti, cartelle intere. Non scende nelle sottocartelle."),
      TRK("Rapporti e registri possono contenere dati che identificano l'uso della scheda: cancellarli e un tuo diritto. Si registra una riga nel registro con le categorie e i byte liberati."), nullptr},
    "vesevos-cleaner\nvesevos-cleaner tmp crash\nvesevos-cleaner -y tmp"},
  {"vesevos-drive_manager",
    {TRK("Elenca i volumi: memoria interna (LittleFS, su /) e microSD se la periferica 'sd' e configurata. Per ognuno stato (montata, non montata, non presente, formato non supportato), formato, dimensione, usato, libero e, per la microSD, la frequenza SPI."),
      TRK("mount, unmount e test (solo Admin) sono i comandi 'sd mount', 'sd unmount' e 'sd test': passano dal percorso normale, con gli stessi controlli. unmount chiede conferma se la scheda e in uso. status e come l'elenco (l'Admin vede anche 'sd status')."),
      TRK("NON c'e format: VesevOS non formatta mai la microSD (formattala in FAT32 sul computer) e exFAT non e supportato. Se la microSD non e configurata, aggiungila dalla pagina Periferiche."), nullptr},
    "vesevos-drive_manager\nvesevos-drive_manager mount\nvesevos-drive_manager unmount"},
  {"vesevos-network",
    {TRK("Riepilogo di rete in testo: modo (client, hotspot, aereo), IP, maschera, gateway, DNS, canale, segnale, rete Wi-Fi, nome host e mDNS, MAC, DHCP, NTP, traffico, hotspot e servizi aperti con le porte."),
      TRK("Mai le password. Il MAC e mascherato (AA:BB:CC:xx:xx:xx) come in vesevos-bugreport; con -f (solo Admin) e intero. Non fa scansioni ne prove: e veloce."),
      TRK("scan e uguale a 'wifi scan' e ping a 'ping': passano dal percorso normale (solo Admin, stessi limiti). ip, net e wifi restano com'erano."), nullptr},
    "vesevos-network\nvesevos-network -f\nvesevos-network scan\nvesevos-network ping 192.168.1.1"},
  {"vesevos-backup",
    {TRK("Solo Admin. Salva e ripristina la CONFIGURAZIONE (rete, utenti, servizi, periferiche) in /backup, sempre CIFRATA con una frase che scegli tu. Senza argomenti mostra i backup, lo spazio libero e l'avvertenza."),
      TRK("create [nome] crea /backup/<nome>.vbk (la frase si chiede due volte). restore <nome> chiede la frase, dice quante righe cambierebbero (mai i valori) e chiede conferma prima di applicare. delete <nome> cancella dopo conferma. Al massimo 5 backup."),
      TRK("Il backup contiene password e chiavi: la frase non si scrive mai nella riga (la chiedo io, senza mostrarla) e non resta in cronologia. Se la perdi non si recupera. Tienilo dove decidi tu: e un tuo dato personale."),
      TRK("Stesso formato del backup con i segreti della pagina: si ripristina anche da li (scaricalo da Sistema > File) e viceversa (caricalo in /backup con il nome che finisce in .vbk). Nomi: lettere, numeri, - e _."), nullptr},
    "vesevos-backup\nvesevos-backup create prima-della-prova\nvesevos-backup restore prima-della-prova\nvesevos-backup delete prima-della-prova"},
  {"vesevos-update",
    {TRK("Solo Admin. Senza argomenti mostra versione del firmware e del recovery, le partizioni, lo spazio e l'esito dell'ultimo ordine. Non cerca aggiornamenti in rete da solo."),
      TRK("file [ip]: la scheda si riavvia nel recovery e accetta UN file di firmware dal computer. Stampa una sola volta l'indirizzo http://<ip>/#g=<codice>: il codice vale 15 minuti ed e legato a quel computer, non condividerlo. Dalla seriale scrivi l'IP del computer."),
      TRK("net <url-https> <sha256> [versione]: il recovery scarica da un indirizzo https consentito (le Release ufficiali o i prefissi di /updurl.txt) e controlla lunghezza e SHA-256. Sovrascrive il firmware prima di verificare lo SHA completo."),
      TRK("Chiede sempre conferma (y/n, o yes dalla pagina). Sono gli stessi ordini della pagina Aggiornamento: nessun permesso in piu. Il recovery lavora in HTTP sulla rete locale."), nullptr},
    "vesevos-update\nvesevos-update file 192.168.1.20\nvesevos-update net https://github.com/paolellad79/VesevOS/releases/download/v1.7.71/VesevOS-1.7.71.bin <sha256>"},
  {"btn",
    {TRK("Mostra o cambia l'azione di 1, 2 o 3 click sul tasto BOOT: none (niente), airplane (modo aereo), wps, find (trova la scheda)."),
      TRK("I vecchi nomi niente, aereo, wps e trova restano validi. Cambiare e solo per l'Admin."), nullptr},
    "btn\nbtn 2 airplane"},
};

const ManText* manFind(const char* name) {
  for (const ManText& m : kMan)
    if (strcmp(m.name, name) == 0) return &m;
  return nullptr;
}

const char* roleName(int role) {
  return role >= ROLE_ADMIN ? tr("Amministratore") : role == ROLE_OPER ? tr("Operatore") : tr("Ospite");
}

// Il ruolo minimo per il comando scritto da solo, e per ognuna delle sue parole (sottocomandi): la tabella dei ruoli e l'unica fonte.
int minRole(const CmdInfo& ci, const char* sub) {
  return shx::roleMin(Word(ci.name, strlen(ci.name)), Word(sub, strlen(sub)));
}

// Scrive le parole di ci.subs il cui ruolo minimo e (elevated = false) uguale o sotto a quello del comando, oppure (true) sopra.
// Ritorna quante sono; con o = nullptr le conta soltanto. Le parole che non sono sottocomandi (numeri, "-h") valgono come il comando.
int printSubs(OutSink* o, const CmdInfo& ci, bool elevated) {
  int base = minRole(ci, "");
  int n = 0;
  const char* p = ci.subs;
  while (*p) {
    while (*p == ' ') p++;
    const char* b = p;
    while (*p && *p != ' ') p++;
    size_t len = (size_t)(p - b);
    if (len == 0 || len > Word::MAX) continue;
    char w[Word::MAX + 1];
    memcpy(w, b, len);
    w[len] = 0;
    if ((minRole(ci, w) > base) != elevated) continue;
    if (o) {
      o->print(n ? " " : "    ");
      o->print(w);
    }
    n++;
  }
  if (n && o) o->println();
  return n;
}

bool hasSubs(const CmdInfo& ci) { return ci.subs[0] && strcmp(ci.subs, "@commands") != 0; }

}  // namespace

void shx::cmdUsage(OutSink& o, const char* name) {
  const CmdInfo* ci = cmdFind(name);
  o.println(trf("Uso: %s", ci ? ci->usage : name));
}

void shx::cmdHelpList(OutSink& o, int role) {
  o.println(tr("Comandi del tuo ruolo (help <comando> e man <comando> spiegano il singolo comando):"));
  for (size_t i = 0; i < cmdCount(); i++) {
    const CmdInfo& ci = cmdAt(i);
    if (ci.flags & CMD_HIDDEN) continue;
    if (!roleAllows(role, Word(ci.name, strlen(ci.name)), Word())) continue;
    o.printf("  %-22s %s\r\n", ci.name, tr(ci.brief));
  }
  o.println(tr("Tab completa comandi, opzioni e percorsi; due Tab di fila elencano le possibilita."));
}

bool shx::cmdHelpOne(OutSink& o, const char* name, int role, bool full) {
  Word w(name, strlen(name));
  w.lower();
  canonicalName(w);
  const CmdInfo* ci = cmdFind(w.c_str());
  if (!ci || !roleAllows(role, Word(ci->name, strlen(ci->name)), Word())) return false;   // stessa risposta per "non esiste" e "non per te"
  const ManText* man = manFind(ci->name);
  int base = minRole(*ci, "");

  if (!full) {                                                       // help <comando>: la scheda breve
    o.println(ci->usage);
    o.print("    ");
    o.println(tr(ci->brief));
    if (ci->alias[0]) o.println(trf("Alias: %s", ci->alias));
    o.println(trf("Ruolo minimo: %s", roleName(base)));
    if (man) o.println(trf("Per i dettagli: man %s", ci->name));
    return true;
  }

  o.println(tr("NOME"));
  o.print("    ");
  o.print(ci->name);
  o.print(" - ");
  o.println(tr(ci->brief));
  o.println(tr("USO"));
  o.print("    ");
  o.println(ci->usage);
  if (man) {
    o.println(tr("DESCRIZIONE"));
    for (const char* const* l = man->lines; *l; l++) {
      o.print("    ");
      o.println(tr(*l));
    }
  }
  if (hasSubs(*ci)) {
    o.println(tr("OPZIONI E SOTTOCOMANDI"));
    printSubs(&o, *ci, false);
    if (printSubs(nullptr, *ci, true)) {
      o.print("  ");
      o.println(tr("Richiedono un ruolo piu alto:"));
      printSubs(&o, *ci, true);
    }
  }
  if (ci->alias[0]) {
    o.println(tr("ALIAS (vecchi nomi, ancora validi)"));
    o.print("    ");
    o.println(ci->alias);
  }
  o.println(tr("RUOLO MINIMO"));
  o.print("    ");
  o.println(roleName(base));
  if (man && man->examples[0]) {
    o.println(tr("ESEMPI"));
    for (const char* p = man->examples; *p;) {
      const char* e = strchr(p, '\n');
      size_t n = e ? (size_t)(e - p) : strlen(p);
      o.print("    ");
      o.write(p, n);
      o.println();
      p += n + (e ? 1 : 0);
    }
  }
  return true;
}
