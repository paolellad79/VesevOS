/* ===== prima configurazione guidata ===== */
var SU={i:0,d:{},m:null,ap:null};
var SUSTEPS=["Lingua","Paese","Nome","Antenna","Wi-Fi di casa o hotspot","Ora","Fine"];
function suSteps(){return SU.d&&SU.d.ssid?SUSTEPS.filter(function(x){return x!="Ora"}):SUSTEPS}
function setupOpen(){Promise.all([commonLoad(),api("/api/region").then(function(r){return r.json()}),api("/api/settings").then(function(r){return r.json()})]).then(function(a){var r=a[1],s=a[2];
 SU.d={cc:r.country,tz:r.tz,tzn:r.tzName,ntp:r.ntp,f:{d:r.dateFmt,h:r.timeFmt,s:r.decSep,w:r.weekStart,t:r.tempUnit},ant:r.antExt,gain:r.gain,host:s.hostname,host0:s.hostname,ssid:"",pass:""};
 SU.i=0;$("setup").className="setup";setupDraw()}).catch(function(){})}
function suAlready(){dlg({title:t("La scheda e gia configurata?"),body:'<p style="margin:0">'+esc(t("La guida non comparira piu a ogni avvio. Le impostazioni restano come sono."))+'</p>',ok:t("Si, e configurata"),
 onOk:function(){return post("/api/setup/done",{}).then(function(r){return r.json()}).then(function(j){if(!j.ok)return j.err||t("Errore");AUTH.setup=true;setupClose();show("sum");return""})}})}
function setupClose(){var e=$("setup");if(e)e.className="setup hide"}
function suDots(){var h='<div class="sdots" aria-hidden="true">';suSteps().forEach(function(x,i){h+='<i class="'+(i<=SU.i?"on":"")+'"></i>'});return h+'</div>'}
function setupDraw(){var i=SU.i,d=SU.d,h='<div class="sw"><div class="hero"><svg class="logo" aria-hidden="true"><use href="#i-chip"/></svg></div><h2 style="margin:0">'+esc(t("Prima configurazione"))+'</h2><div class="subt">'+esc(tf("Passo {0} di {1}: {2}",i+1,suSteps().length,t(suSteps()[i])))+'</div>'+suDots()+'<div class="card">';
 if(i==0){h+='<h3>'+esc(t("Scegli la lingua"))+'</h3><div class="lgrid">';LANGS.forEach(function(l){h+='<button type="button" class="'+(l.code==LANG?"on":"")+'" onclick="setLang(\''+esc(l.code)+'\');setTimeout(setupDraw,400)">'+flagImg(l)+'<span>'+esc(l.name)+'</span></button>'});h+='</div><p class="fhint">'+esc(t("Altre lingue si aggiungono dopo, con un file (Sistema > Localizzazione)."))+'</p>'}
 else if(i==1){h+='<h3>'+esc(t("Dove si trova la scheda?"))+'</h3><div class="agrid"><div><input id="su_q" type="search" placeholder="'+esc(t("Cerca (es. Italia, IT)"))+'" oninput="suFind()"></div><div>'+cSelHtml("su_c",d.cc,"suSel(this.value)")+'</div></div><div class="law">'+esc(t("Il paese decide i canali e la potenza della radio secondo le sue leggi: scegli quello dove la scheda si trova davvero. Non e consulenza legale."))+'</div>'}
 else if(i==2){h+='<h3>'+esc(t("Come si chiama questa scheda?"))+'</h3><label for="su_h">'+esc(t("Nome host"))+'</label><input id="su_h" maxlength="32" value="'+esc(d.host)+'"><p class="fhint">'+esc(t("Lettere, numeri e trattino. La ritrovi come NOME.local sulla rete di casa."))+'</p>'}
 else if(i==3){h+='<h3>'+esc(t("Antenna"))+'</h3><select id="su_a" onchange="$(\'su_gw\').className=this.value==1?\'\':\'hide\'"><option value="0"'+(d.ant?"":" selected")+'>'+esc(t("Interna (sul circuito)"))+'</option><option value="1"'+(d.ant?" selected":"")+'>'+esc(t("Esterna"))+'</option></select>'+
  '<div id="su_gw" class="'+(d.ant?"":"hide")+'"><label for="su_g">'+esc(t("Guadagno dell'antenna esterna (dBi)"))+'</label><input type="number" id="su_g" min="0" max="15" value="'+d.gain+'"></div><p class="fhint">'+esc(t("Se non sai, lascia Interna. Con un'antenna esterna la potenza scende da sola per restare nel limite del paese."))+'</p>'}
 else if(i==4){h+='<h3>'+esc(t("Wi-Fi di casa (facoltativa)"))+'</h3><p class="fhint">'+esc(t("Scegli la rete e scrivi la password, poi premi Avanti: la scheda prova subito a collegarsi e ti dice se ha funzionato. Se la colleghi, prende l'ora da Internet e la raggiungi dalla tua rete. L'hotspot resta acceso."))+'</p>'+
  '<button class="btn gray" type="button" onclick="suScan()">'+esc(t("Cerca reti"))+'</button><div id="su_sl"></div><label for="su_ss">'+esc(t("Rete (SSID)"))+'</label><input id="su_ss" value="'+esc(d.ssid)+'"><label for="su_pw">'+esc(t("Password Wi-Fi"))+'</label><input type="password" id="su_pw" value="'+esc(d.pass)+'" autocomplete="new-password"><div id="su_wt" class="msg" role="status"></div><h3 style="margin-top:18px">'+esc(t("Salva la password dell'hotspot"))+'</h3><div id="su_ap">'+esc(t("Carico..."))+'</div><div class="law">'+esc(t(APLAW))+'</div><p class="fhint">'+esc(t("Il QR e la stampa sono in Rete > Punto di accesso."))+'</p>'}
 else if(i==5&&!d.ssid){var tzl=(COMMON.by[d.cc]||{tz:[]}).tz.slice(),to="",nw=new Date(),pz=function(n){return("0"+n).slice(-2)};if(!tzl.some(function(z){return z[1]==d.tz}))tzl.push([d.tzn,d.tz]);tzl.forEach(function(z,k){to+='<option value="'+k+'"'+(z[1]==d.tz?" selected":"")+'>'+esc(z[0])+'</option>'});SU.tzl=tzl;
  h+='<h3>'+esc(t("Imposta l'ora"))+'</h3><p class="fhint">'+esc(t("Senza Internet la scheda non sa che ora e. Controlla data, ora e fuso orario: sono quelli di questo dispositivo."))+'</p><label for="su_z">'+esc(t("Fuso orario"))+'</label><select id="su_z" onchange="suTz(this.value)">'+to+'</select><div class="agrid"><div><label for="su_d">'+esc(t("Data"))+'</label><input type="date" id="su_d" value="'+nw.getFullYear()+"-"+pz(nw.getMonth()+1)+"-"+pz(nw.getDate())+'" oninput="suTm()"></div><div><label for="su_t">'+esc(t("Ora"))+'</label><input type="time" id="su_t" step="1" value="'+pz(nw.getHours())+":"+pz(nw.getMinutes())+":"+pz(nw.getSeconds())+'" oninput="suTm()"></div></div>'}
 else{var c2=COMMON.by[d.cc];h+='<h3>'+esc(t("Tutto pronto"))+'</h3>'+row(esc(t("Paese")),esc(c2?cName(c2):"-"))+row(esc(t("Nome")),esc(d.host))+row(esc(t("Fuso orario")),esc(d.tzn)+" ("+esc(t("si cambia in Sistema > Localizzazione"))+")")+row(esc(t("Antenna")),esc(d.ant?tf("esterna, {0} dBi",d.gain):t("interna")))+row(esc(t("Rete di casa")),esc(d.ssid||t("nessuna (solo hotspot)")))+
  '<div class="law">'+esc(t("MQTT, rete tra schede, Bluetooth, server dell'ora e filtro IP restano spenti: li accendi tu quando servono (categoria Servizi e Sicurezza)."))+'</div>'+
  (d.ssid?'<div class="law">'+esc(tf("Dopo il salvataggio la scheda si collega a {0}: collega il telefono o il PC a quella rete e apri l'indirizzo che ti mostro. Al primo accesso da quella rete hotspot, portale automatico e HTTP si spengono e restano spenti: li riaccendi tu quando vuoi (Servizi). Se il collegamento non riesce, l'hotspot torna da solo.",d.ssid))+'</div>':'<div class="law">'+esc(t("Senza Wi-Fi di casa l'hotspot e HTTP restano accesi: sono l'unico modo di entrare. La scheda non ha batteria per l'orologio: se la spegni l'ora va rimessa (Sistema > Ora)."))+'</div>')}
 h+='<div id="sumsg" class="msg"></div><div class="sbt">'+(i?'<button class="btn gray" type="button" onclick="suGo(-1)">'+esc(t("Indietro"))+'</button>':"")+'<button class="btn gray" type="button" onclick="setupClose()">'+esc(t("Piu tardi"))+'</button><button class="btn gray" type="button" onclick="suAlready()">'+esc(t("La scheda e gia configurata"))+'</button><span class="sp"></span>'+(i==4?'<button class="btn gray" type="button" onclick="SU.d.ssid=\'\';SU.d.pass=\'\';SU.d.wok=false;SU.i=5;setupDraw()">'+esc(t("Salta"))+'</button>':"")+'<button class="btn" type="button" onclick="suGo(1)">'+esc(i==suSteps().length-1?t("Salva e finisci"):t("Avanti"))+'</button></div></div></div>';
 $("setup").innerHTML=h;
 if(i==4)api("/api/ap").then(function(r){return r.json()}).then(function(j){$("su_ap").innerHTML='<div class="subt">'+esc(t("Rete Wi-Fi della scheda"))+'</div><div class="big">'+esc(j.ssid)+'</div><div class="subt" style="margin-top:6px">'+esc(t("Password"))+'</div><div class="pw">'+esc(j.pass)+'</div>'}).catch(function(){$("su_ap").textContent=""});
 var f=$("setup").querySelector("input,select");if(f&&i)f.focus()}
function suTz(v){var z=SU.tzl[+v];if(z){SU.d.tz=z[1];SU.d.tzn=z[0]}}
function suTm(){SU.d.tm=[$("su_d").value,$("su_t").value]}
function suFind(){var c=cSearch($("su_q").value);if(c)suSel(c)}
function suSel(cc){if(!cc)return;var c=COMMON.by[cc],d=SU.d;d.cc=cc;$("su_c").value=cc;
 d.tz=c.tz[0][1];d.tzn=c.tz[0][0];d.f={d:c.f.d,h:c.f.h,s:c.f.s,w:c.f.w,t:c.f.t};d.ntp=c.ntp||d.ntp}
function suScan(){$("su_sl").textContent=t("Ricerca in corso...");api("/api/wifi/scan",{method:"POST"}).then(function(){var go=function(){api("/api/wifi/scan").then(function(r){return r.json()}).then(function(j){if(j.running){setTimeout(go,1500);return}
 var h="";j.list.slice(0,10).forEach(function(n){h+='<button type="button" class="btn gray sm" onclick="$(\'su_ss\').value=this.textContent;$(\'su_pw\').focus()">'+esc(n.ssid)+'</button>'});$("su_sl").innerHTML=h||esc(t("Nessuna rete trovata"))})};setTimeout(go,3000)})}
function suWifiTest(){var d=SU.d,go=$("setup").querySelector(".sbt .btn:not(.gray)");if(go)go.disabled=true;msg("su_wt",tf("Mi collego a {0} (fino a 20 secondi)...",d.ssid),true);
 post("/api/wifi/test",{ssid:d.ssid,pass:d.pass}).then(function(r){return r.json()}).then(function(j){if(j.ok===false)throw j.err;var n=0;
  var poll=function(){setTimeout(function(){api("/api/wifi/test").then(function(r){return r.json()}).then(function(j){
   if(j.state==1)return poll();
   if(go)go.disabled=false;
   if(j.state==2){d.wok=true;d.wss=d.ssid;d.wpw=d.pass;msg("su_wt",tf("Collegata a {0}. Indirizzo della scheda su quella rete: {1}",d.ssid,j.ip),true);setTimeout(function(){if(SU.i==4){SU.i=5;setupDraw()}},1500)}
   else{d.wok=false;msg("su_wt",j.err||t("Collegamento non riuscito: controlla la password"),false)}
  }).catch(function(){if(++n<30)poll();else{if(go)go.disabled=false;msg("su_wt",t("La scheda non risponde: riprova tra poco"),false)}})},1500)};poll()
 }).catch(function(e){if(go)go.disabled=false;msg("su_wt",typeof e=="string"?e:t("Errore"),false)})}
function suGo(k){var d=SU.d,i=SU.i;
 if(k>0){if(i==1&&!d.cc){msg("sumsg",t("Scegli il paese"),false);return}
  if(i==2){var hn=$("su_h").value.trim().toLowerCase();if(!/^[a-z0-9]([a-z0-9-]{0,30}[a-z0-9])?$/.test(hn)){msg("sumsg",t("Nome host non valido (1-32 caratteri: lettere, numeri, trattino; non all'inizio o alla fine)"),false);return}d.host=hn}
  if(i==3){d.ant=+$("su_a").value;d.gain=d.ant?(+$("su_g").value||0):0}
  if(i==4){d.ssid=$("su_ss").value.trim();d.pass=$("su_pw").value;if(d.ssid&&d.pass&&(d.pass.length<8||d.pass.length>63)){msg("su_wt",t("Password Wi-Fi: da 8 a 63 caratteri"),false);return}
   if(d.ssid&&!(d.wok&&d.wss==d.ssid&&d.wpw==d.pass)){suWifiTest();return}}
  if(i==5&&!d.ssid&&d.tm&&(!d.tm[0]||!d.tm[1])){msg("sumsg",t("Scegli data e ora"),false);return}
  if(i==suSteps().length-1){suFinish();return}}
 SU.i=Math.max(0,Math.min(suSteps().length-1,i+k));setupDraw()}
function suFinish(){var d=SU.d;msg("sumsg",t("Salvo..."),true);
 post("/api/region",{country:d.cc,tz:d.tz,tzname:d.tzn,ntp:d.ntp,datefmt:d.f.d,timefmt:d.f.h,decsep:d.f.s,weekstart:d.f.w,tempunit:d.f.t,antenna:d.ant,gain:d.gain}).then(function(r){return r.json()}).then(function(j){if(j.ok===false)throw j.err;
  return !d.ssid?post("/api/time/set",d.tm?{local:d.tm[0]+" "+(d.tm[1].length==5?d.tm[1]+":00":d.tm[1])}:{epoch:Math.floor(Date.now()/1000)}).then(function(r){return r.json()}):{ok:true}}).then(function(j){if(j.ok===false)throw j.err;
  return post("/api/setup/done",{ntp:d.ssid?"1":"0"})}).then(function(r){return r.json()}).then(function(j){if(!j.ok)throw j.err;AUTH.setup=true;
  return d.host!=d.host0?post("/api/system",{hostname:d.host,domain:""}).then(function(r){return r.json()}):{ok:true}}).then(function(j){if(!j.ok)throw j.err;
  var u="https://"+d.host+".local"+(SV.httpsOn!==false&&SV.httpsPort&&SV.httpsPort!=443?":"+SV.httpsPort:"");
  if(d.ssid){var ws=function(n){post("/api/wifi/save",{ssid:d.ssid,pass:d.pass,dhcp:1,cleanup:1}).catch(function(){if(n)setTimeout(function(){ws(n-1)},4000)})};setTimeout(function(){ws(1)},d.host!=d.host0?1500:0)}
  setupClose();show("sum");dlg({title:t("Configurazione completata"),body:d.ssid?'<p style="margin:0 0 8px">'+esc(tf("La scheda si sta collegando a {0}. Collega il telefono o il PC a quella rete e apri questo indirizzo:",d.ssid))+'</p><div class="lab"><div class="qrbox" style="max-width:160px">'+qrSvg(u)+'</div><div><div class="big">'+esc(u)+'</div></div></div><p class="fhint">'+esc(t("Se non riesce a collegarsi, l'hotspot torna da solo: riconnettiti e controlla la password della Wi-Fi in Rete."))+'</p>':'<p style="margin:0">'+esc(t("La scheda e pronta. Puoi collegarla alla rete di casa quando vuoi da Rete > Wi-Fi."))+'</p>',buttons:[["ok",t("OK"),""]],onOk:function(){return""}})})
 .catch(function(e){msg("sumsg",typeof e=="string"?e:t("Errore"),false)})}

/* ===== Home: posizione ===== */
function homeLoc(){var e=$("hlocn");if(!e)return;Promise.all([commonLoad(),api("/api/region").then(function(r){return r.json()})]).then(function(a){var r=a[1],c=COMMON.by[r.country];
 $("hlocn").textContent=c?cName(c)+" · "+r.tzName:t("Paese non scelto");}).catch(function(){})}

function lHelp(){dlg({title:t("Aiuto"),body:$("t_help").querySelector(".hlp").innerHTML,buttons:[["ok",t("OK"),""]],onOk:function(){return""}})}
authLoad().then(function(j){initLang(j.lang);if(j.user!==undefined){USER=j.user;start()}else showLogin()}).catch(function(){initLang("it");showLogin()});
themeApply();
