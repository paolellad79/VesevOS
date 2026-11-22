#!/usr/bin/env python3
# VesevOS - genera recovery/VesevOS_Recovery_IDF/recovery_page.h (la pagina del recovery).
# La parte SHA-256/HMAC/hashPass e COPIATA da web/src/js_cripto.js (stessa formula della pagina del firmware).
# La pagina del recovery riceve il codice compattato: nei sorgenti le funzioni stanno su piu righe.
import os, re
from compattajs import compatta_js
R = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
js = open(os.path.join(R, 'web/src/js_cripto.js'), encoding='utf-8').read().split('\n')
a = [i for i, l in enumerate(js) if l.startswith('var SHK=')][0]
b = [i for i, l in enumerate(js) if l.startswith('function hashPass(')][0]
c = [i for i, l in enumerate(js) if i > b and l == '}'][0]   # la graffa in colonna zero chiude hashPass
crypto = compatta_js('\n'.join(js[a:c + 1]))
# Grafica del login di VesevOS (disposizione del 20/11/2026): due colonne sopra 820 px, una colonna sotto. Variabili di colore con gli stessi
# nomi della pagina di VesevOS, cosi il tema /etc/tema.css (letto dal recovery) funziona allo stesso modo.
css = '''
/* recovery: ogni tonalita azzurra del login diventa rossa (accento); errore arancione, ok verde, cosi non si confondono con l'accento */
:root{--act:var(--ac);--okt:var(--ok);--kot:var(--ko);--bg:#0b1220;--card:#111d31;--tx:#e6edf3;--mut:#9aa8b9;--ac:#FF6B63;--acink:#0b1220;
 --ok:#3ddc84;--ko:#fdba74;--in:#0e1829;--g2:#263752;--bd:#1f2e44;--vbg:#3a1a1c;--pan:#CC0605;--paninc:#fff}
:root[data-theme=light]{--act:#CC0605;--okt:#0b7040;--kot:#9a3412;--bg:#f4f7fb;--card:#fff;--tx:#0b1220;--mut:#4d5c70;--ac:#CC0605;
 --acink:#fff;--ok:#16a765;--ko:#9a3412;--in:#eef3f9;--g2:#d6e0ec;--bd:#dbe4ef;--vbg:#fde8e7}
*{box-sizing:border-box}
/* accessibilita (stesse scelte e stesse chiavi della pagina di VesevOS: dimensione del testo, contrasto alto, daltonici, meno animazioni) */
:root[data-hc="1"]{--tx:#fff;--mut:#cfd8e3;--bg:#000;--card:#0a0e13;--in:#000;--bd:#6f8196;--g2:#6f8196}
:root[data-hc="1"][data-theme=light]{--tx:#000;--mut:#2b3743;--bg:#fff;--card:#fff;--in:#fff;--bd:#5c6b7a;--g2:#5c6b7a}
:root[data-cb="1"]{--ok:#56b4e9;--ko:#ff9a3c}
:root[data-cb="1"][data-theme=light]{--okt:#0072b2;--kot:#a84600;--ok:#0072b2;--ko:#c25200}
:root[data-fs="1"]{zoom:1.15}
:root[data-fs="2"]{zoom:1.3}
:root[data-rm="1"] *,:root[data-rm="1"] *::before,:root[data-rm="1"] *::after{animation-duration:.001ms!important;animation-iteration-count:1!important;transition-duration:.001ms!important}
.lbar{position:absolute;top:12px;right:12px;z-index:5;display:flex;gap:6px}   /* in alto a destra della pagina, come nel login di VesevOS */
.ib{width:40px;height:40px;padding:0;margin:0;display:grid;place-items:center;background:none;border:1px solid transparent;border-radius:10px;color:var(--mut);cursor:pointer}
.ib:hover,.ib[aria-expanded=true]{background:var(--vbg);color:var(--tx)}
svg.i{width:24px;height:24px;fill:none;stroke:currentColor;stroke-width:1.8;stroke-linecap:round;stroke-linejoin:round;display:block}
.pop{position:fixed;z-index:30;background:var(--card);border:1px solid var(--bd);border-radius:14px;padding:8px;min-width:240px;max-width:min(92vw,330px);box-shadow:0 8px 30px rgba(0,0,0,.35)}
.pop[hidden]{display:none}
.pop button.op{display:flex;align-items:center;gap:10px;width:100%;text-align:left;border:0;background:none;color:var(--tx);padding:9px 10px;border-radius:10px;margin:0;cursor:pointer}
.pop button.op:hover,.pop button.op.on{background:var(--vbg)}
.pop b{display:block;margin:2px 4px 8px}
.pop label{display:block;margin:8px 4px 0}
select{font:inherit;padding:8px;border-radius:10px;border:1px solid var(--g2);background:var(--in);color:inherit;width:100%;margin:4px 0}
#info{white-space:pre-line}
.pwf{position:relative;display:flex;align-items:center}
.pwf input{padding-right:46px;margin:6px 0}
.pwf .eye{position:absolute;right:4px;top:50%;transform:translateY(-50%);width:36px;height:36px;margin:0}
body{margin:0;background:var(--bg);color:var(--tx);font:16px/1.5 system-ui,-apple-system,"Segoe UI",Arial,sans-serif}
/* telefono: una colonna; i due pannelli si dissolvono e i pezzi si mettono in ordine */
#rc{display:flex;flex-direction:column;max-width:30rem;margin:0 auto;padding:16px}
.lpan,.lfrm,.lbasso{display:contents}
.lfrm>*{order:1}
.lpan>*,.lbasso>*{order:2}
.hero{text-align:center;margin:6px 0 10px}
.hname{display:flex;flex-direction:column;align-items:center;justify-content:center;gap:6px;margin:0 0 6px;font-size:26px;letter-spacing:.5px}   /* cornetto al centro, scritta sotto (come il login, 20/11/2026) */
.logo{width:104px;height:104px;display:block}   /* misura scelta dall'utente il 20/11/2026, uguale al login */
/* Safari: un svg senza altezza dentro un contenitore flex sparisce, quindi aspect-ratio e height:auto sono obbligatori */
.mount{width:100%;height:auto;aspect-ratio:852/170;display:block;margin:0 auto}
.hmount{max-width:300px;color:var(--ac)}
.lmount,.lben{display:none}
.hrec{margin:0;font-size:15px;font-weight:600;color:var(--ac)}   /* telefono: la parola Recovery sta accanto al nome, perche il pannello grande non c e */
.c{background:var(--card);border:1px solid var(--bd);border-radius:16px;padding:14px;margin:12px 0}
label{color:var(--mut);font-size:13px}
input,button{font:inherit;padding:10px;border-radius:12px;border:1px solid var(--g2);background:var(--in);color:inherit;width:100%;margin:6px 0}
button{background:var(--ac);color:var(--acink);border:0;border-radius:999px;font-weight:700;min-height:44px;cursor:pointer}
button.gray{background:var(--g2);color:var(--tx)}
button:disabled{opacity:.5}
.m{min-height:1.4em}
.e{color:var(--kot)}
.o{color:var(--okt)}
/* oltre al colore, una dicitura: l'errore non si distingue dall'accento rosso solo dal colore */
.e:not(:empty)::before{content:"Errore: ";font-weight:700}
.o:not(:empty)::before{content:"Fatto: ";font-weight:700}
progress{width:100%;accent-color:var(--ac)}
:focus-visible{outline:2px solid var(--ac);outline-offset:2px}
small{color:var(--mut)}
#dz{border:2px dashed var(--g2);border-radius:12px;padding:8px;margin:6px 0}
#dz.h{border-color:var(--ac);background:var(--vbg)}
.lclk{text-align:center;margin:14px 0 0;color:var(--mut)}
.lt1{font-size:26px;font-weight:600;color:var(--tx);font-variant-numeric:tabular-nums}
.lt1 i{font-style:normal}
.lt1 i.off{opacity:.15}
.lt2,.lt3{font-size:12px;margin-top:2px}
.lmsg{text-align:center;color:var(--mut);margin:6px 0 0;white-space:pre-line;overflow-wrap:anywhere}
.hide{display:none}
@media(prefers-reduced-motion:reduce){.lt1 i.off{opacity:1}}
/* computer: due colonne a tutta altezza, pannello azzurro a sinistra e controlli a destra */
@media(min-width:821px){
 body{background:var(--card)}
 .lbar{top:16px;right:16px}
 #rc{display:grid;grid-template-columns:1fr 1fr;max-width:none;margin:0;padding:0;min-height:100vh}
 /* pannello rosso (rosso del cornetto, scritte bianche), uguale nei temi chiaro e scuro; un tema puo cambiare --pan e --paninc */
 /* tre righe: ora e data in alto, il Vesuvio esattamente al centro (verticale e orizzontale), Recovery e messaggio sotto (20/11/2026) */
 .lpan{display:grid;grid-template-rows:1fr auto 1fr;justify-items:center;padding:12vh 5vw;background:var(--pan);color:var(--paninc)}
 .lbasso{display:flex;flex-direction:column;align-items:center;align-self:start;padding-top:32px;width:100%}
 .lpan>*,.lfrm>*{order:0}
 .lfrm{display:flex;flex-direction:column;justify-content:center;padding:48px max(5vw,calc((50vw - 380px)/2))}
 .c{background:none;border:0;border-radius:0;padding:0;margin:0}
 .lpan .lclk{align-self:start;justify-self:stretch;margin:0;color:inherit}
 .lpan .lt1{font-size:clamp(44px,7vw,96px);line-height:1.05;color:inherit}
 .lpan .lt2{font-size:clamp(15px,1.5vw,20px);margin-top:8px}
 .lpan .lt3{font-size:clamp(12px,1vw,14px);margin-top:4px}
 .lmount{display:block;width:min(100%,420px);margin:0;align-self:center;color:inherit}
 .hmount{display:none}
 .hrec{display:none}
 .lben{display:block;align-self:stretch;text-align:center;font-size:clamp(40px,5vw,72px);font-weight:700;line-height:1.1;margin:0}
 .lmsg{align-self:center;text-align:center;color:inherit;font-size:clamp(16px,1.5vw,20px);margin:12px 0 0;max-width:36em}
}
'''
css = re.sub(r'/\*.*?\*/', '', css, flags=re.S)
css = re.sub(r'\s*\n\s*', '', css)
# i simboli (cornetto e Vesuvio) si prendono dalla pagina di VesevOS: un solo disegno, niente copie a mano
body_top = open(os.path.join(R, 'web/src/body_top.html'), encoding='utf-8').read()
simboli = ''.join(re.search(r'<symbol id="%s".*?</symbol>' % i, body_top, re.S).group(0) for i in ('i-cornetto', 'i-vesuvio', 'i-auto', 'i-sun', 'i-moon', 'i-a11y', 'i-occhio', 'i-occhio-barrato'))
ver = re.search(r'REC_VERSION "([^"]+)"', open(os.path.join(R, 'recovery/VesevOS_Recovery_IDF/rec_version.h')).read()).group(1)
# Orologio e messaggio del login. Il recovery non ha l'ora: si usa quella del browser (l'ora della scheda non e disponibile).
# Il messaggio arriva da /api/vista (JSON) e si mostra SEMPRE con textContent, mai come HTML.
js_vista_src = """
var GIORNI=["Domenica","Lunedi","Martedi","Mercoledi","Giovedi","Venerdi","Sabato"],
 MESI=["gennaio","febbraio","marzo","aprile","maggio","giugno","luglio","agosto","settembre","ottobre","novembre","dicembre"],
 NOME="",TESTO="";
function due(n){
 return(n<10?"0":"")+n
}
function fusoTesto(d){
 var o=-d.getTimezoneOffset(),a=Math.abs(o),tz="";
 try{
  tz=Intl.DateTimeFormat().resolvedOptions().timeZone||""
 }catch(e){}
 return(tz?tz+", ":"")+"UTC"+(o<0?"-":"+")+due(Math.floor(a/60))+":"+due(a%60)
}
function dataLunga(d){
 return GIORNI[d.getDay()]+" "+d.getDate()+" "+MESI[d.getMonth()]+" "+d.getFullYear()
}
function msgVar(testo,d){
 return String(testo||"").replace(/\\{\\{|\\{(nome|scheda|data|ora|fuso)\\}/g,function(m,k){
  if(m=="{{")return"{";
  if(k=="nome")return NOME;
  if(k=="data")return dataLunga(d);
  if(k=="ora")return d.getHours()+":"+due(d.getMinutes());
  if(k=="fuso")return fusoTesto(d);
  return m
 })
}
function orologio(){
 var d=new Date();
 $("ch").textContent=d.getHours();
 $("cm").textContent=due(d.getMinutes());
 $("cc").className=d.getSeconds()%2?"off":"";
 $("cd").textContent=dataLunga(d);
 $("cz").textContent="Ora del tuo dispositivo: "+fusoTesto(d);
 if(TESTO)$("lmsg").textContent=msgVar(TESTO,d)
}
setInterval(orologio,1000);
orologio();
fetch("/api/vista").then(function(r){
 return r.json()
}).then(function(j){
 NOME=j.nm||"";
 TESTO=j.ml||"";
 $("lmsg").className=TESTO?"lmsg":"lmsg hide";
 orologio()
}).catch(function(){});
"""
js_vista_src = js_vista_src + """
var TEMA="auto",A11={fs:0,hc:"auto",cb:0,rm:"auto"};
try{
 TEMA=localStorage.getItem("vth")||"auto";
 var sv=JSON.parse(localStorage.getItem("va11y")||"{}");
 for(var kk in A11)if(sv[kk]!==undefined)A11[kk]=sv[kk]
}catch(e){}
function chiedi(x){
 return!!(window.matchMedia&&matchMedia(x).matches)
}
function applicaAspetto(){
 var d=document.documentElement;
 d.setAttribute("data-theme",TEMA=="auto"?(chiedi("(prefers-color-scheme: light)")?"light":"dark"):TEMA);
 d.setAttribute("data-fs",A11.fs);
 d.setAttribute("data-hc",(A11.hc=="on"||(A11.hc=="auto"&&chiedi("(prefers-contrast: more)")))?"1":"0");
 d.setAttribute("data-cb",A11.cb?"1":"0");
 d.setAttribute("data-rm",(A11.rm=="on"||(A11.rm=="auto"&&chiedi("(prefers-reduced-motion: reduce)")))?"1":"0");
 $("lthu").setAttribute("href",TEMA=="auto"?"#i-auto":TEMA=="light"?"#i-sun":"#i-moon");
 $("lthb").title="Tema: "+(TEMA=="auto"?"automatico":TEMA=="light"?"chiaro":"scuro");
 ["auto","light","dark"].forEach(function(k){
  $("tm_"+k).className=k==TEMA?"op on":"op"
 });
 ["fs","hc","cb","rm"].forEach(function(k){
  $("l_"+k).value=A11[k]
 })
}
function sotto(m,e){
 var r=e.currentTarget.getBoundingClientRect();
 m.style.top=(r.bottom+6)+"px";
 m.style.right=Math.max(8,innerWidth-r.right)+"px"
}
function chiudiFinestre(){
 $("thm").hidden=true;
 $("a11p").hidden=true;
 $("lthb").setAttribute("aria-expanded","false");
 $("lab").setAttribute("aria-expanded","false")
}
function temaMenu(e){
 e.stopPropagation();
 var apri=$("thm").hidden;
 chiudiFinestre();
 if(apri){
  sotto($("thm"),e);
  $("thm").hidden=false;
  $("lthb").setAttribute("aria-expanded","true")
 }
}
function a11yMenu(e){
 e.stopPropagation();
 var apri=$("a11p").hidden;
 chiudiFinestre();
 if(apri){
  sotto($("a11p"),e);
  $("a11p").hidden=false;
  $("lab").setAttribute("aria-expanded","true");
  $("l_fs").focus()
 }
}
function temaScegli(m){
 TEMA=m;
 try{localStorage.setItem("vth",m)}catch(e){}
 applicaAspetto();
 chiudiFinestre()
}
function a11yScegli(k,v){
 A11[k]=v;
 try{localStorage.setItem("va11y",JSON.stringify(A11))}catch(e){}
 applicaAspetto()
}
function a11yRipristina(){
 A11={fs:0,hc:"auto",cb:0,rm:"auto"};
 try{localStorage.removeItem("va11y")}catch(e){}
 applicaAspetto()
}
document.addEventListener("click",function(e){
 if(!e.target.closest(".pop"))chiudiFinestre()
});
document.addEventListener("keydown",function(e){
 if(e.key=="Escape")chiudiFinestre()
});
function occhio(){
 var i=$("p"),on=i.type=="password";
 i.type=on?"text":"password";
 $("leyes").setAttribute("href",on?"#i-occhio-barrato":"#i-occhio");
 $("leye").setAttribute("aria-pressed",on?"true":"false");
 $("leye").setAttribute("aria-label",on?"Nascondi la password":"Mostra la password")
}
applicaAspetto();
/* Dopo il riavvio della scheda la pagina si riapre da sola: si chiede /api/ver ogni 2 secondi. Finche risponde il recovery si aspetta;
   quando la scheda non risponde si aspetta ancora; quando risponde VesevOS (rimando al https o 404) si apre la pagina. Dopo 2 minuti si smette. */
var RIAVVIO=0;
function attendiRiavvio(){
 if(RIAVVIO)return;
 RIAVVIO=Date.now();
 function chiedi(){
  if(Date.now()-RIAVVIO>120000){
   RIAVVIO=0;
   msg("um","La scheda non risponde ancora: ricarica la pagina quando riparte",false);
   return
  }
  fetch("/api/ver",{cache:"no-store",redirect:"manual"}).then(function(r){
   if(r.type=="opaqueredirect"||r.status==404){
    location.replace("/");
    return
   }
   return r.json().then(function(){
    setTimeout(chiedi,2000)
   })
  }).catch(function(){
   setTimeout(chiedi,2000)
  })
 }
 setTimeout(chiedi,6000)
}
"""
js_vista = compatta_js(js_vista_src)
html = r'''<!doctype html><html lang="it"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>VesevOS Recovery</title>
<script>(function(){var d=document.documentElement,m="auto",A={fs:0,hc:"auto",cb:0,rm:"auto"};try{m=localStorage.getItem("vth")||"auto";var s=JSON.parse(localStorage.getItem("va11y")||"{}");for(var k in A)if(s[k]!==undefined)A[k]=s[k]}catch(e){}
var q=function(x){return!!(window.matchMedia&&matchMedia(x).matches)};
d.setAttribute("data-theme",m=="auto"?(q("(prefers-color-scheme: light)")?"light":"dark"):m);
d.setAttribute("data-fs",A.fs);d.setAttribute("data-hc",(A.hc=="on"||(A.hc=="auto"&&q("(prefers-contrast: more)")))?"1":"0");
d.setAttribute("data-cb",A.cb?"1":"0");d.setAttribute("data-rm",(A.rm=="on"||(A.rm=="auto"&&q("(prefers-reduced-motion: reduce)")))?"1":"0")})()</script>
<style>__CSS__</style><link rel="stylesheet" href="/tema.css"></head>
<body><svg width="0" height="0" style="position:absolute" aria-hidden="true">__SIMBOLI__</svg>
<div id="rc"><section class="lpan"><div class="lclk"><div class="lt1" translate="no"><span id="ch"></span><i id="cc">:</i><span id="cm"></span></div>
<div class="lt2" id="cd"></div><div class="lt3" id="cz"></div></div>
<svg class="mount lmount" aria-hidden="true"><use href="#i-vesuvio"/></svg>
<div class="lbasso"><div class="lben">Recovery</div><div class="lmsg hide" id="lmsg" translate="no"></div></div></section>
<main class="lfrm"><div class="lbar"><button class="ib" id="lthb" onclick="temaMenu(event)" aria-haspopup="menu" aria-expanded="false" title="Tema"><svg class="i" aria-hidden="true"><use id="lthu" href="#i-auto"/></svg></button><button class="ib" id="lab" onclick="a11yMenu(event)" aria-haspopup="dialog" aria-expanded="false" title="Accessibilita"><svg class="i" aria-hidden="true"><use href="#i-a11y"/></svg></button></div><div class="hero"><h1 class="hname"><svg class="logo" aria-hidden="true"><use href="#i-cornetto"/></svg><b>VesevOS</b><small class="hrec">Recovery</small></h1>
<svg class="mount hmount" aria-hidden="true"><use href="#i-vesuvio"/></svg></div>
<div class="c" id="a"><div id="lu1"><label>Utente<input id="u" autocomplete="username"></label></div>
<label id="pl">Password<div class="pwf"><input id="p" type="password" autocomplete="current-password"><button type="button" class="ib eye" id="leye" onclick="occhio()" aria-label="Mostra la password" aria-pressed="false"><svg class="i" width="20" height="20" aria-hidden="true"><use id="leyes" href="#i-occhio"/></svg></button></div></label>
<button id="bl" onclick="login()">Entra</button><div class="m" id="lm"></div>
<small>Recovery 0.4.3. Per entrare serve un amministratore di VesevOS.</small></div>
<div class="c" id="b" hidden><div id="info" class="m"></div>
<div id="dz"><label>Firmware (file .bin di VesevOS)<input id="f" type="file" accept=".bin"></label><div class="m" id="fn"><small>Trascina qui il file .bin oppure usa il pulsante.</small></div></div>
<label>SHA-256 del file (obbligatorio: lo trovi nella pagina del rilascio)<input id="sh" autocomplete="off" spellcheck="false" placeholder="64 caratteri esadecimali"></label>
<button id="bu" onclick="up()">Carica e installa</button><progress id="pg" value="0" max="100" hidden></progress><div class="m" id="um"></div>
<button id="bb" class="gray" onclick="boot()">Avvia VesevOS senza aggiornare</button></div>
</main></div>
<div class="pop" id="thm" role="menu" hidden><button class="op" role="menuitem" onclick="temaScegli('auto')" id="tm_auto"><svg class="i" aria-hidden="true"><use href="#i-auto"/></svg><span>Automatico (come il computer)</span></button><button class="op" role="menuitem" onclick="temaScegli('light')" id="tm_light"><svg class="i" aria-hidden="true"><use href="#i-sun"/></svg><span>Chiaro</span></button><button class="op" role="menuitem" onclick="temaScegli('dark')" id="tm_dark"><svg class="i" aria-hidden="true"><use href="#i-moon"/></svg><span>Scuro</span></button></div>
<div class="pop" id="a11p" role="dialog" aria-label="Accessibilita" hidden><b>Accessibilita</b>
<label for="l_fs">Dimensione del testo</label><select id="l_fs" onchange="a11yScegli('fs',+this.value)"><option value="0">Normale</option><option value="1">Grande</option><option value="2">Molto grande</option></select>
<label for="l_hc">Contrasto alto</label><select id="l_hc" onchange="a11yScegli('hc',this.value)"><option value="auto">Automatico (come il dispositivo)</option><option value="on">Attivo</option><option value="off">Spento</option></select>
<label for="l_cb">Colori adatti ai daltonici</label><select id="l_cb" onchange="a11yScegli('cb',+this.value)"><option value="0">Spento</option><option value="1">Attivo (blu, arancione, giallo)</option></select>
<label for="l_rm">Riduci le animazioni</label><select id="l_rm" onchange="a11yScegli('rm',this.value)"><option value="auto">Automatico (come il dispositivo)</option><option value="on">Attivo</option><option value="off">Spento</option></select>
<button class="gray" onclick="a11yRipristina()">Ripristina</button></div>
<script>
function $(i){return document.getElementById(i)}
function msg(i,t,ok){var e=$(i);e.textContent=t;e.className="m "+(ok?"o":"e")}
__CRYPTO__
var TOK="",CODE=false;
function post(u,o){var b=new URLSearchParams(o);return fetch(u,{method:"POST",body:b,headers:TOK?{"X-T":TOK}:{}}).then(function(r){return r.json()})}
fetch("/api/ver").then(function(r){return r.json()}).then(function(j){CODE=!!j.code;if(CODE){$("lu1").hidden=true;$("pl").firstChild.textContent="Codice mostrato sulla seriale"}}).catch(function(){});
function login(){var u=$("u").value.trim(),p=$("p").value;$("bl").disabled=true;msg("lm","Controllo...",true);
 var done=function(j){$("bl").disabled=false;if(j.ok){TOK=j.t;$("p").value="";$("a").hidden=true;$("b").hidden=false;info()}else msg("lm",j.err||"Errore",false)};
 if(CODE){post("/api/login",{code:p}).then(done).catch(function(){done({})});return}
 post("/api/start",{u:u}).then(function(s){if(!s.ok){done(s);return}
  var mac=hmacHex(hashPass(s.salt,p,s.iter),s.nonce);return post("/api/login",{u:u,nonce:s.nonce,mac:mac}).then(done)}).catch(function(){done({})})}
/* "VesevOS 1": nome del programma e numero scritto nel file dal sistema di compilazione; sui firmware fino alla 1.8.28a vale sempre 1 (non e la versione vera) */
function descriviApp(a){if(!a)return "nessuno valido";var m=a.match(/^(.+) (\S+)$/);if(!m||/^NON/.test(a))return a;return m[1]+(m[2]=="1"?" (versione non indicata)":" "+m[2])}
function info(){post("/api/info",{}).then(function(j){if(j.ok){MAX=j.max||0;$("info").textContent="Firmware attuale: "+descriviApp(j.app)+"\nSpazio massimo: "+Math.floor(j.max/1024)+" KB"+(j.board?"\nScheda: "+j.board:"")}})}
function fnm(){var f=$("f").files[0];$("fn").textContent=f?f.name+" ("+Math.floor(f.size/1024)+" KB)":"Trascina qui il file .bin oppure usa il pulsante."}
$("f").onchange=fnm;
window.addEventListener("dragover",function(e){e.preventDefault();if(!$("b").hidden)$("dz").className="h"});
window.addEventListener("dragleave",function(e){if(!e.relatedTarget)$("dz").className=""});
window.addEventListener("drop",function(e){e.preventDefault();$("dz").className="";if($("b").hidden)return;var l=e.dataTransfer&&e.dataTransfer.files;if(!l||!l.length)return;if(l.length>1){msg("um","Trascina un solo file",false);return}$("f").files=l;fnm();msg("um","",true)});
var MAX=0,MARK=String.fromCharCode(123,70,87,58,86,101,115,101,118,79,83,125);
function chk(buf){var v=new Uint8Array(buf);if(v.length<48)return "File troppo piccolo: non e un firmware";
 if(v[0]!==0xE9)return "Non e un firmware (intestazione errata)";if((v[12]|(v[13]<<8))!==9)return "Firmware per un altro tipo di chip (serve ESP32-S3)";
 if((v[32]|(v[33]<<8)|(v[34]<<16)|(v[35]<<24))>>>0!==0xABCD5432)return "Non e un firmware Arduino-ESP32 valido";
 if(MAX&&v.length>MAX)return "File troppo grande per lo spazio disponibile ("+Math.floor(v.length/1024)+" KB, massimo "+Math.floor(MAX/1024)+" KB)";
 var m=[];for(var i=0;i<MARK.length;i++)m.push(MARK.charCodeAt(i));var ok=false;
 for(var k=0;k+m.length<=v.length&&!ok;k++){if(v[k]!==m[0])continue;var j=1;while(j<m.length&&v[k+j]===m[j])j++;if(j===m.length)ok=true}
 return ok?"":"Non e un firmware VesevOS (manca il marchio)"}
function up(){var f=$("f").files[0];if(!f){msg("um","Scegli il file .bin",false);return}
 var sh=$("sh").value.trim().toLowerCase();if(!/^[0-9a-f]{64}$/.test(sh)){msg("um","Serve lo SHA-256 del file: 64 caratteri (0-9, a-f)",false);return}
 $("bu").disabled=true;$("bb").disabled=true;msg("um","Controllo il file...",true);
 var r=new FileReader();r.onerror=function(){$("bu").disabled=false;$("bb").disabled=false;msg("um","Non riesco a leggere il file",false)};
 r.onload=function(){var e=chk(r.result);if(e){$("bu").disabled=false;$("bb").disabled=false;msg("um",e,false);return}send(f,sh)};r.readAsArrayBuffer(f)}
function send(f,sh){$("pg").hidden=false;msg("um","Carico "+Math.floor(f.size/1024)+" KB... non spegnere la scheda",true);
 var x=new XMLHttpRequest(),d=new FormData();d.append("fw",f,f.name);x.open("POST","/upload");x.setRequestHeader("X-T",TOK);if(sh)x.setRequestHeader("X-Sha",sh);
 x.upload.onprogress=function(e){if(e.lengthComputable)$("pg").value=Math.round(e.loaded*100/e.total)};
 x.onload=function(){var j={};try{j=JSON.parse(x.responseText)}catch(e){}$("bu").disabled=false;$("bb").disabled=false;
  if(j.ok)msg("um","Installato e controllato (SHA-256 "+(j.sha||"").slice(0,16)+"...). La scheda riparte con il nuovo firmware: la pagina si riapre da sola (circa 10-20 secondi).",true),attendiRiavvio();else msg("um",j.err||"Errore nel caricamento",false)};
 x.onerror=function(){$("bu").disabled=false;$("bb").disabled=false;msg("um","Collegamento interrotto",false)};x.send(d)}
function boot(){post("/api/boot",{}).then(function(j){msg("um",j.ok?"VesevOS riparte (circa 10 secondi).":(j.err||"Errore"),!!j.ok)})}
__VISTA__</script></body></html>'''.replace('__CRYPTO__', crypto).replace('__CSS__', css).replace('__SIMBOLI__', simboli).replace('__VISTA__', js_vista)
# La copia per lo sketch Arduino (recovery/VesevOS_Recovery/recovery_page.h) NON si rigenera piu (20/11/2026): lo sketch e sostituito
# e la pagina nuova usa rotte (/tema.css, /api/vista) che lo sketch non ha. Il file resta com'era.

# Recovery 0.5.1: il gettone vale anche per l'avvio normale (/api/boot); errori in italiano e distinti (scaduto, usato, mancante).
# Recovery 0.5.0: la pagina accetta anche il permesso di VesevOS (#g=<gettone> nell'indirizzo, modo 2 dell'ordine): nessun login, il gettone va in X-G.
idf_html = html.replace('Recovery 0.4.3.', 'Recovery ' + ver + '.')
# Solo per il recovery IDF: la frase non nomina piu Arduino-ESP32 (il controllo e lo stesso: parola ABCD5432 dell'intestazione dell'app).
idf_html = idf_html.replace('Non e un firmware Arduino-ESP32 valido', 'Non e un firmware ESP32 valido')
for a, b in [
    ('headers:TOK?{"X-T":TOK}:{}', 'headers:TOK?{"X-T":TOK}:GR?{"X-G":GR}:{}'),
    ('var TOK="",CODE=false;', 'var TOK="",CODE=false,GR=(location.hash.match(/^#g=([0-9a-f]{32})$/)||[])[1]||"";if(GR)history.replaceState(null,"",location.pathname);'),
    ('x.setRequestHeader("X-T",TOK);', 'if(TOK)x.setRequestHeader("X-T",TOK);else if(GR)x.setRequestHeader("X-G",GR);'),
    ('$("info").textContent="Firmware attuale: "', '$("info").textContent=(GR&&!TOK?"Permesso di VesevOS attivo. ":"")+"Firmware attuale: "'),
    ('MAX=j.max||0;', 'MAX=j.max||0;CHIP=j.chipId||0;CHIPN=j.chipName||"";'),
    # Il chip lo dichiara la scheda (/api/info); senza il dato resta il solo controllo dell'intestazione e decide il server.
    ('var MAX=0,', 'var MAX=0,CHIP=0,CHIPN="",'),
    ('if((v[12]|(v[13]<<8))!==9)return "Firmware per un altro tipo di chip (serve ESP32-S3)";', 'if(CHIP&&(v[12]|(v[13]<<8))!==CHIP)return "Firmware per un altro tipo di chip (serve "+CHIPN+")";'),
    ('function info(){post("/api/info",{}).then(function(j){if(j.ok){', 'function info(){post("/api/info",{}).then(function(j){if(!j.ok&&GR&&!TOK){GR="";$("a").hidden=false;$("b").hidden=true;msg("lm",j.err||"Il link di VesevOS non e valido: entra con la password",false)}if(j.ok){'),
    ('fetch("/api/ver").then(', 'if(GR){$("a").hidden=true;$("b").hidden=false;info()}\nfetch("/api/ver").then('),
    ('function boot(){post("/api/boot",{}).then(function(j){msg("um",j.ok?"VesevOS riparte (circa 10 secondi).":(j.err||"Errore"),!!j.ok)})}',
     'function boot(){post("/api/boot",{}).then(function(j){msg("um",j.ok?"VesevOS riparte: la pagina si riapre da sola (circa 10-20 secondi).":(j.err||"Non riesco ad avviare VesevOS"),!!j.ok);if(j.ok)attendiRiavvio()}).catch(function(){msg("um","Collegamento interrotto: VesevOS sta forse ripartendo, riprovo da solo",true);attendiRiavvio()})}'),
]:
    assert a in idf_html, a
    idf_html = idf_html.replace(a, b)

# Recovery 0.5.0 (ESP-IDF puro): stessa pagina, versione nuova, senza Arduino.h/PROGMEM.
out = os.path.join(R, 'recovery/VesevOS_Recovery_IDF/recovery_page.h')
with open(out, 'w', encoding='utf-8') as f:
    f.write('// Generato da tools/mkrecpage.py - NON modificare a mano.\n#pragma once\n')
    f.write('static const char REC_PAGE[] = R"VOSREC(' + idf_html + ')VOSREC";\n')
print('ok recovery IDF recovery_page.h')
