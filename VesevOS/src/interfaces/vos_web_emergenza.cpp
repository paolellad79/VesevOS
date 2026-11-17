// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web_emergenza.cpp
// Pagina di emergenza: piccola, dentro il firmware, per quando i file della pagina (.vrs) mancano o non sono validi.
// Fa solo due cose: l'accesso dell'amministratore (con il login di sempre: sfida con SHA-256, prova di lavoro, codice MFA)
// e il caricamento di un file .vrs verso POST /api/risorse/carica (vos_web_risorse.cpp).
// Con la lingua in errore (lingua = true) la stessa pagina avvisa, carica i file .vld/.vlp con POST /api/fs/up?dir=/lang (stessa rotta e stessi
// controlli della pagina normale) e offre "Usa l'italiano" (POST /api/lang, code=it).
// Senza amministratore (scheda mai configurata) non offre niente: dice solo cosa fare. Nessun caricamento senza accesso.
// I testi arrivano dal firmware con tr(): la pagina e uguale in tutte le lingue. Il resto e fisso (JavaScript compatto).
// Prova del JavaScript: build/tests/risorse/prova_emergenza.js (SHA-256, formula della password, HMAC, uguali al firmware).
#include "vos_web_int.h"

namespace webx {

namespace {

// Prima parte: tutto fino ai testi. Gli id sono corti apposta (la pagina deve restare piccola).
const char kTesta[] =
    "<meta charset=utf-8><meta name=viewport content=\"width=device-width,initial-scale=1\">"
    "<meta name=color-scheme content=\"light dark\"><title>VesevOS</title>"
    "<style>body{font:16px sans-serif;max-width:28em;margin:2em auto;padding:0 1em}"
    "input,button{font:inherit;padding:.4em;margin:.2em 0}input:not([type=file]){width:100%;box-sizing:border-box}input[type=file]{max-width:100%}</style>"
    "<h1 id=a></h1><p id=b></p>"
    "<form id=l hidden><p><label for=u id=c></label><input id=u autocomplete=username>"
    "<p><label for=w id=d></label><input id=w type=password autocomplete=current-password>"
    "<p id=m hidden><label for=k id=e></label><input id=k autocomplete=one-time-code inputmode=numeric>"
    "<p><button id=g></button></form>"
    "<form id=v hidden><p><label for=r id=i></label><input id=r type=file accept=.vrs required>"
    "<p><button id=j></button><p><button id=z type=button hidden></button></form>"
    "<p id=s role=status></p><script>var L=";

// Seconda parte: il programma. SHA-256 compatto (costanti calcolate), formula della password uguale a hashPass della pagina
// vera (security/vos_auth.cpp), HMAC, prova di lavoro a pezzi, accesso, codice MFA, caricamento.
const char kCoda[] = R"JS(;
var $=function(i){return document.getElementById(i)},E=new TextEncoder,tok;
function say(t){$("s").textContent=t}
function F(x){return(x-Math.floor(x))*4294967296|0}
var K=[],I=[];
(function(){for(var n=2,c=0,d;c<64;n++){for(d=2;d*d<=n&&n%d;d++);if(d*d>n){if(c<8)I[c]=F(Math.sqrt(n));K[c++]=F(Math.cbrt(n))}}})();
function R(x,n){return x>>>n|x<<32-n}
function Hb(m){
var n=m.length,N=n+72>>6<<4,w=new Int32Array(N),W=new Int32Array(64),h=I.slice(),i,j,a,x,y,t,u;
for(i=0;i<n;i++)w[i>>2]|=m[i]<<24-8*(i&3);
w[n>>2]|=128<<24-8*(n&3);w[N-1]=n*8;
for(i=0;i<N;i+=16){
for(j=0;j<64;j++){if(j<16)W[j]=w[i+j];else{x=W[j-15];y=W[j-2];W[j]=W[j-16]+(R(x,7)^R(x,18)^x>>>3)+W[j-7]+(R(y,17)^R(y,19)^y>>>10)}}
a=h.slice();
for(j=0;j<64;j++){
t=a[7]+(R(a[4],6)^R(a[4],11)^R(a[4],25))+(a[4]&a[5]^~a[4]&a[6])+K[j]+W[j];
u=(R(a[0],2)^R(a[0],13)^R(a[0],22))+(a[0]&a[1]^a[0]&a[2]^a[1]&a[2]);
a.pop();a.unshift(t+u|0);a[4]=a[4]+t|0}
for(j=0;j<8;j++)h[j]=h[j]+a[j]|0}
var o=new Uint8Array(32);
for(j=0;j<32;j++)o[j]=h[j>>2]>>>24-8*(j&3);
return o}
function X(b){for(var s="",i=0;i<b.length;i++)s+=("0"+b[i].toString(16)).slice(-2);return s}
function S(s){return X(Hb(E.encode(s)))}
function hm(k,m){
k=E.encode(k);m=E.encode(m);
var a=new Uint8Array(64+m.length),b=new Uint8Array(96),i;
for(i=0;i<64;i++){a[i]=(k[i]||0)^54;b[i]=(k[i]||0)^92}
a.set(m,64);b.set(Hb(a),64);
return X(Hb(b))}
function pow(nc,bits){
return new Promise(function(ok){
if(!bits)return ok("");
var n=0,z=bits>>2,r=bits&3;
(function st(){
for(var e=n+1500,h,i;n<e;n++){
h=S(nc+":"+n);
for(i=0;i<z&&h[i]=="0";)i++;
if(i==z&&(!r||parseInt(h[z],16)<16>>r))return ok(""+n)}
setTimeout(st)})()})}
function P(u,d){return fetch(u,{method:"POST",body:new URLSearchParams(d)}).then(function(r){return r.json()})}
function done(){$("l").hidden=true;$("v").hidden=false;$("z").hidden=!L[12];say("")}
$("l").onsubmit=function(e){
e.preventDefault();
var u=$("u").value.trim(),p=$("w").value,q;
say(L[11]);
if(tok)q=P("/api/login/mfa",{tok:tok,code:$("k").value.trim(),now:Math.floor(Date.now()/1000)});
else q=P("/api/login/start",{u:u}).then(function(s){
if(s.ok===false)return s;
var h=S(s.salt+p),i;
for(i=0;i<s.iter;i++)h=S(h+s.salt+p);
return pow(s.nonce,s.pow||0).then(function(w){return P("/api/login",{u:u,nonce:s.nonce,mac:hm(h,s.nonce),pw:w,hp:""})})});
q.then(function(j){
if(j.ok)return done();
if(j.mfa){tok=j.tok;$("m").hidden=false;$("k").focus();say("");return}
say(j.err||L[10])}).catch(function(){say(L[10])})};
$("v").onsubmit=function(e){
e.preventDefault();
var f=[].slice.call($("r").files).sort(function(a,b){return a.name<b.name?-1:1}),m=L[12];
say(L[11]);
(function n(i){
if(i==f.length){say(L[9]);return setTimeout(function(){location.href="/"},1500)}
var d=new FormData;
d.append(m?"file":"f",f[i]);
fetch(m?"/api/fs/up?dir=/lang":"/api/risorse/carica",{method:"POST",body:d}).then(function(r){return r.json()}).then(function(j){
if(j.ok)n(i+1);else say(j.err||L[10])}).catch(function(){say(L[10])})})(0)};
$("z").onclick=function(){
say(L[11]);
P("/api/lang",{code:"it"}).then(function(j){
if(j.ok)location.href="/";else say(j.err||L[10])}).catch(function(){say(L[10])})};
if(L[12]){$("r").accept=".vld,.vlp";$("r").multiple=true}
[[0,"a"],[3,"c"],[4,"d"],[5,"g"],[6,"e"],[7,"i"],[8,"j"],[12,"z"]].forEach(function(p){$(p[1]).textContent=L[p[0]]});
fetch("/api/auth").then(function(r){return r.json()}).then(function(j){
if(!j.set){$("b").textContent=L[2];return}
$("b").textContent=L[1];
if(j.role==2)done();else $("l").hidden=false}).catch(function(){say(L[10])});
</script>
)JS";

// Un testo come stringa JSON dentro l'array L (con la virgola davanti, tranne il primo).
void aggiungiTesto(HeapBuf& p, const char* testo, bool virgola = true) {
  if (virgola) p.add(',');
  p.add('"');
  p.addJson(testo, strlen(testo));
  p.add('"');
}

}  // namespace

// La pagina di emergenza, servita da "/" quando /usr/share/vesevos/html.vrs manca o non e valido, oppure (lingua = true) quando la
// lingua in uso e in errore (vos_web_pagina.cpp): stessa pagina, con l'avviso, il caricamento dei file .vld/.vlp e "Usa l'italiano". Aggiunge in mezzo i testi tradotti
// (array JSON L). Circa 5 KB in tutto, in RAM solo per la risposta.
esp_err_t sendEmergenza(Res* s, bool lingua) {
  HeapBuf p(8192);
  p.add("<!doctype html><html lang=\"");        // lingua della pagina per i lettori di schermo
  if (lingua) p.add("it");                       // con la lingua in errore i testi sono in italiano
  else p.addJson(cfg.lang.c_str(), cfg.lang.length());
  p.add("\">");
  p.add(kTesta);
  p.add("[");
  char code[LANG_CODE_BUF] = "";
  int motivo = 0;
  if (lingua) langErrore(code, sizeof code, motivo);
  aggiungiTesto(p, lingua ? tr("Lingua da ripristinare") : tr("Pagina da ripristinare"), false);                    // 0
  aggiungiTesto(p, lingua ? trf("Il file della lingua %s non e valido: ricaricalo oppure usa l'italiano", code).c_str()
                          : tr("I file della pagina mancano o non sono validi. Accedi come amministratore e carica il file .vrs."));   // 1
  aggiungiTesto(p, tr("Questa scheda non ha ancora un amministratore. Installala dal sito, da microSD o dalla seriale."));   // 2
  aggiungiTesto(p, tr("Nome utente"));                                                                              // 3
  aggiungiTesto(p, tr("Password utente"));                                                                                 // 4
  aggiungiTesto(p, tr("Accedi"));                                                                                   // 5
  aggiungiTesto(p, tr("Codice a due fattori"));                                                                     // 6
  aggiungiTesto(p, lingua ? tr("File .vld e .vlp") : tr("File .vrs"));                                              // 7
  aggiungiTesto(p, lingua ? tr("Carica i file") : tr("Carica il file"));                                                    // 8
  aggiungiTesto(p, tr("Fatto. Torno alla pagina..."));                                                              // 9
  aggiungiTesto(p, tr("Errore"));                                                                                   // 10
  aggiungiTesto(p, tr("Attendere..."));                                                                             // 11
  aggiungiTesto(p, lingua ? tr("Usa l'italiano") : "");                                                           // 12: pieno = modo lingua
  p.add("]");
  p.add(kCoda);
  if (p.oom() || p.over()) return ko(s, tr("Memoria insufficiente"), "off");
  return sendText(s, 200, "text/html; charset=utf-8", p.c_str(), p.length());
}

}  // namespace webx
