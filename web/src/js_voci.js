/* ===== tabella unica delle voci del menu =====
   Una riga per voce, nell'ordine del menu: GMAP, TABS, TSEC e TMIN si ricavano da qui.
   La pagina HTML della voce resta nei file pagine_*.html, con id "t_" + id.
   id, testo: nome interno e testo del menu (letterale: lo legge tools/mklang.py).
   gruppo:    gruppo del menu (GRP).
   ruolo:     ruolo minimo; se manca vale 0 (ospite).
   sezione:   titolo nel sottomenu, indice in SECN; se manca, nessun titolo.
   apri:      si chiama quando la voce si apre, con il suo id.
   esci:      ferma i timer della voce; si chiama a ogni show() su una voce diversa. */
var VOCI=[
 {id:"sum",testo:"Panoramica",gruppo:"home",apri:homeDraw},

 {id:"dmgr",testo:"Elenco dispositivi",gruppo:"dev",ruolo:1,apri:dmLoad},
 {id:"mem",testo:"Memoria",gruppo:"dev",sezione:0,apri:apriMemoria},
 {id:"cpu",testo:"Processore",gruppo:"dev",ruolo:1,sezione:0,apri:cpuLoad,esci:fermaProcessore},
 {id:"sens",testo:"Sensori",gruppo:"dev",ruolo:1,sezione:0,apri:sensLoad,esci:fermaSensori},
 {id:"led",testo:"LED",gruppo:"dev",ruolo:1,sezione:1,apri:fillLed},
 {id:"pin",testo:"Pin",gruppo:"dev",ruolo:1,sezione:1,apri:loadPins},
 {id:"io",testo:"Periferiche",gruppo:"dev",ruolo:1,sezione:1,apri:apriPeriferiche,esci:ioStop},
 {id:"btn",testo:"Tasto",gruppo:"dev",ruolo:1,sezione:1,apri:apriTasto},
 {id:"rad",testo:"Radio",gruppo:"dev",ruolo:1,sezione:2,apri:apriRadio},
 {id:"usb",testo:"USB",gruppo:"dev",ruolo:1,sezione:2,apri:dvRows},
 {id:"ser",testo:"Seriale",gruppo:"dev",ruolo:2,sezione:2,apri:fillSer},

 {id:"wifi",testo:"Wi-Fi",gruppo:"rete",ruolo:1,sezione:4,apri:fillNet},
 {id:"wps",testo:"WPS",gruppo:"rete",ruolo:2,sezione:4,apri:wpsLoad,esci:fermaWps},
 {id:"ip",testo:"IP",gruppo:"rete",ruolo:1,sezione:4,apri:fillNet},
 {id:"nome",testo:"Nome",gruppo:"rete",ruolo:1,sezione:4,apri:apriNome},
 {id:"ntp",testo:"NTP",gruppo:"rete",ruolo:1,sezione:4,apri:loadTime},
 {id:"mqtt",testo:"MQTT",gruppo:"rete",ruolo:1,sezione:4,apri:mqLoad},
 {id:"ap",testo:"Punto di accesso",gruppo:"rete",ruolo:1,sezione:5,apri:apriPuntoAccesso},
 {id:"web",testo:"Web",gruppo:"rete",ruolo:1,sezione:5,apri:apriWeb},
 {id:"sntp",testo:"Server NTP",gruppo:"rete",ruolo:1,sezione:5,apri:loadTime},
 {id:"mesh",testo:"Rete tra schede",gruppo:"rete",ruolo:1,sezione:5,apri:msLoad},

 {id:"auto",testo:"Automazioni",gruppo:"prog",ruolo:1,apri:rLoad,esci:fermaAutomazioni},
 {id:"script",testo:"Script",gruppo:"prog",ruolo:2,apri:scLoad,esci:scTimerStop},
 {id:"app",testo:"App",gruppo:"prog",ruolo:1,apri:apLoad,esci:apTimerStop},

 {id:"ora",testo:"Ora",gruppo:"sys",ruolo:1,apri:loadTime},
 {id:"loc",testo:"Localizzazione",gruppo:"sys",ruolo:1,apri:apriLocalizzazione},
 {id:"power",testo:"Risparmio energia",gruppo:"sys",ruolo:1,apri:pwLoad},
 {id:"func",testo:"Funzionalita",gruppo:"sys",ruolo:1,apri:fnLoad},
 {id:"file",testo:"File",gruppo:"sys",ruolo:1,apri:apriFile},
 {id:"acc",testo:"Accessibilita",gruppo:"sys"},
 {id:"conf",testo:"Configurazione",gruppo:"sys",ruolo:2},
 {id:"boot",testo:"Avvio",gruppo:"sys",ruolo:1,apri:bLoad},
 {id:"selftest",testo:"Autotest",gruppo:"sys",ruolo:2,apri:stlPoll},
 {id:"task",testo:"Task",gruppo:"sys",ruolo:1,sezione:3,apri:loadTasks,esci:fermaTask},
 {id:"wd",testo:"Watchdog",gruppo:"sys",ruolo:1,sezione:3,apri:wdLoad},
 {id:"shell",testo:"Terminale",gruppo:"sys",ruolo:1,sezione:3,apri:apriTerminale},
 {id:"stat",testo:"Statistiche",gruppo:"sys",ruolo:1,sezione:3,apri:apriStatistiche,esci:fermaStatistiche},

 {id:"pass",testo:"Password",gruppo:"sec",apri:fillPass,esci:pwVuota},
 {id:"sec",testo:"Utenti",gruppo:"sec",ruolo:2,apri:usLoad},
 {id:"mfa",testo:"MFA",gruppo:"sec",apri:mfLoad},
 {id:"fw",testo:"Filtro IP",gruppo:"sec",ruolo:2,apri:apriFiltroIp},
 {id:"cert",testo:"Certificato",gruppo:"sec",ruolo:1,apri:tlsLoad},
 {id:"agg",testo:"Aggiornamenti",gruppo:"sec",ruolo:2,apri:agLoad},
 {id:"audit",testo:"Stato conformita",gruppo:"sec",ruolo:1,apri:auLoad},
 {id:"evs",testo:"Eventi di sicurezza",gruppo:"sec",ruolo:2,apri:evLoad},
 {id:"log",testo:"Log",gruppo:"sec",apri:apriLog,esci:fermaLog},

 {id:"help",testo:"Aiuto",gruppo:"info"},
 {id:"legal",testo:"Note legali",gruppo:"info",apri:lgLoad}
];

/* Le voci che fanno piu di una cosa all'apertura. */
/* Memoria: la voce e per tutti (Memoria RAM viene da /api/status); il Riepilogo, da /api/dev, dall'Operatore in su. */
function apriMemoria(){
  $("sk_mem").className=ROLE<1?"hide":"sub";
  if(ROLE<1)skApri("mem",1);
  else dvRows("mem");
  memDraw()
}
function memDraw(){
  if(S)$("mem_ram").innerHTML=memCard(S)
}
function apriTasto(id){
 dvRows(id);
 btnLoad();
}
/* Dispositivi > Interfacce > Radio: paese e limiti, stato del modo aereo e MAC da /api/status. */
function apriRadio(){
  lcLoad();
  radLoad();
  api("/api/status").then(function(r){return r.json()}).then(function(j){
    RN.n=j.net;
    airUi();
    $("ifmac").innerHTML=macRow(t("Indirizzo MAC"),j.net.mac)
  }).catch(function(){})
}
/* Periferiche: la scheda Driver e solo per l'Admin, e si aggiorna con la voce. */
function apriPeriferiche(){
 ioLoad();
 if(canSee("drv"))drvLoad();
}
function apriPuntoAccesso(){
 fillNet();
 svcLoad();
}
function apriWeb(){
 tlsLoad();
 svcLoad();
}
function apriNome(){
 fillSys();
 svcLoad();
}
function apriLocalizzazione(){
 loadLangs();
 lcLoad();
}
function apriFiltroIp(){
 fwLoad();
 banLoad();
}
/* Log: la scheda Ultime modifiche e dall'Operatore in su, l'Ospite non la carica. */
function apriLog(){
 lgInit();
 loadLog();
 lgAuto();
 if(ROLE>=1)auLoad();
}
function apriTerminale(){
 cpToggle(true);
}
function apriFile(){
 fList(fp);
}
function apriStatistiche(){
 stLoad();
 clearInterval(STT);
 STT=setInterval(stLoad,5000);
}

/* I timer da fermare quando si lascia una voce. */
function fermaStatistiche(){
 clearInterval(STT);
}
function fermaProcessore(){
 clearInterval(CPU.tm);
}
function fermaSensori(){
 clearInterval(SENS.tm);
}
function fermaWps(){
 clearTimeout(WP.tm);
}
function fermaAutomazioni(){
 clearInterval(RU.tm);
}
function fermaTask(){
 clearInterval(TK.tm);
}
function fermaLog(){
 clearInterval(LG.tm);
}

/* Elenchi ricavati dalla tabella: stessi nomi di prima, li leggono anche le prove. */
var GMAP={},TABS=[],TSEC={},TMIN={};
VOCI.forEach(function(v){
 if(!GMAP[v.gruppo])GMAP[v.gruppo]=[];
 GMAP[v.gruppo].push(v.id);
 TABS.push([v.id,v.testo]);
 if(v.sezione!==undefined)TSEC[v.id]=v.sezione;
 if(v.ruolo)TMIN[v.id]=v.ruolo;
});
/* Nomi che non sono voci: portano sulla voce e sulla scheda giusta (show). */
var VIA={temp:["sens"],mdns:["nome",1],http:["web",0],https:["web",1],dhcp:["ap",3],dns:["ap",4],drv:["io",2]};
/* Driver non e una voce ma una scheda di Periferiche: il suo ruolo minimo resta qui. */
TMIN.drv=2;
