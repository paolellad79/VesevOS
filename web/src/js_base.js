(function(){var of=window.fetch.bind(window),act=0,q=[];
 function run(j){act++;var done=false,rel=function(){if(!done){done=true;act--;nx()}},tm=setTimeout(rel,15000);
  of(j.u,j.o).then(function(r){clearTimeout(tm);rel();j.res(r)},function(e){clearTimeout(tm);rel();j.rej(e)})}
 function nx(){while(act<2&&q.length)run(q.shift())}
 window.fetch=function(u,o){return new Promise(function(res,rej){q.push({u:u,o:o,res:res,rej:rej});nx()})}})();
var GRP=[["home","Home"],["dev","Dispositivi"],["rete","Rete"],["prog","Programmi"],["sys","Sistema"],["sec","Sicurezza"],["adm","Admin"],["info","Informazioni"]];
var GMAP={home:["sum"],dev:["dmgr","stat","temp","mem","cpu","led","pin","io","btn","usb","ble","rad"],rete:["wifi","wps","ap","radio","dhcp","dns","http","https","mdns","mqtt","mesh"],prog:["auto","script","app"],sys:["ora","ntp","loc","power","file","task","wd","log","shell"],sec:["sec","mfa","users","fw","cert","audit"],adm:["conf","boot","selftest"],info:["help","acc","legal"]};
var GICO={home:"i-home",dev:"i-usb",rete:"i-wifi",prog:"i-prog",sys:"i-gear",sec:"i-shield",adm:"i-adm",info:"i-info"};
var TABS=[["sum","Panoramica"],["wifi","Wi-Fi"],["wps","WPS"],["mqtt","MQTT"],["mesh","Rete tra schede"],["pin","Pin"],["io","Periferiche"],["led","LED"],["task","Task"],["file","File"],["ora","Ora"],["loc","Localizzazione"],["auto","Automazioni"],["script","Script"],["app","App"],["boot","Avvio"],["wd","Watchdog"],["log","Log"],["shell","Terminale"],["conf","Configurazione"],["legal","Note legali"],["help","Aiuto"],["sec","Password"],["users","Utenti"],["fw","Filtro IP"],["https","HTTPS"],["audit","Stato conformita"],["mfa","MFA"],["cert","Certificato"],["stat","Statistiche"],["selftest","Autotest"],["power","Risparmio energia"],["acc","Accessibilita"],["ap","Punto di accesso"],["http","HTTP"],["radio","Radio"],["ntp","NTP"],["dhcp","DHCP"],["dns","DNS"],["mdns","mDNS"],["ble","Bluetooth"],["dmgr","Elenco dispositivi"],["btn","Tasto BOOT"],["usb","USB"],["temp","Temperatura del chip"],["mem","Memoria"],["cpu","Processore"],["rad","Radio 2,4 GHz"]];
var SECN=["Stato","Comandi","Radio","Diagnostica"];
var TSEC={stat:0,temp:0,mem:0,cpu:0,led:1,pin:1,io:1,btn:1,usb:1,ble:2,rad:2,task:3,wd:3,log:3,shell:3};
var LASTG={};
function gOf(id){for(var g in GMAP)if(GMAP[g].indexOf(id)>=0)return g;return"home"}
var A11Y={fs:0,hc:"auto",cb:0,rm:"auto"};try{var _a=JSON.parse(localStorage.getItem("va11y")||"{}");for(var _k in A11Y)if(_a[_k]!==undefined)A11Y[_k]=_a[_k]}catch(e){}
function a11yApply(){var d=document.documentElement,mm=function(q){return!!(window.matchMedia&&matchMedia(q).matches)};
 d.setAttribute("data-fs",A11Y.fs);d.setAttribute("data-hc",(A11Y.hc=="on"||(A11Y.hc=="auto"&&mm("(prefers-contrast: more)")))?"1":"0");
 d.setAttribute("data-cb",A11Y.cb?"1":"0");d.setAttribute("data-rm",(A11Y.rm=="on"||(A11Y.rm=="auto"&&mm("(prefers-reduced-motion: reduce)")))?"1":"0");
 ["fs","hc","cb","rm"].forEach(function(k){var e=$("a_"+k);if(e)e.value=A11Y[k]});if(typeof cpApply=="function")try{cpApply()}catch(e){}}
function a11ySet(k,v){A11Y[k]=v;try{localStorage.setItem("va11y",JSON.stringify(A11Y))}catch(e){}a11yApply();live(t("Salvato"))}
function a11yReset(){A11Y={fs:0,hc:"auto",cb:0,rm:"auto"};try{localStorage.removeItem("va11y")}catch(e){}a11yApply();live(t("Ripristinato"))}
function a11yOpen(){$("thm").className="pop hide";show("acc");setTimeout(function(){var e=$("a_fs");if(e){e.scrollIntoView({block:"center"});e.focus()}},60)}
function live(x){var e=$("live");if(!e)return;e.textContent="";setTimeout(function(){e.textContent=x},30)}
function a11yLabels(){[].forEach.call(document.querySelectorAll("button[title],a[title],label.btn"),function(b){if(!b.getAttribute("aria-label")&&!b.textContent.trim()&&b.title)b.setAttribute("aria-label",b.title)});
 [].forEach.call(document.querySelectorAll("svg.i:not([aria-hidden])"),function(s){s.setAttribute("aria-hidden","true")});
 [].forEach.call(document.querySelectorAll(".msg:not([role])"),function(m){m.setAttribute("role","status")});
 [].forEach.call(document.querySelectorAll("label:not([for])"),function(l){var n=l.nextElementSibling;if(n&&/^(INPUT|SELECT|TEXTAREA)$/.test(n.tagName)&&n.id&&!l.querySelector("input,select,textarea"))l.htmlFor=n.id});
 [].forEach.call(document.querySelectorAll("input[placeholder]:not([aria-label])"),function(i){if(!(i.labels&&i.labels.length))i.setAttribute("aria-label",i.placeholder)});
 [].forEach.call(document.querySelectorAll("[onclick]:not(button):not(a):not(input):not(select):not(textarea):not(label):not([role]):not([tabindex])"),function(e){if(e.closest("svg"))return;e.setAttribute("role","button");e.tabIndex=0;e.setAttribute("data-kb","1")});
 [].forEach.call(document.querySelectorAll("svg:not([role]):not([aria-label]):not([aria-hidden])"),function(s){s.setAttribute("aria-hidden","true")})}
document.addEventListener("keydown",function(e){var x=e.target;if((e.key=="Enter"||e.key==" ")&&x&&x.getAttribute&&x.getAttribute("data-kb")){e.preventDefault();x.click()}});
(function(){var tm=0;new MutationObserver(function(){clearTimeout(tm);tm=setTimeout(a11yLabels,120)}).observe(document.body,{childList:true,subtree:true});
 if(window.matchMedia){["(prefers-contrast: more)","(prefers-reduced-motion: reduce)"].forEach(function(q){var m=matchMedia(q);if(m.addEventListener)m.addEventListener("change",a11yApply)})}})();
a11yApply();a11yLabels();
var THEME="auto";try{THEME=localStorage.getItem("vth")||"auto"}catch(e){}
function themeApply(){var m=THEME;if(m=="auto")m=(window.matchMedia&&matchMedia("(prefers-color-scheme: light)").matches)?"light":"dark";document.documentElement.setAttribute("data-theme",m);
 var nm=THEME=="auto"?t("Automatico"):THEME=="light"?t("Chiaro"):t("Scuro");
 var u=$("thu");if(u)u.setAttribute("href",THEME=="auto"?"#i-auto":THEME=="light"?"#i-sun":"#i-moon");var b=$("thb");if(b)b.title=t("Tema")+": "+nm;var l=$("thl");if(l)l.textContent=nm;
 ["auto","light","dark"].forEach(function(k){var e=$("tm_"+k);if(e)e.className=k==THEME?"on":""})}
function themeSet(m){THEME=m;try{localStorage.setItem("vth",THEME)}catch(e){}themeApply();$("thm").className="pop hide"}
function thMenu(e){e.stopPropagation();var m=$("thm");m.className=m.classList.contains("hide")?"pop":"pop hide"}
document.addEventListener("click",function(e){var m=$("thm");if(m&&!m.classList.contains("hide")&&!e.target.closest("#thm"))m.className="pop hide"});
document.addEventListener("keydown",function(e){if(e.key=="Escape"){var m=$("thm");if(m)m.className="pop hide";var um=$("um");if(um)um.className="pop hide";var al=$("alerts");if(al&&!al.classList.contains("hide"))al.className="alerts hide"}
 if(e.ctrlKey&&(e.key=="`"||e.code=="Backquote")&&loggedIn()){e.preventDefault();cpToggle()}});
if(window.matchMedia)try{matchMedia("(prefers-color-scheme: light)").addEventListener("change",function(){if(THEME=="auto")themeApply()})}catch(e){}
var DICT={},LANG="it",LANGS=[{code:"it",name:"Italiano"}];
function t(s){var v=DICT[s];return v?v:s}
function tf(s){var a=arguments;return t(s).replace(/\{(\d)\}/g,function(m,i){return a[+i+1]})}
/* durata in secondi: "45 s", "12 min", "3 h 5 min", "5 g 23 h 11 min" */
function fmtDur(n){n=Math.max(0,Math.floor(+n||0));var d=Math.floor(n/86400),h=Math.floor(n%86400/3600),m=Math.floor(n%3600/60);
 return n<60?tf("{0} s",n):n<3600?tf("{0} min",m):n<86400?tf("{0} h",h)+" "+tf("{0} min",m):tf("{0} g",d)+" "+tf("{0} h",h)+" "+tf("{0} min",m)}
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
 ["title","aria-label"].forEach(function(at){var L=document.querySelectorAll("[data-tt]["+at+"]");for(var i=0;i<L.length;i++){var e=L[i],k="__"+at;if(e[k]===undefined)e[k]=e.getAttribute(at);
  e.setAttribute(at,(LANG!="it"&&DICT[e[k]])?DICT[e[k]]:e[k])}});
 document.documentElement.lang=LANG;a11yLabels()}
function loadDict(code){if(code=="it"){DICT={};return Promise.resolve()}
 return fetch("/api/langfile?code="+encodeURIComponent(code)).then(function(r){if(!r.ok)throw 0;return r.json()}).then(function(d){DICT=d}).catch(function(){DICT={};LANG="it"})}
function loggedIn(){return !$("app").classList.contains("hide")}
function refreshView(){fillLangSel();applyStatic();
 if(loggedIn()){buildNav();poll();if(cur=="loc")loadLangs();if(cur=="log")loadLog()}else showLoginText()}
function setLang(code,noSave){LANG=code;try{localStorage.setItem("vl",code)}catch(e){}
 loadDict(code).then(function(){refreshView();
  if(!noSave&&loggedIn())post("/api/lang",{code:LANG}).catch(function(){})})}
function initLang(dev){var c=dev||"it";try{c=localStorage.getItem("vl")||c}catch(e){}
 fetch("/api/langs").then(function(r){return r.json()}).then(function(l){LANGS=l;if(!l.some(function(x){return x.code==c}))c="it";setLang(c,true)}).catch(function(){setLang("it",true)})}
var cur="sum",S=null,timer=null;
var CP={o:false,h:280};try{CP.h=+localStorage.getItem("vph")||280;CP.o=localStorage.getItem("vpo")=="1"}catch(e){}
function cpApply(){var mx=Math.max(140,innerHeight-130);CP.h=Math.max(120,Math.min(mx,CP.h));document.documentElement.style.setProperty("--ph",CP.h+"px");
 document.body.classList.toggle("cpo",CP.o&&loggedIn());try{localStorage.setItem("vph",CP.h);localStorage.setItem("vpo",CP.o?"1":"0")}catch(e){}
 var o=$("tbt");if(o)o.style.color=CP.o?"var(--ac)":""}
function cpToggle(f){CP.o=f===undefined?!CP.o:f;cpApply();if(CP.o){shLoadCmds();var i=$("si");if(i)i.focus()}}
function cpSize(k){CP.h=k==0?170:k==1?Math.round(innerHeight*.45):innerHeight-130;if(!CP.o)CP.o=true;cpApply()}
function cpTitles(){var m={tbt:"Terminale",cps0:"Piccolo",cps1:"Medio",cps2:"Quasi a tutto schermo",cpx:"Chiudi"};for(var k in m){var e=$(k);if(e){e.title=t(m[k]);e.setAttribute("aria-label",t(m[k]))}}}
(function(){var h=$("cph"),y0=0,h0=0,dr=false;
 h.addEventListener("pointerdown",function(e){if(e.target.closest("button"))return;dr=true;y0=e.clientY;h0=CP.h;try{h.setPointerCapture(e.pointerId)}catch(x){}});
 h.addEventListener("pointermove",function(e){if(!dr)return;CP.h=h0+(y0-e.clientY);cpApply()});
 function up(){dr=false}h.addEventListener("pointerup",up);h.addEventListener("pointercancel",up);
 window.addEventListener("resize",cpApply)})();

function $(i){return document.getElementById(i)}
function api(u,o){return fetch(u,o).then(function(r){connOk();if(r.status==401){showLogin();throw 0}if(r.status==403&&location.protocol=="http:")return r.clone().json().then(function(j){if(j.err=="https"){location.href="https://"+location.hostname+"/";throw 0}return r},function(){return r});return r},function(e){connFail();throw e})}
function post(u,d){if(/^\/api\/(reboot|sleep|factory|recovery)/.test(u))CONN.hold=Date.now()+120000;var b=new URLSearchParams(d).toString();return api(u,{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:b})}
function msg(id,t,ok){var e=$(id);clearTimeout(e._tm);e.setAttribute("role",ok?"status":"alert");e.className="msg";void e.offsetWidth;e.textContent=t;e.className="msg "+(ok?"ok":"ko");
 if(ok)e._tm=setTimeout(function(){e.classList.add("out");e._tm=setTimeout(function(){e.className="msg";e.textContent=""},400)},3000)}

/* ===== scheda non raggiungibile: pagina in grigio con finestra e pulsante Aggiorna ===== */
var CONN={bad:0,off:false,back:false,hold:0};
function connTxt(back){CONN.back=back;$("cnt").textContent=back?t("Scheda di nuovo raggiungibile"):t("Scheda non raggiungibile");
 $("cnd").textContent=back?t("La scheda risponde di nuovo: premi Aggiorna per ricaricare la pagina."):t("La pagina non riesce piu a parlare con la scheda: puo essere spenta, riavviata o fuori dalla rete. Controlla l'alimentazione e il Wi-Fi, poi premi Aggiorna.")}
function connOk(){CONN.bad=0;if(CONN.off&&!CONN.back)connTxt(true)}
function connFail(){if(Date.now()<CONN.hold)return;if(++CONN.bad<2)return;
 if(!CONN.off){CONN.off=true;document.body.classList.add("cx");$("conn").className="dlgw";connTxt(false);setTimeout(function(){$("cnb").focus()},30)}
 else if(CONN.back)connTxt(false)}
setInterval(function(){if(document.hidden||Date.now()<CONN.hold)return;fetch("/api/auth",{cache:"no-store"}).then(connOk,connFail)},5000);
window.addEventListener("offline",function(){CONN.bad=1;connFail()});
