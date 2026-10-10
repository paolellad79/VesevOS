// Generato da tools/mkrecpage.py - NON modificare a mano.
#pragma once
#include <Arduino.h>
static const char REC_PAGE[] PROGMEM = R"VOSREC(<!doctype html><html lang="it"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
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
var SHK=[0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2];
function utf8(s){return new TextEncoder().encode(s)}
function sha256b(m){var l=m.length,n=((l+9+63)>>6)<<6,b=new Uint8Array(n),W=new Int32Array(64),H=[0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19],i,j;
 b.set(m);b[l]=0x80;var bl=l*8;b[n-4]=bl>>>24;b[n-3]=bl>>>16;b[n-2]=bl>>>8;b[n-1]=bl;b[n-5]=Math.floor(l/536870912);
 for(i=0;i<n;i+=64){for(j=0;j<16;j++)W[j]=b[i+4*j]<<24|b[i+4*j+1]<<16|b[i+4*j+2]<<8|b[i+4*j+3];
  for(j=16;j<64;j++){var x=W[j-15],y=W[j-2];W[j]=(((x>>>7|x<<25)^(x>>>18|x<<14)^(x>>>3))+W[j-7]+((y>>>17|y<<15)^(y>>>19|y<<13)^(y>>>10))+W[j-16])|0}
  var a=H[0],c=H[1],d=H[2],e=H[3],f=H[4],g=H[5],h=H[6],k=H[7];
  for(j=0;j<64;j++){var t1=(k+((f>>>6|f<<26)^(f>>>11|f<<21)^(f>>>25|f<<7))+((f&g)^(~f&h))+SHK[j]+W[j])|0,t2=(((a>>>2|a<<30)^(a>>>13|a<<19)^(a>>>22|a<<10))+((a&c)^(a&d)^(c&d)))|0;
   k=h;h=g;g=f;f=(e+t1)|0;e=d;d=c;c=a;a=(t1+t2)|0}
  H[0]=H[0]+a|0;H[1]=H[1]+c|0;H[2]=H[2]+d|0;H[3]=H[3]+e|0;H[4]=H[4]+f|0;H[5]=H[5]+g|0;H[6]=H[6]+h|0;H[7]=H[7]+k|0}
 var o=new Uint8Array(32);for(i=0;i<8;i++){o[4*i]=H[i]>>>24;o[4*i+1]=H[i]>>>16;o[4*i+2]=H[i]>>>8;o[4*i+3]=H[i]}return o}
function hexb(b){var s="";for(var i=0;i<b.length;i++)s+=(b[i]<16?"0":"")+b[i].toString(16);return s}
function sha256Hex(s){return hexb(sha256b(utf8(s)))}
function hmacHex(key,msg){var k=utf8(key);if(k.length>64)k=sha256b(k);var ip=new Uint8Array(64),op=new Uint8Array(64),m=utf8(msg),i;
 for(i=0;i<64;i++){ip[i]=(k[i]||0)^0x36;op[i]=(k[i]||0)^0x5c}
 var a=new Uint8Array(64+m.length);a.set(ip);a.set(m,64);var ih=sha256b(a),z=new Uint8Array(96);z.set(op);z.set(ih,64);return hexb(sha256b(z))}
function hashPass(salt,p,it){var h=sha256Hex(salt+p);for(var i=0;i<it;i++)h=sha256Hex(h+salt+p);return h}
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
</script></body></html>)VOSREC";
