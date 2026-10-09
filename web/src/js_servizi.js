/* ===== Servizi di rete: hotspot, HTTP, HTTPS, porte ===== */
var SV={};
function svcLoad(){return api("/api/svc").then(function(r){return r.json()}).then(svcDraw).catch(function(){})}
function svcDraw(j){if(!j||j.apOn===undefined)return;SV=j;
 svcSt("ap",j.apOn);$("sv_cap").classList.toggle("on",j.captive);$("apdep").className=j.apOn?"":"dim";
 var dp=document.querySelectorAll(".dpap");for(var i=0;i<dp.length;i++)dp[i].classList.toggle("dim",!j.apOn);
 $("sv_apn").textContent=j.apOn?(j.httpOn&&j.httpPort==80?"":t("Il portale automatico serve HTTP acceso sulla porta 80.")):t("Punto di accesso spento: se la Wi-Fi di casa non e raggiungibile la scheda non si vede. Recupero: tasto BOOT 8 secondi o cavo USB.");
 if(j.dhcpOn!==undefined){$("sv_lease").value=j.lease;svcSt("dhcp",j.dhcpOn);svcSt("mdns",j.mdnsOn);svcSt("dns",j.captive)}
 $("sv_hp").value=j.httpPort;$("sv_sp").value=j.httpsPort;
 var pd=!!j.pending;svcSt("http",j.httpOn,pd,!!j.runHttp);svcSt("https",j.httpsOn,pd||!!j.httpsOn!=!!AUTH.https,!!AUTH.https);
 $("sv_hw").className=j.httpOn&&j.clientOk?"law":"hide";$("sv_hw").textContent=j.httpOn&&j.clientOk?t("Sei collegato alla tua Wi-Fi: puoi spegnere HTTP e usare solo HTTPS (consigliato)."):"";
 var nh=!j.httpsOn;$("sv_sw").className=nh?"law":"hide";$("sv_sw").textContent=nh?t("Senza HTTPS il traffico non e cifrato: il login non manda la password (HMAC), ma sessione e dati si leggono sulla rete. Scelta tua: usa una rete di cui ti fidi."):"";
 var pe=j.pending?t("Le modifiche valgono dopo Applica (riavvia la scheda)."):"";
 ["sv_hpend","sv_spend"].forEach(function(id){$(id).className=pe?"law":"hide";$(id).textContent=pe})}
function svcPost(d,mid){return post("/api/svc",d).then(function(r){return r.json()}).then(function(j){if(j.ok===false){msg(mid,j.err,false);svcLoad();return false}svcDraw(j);msg(mid,t("Salvato"),true);return true}).catch(function(){})}
function svcTog(k,e,mid){var d={};d[k]=e.classList.contains("on")?"0":"1";svcPost(d,mid||(k=="apOn"||k=="captive"?"svm_ap":"svm_http"))}
function svcSet(k,mid){return function(v){var d={};d[k]=v?"1":"0";return svcPost(d,mid)}}
/* ===== Servizi: UN componente per tutti (banner Avviato/Fermo + icone) ===== */
var SVT={ap:{set:svcSet("apOn","svm_ap")},dhcp:{set:svcSet("dhcpOn","svm_dhcp")},dns:{set:svcSet("captive","svm_dns")},mdns:{set:svcSet("mdnsOn","svm_mdns")},
 http:{set:svcSet("httpOn","svm_http"),ap:1},https:{set:function(v){return tlsSet(v)},ap:1},
 mqtt:{set:function(v){return mqRun(v?"start":"stop")},rs:function(){return mqRun("restart")},cls:"op"},
 mesh:{set:function(v){return msRun(v?"start":"stop")},rs:function(){return msRun("restart")}},
 stat:{set:function(v){return stSet(v)}},
 ntp:{set:function(v){return saveTime(v)}},
 ble:{set:function(v){return bleRun(v?"start":"stop")},rs:function(){return bleRun("stop").then(function(){return bleRun("start")})}},
 power:{set:function(v){return pwSave(v)}},
 mfa:{set:function(v){return v?mfBegin():mfOff(-1)},cls:""}},SVS={};
function svcSt(id,on,pend,run){SVS[id]={on:!!on,pend:!!pend,run:run===undefined?!!on:!!run,busy:""};svcCtl(id)}
function sibB(cls,ic,lab,act,dis,oc){return'<button type="button" class="sib '+cls+(act?" act":"")+'"'+(oc?' onclick="'+oc+'"':"")+(dis?" disabled":"")+' aria-label="'+esc(lab)+'" title="'+esc(lab)+'" aria-pressed="'+(act?"true":"false")+'"><svg viewBox="0 0 24 24" aria-hidden="true"><use href="#'+ic+'"/></svg></button>'}
function svcCtl(id){var e=$("sc_"+id),d=SVT[id],s=SVS[id];if(!e||!d||!s)return;var b=s.busy,
 B=function(k,cls,ic,lab,act,dis){return sibB(cls,ic,lab,act,dis,"svcAct('"+id+"','"+k+"')")};
 e.innerHTML='<div class="svb '+(s.run?"ok":"ko")+'" role="status">'+esc(s.run?t("Avviato"):t("Fermo"))+'</div><div class="svi '+(d.cls||"adm")+'" role="group" aria-label="'+esc(t("Controlli del servizio"))+'">'+
  B("go","go","i-sgo",t("Avvia"),b?b=="go":s.on)+B("st","st","i-sst",t("Ferma"),b?b=="st":!s.on)+B("rs","rs","i-srs",t("Riavvia"),b=="rs",!d.rs||!s.on)+(d.ap?B("ap","ap","i-sap",t("Applica"),b=="ap",!s.pend):"")+'</div>'}
function svcAct(id,k){var d=SVT[id],s=SVS[id];if(!d||!s||s.busy)return;if(k=="go"&&s.on||k=="st"&&!s.on)return;s.busy=k;svcCtl(id);var p=k=="rs"?d.rs():k=="ap"?uReboot():d.set(k=="go");
 Promise.resolve(p).catch(function(){}).then(function(){setTimeout(function(){var x=SVS[id];if(x&&x.busy){x.busy="";svcCtl(id)}},k=="ap"?300:1400)})}
function svcPort(k,id,mid){var d={};d[k]=$(id).value;svcPost(d,mid)}


/* ===== Risparmio energia ===== */
var PW=null;
function pwLoad(){api("/api/power").then(function(r){return r.json()}).then(pwDraw).catch(function(){})}
function pwDraw(j){PW=j;svcSt("power",j.mode!=0);var x=document.activeElement;if((!x||x.id!="pw_mode")&&j.mode!=0){$("pw_mode").value=j.mode}if(!x||x.id!="pw_aw")$("pw_aw").value=j.awake;if(!x||x.id!="pw_sl")$("pw_sl").value=j.sleep;pwUi();
 var h=j.mode==0?t("Risparmio energia spento"):j.mode==1?t("Wi-Fi a risparmio massimo"):(j.guide?tf("Sonno a cicli: dormira tra {0}",durText(j.in)):t("Sonno a cicli: non dorme finche la guida di configurazione non e finita"));
 $("pwst").className="msg";$("pwst").style.display="block";$("pwst").textContent=h+" · "+tf("risvegli: {0}, dormito circa {1}",j.wakes,durText(j.slept))}
function pwUi(){var m=$("pw_mode").value;$("pw_c").className=m==2?"agrid":"agrid hide";$("pw_w1").className=m==1?"law":"law hide";$("pw_w2").className=m==2?"law":"law hide"}
function pwSave(on){if(on===undefined)on=!!(SVS.power&&SVS.power.on);return post("/api/power",{mode:on?$("pw_mode").value:0,awake:$("pw_aw").value,sleep:$("pw_sl").value}).then(function(r){return r.json()}).then(function(j){if(j.ok===false){msg("pwm",j.err,false);return}pwDraw(j);msg("pwm",t("Salvato"),true)}).catch(function(){})}
function pwNow(){var m=+$("pw_now").value;if(!(m>=1&&m<=10080)){msg("pwm2",t("Tempo di sonno: da 1 minuto a 7 giorni"),false);return}
 dlg({title:t("Dormire adesso?"),body:'<p style="margin:0">'+esc(t("La pagina non si raggiunge piu fino al risveglio."))+'</p><p style="margin:8px 0 0">'+esc(tf("La scheda riparte da sola tra {0} (o subito con il tasto RESET).",durText(m*60)))+'</p>',ok:t("Dormi"),danger:true,
  onOk:function(){return post("/api/sleep",{min:m}).then(function(r){return r.json()}).then(function(j){return j.ok===false?j.err:""})}})}

/* ===== Autotest ===== */
var SL={tm:null,seq:0,run:false};
function stlGo(){$("stl_go").disabled=true;post("/api/selftest",{active:$("stl_act").classList.contains("on")?1:0,names:$("stl_nm").classList.contains("on")?1:0}).then(function(r){return r.json()}).then(function(j){if(j.ok===false){msg("stlm",j.err,false);$("stl_go").disabled=false;return}$("stlm").className="msg";SL.run=true;stlPoll()}).catch(function(){$("stl_go").disabled=false})}
function stlPoll(){clearTimeout(SL.tm);var id=++SL.seq;api("/api/selftest").then(function(r){return r.json()}).then(function(j){if(id!=SL.seq)return;stlDraw(j)}).catch(function(){if(id!=SL.seq||cur!="selftest")return;if(SL.run){$("stl_sum").textContent=t("La scheda non risponde, riprovo...");SL.tm=setTimeout(stlPoll,1500)}})}
var STLW=["Saltata","OK","Avviso","Errore","Saltata"];var STLC=["","a","w","k",""];
function stlDraw(j){if(cur!="selftest")return;var run=j.state==1,fin=j.state==2,c=[0,0,0,0,0];SL.run=run;
 $("stl_go").disabled=run;$("stl_dl").className=fin?"btn gray":"btn gray hide";$("stl_cp").className=fin?"btn gray":"btn gray hide";$("stl_cl").className=j.state?"btn gray":"btn gray hide";
 $("stl_bar").className=run?"":"hide";if(run)$("stl_bar").innerHTML=bar(Math.round(100*j.list.length/j.total));
 var h="";j.list.forEach(function(x){c[x.s]++;h+='<div class="row" style="align-items:flex-start;gap:10px"><span class="pill '+STLC[x.s]+'" style="min-width:64px;text-align:center">'+esc(t(STLW[x.s]))+'</span><span style="flex:1"><b>'+esc(x.n)+'</b> <span class="fhint">('+x.ms+' ms)</span><br><span>'+esc(x.d)+'</span></span></div>'});
 $("stl_list").innerHTML=h;
 $("stl_sum").textContent=run?tf("In corso: prova {0} di {1}",j.cur+1,j.total):fin?tf("Finita: {0} OK, {1} avvisi, {2} errori, {3} saltate",c[1],c[2],c[3],c[4]):"";
 if(run)SL.tm=setTimeout(stlPoll,700)}
function stlCopy(){api("/api/selftest/report").then(function(r){return r.text()}).then(function(x){var ok=function(){msg("stlm",t("Report copiato"),true)};
 if(navigator.clipboard&&navigator.clipboard.writeText)navigator.clipboard.writeText(x).then(ok).catch(function(){});
 else{var a=document.createElement("textarea");a.value=x;document.body.appendChild(a);a.select();try{document.execCommand("copy");ok()}catch(e){}document.body.removeChild(a)}})}
function stlClear(){post("/api/selftest",{clear:1}).then(function(){stlPoll()})}
/* ===== Statistiche d'uso ===== */
var STT=0;
function stLoad(){api("/api/stats").then(function(r){return r.json()}).then(stDraw).catch(function(){})}
function stDraw(j){svcSt("stat",j.on);$("st_csv").className=j.on?"btn gray":"btn gray hide";$("st_rst").className=j.on?"btn gray adm":"btn gray adm hide";
 if(!j.on){$("stbody").innerHTML='<p class="fhint">'+esc(t("Statistiche spente"))+'</p>';return}
 var h=row(esc(t("Conteggio da")),esc(durText(j.sec)))+
  row(esc(t("Wi-Fi: collegamenti / cadute")),j.wifiUp+" / "+j.wifiDown)+row(esc(t("Wi-Fi: tempo collegato")),esc(durText(j.wifiSec)))+row(esc(t("Segnale medio / minimo")),j.rssiAvg+" / "+j.rssiMin+" dBm")+
  row(esc(t("Hotspot: telefoni insieme (massimo)")),j.apMax)+row(esc(t("Bluetooth: accensioni / collegamenti")),j.bleStart+" / "+j.bleConn)+
  row(esc(t("Tasto BOOT: brevi / 2 s / 8 s / 20 s")),j.boot.join(" / "))+(j.bootEver?row(esc(t("Ultima pressione di BOOT")),esc(tf("{0} fa",durText(j.bootLast)))):"")+
  row(esc(t("LED: stato / battito / fisso / spento")),j.led.map(durText).join(" / "));
 $("stbody").innerHTML=h}
function stSet(on){return post("/api/stats",{on:on?1:0}).then(function(r){return r.json()}).then(function(j){if(j.ok===false){msg("stm",j.err,false);return}stDraw(j);msg("stm",t("Salvato"),true)}).catch(function(){})}
function stReset(){post("/api/stats",{reset:1}).then(function(r){return r.json()}).then(function(j){stDraw(j);msg("stm",t("Azzerate"),true)}).catch(function(){})}
/* ===== MFA ===== */
var MF=null;
function mfLoad(){api("/api/mfa").then(function(r){return r.json()}).then(mfDraw).catch(function(){})}
function mfDraw(j){MF=j;var me=null;j.list.forEach(function(x){if(x.name==USER)me=x});
 var on=me&&me.on;$("mfst").className="msg";$("mfst").style.display="block";$("mfst").textContent=on?tf("MFA acceso. Codici di recupero rimasti: {0}",me.rec):t("MFA spento");
 svcSt("mfa",on);$("mf_rec").className=on?"btn gray":"hide";
 $("mftime").textContent=j.time?t("Ora della scheda valida."):t("Ora della scheda NON valida: impostala in Sistema > Ora prima di attivare l'MFA.");
 $("mfadm").className=ROLE==2?"card":"card hide";
 if(ROLE==2){$("mf_nt").value=j.nt;$("mf_pow").value=j.pow;var h='<thead><tr><th>'+esc(t("Nome"))+'</th><th>MFA</th><th></th></tr></thead><tbody>';
  j.list.forEach(function(x){h+='<tr><td>'+esc(x.name)+'</td><td>'+esc(x.on?t("acceso"):t("spento"))+'</td><td>'+(x.on?'<button class="btn gray sm" onclick="mfOff('+x.i+')">'+esc(t("Spegni"))+'</button>':"")+'</td></tr>'});$("mftab").innerHTML=h+'</tbody>'}}
function mfBegin(){return post("/api/mfa/begin",{}).then(function(r){return r.json()}).then(function(j){if(j.ok===false){msg("mfm",j.err,false);return}
 $("mfqr").innerHTML=qrSvg(j.uri);$("mfkey").textContent=j.key;$("mfact").className="";$("mf_code").value="";$("mfrec").className="hide";$("mf_code").focus()}).catch(function(){})}
function mfConfirm(){post("/api/mfa/confirm",{code:$("mf_code").value.trim()}).then(function(r){return r.json()}).then(function(j){if(j.ok===false){msg("mfm",j.err,false);return}
 $("mfact").className="hide";mfShowRec(j.rec);msg("mfm",t("MFA acceso"),true);mfLoad()}).catch(function(){})}
function mfShowRec(r){$("mfqr").innerHTML="";$("mfkey").textContent="";$("mfrecl").textContent=r.split(" ").join("   ");$("mfrec").className="law"}
function mfRec(){dlg({title:t("Nuovi codici di recupero?"),body:'<p style="margin:0">'+esc(t("I codici vecchi smettono di valere."))+'</p>',ok:t("Crea"),onOk:function(){return post("/api/mfa/recovery",{}).then(function(r){return r.json()}).then(function(j){if(j.ok===false)return j.err;mfShowRec(j.rec);mfLoad();return""})}})}
function mfOff(i){dlg({title:t("Spegnere l'MFA?"),body:'<p style="margin:0">'+esc(t("L'accesso torna a chiedere solo la password."))+'</p>',ok:t("Spegni"),danger:true,onOk:function(){return post("/api/mfa/off",i<0?{}:{i:i}).then(function(r){return r.json()}).then(function(j){if(j.ok===false)return j.err;mfLoad();return""})}})}
function mfSet(){post("/api/mfa/set",{nt:$("mf_nt").value,pow:$("mf_pow").value}).then(function(r){return r.json()}).then(function(j){if(j.ok===false){msg("mfm2",j.err,false);mfLoad();return}mfDraw(j);msg("mfm2",t("Salvato"),true)}).catch(function(){})}
function mfCopy(){var x=$("mfrecl").textContent;try{navigator.clipboard.writeText(x);msg("mfm",t("Copiato"),true)}catch(e){msg("mfm",t("Non riesco a copiare: scrivili a mano"),false)}}
/* ===== Controlli della configurazione (allarmi) ===== */
var AU={al:[],tm:null};
function auPoll(){clearInterval(AU.tm);var go=function(){if(!loggedIn()||document.hidden)return;api("/api/audit").then(function(r){return r.json()}).then(function(a){AU.al=a.alarms;AU.hist=a.history;if(S)alertsCalc(S);if(cur=="audit")auDraw()}).catch(function(){})};go();AU.tm=setInterval(go,15000)}
function auLoad(){api("/api/audit").then(function(r){return r.json()}).then(function(a){AU.al=a.alarms;AU.hist=a.history;auDraw()}).catch(function(){})}
function auDraw(){var h="";if(!AU.al.length)h='<p style="color:var(--okt)">'+esc(t("Nessun allarme: configurazione in regola"))+'</p>';
 AU.al.forEach(function(a){h+='<div class="alv"><span class="dot" style="background:'+(a.lv==2?"var(--ko)":"var(--wa)")+'"></span><div style="flex:1">'+esc(a.text)+(a.ack?' <span class="pill">'+esc(t("visto"))+'</span>':"")+'</div>'+(a.ack?"":'<button class="btn gray sm op" onclick="auAck('+a.n+')">'+esc(t("Visto"))+'</button>')+'</div>'});
 $("aulist").innerHTML=h;$("auhist").innerHTML=(AU.hist||[]).map(function(x){return esc(x)}).join("<br>")||esc(t("Nessuna modifica registrata"))}
function auAck(n){post("/api/audit/ack",{n:n}).then(function(r){return r.json()}).then(function(j){msg("aum",j.ok?t("Fatto"):j.err,j.ok);auLoad()})}

/* ===== Note legali ===== */
function lgLoad(){loadLic();commonLoad().then(function(c){var h='<thead><tr><th>'+esc(t("Nome"))+'</th><th>'+esc(t("Versione"))+'</th><th>'+esc(t("Licenza"))+'</th></tr></thead><tbody>';
 c.sbom.forEach(function(x){h+='<tr><td><a href="'+esc(x.url)+'" target="_blank" rel="noopener">'+esc(x.name)+'</a></td><td>'+esc(x.ver)+'</td><td>'+esc(x.lic)+'</td></tr>'});$("sbom").innerHTML=h+'</tbody>'});
 api("/api/region").then(function(r){return r.json()}).then(function(r){commonLoad().then(function(){var c=COMMON.by[r.country],g=regRule(c);
  $("lgradio").innerHTML='<p>'+esc(c?tf("Paese: {0}. Canali 1-{1}, potenza massima {2} dBm EIRP.",cName(c),r.ch,r.limit):t("Paese non scelto: la radio usa le regole piu prudenti (canali 1-11)"))+'</p>'+(g?'<p>'+esc(tf("Regole: {0}",g.rule))+'</p>':"")+'<p class="fhint">'+esc(t("Il firmware rispetta i limiti del paese scelto. La conformita del prodotto finito (marchi CE, UKCA, FCC...) e responsabilita di chi lo mette in commercio. Non e consulenza legale."))+'</p>'})}).catch(function(){})}

