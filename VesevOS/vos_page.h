// VesevOS - vos_page.h (GENERATO, solo ASCII)
#pragma once
#include <Arduino.h>
static const char INDEX_HTML[] PROGMEM =
R"VOSPAGE(<!DOCTYPE html>
<html lang="it"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>VesevOS</title>
<style>
:root{--bg:#10151c;--card:#1a222d;--tx:#e6edf3;--mut:#8b98a8;--ac:#3fa7ff;--ok:#3ddc84;--ko:#ff5c5c}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--tx);font:15px system-ui,Arial,sans-serif}
header{padding:12px 16px;background:var(--card);display:flex;justify-content:space-between;align-items:center}
header b{font-size:18px}
nav{display:flex;flex-wrap:wrap;gap:4px;padding:8px 10px;background:#141b24}
nav button{background:transparent;color:var(--mut);border:0;padding:8px 12px;border-radius:8px;cursor:pointer;font-size:14px}
nav button.on{background:var(--ac);color:#06121c}
main{padding:12px;max-width:900px;margin:auto}
.card{background:var(--card);border-radius:12px;padding:14px;margin-bottom:12px}
.card h3{margin:0 0 10px;font-size:15px;color:var(--ac)}
.row{display:flex;justify-content:space-between;gap:10px;padding:5px 0;border-bottom:1px solid #222c39}
.row span:first-child{color:var(--mut)}
.bar{height:10px;background:#0c1118;border-radius:6px;overflow:hidden}
.bar i{display:block;height:100%;background:var(--ac)}
input,select,textarea{width:100%;padding:9px;margin:4px 0 10px;background:#0c1118;color:var(--tx);border:1px solid #2a3647;border-radius:8px;font:inherit}
label{color:var(--mut);font-size:13px}
.btn{background:var(--ac);color:#06121c;border:0;padding:9px 16px;border-radius:8px;cursor:pointer;font-weight:600;margin-right:6px}
.btn.red{background:var(--ko);color:#fff}
.btn.gray{background:#2a3647;color:var(--tx)}
pre{background:#0c1118;padding:10px;border-radius:8px;overflow:auto;max-height:340px;white-space:pre-wrap;word-break:break-all;margin:0}
table{width:100%;border-collapse:collapse}
td,th{text-align:left;padding:6px;border-bottom:1px solid #222c39;font-size:14px}
.msg{padding:8px;border-radius:8px;margin:8px 0;display:none}
.msg.ok{display:block;background:#12301f;color:var(--ok)}
.msg.ko{display:block;background:#3a1717;color:var(--ko)}
#login{max-width:340px;margin:60px auto}
.hide{display:none}
.foot{text-align:center;font-size:12px;color:var(--mut);margin:14px 0}
.foot a{color:var(--mut)}
</style></head>
<body>
<div id="login" class="card hide">
 <select class="lsel" onchange="setLang(this.value)" style="width:auto;float:right;margin:0;padding:4px 8px"></select>
 <h3 id="lt">Accesso</h3>
 <div id="lnew" class="hide"><p>Primo accesso: scegli una password (almeno 6 caratteri).</p></div>
 <label>Password</label><input type="password" id="lp" autocomplete="current-password">
 <button class="btn" onclick="doLogin()">Entra</button>
 <div id="lm" class="msg"></div>
 <div class="foot"><a href="/api/license?id=notice" target="_blank">Note legali e licenze</a></div>
</div>

<div id="app" class="hide">
<header><b>VesevOS</b><span id="hd" style="color:var(--mut)"></span><span><select class="lsel" onchange="setLang(this.value)" style="width:auto;margin:0 8px 0 0;padding:4px 8px"></select><button class="btn gray" onclick="logout()">Esci</button></span></header>
<nav id="nav"></nav>
<main>
<section id="t_sum">
 <div class="card"><h3>Riepilogo</h3><div id="sum"></div></div>
</section>
<section id="t_stato" class="hide">
 <div class="card"><h3>Uso risorse</h3><div id="res"></div></div>
 <div class="card"><h3>Velocita CPU</h3><select id="cpumode" onchange="setCpu()"><option value="0">Automatica (scala da sola)</option><option value="80">Fissa 80 MHz</option><option value="160">Fissa 160 MHz</option><option value="240">Fissa 240 MHz</option></select><div id="cpum" class="msg"></div></div>
 <div class="card"><h3>CPU (ultimi 60 s)</h3><canvas id="cc" width="600" height="110" style="width:100%"></canvas></div>
 <div class="card"><h3>Temperatura CPU (ultimi 60 s)</h3><canvas id="tc" width="600" height="110" style="width:100%"></canvas></div>
</section>
<section id="t_wifi" class="hide">
 <div class="card"><h3>Reti Wi-Fi</h3>
  <button class="btn" onclick="scan()">Cerca reti</button><div id="scanmsg" style="color:var(--mut);margin-top:6px"></div>
  <table id="scanlist"></table></div>
 <div class="card"><h3>Connessione client</h3>
  <label>Rete (SSID)</label><input id="w_ssid">
  <label>Password Wi-Fi</label><input type="password" id="w_pass" placeholder="(lascia vuoto per non cambiare)">
  <label>Indirizzo IP</label>
  <select id="w_dhcp" onchange="dh()"><option value="1">Automatico (DHCP)</option><option value="0">Statico</option></select>
  <div id="stat" class="hide">
   <label>IP</label><input id="w_ip"><label>Subnet mask</label><input id="w_mask">
   <label>Gateway</label><input id="w_gw"><label>DNS 1</label><input id="w_d1"><label>DNS 2</label><input id="w_d2">
  </div>
  <button class="btn" onclick="saveWifi()">Salva e collega</button>
  <button class="btn gray" onclick="apOnly()">Torna in modo AP</button>
  <div id="wm" class="msg"></div></div>
</section>
<section id="t_shell" class="hide">
 <div class="card"><h3>Shell</h3><pre id="so" style="height:260px"></pre>
  <input id="si" placeholder="scrivi un comando, es. help" onkeydown="if(event.key=='Enter')runCmd()"></div>
</section>
<section id="t_task" class="hide">
 <div class="card"><h3>Task in esecuzione</h3><pre id="tk"></pre><button class="btn" onclick="loadTasks()">Aggiorna</button></div>
</section>
<section id="t_ora" class="hide">
 <div class="card"><h3>Data e ora</h3><div id="oranow" style="font-size:22px;margin-bottom:8px"></div>
  <button class="btn gray" onclick="setFromBrowser()">Usa l'ora di questo dispositivo</button><div id="om2" class="msg"></div></div>
 <div class="card"><h3>Impostazioni</h3>
  <label>Sincronizzazione automatica (NTP)</label>
  <select id="o_ntp"><option value="1">Attiva</option><option value="0">Spenta (ora a mano)</option></select>
  <label>Server NTP</label><input id="o_srv">
  <label>Fuso orario</label><select id="o_tzs" onchange="tzPick()"></select>
  <label>Regola del fuso (formato POSIX)</label><input id="o_tz">
  <label>Nome del fuso</label><input id="o_tzn">
  <label>Formato data</label>
  <select id="o_df"><option value="0">GG/MM/AAAA (31/12/2026)</option><option value="1">AAAA-MM-GG (2026-12-31)</option><option value="2">MM/GG/AAAA (12/31/2026)</option></select>
  <label>Formato ora</label>
  <select id="o_tf"><option value="0">24 ore (18:30:00)</option><option value="1">12 ore (06:30:00 PM)</option></select>
  <label>Unita di temperatura</label>
  <select id="o_tu"><option value="0">Gradi Celsius (C)</option><option value="1">Gradi Fahrenheit (F)</option></select>
  <label>Questa scheda come server NTP per altri dispositivi</label>
  <select id="o_serve"><option value="0">No</option><option value="1">Si (UDP 123)</option></select>
  <button class="btn" onclick="saveTime()">Salva</button><div id="om" class="msg"></div></div>
</section>
<section id="t_file" class="hide">
 <div class="card"><h3>File - memoria interna</h3>
  <div id="fpath" style="margin-bottom:8px;color:var(--mut)"></div>
  <button class="btn gray" onclick="fUp()">Su</button>
  <button class="btn gray" onclick="fMk()">Nuova cartella</button>
  <button class="btn gray" onclick="fNew()">Nuovo file di testo</button>
  <input type="file" id="fup" style="margin-top:10px"><button class="btn" onclick="fUpload()">Carica</button>
  <table id="ftab"></table><div id="fm" class="msg"></div><div id="fspace" style="color:var(--mut);margin-top:6px"></div></div>
 <div class="card hide" id="fed"><h3 id="fedt">Modifica</h3>
  <textarea id="fedx" rows="12"></textarea>
  <button class="btn" onclick="fSave()">Salva</button><button class="btn gray" onclick="$('fed').className='card hide'">Chiudi</button></div>
</section>
<section id="t_led" class="hide">
 <div class="card"><h3>LED</h3>
  <label>Modo</label><select id="l_mode"><option value="0">Colore = stato del sistema</option><option value="1">Battito (heartbeat) colore scelto</option><option value="2">Colore fisso</option><option value="3">Spento</option></select>
)VOSPAGE"
R"VOSPAGE(  <label>Colore</label><input type="color" id="l_col">
  <label>Luminosita (0-255)</label><input type="number" id="l_br" min="0" max="255">
  <button class="btn" onclick="saveLed()">Salva</button><div id="lm2" class="msg"></div>
  <p style="color:var(--mut)">Colori di stato: arancio = avvio, blu = AP, giallo = connessione, verde = client collegato, rosso lampeggiante = errore.</p></div>
<div class="card"><h3>LED aggiuntivo (rosso)</h3>
  <label>Modo</label><select id="m_mode"><option value="0">Spento</option><option value="1">Acceso fisso</option><option value="2">Battito (heartbeat) legato alla CPU</option></select>
  <label>Pin (GPIO)</label><input type="number" id="m_pin" min="1" max="47">
  <label>Luminosita (0-255)</label><input type="number" id="m_br" min="0" max="255">
  <label>Si accende con</label><select id="m_inv"><option value="0">Livello alto (normale)</option><option value="1">Livello basso (invertito)</option></select>
  <button class="btn" onclick="saveLed2()">Salva</button><div id="lm3" class="msg"></div>
  <p style="color:var(--mut)">Pin ammessi: 1-18, 21 e 38-47. Se il LED non reagisce prova un altro pin o il livello invertito. Il pin scelto compare nella scheda Pin.</p></div>
</section>
<section id="t_pin" class="hide">
 <div class="card"><h3>Pin e prove</h3>
  <p style="color:var(--mut);margin-top:0">Tocca un pin verde per provarlo. Blu = pin in uso, grigio = vietato.</p>
  <svg id="pinsvg" viewBox="0 0 300 250" style="width:100%;max-width:420px"></svg>
  <div id="pinpanel" style="margin-top:8px"></div></div>
 <div class="card"><h3>Pin usati</h3><table id="pintab"></table></div>
</section>
<section id="t_conf" class="hide">
 <div class="card"><h3>Nome del dispositivo</h3>
  <label>Nome host</label><input id="s_host" maxlength="32">
  <label>Dominio DNS (facoltativo, es. casa.lan)</label><input id="s_dom" maxlength="60">
  <div id="s_fq" style="color:var(--mut);margin-bottom:8px"></div>
  <button class="btn" onclick="saveSys()">Salva</button><div id="sm" class="msg"></div>
  <p style="color:var(--mut)">Dopo il cambio la scheda si raggiunge anche come NOME.local (se il tuo computer supporta mDNS). La rete si riconnette per un momento.</p></div>
 <div class="card"><h3>Configurazione</h3>
  <button class="btn" onclick="location.href='/api/config/download'">Scarica file</button>
  <label style="display:block;margin-top:12px">Ripristina da file</label><input type="file" id="cf">
  <button class="btn gray" onclick="restoreCfg()">Ripristina</button>
  <button class="btn red" onclick="factory()">Azzera tutto</button>
  <div id="cm" class="msg"></div></div>
 <div class="card"><h3>Cambia password</h3>
  <label>Vecchia</label><input type="password" id="p_old"><label>Nuova (min 6)</label><input type="password" id="p_new">
  <button class="btn" onclick="chPass()">Cambia</button><div id="pm" class="msg"></div></div>
 <div class="card"><h3>Lingue</h3>
  <div id="lglist"></div>
  <label style="display:block;margin-top:12px">Carica un file di lingua (.json)</label><input type="file" id="lgf" accept=".json">
  <button class="btn" onclick="upLang()">Carica lingua</button><div id="lgm" class="msg"></div>
  <p style="color:var(--mut)">Un file per lingua, con il nome uguale al codice (per esempio en.json). Lo stesso file traduce la pagina e i messaggi della shell.</p>
  <div id="lgsp" style="color:var(--mut)"></div></div>
 <div class="card"><h3>Licenze e note legali</h3>
  <div id="licbtn"></div>
  <pre id="licx" class="hide" style="margin-top:10px"></pre>
  <p style="color:var(--mut)">I testi legali sono dentro il firmware e si leggono anche dalla seriale con il comando license.</p></div>
 <div class="card"><h3>Log</h3><pre id="lg"></pre><button class="btn" onclick="loadLog()">Aggiorna</button></div>
</section>
<div class="foot"><a href="/api/license?id=notice" target="_blank">Note legali e licenze</a></div>
</main></div>
<script>
var TABS=[["sum","Riepilogo"],["stato","Stato"],["wifi","Wi-Fi"],["shell","Shell"],["task","Task"],["ora","Ora"],["file","File"],["led","LED"],["pin","Pin"],["conf","Config"]];
var DICT={},LANG="it",LANGS=[{code:"it",name:"Italiano"}];
function t(s){var v=DICT[s];return v?v:s}
function tf(s){var a=arguments;return t(s).replace(/\{(\d)\}/g,function(m,i){return a[+i+1]})}
function applyStatic(){
 var w=document.createTreeWalker(document.body,NodeFilter.SHOW_TEXT),n,list=[];
 while((n=w.nextNode()))list.push(n);
 list.forEach(function(n){var p=n.parentNode;if(p&&(p.tagName=="SCRIPT"||p.tagName=="STYLE"))return;
  if(n.__o===undefined)n.__o=n.nodeValue;
  var k=n.__o.trim().replace(/\s+/g," ");if(!k)return;
  var m=n.__o.match(/^(\s*)[\s\S]*?(\s*)$/);
  n.nodeValue=(LANG!="it"&&DICT[k])?m[1]+DICT[k]+m[2]:n.__o});
 var els=document.querySelectorAll("[placeholder]");
 for(var i=0;i<els.length;i++){var e=els[i];if(e.__ph===undefined)e.__ph=e.getAttribute("placeholder");
  e.setAttribute("placeholder",(LANG!="it"&&DICT[e.__ph])?DICT[e.__ph]:e.__ph)}
 document.documentElement.lang=LANG}
function fillLangSel(){var h="";LANGS.forEach(function(l){h+='<option value="'+esc(l.code)+'"'+(l.code==LANG?" selected":"")+'>'+esc(l.name)+'</option>'});
 var ss=document.querySelectorAll(".lsel");for(var i=0;i<ss.length;i++)ss[i].innerHTML=h}
function loadDict(code){if(code=="it"){DICT={};return Promise.resolve()}
 return fetch("/api/langfile?code="+encodeURIComponent(code)).then(function(r){if(!r.ok)throw 0;return r.json()}).then(function(d){DICT=d}).catch(function(){DICT={};LANG="it"})}
function loggedIn(){return !$("app").classList.contains("hide")}
function refreshView(){fillLangSel();applyStatic();
 if(loggedIn()){buildNav();poll();if(cur=="conf"){loadLangs();loadLog()}}else showLoginText()}
function setLang(code,noSave){LANG=code;try{localStorage.setItem("vl",code)}catch(e){}
 loadDict(code).then(function(){refreshView();
  if(!noSave&&loggedIn())post("/api/lang",{code:LANG}).catch(function(){})})}
function initLang(dev){var c=dev||"it";try{c=localStorage.getItem("vl")||c}catch(e){}
 fetch("/api/langs").then(function(r){return r.json()}).then(function(l){LANGS=l;if(!l.some(function(x){return x.code==c}))c="it";setLang(c,true)}).catch(function(){setLang("it",true)})}
var cur="sum",S=null,timer=null;
function $(i){return document.getElementById(i)}
function api(u,o){return fetch(u,o).then(function(r){if(r.status==401){showLogin();throw 0}return r})}
function post(u,d){var b=new URLSearchParams(d).toString();return api(u,{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:b})}
function msg(id,t,ok){var e=$(id);e.textContent=t;e.className="msg "+(ok?"ok":"ko")}
function buildNav(){var h="";TABS.forEach(function(x){h+='<button id="n_'+x[0]+'" onclick="show(\''+x[0]+'\')">'+esc(t(x[1]))+'</button>'});$("nav").innerHTML=h;show(cur)}
function show(t){cur=t;TABS.forEach(function(x){$("t_"+x[0]).className=x[0]==t?"":"hide";$("n_"+x[0]).className=x[0]==t?"on":""});
 if(t=="task")loadTasks();if(t=="pin")loadPins();if(t=="conf"){loadLog();fillSys();loadLangs();loadLic()};if(t=="wifi")fillWifi();if(t=="led")fillLed();if(t=="ora")loadTime();if(t=="file")fList(fp)}
var AUTHSET=true;
function showLoginText(){$("lt").textContent=AUTHSET?t("Accesso"):t("Imposta password")}
function showLogin(){clearInterval(timer);$("app").className="hide";$("login").className="card";
 fetch("/api/auth").then(function(r){return r.json()}).then(function(j){AUTHSET=j.set;$("lnew").className=j.set?"hide":"";showLoginText()})}
function doLogin(){post("/api/login",{p:$("lp").value}).then(function(r){return r.json()}).then(function(j){
 if(j.ok){$("lp").value="";start()}else msg("lm",j.err||t("Errore"),false)}).catch(function(){})}
function logout(){post("/api/logout",{}).then(showLogin)}
function start(){$("login").className="hide";$("app").className="";buildNav();poll();timer=setInterval(poll,2000)}
function row(a,b){return '<div class="row"><span>'+a+'</span><span>'+b+'</span></div>'}
)VOSPAGE"
R"VOSPAGE(function bar(p){return '<div class="bar"><i style="width:'+p+'%"></i></div>'}
function tmp(c,u){return u?(c*9/5+32).toFixed(1)+" &deg;F":c.toFixed(1)+" &deg;C"}
function kb(n){return Math.round(n/1024)+" KB"}
function poll(){api("/api/status").then(function(r){return r.json()}).then(function(s){S=s;
 $("hd").textContent=s.net.ip;
 $("sum").innerHTML=row(t("Sistema"),s.name+" "+s.version)+row(t("Acceso da"),s.uptime)+row(t("Ultimo reset"),s.reset)+row(t("Avvii totali"),s.boots)+
  row(t("Rete"),s.net.mode+" - "+s.net.ssid)+row(t("Nome"),s.net.fqdn)+row(t("Indirizzo IP"),s.net.ip)+row(t("CPU"),tf("{0}% a {1} MHz",s.cpu,s.cpuMhz))+row(t("Temperatura CPU"),tmp(s.temp,s.tempUnit)+(s.hot?" - "+t("TROPPO CALDO"):""));
 var hp=Math.round(100-100*s.heapFree/s.heapTotal),pp=s.psramTotal?Math.round(100-100*s.psramFree/s.psramTotal):0,fp=s.flashTotal?Math.round(100*s.flashUsed/s.flashTotal):0;
 $("res").innerHTML=row(t("CPU"),s.cpu+"%")+bar(s.cpu)+row("RAM",kb(s.heapTotal-s.heapFree)+" / "+kb(s.heapTotal))+bar(hp)+
  row("PSRAM",kb(s.psramTotal-s.psramFree)+" / "+kb(s.psramTotal))+bar(pp)+row(t("Flash (file)"),kb(s.flashUsed)+" / "+kb(s.flashTotal))+bar(fp);
 if(document.activeElement!=$("cpumode"))$("cpumode").value=s.cpuMode;draw("cc",s.cpuHist,0,100);draw("tc",s.tempUnit?s.tempHist.map(function(c){return c*9/5+32}):s.tempHist,null,null)}).catch(function(){})}
function draw(id,a,mn,mx){var c=$(id),x=c.getContext("2d");x.clearRect(0,0,c.width,c.height);if(!a||a.length<2)return;
 if(mn===null){mn=Math.min.apply(null,a)-2;mx=Math.max.apply(null,a)+2}
 x.strokeStyle="#3fa7ff";x.lineWidth=2;x.beginPath();
 for(var i=0;i<a.length;i++){var X=i*c.width/59,Y=c.height-(a[i]-mn)/(mx-mn)*c.height;if(i)x.lineTo(X,Y);else x.moveTo(X,Y)}x.stroke();
 x.fillStyle="#8b98a8";x.font="12px sans-serif";x.fillText(mx.toFixed(0),4,12);x.fillText(mn.toFixed(0),4,c.height-4)}
function scan(){$("scanmsg").textContent=t("Ricerca in corso...");api("/api/wifi/scan",{method:"POST"}).then(function(){setTimeout(scanRes,3000)})}
function scanRes(){api("/api/wifi/scan").then(function(r){return r.json()}).then(function(j){
 if(j.running){setTimeout(scanRes,1500);return}$("scanmsg").textContent=tf("{0} reti trovate",j.list.length);
 var h="<tr><th>"+t("Rete")+"</th><th>"+t("Segnale")+"</th><th>"+t("Canale")+"</th><th></th></tr>";
 j.list.forEach(function(n,i){h+="<tr><td>"+esc(n.ssid)+(n.open?" ("+t("aperta")+")":"")+"</td><td>"+n.rssi+" dBm</td><td>"+n.ch+'</td><td><button class="btn gray" onclick="pick('+i+')">'+t("Scegli")+'</button></td></tr>'});
 $("scanlist").innerHTML=h;window._sc=j.list})}
function pick(i){$("w_ssid").value=window._sc[i].ssid;$("w_pass").focus()}

var TZS=[["Europe/Rome","CET-1CEST,M3.5.0,M10.5.0/3"],["Europe/London","GMT0BST,M3.5.0/1,M10.5.0"],["Europe/Athens","EET-2EEST,M3.5.0/3,M10.5.0/4"],["Europe/Moscow","MSK-3"],["UTC","UTC0"],["America/New_York","EST5EDT,M3.2.0,M11.1.0"],["America/Chicago","CST6CDT,M3.2.0,M11.1.0"],["America/Los_Angeles","PST8PDT,M3.2.0,M11.1.0"],["Asia/Tokyo","JST-9"],["Australia/Sydney","AEST-10AEDT,M10.1.0,M4.1.0/3"]];
var fp="/",fedp="";
function tzPick(){var v=$("o_tzs").value;if(v!="-1"){$("o_tz").value=TZS[v][1];$("o_tzn").value=TZS[v][0]}}
function loadTime(){api("/api/time").then(function(r){return r.json()}).then(function(t){
 $("oranow").textContent=t.valid?t.now:window.t("Ora non impostata");$("o_ntp").value=t.ntp?"1":"0";$("o_srv").value=t.server;$("o_tz").value=t.tz;$("o_tzn").value=t.tzName;$("o_serve").value=t.serve?"1":"0";$("o_df").value=t.dateFmt;$("o_tf").value=t.timeFmt;$("o_tu").value=t.tempUnit;
 var h="",f=-1;TZS.forEach(function(z,i){h+='<option value="'+i+'">'+z[0]+'</option>';if(z[1]==t.tz)f=i});h+='<option value="-1">'+esc(window.t("Personalizzato"))+'</option>';$("o_tzs").innerHTML=h;$("o_tzs").value=f})}
function saveTime(){post("/api/time",{ntp:$("o_ntp").value,server:$("o_srv").value,tz:$("o_tz").value,tzname:$("o_tzn").value,serve:$("o_serve").value,datefmt:$("o_df").value,timefmt:$("o_tf").value,tempunit:$("o_tu").value})
 .then(function(r){return r.json()}).then(function(j){msg("om",j.ok?t("Salvato"):j.err,j.ok);if(j.ok)setTimeout(loadTime,1500)})}
function setFromBrowser(){post("/api/time/set",{epoch:Math.floor(Date.now()/1000)}).then(function(r){return r.json()}).then(function(j){msg("om2",j.ok?t("Ora impostata"):j.err,j.ok);if(j.ok)loadTime()})}
function setCpu(){post("/api/cpu",{mode:$("cpumode").value}).then(function(r){return r.json()}).then(function(j){msg("cpum",j.ok?t("Salvato"):j.err,j.ok)})}
function enc(s){return encodeURIComponent(s)}
function join(a,b){return a=="/"?"/"+b:a+"/"+b}
function fList(p){fp=p;api("/api/fs/list?path="+enc(p)).then(function(r){return r.json()}).then(function(j){
 if(!j.ok){msg("fm",j.err,false);return}$("fpath").textContent=j.path;
 var h="<tr><th>"+t("Nome")+"</th><th>"+t("Dimensione")+"</th><th></th></tr>";
 j.list.sort(function(a,b){return (b.d-a.d)||a.n.localeCompare(b.n)});
 j.list.forEach(function(f){var q=esc(join(j.path,f.n)).replace(/'/g,"&#39;");
  h+="<tr><td>"+(f.d?'<a href="#" style="color:var(--ac)" onclick="fList(\''+q+'\');return false">[D] '+esc(f.n)+'</a>':esc(f.n))+"</td><td>"+(f.d?"":f.s+" B")+"</td><td>";
  if(!f.d)h+='<button class="btn gray" onclick="location.href=\'/api/fs/get?path='+enc(join(j.path,f.n))+'\'">'+t("Scarica")+'</button><button class="btn gray" onclick="fEdit(\''+q+'\')">'+t("Modifica")+'</button>';
  h+='<button class="btn gray" onclick="fRen(\''+q+'\')">'+t("Rinomina")+'</button><button class="btn red" onclick="fDel(\''+q+'\')">'+t("Elimina")+'</button></td></tr>'});
 $("ftab").innerHTML=h;$("fspace").textContent=tf("Usati {0} su {1}",kb(j.used),kb(j.total))})}
function fUp(){if(fp=="/")return;fList(fp.substring(0,fp.lastIndexOf("/"))||"/")}
function fDo(u,d){return post(u,d).then(function(r){return r.json()}).then(function(j){msg("fm",j.ok?t("Fatto"):j.err,j.ok);if(j.ok)fList(fp)})}
function fMk(){var n=prompt(t("Nome della nuova cartella:"));if(n)fDo("/api/fs/mkdir",{path:join(fp,n)})}
function fNew(){var n=prompt(t("Nome del nuovo file:"));if(n){fedp=join(fp,n);$("fedt").textContent=t("Nuovo file:")+" "+fedp;$("fedx").value="";$("fed").className="card"}}
function fEdit(p){api("/api/fs/text?path="+enc(p)).then(function(r){return r.text()}).then(function(t){fedp=p;$("fedt").textContent=t("Modifica:")+" "+p;$("fedx").value=t;$("fed").className="card"})}
function fSave(){post("/api/fs/save",{path:fedp,text:$("fedx").value}).then(function(r){return r.json()}).then(function(j){msg("fm",j.ok?t("Salvato"):j.err,j.ok);if(j.ok){$("fed").className="card hide";fList(fp)}})}
function fRen(p){var n=prompt(t("Nuovo nome o percorso:"),p);if(n&&n!=p)fDo("/api/fs/ren",{from:p,to:n})}
function fDel(p){if(confirm(tf("Eliminare {0}?",p)))fDo("/api/fs/del",{path:p})}
function fUpload(){var f=$("fup").files[0];if(!f){msg("fm",t("Scegli un file"),false);return}
 var fd=new FormData();fd.append("file",f,f.name);
 api("/api/fs/up?dir="+enc(fp),{method:"POST",body:fd}).then(function(r){return r.json()}).then(function(j){msg("fm",j.ok?t("Caricato"):j.err,j.ok);if(j.ok)fList(fp)})}
function esc(s){return String(s).replace(/[&<>"]/g,function(c){return{"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;"}[c]})}
function dh(){$("stat").className=$("w_dhcp").value=="0"?"":"hide"}
function fillWifi(){api("/api/settings").then(function(r){return r.json()}).then(function(c){
 $("w_ssid").value=c.staSsid;$("w_dhcp").value=c.staDhcp?"1":"0";$("w_ip").value=c.ip;$("w_mask").value=c.mask;$("w_gw").value=c.gw;$("w_d1").value=c.dns1;$("w_d2").value=c.dns2;dh()})}
function saveWifi(){post("/api/wifi/save",{ssid:$("w_ssid").value,pass:$("w_pass").value,dhcp:$("w_dhcp").value,ip:$("w_ip").value,mask:$("w_mask").value,gw:$("w_gw").value,d1:$("w_d1").value,d2:$("w_d2").value})
 .then(function(r){return r.json()}).then(function(j){msg("wm",j.ok?t("Salvato. La scheda si sta collegando: se cambia indirizzo riconnettiti."):j.err,j.ok)})}
)VOSPAGE"
R"VOSPAGE(function apOnly(){post("/api/wifi/ap",{}).then(function(r){return r.json()}).then(function(j){msg("wm",j.ok?t("Modo AP attivo (192.168.4.1)"):j.err,j.ok)})}
function runCmd(){var v=$("si").value;$("si").value="";$("so").textContent+="> "+v+"\n";
 post("/api/shell",{c:v}).then(function(r){return r.text()}).then(function(t){var o=$("so");o.textContent+=t+"\n";o.scrollTop=o.scrollHeight})}
function loadTasks(){api("/api/tasks").then(function(r){return r.text()}).then(function(t){$("tk").textContent=t})}
function fillLed(){api("/api/settings").then(function(r){return r.json()}).then(function(c){$("l_mode").value=c.ledMode;
 $("l_col").value="#"+("000000"+c.ledColor.toString(16)).slice(-6);$("l_br").value=c.ledBrightness;$("m_mode").value=c.led2Mode;$("m_pin").value=c.led2Pin;$("m_br").value=c.led2Bright;$("m_inv").value=c.led2Invert?"1":"0"})}
function saveLed(){post("/api/led",{mode:$("l_mode").value,color:parseInt($("l_col").value.slice(1),16),br:$("l_br").value})
 .then(function(r){return r.json()}).then(function(j){msg("lm2",j.ok?t("Salvato"):j.err,j.ok)})}
function saveLed2(){post("/api/led2",{mode:$("m_mode").value,pin:$("m_pin").value,br:$("m_br").value,inv:$("m_inv").value})
 .then(function(r){return r.json()}).then(function(j){msg("lm3",j.ok?t("Salvato"):j.err,j.ok)})}
var PM=[],PSEL=-1,PWARN=false,PTM=null,PT={gpio:-1};
function pinPos(g){if(g<=10)return[20+g*26,40];if(g<=21)return[20+(g-11)*26,100];if(g<=47)return[20+(g-38)*26,160];return[150,215]}
function loadPins(){api("/api/pins").then(function(r){return r.json()}).then(function(p){
 var h="<tr><th>GPIO</th><th>"+t("Usato da")+"</th><th>"+t("Descrizione")+"</th></tr>";
 p.forEach(function(x){h+="<tr><td>"+x.gpio+"</td><td>"+esc(x.owner)+"</td><td>"+esc(x.note)+"</td></tr>"});
 $("pintab").innerHTML=h});
 api("/api/pinmap").then(function(r){return r.json()}).then(function(m){PM=m;drawPins();pinPanel()});
 clearInterval(PTM);PTM=setInterval(pinPoll,1000);pinPoll()}
function pinInfo(g){for(var i=0;i<PM.length;i++)if(PM[i].g==g)return PM[i];return null}
function drawPins(){var s="";
 PM.forEach(function(x){var q=pinPos(x.g),c=x.ok?"#2e9e5b":(x.owner&&x.g!=PT.gpio?"#3fa7ff":"#3a4352");
  if(x.g==PT.gpio)c="#e0a020";
  s+='<g style="cursor:pointer" onclick="pinSel('+x.g+')"><circle cx="'+q[0]+'" cy="'+q[1]+'" r="10" fill="'+c+'" stroke="'+(x.g==PSEL?"#fff":"none")+'" stroke-width="2"/><text x="'+q[0]+'" y="'+(q[1]+3)+'" font-size="8" text-anchor="middle" fill="#fff">'+x.g+'</text></g>'});
 s+='<text x="150" y="240" font-size="10" text-anchor="middle" fill="#8b98a8">'+esc(t("Verde = provabile, arancio = in prova"))+'</text>';
 $("pinsvg").innerHTML=s}
function pinSel(g){PSEL=g;drawPins();pinPanel()}
function pinPanel(){var e=$("pinpanel"),h="";
 h+='<label>'+esc(t("Altro GPIO"))+'</label><input type="number" id="pn_g" min="0" max="48" value="'+(PSEL<0?"":PSEL)+'" style="width:90px" onchange="pinSel(parseInt(this.value))"> ';
 var x=pinInfo(PSEL);
 if(PSEL<0){e.innerHTML=h;return}
 h+='<h3 style="margin-top:10px">GPIO'+PSEL+'</h3>';
 if(!x){h+='<div class="msg ko">'+esc(t("Pin inesistente su questa scheda"))+'</div>'}
 else if(!x.ok&&PSEL!=PT.gpio){h+='<div class="msg ko">'+esc(t("Non provabile:"))+" "+esc(x.why)+'</div>'}
 else{
  h+='<div id="pnlv" style="margin:6px 0"></div>';
  h+='<button class="btn" onclick="pinGo(\'high\')">'+esc(t("Alto (3,3 V)"))+'</button><button class="btn gray" onclick="pinGo(\'low\')">'+esc(t("Basso (0 V)"))+'</button><button class="btn" onclick="pinGo(\'blink\')">'+esc(t("Lampeggia"))+'</button>';
  h+='<div style="margin-top:8px"><button class="btn" onclick="pinGo(\'read\')">'+esc(t("Leggi"))+'</button> <select id="pn_pull"><option value="none">'+esc(t("senza resistenza"))+'</option><option value="up">'+esc(t("resistenza verso 3,3 V"))+'</option><option value="down">'+esc(t("resistenza verso massa"))+'</option></select></div>';
  h+='<div style="margin-top:8px"><button class="btn red" onclick="pinGo(\'off\')">'+esc(t("Rilascia"))+'</button></div>';
  h+='<div id="pnm" class="msg"></div>'}
 e.innerHTML=h;pinLevel()}
function pinWarn(){if(PWARN)return true;
 var w=[t("ATTENZIONE: una prova sbagliata puo danneggiare la scheda."),"",t("- Usa solo 3,3 V. Mai 5 V o tensioni piu alte su un pin."),t("- Ogni pin regge pochi mA: un LED vuole una resistenza da 220-470 ohm."),t("- Motori, rel&#232; e carichi grandi vanno pilotati con un transistor."),t("- Non collegare due uscite insieme e non toccare i pin di alimentazione."),"",t("Vuoi continuare?")].join("\n");
 if(!confirm(w))return false;PWARN=true;return true}
function pinGo(a){if(PSEL<0)return;if(a!="off"&&!pinWarn())return;
 var d={gpio:PSEL,action:a};if(a=="read")d.pull=$("pn_pull").value;
 post("/api/pintest",d).then(function(r){return r.json()}).then(function(j){
  if(!j.ok){msg("pnm",j.err,false);return}setTimeout(pinPoll,300)})}
function pinPoll(){if(cur!="pin"){clearInterval(PTM);return}
 api("/api/pintest").then(function(r){return r.json()}).then(function(j){var ch=(j.gpio!=PT.gpio);PT=j;
  if(ch){api("/api/pinmap").then(function(r){return r.json()}).then(function(m){PM=m;drawPins();pinPanel()})}else pinLevel()}).catch(function(){})}
function pinLevel(){var e=$("pnlv");if(!e)return;
 if(PT.gpio!=PSEL){e.textContent=t("Nessuna prova in corso su questo pin");return}
 var an={high:t("Alto"),low:t("Basso"),blink:t("Lampeggia"),read:t("Lettura")}[PT.action];
 e.innerHTML=esc(an)+" - "+esc(t("livello letto"))+": <b>"+(PT.level?t("ALTO"):t("BASSO"))+"</b> - "+esc(tf("si spegne tra {0} s",PT.left))}
function fillSys(){api("/api/settings").then(function(r){return r.json()}).then(function(c){$("s_host").value=c.hostname;$("s_dom").value=c.domain;$("s_fq").textContent=t("Nome completo:")+" "+c.hostname+(c.domain?"."+c.domain:"")})}
function saveSys(){post("/api/system",{hostname:$("s_host").value,domain:$("s_dom").value}).then(function(r){return r.json()}).then(function(j){msg("sm",j.ok?t("Salvato"):j.err,j.ok);if(j.ok)fillSys()})}
function loadLangs(){
 api("/api/langs").then(function(r){return r.json()}).then(function(l){LANGS=l;fillLangSel();
  var h="<table><tr><th>"+t("Codice")+"</th><th>"+t("Lingua")+"</th><th></th></tr>";
  l.forEach(function(x){h+="<tr><td>"+esc(x.code)+"</td><td>"+esc(x.name)+"</td><td>"+(x.code=="it"?esc(t("Predefinita (nel firmware)")):'<button class="btn red" onclick="delLang(\''+esc(x.code)+'\')">'+esc(t("Elimina"))+'</button>')+"</td></tr>"});
  $("lglist").innerHTML=h+"</table>"});
 api("/api/fs/list?path=/").then(function(r){return r.json()}).then(function(j){if(j.ok)$("lgsp").textContent=tf("Memoria interna: usati {0} su {1}",kb(j.used),kb(j.total))})}
function upLang(){var f=$("lgf").files[0];if(!f){msg("lgm",t("Scegli un file"),false);return}
 var m=f.name.match(/^([a-z0-9]{2,8})\.json$/);if(!m||m[1]=="it"){msg("lgm",t("Il nome deve essere come en.json (codice di 2-8 lettere minuscole)"),false);return}
 var code=m[1],fd=new FormData();fd.append("file",f,f.name);
 api("/api/fs/up?dir=/lang",{method:"POST",body:fd}).then(function(r){return r.json()}).then(function(j){
  if(!j.ok){msg("lgm",j.err,false);return}
  msg("lgm",t("Lingua caricata"),true);$("lgf").value="";
  if(code==LANG)setLang(code);else loadLangs()})}
function delLang(c){if(!confirm(tf("Eliminare la lingua {0}?",c)))return;
 var go=function(){post("/api/fs/del",{path:"/lang/"+c+".json"}).then(function(r){return r.json()}).then(function(j){msg("lgm",j.ok?t("Fatto"):j.err,j.ok);loadLangs()})};
 if(c==LANG){post("/api/lang",{code:"it"}).then(function(){LANG="it";DICT={};try{localStorage.setItem("vl","it")}catch(e){}refreshView();go()})}else go()}
function loadLic(){fetch("/api/license").then(function(r){return r.json()}).then(function(l){
  var h="";l.forEach(function(x){h+='<button class="btn gray" onclick="showLic(\''+esc(x.id)+'\')">'+esc(x.title)+'</button>'});$("licbtn").innerHTML=h}).catch(function(){})}
)VOSPAGE"
R"VOSPAGE(function showLic(id){fetch("/api/license?id="+encodeURIComponent(id)).then(function(r){return r.text()}).then(function(x){var e=$("licx");e.textContent=x;e.className="";e.scrollTop=0})}
function loadLog(){api("/api/log").then(function(r){return r.text()}).then(function(t){$("lg").textContent=t})}
function restoreCfg(){var f=$("cf").files[0];if(!f){msg("cm",t("Scegli un file"),false);return}
 f.text().then(function(t){return post("/api/config/restore",{t:t})}).then(function(r){return r.json()}).then(function(j){msg("cm",j.ok?t("Ripristinato"):j.err,j.ok)})}
function factory(){if(!confirm(t("Azzerare TUTTO (anche la password)?")))return;post("/api/factory",{}).then(function(){msg("cm",t("Riavvio in corso..."),true)})}
function chPass(){post("/api/passwd",{o:$("p_old").value,n:$("p_new").value}).then(function(r){return r.json()}).then(function(j){msg("pm",j.ok?t("Password cambiata"):j.err,j.ok)})}
fetch("/api/auth").then(function(r){return r.json()}).then(function(j){AUTHSET=j.set;initLang(j.lang)}).catch(function(){initLang("it")});
api("/api/status").then(function(){start()}).catch(function(){});
</script></body></html>
)VOSPAGE";
