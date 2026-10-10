#!/usr/bin/env python3
# VesevOS - genera recovery/VesevOS_Recovery/recovery_page.h (la pagina del recovery).
# La parte SHA-256/HMAC/hashPass e COPIATA da web/src/js_pin_accesso.js (stessa formula della pagina del firmware).
import os, re
R = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
js = open(os.path.join(R, 'web/src/js_pin_accesso.js'), encoding='utf-8').read().split('\n')
a = [i for i, l in enumerate(js) if l.startswith('var SHK=')][0]
b = [i for i, l in enumerate(js) if l.startswith('function hashPass(')][0]
crypto = '\n'.join(js[a:b + 1])
html = r'''<!doctype html><html lang="it"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>VesevOS Recovery</title>
<style>body{font:16px system-ui,sans-serif;margin:0;background:#101418;color:#e8edf2}main{max-width:30rem;margin:0 auto;padding:16px}
h1{font-size:1.3rem}.c{background:#1a2128;border-radius:10px;padding:14px;margin:12px 0}input,button{font:inherit;padding:10px;border-radius:8px;border:1px solid #3a4652;background:#0e1318;color:inherit;width:100%;box-sizing:border-box;margin:6px 0}
button{background:#2b6cb0;border:0;cursor:pointer}button:disabled{opacity:.5}.m{min-height:1.4em}.e{color:#ff8a80}.o{color:#8be28b}progress{width:100%}small{color:#9aa7b4}</style></head>
<body><main><h1>VesevOS Recovery</h1>
<div class="c" id="a"><div id="lu1"><label>Utente<input id="u" autocomplete="username"></label></div>
<label id="pl">Password<input id="p" type="password" autocomplete="current-password"></label>
<button id="bl" onclick="login()">Entra</button><div class="m" id="lm"></div>
<small>Recovery 0.3.0. Per entrare serve un amministratore di VesevOS.</small></div>
<div class="c" id="b" hidden><div id="info" class="m"></div>
<label>Firmware (file .bin di VesevOS)<input id="f" type="file" accept=".bin"></label>
<label>SHA-256 atteso (facoltativo: lo trovi nella pagina del rilascio)<input id="sh" autocomplete="off" spellcheck="false" placeholder="64 caratteri esadecimali"></label>
<button id="bu" onclick="up()">Carica e installa</button><progress id="pg" value="0" max="100" hidden></progress><div class="m" id="um"></div>
<button id="bb" onclick="boot()" style="background:#3a4652">Avvia VesevOS senza aggiornare</button></div>
</main>
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
function info(){post("/api/info",{}).then(function(j){if(j.ok){MAX=j.max||0;$("info").textContent="Firmware attuale: "+(j.app||"nessuno valido")+" - spazio massimo "+Math.floor(j.max/1024)+" KB"}})}
var MAX=0,MARK=String.fromCharCode(123,70,87,58,86,101,115,101,118,79,83,125);
function chk(buf){var v=new Uint8Array(buf);if(v.length<48)return "File troppo piccolo: non e un firmware";
 if(v[0]!==0xE9)return "Non e un firmware (intestazione errata)";if((v[12]|(v[13]<<8))!==9)return "Firmware per un altro tipo di chip (serve ESP32-S3)";
 if((v[32]|(v[33]<<8)|(v[34]<<16)|(v[35]<<24))>>>0!==0xABCD5432)return "Non e un firmware Arduino-ESP32 valido";
 if(MAX&&v.length>MAX)return "File troppo grande per lo spazio disponibile ("+Math.floor(v.length/1024)+" KB, massimo "+Math.floor(MAX/1024)+" KB)";
 var m=[];for(var i=0;i<MARK.length;i++)m.push(MARK.charCodeAt(i));var ok=false;
 for(var k=0;k+m.length<=v.length&&!ok;k++){if(v[k]!==m[0])continue;var j=1;while(j<m.length&&v[k+j]===m[j])j++;if(j===m.length)ok=true}
 return ok?"":"Non e un firmware VesevOS (manca il marchio)"}
function up(){var f=$("f").files[0];if(!f){msg("um","Scegli il file .bin",false);return}
 var sh=$("sh").value.trim().toLowerCase();if(sh&&!/^[0-9a-f]{64}$/.test(sh)){msg("um","Lo SHA-256 deve avere 64 caratteri (0-9, a-f)",false);return}
 $("bu").disabled=true;$("bb").disabled=true;msg("um","Controllo il file...",true);
 var r=new FileReader();r.onerror=function(){$("bu").disabled=false;$("bb").disabled=false;msg("um","Non riesco a leggere il file",false)};
 r.onload=function(){var e=chk(r.result);if(e){$("bu").disabled=false;$("bb").disabled=false;msg("um",e,false);return}send(f,sh)};r.readAsArrayBuffer(f)}
function send(f,sh){$("pg").hidden=false;msg("um","Carico "+Math.floor(f.size/1024)+" KB... non spegnere la scheda",true);
 var x=new XMLHttpRequest(),d=new FormData();d.append("fw",f,f.name);x.open("POST","/upload");x.setRequestHeader("X-T",TOK);if(sh)x.setRequestHeader("X-Sha",sh);
 x.upload.onprogress=function(e){if(e.lengthComputable)$("pg").value=Math.round(e.loaded*100/e.total)};
 x.onload=function(){var j={};try{j=JSON.parse(x.responseText)}catch(e){}$("bu").disabled=false;$("bb").disabled=false;
  if(j.ok)msg("um","Installato e controllato (SHA-256 "+(j.sha||"").slice(0,16)+"...). La scheda riparte con il nuovo firmware (circa 10 secondi).",true);else msg("um",j.err||"Errore nel caricamento",false)};
 x.onerror=function(){$("bu").disabled=false;$("bb").disabled=false;msg("um","Collegamento interrotto",false)};x.send(d)}
function boot(){post("/api/boot",{}).then(function(j){msg("um",j.ok?"VesevOS riparte (circa 10 secondi).":(j.err||"Errore"),!!j.ok)})}
</script></body></html>'''.replace('__CRYPTO__', crypto)
out = os.path.join(R, 'recovery/VesevOS_Recovery/recovery_page.h')
with open(out, 'w', encoding='utf-8') as f:
    f.write('// Generato da tools/mkrecpage.py - NON modificare a mano.\n#pragma once\n#include <Arduino.h>\n')
    f.write('static const char REC_PAGE[] PROGMEM = R"VOSREC(' + html + ')VOSREC";\n')
print('ok recovery_page.h:', len(html), 'byte')
