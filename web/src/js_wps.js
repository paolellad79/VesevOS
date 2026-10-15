/* ===== WPS: collegamento con il router (tasto o PIN) ===== */
var WP={tm:null,seq:0,j:null};
SVT.wps={set:function(v){return wpsSetOn(v)}};
function wpsLoad(){clearTimeout(WP.tm);var id=++WP.seq;api("/api/wps").then(function(r){return r.json()}).then(function(j){if(id!=WP.seq)return;wpsDraw(j)}).catch(function(){if(id==WP.seq&&cur=="wps")WP.tm=setTimeout(wpsLoad,2000)})}
function wpsPinFmt(p){return p&&p.length==8?p.slice(0,4)+" "+p.slice(4):(p||"")}
function wpsDraw(j){WP.j=j;if(cur!="wps")return;svcSt("wps",j.on);
 $("wps_swp").classList.toggle("on",!!j.pinOk);$("wps_bt").textContent=j.btn?tf("Dal tasto BOOT: {0} click avviano il WPS (si cambia in Dispositivi > Tasto BOOT).",j.btn):t("Dal tasto BOOT: nessun gesto avvia il WPS (si sceglie in Dispositivi > Tasto BOOT).");
 $("wps_setup").className=j.setup?"law":"hide";$("wps_setup").textContent=j.setup?t("Prima configurazione: il WPS funziona anche con l'interruttore spento. Dopo la guida torna a obbedire all'interruttore."):"";
 var run=j.state==1,netOk=j.net==1||j.net==3,can=j.avail&&j.allowed&&netOk&&!run,st=$("wps_st"),txt="",cls="msg";
 if(!j.avail)txt=t("Questo firmware non contiene il WPS");
 else if(run)txt=j.method=="pin"?(j.pin?tf("Scrivi il PIN nel router: ancora {0} s",j.left):t("Sto creando il PIN...")):tf("In attesa del router: premi il tasto WPS (ancora {0} s)",j.left);
 else if(j.state==2){txt=tf("Fatto: la scheda si collega a {0}.",j.ssid);cls="msg ok"}
 else if(j.state==3){txt=j.err||t("WPS non riuscito");cls="msg ko"}
 else if(!j.allowed)txt=t("WPS spento: accendilo con l'interruttore qui sopra");
 else txt=t("Pronto");
 st.className=cls;st.style.display="block";st.textContent=txt;
 $("wps_pin").className=run&&j.method=="pin"&&j.pin?"":"hide";$("wps_pinv").textContent=wpsPinFmt(j.pin);
 $("wps_go").disabled=!can;$("wps_gp").disabled=!can||!j.pinOk;$("wps_gp").className=j.pinOk?"btn gray":"btn gray hide";
 $("wps_go").className=run?"btn hide":"btn";$("wps_x").className=run?"btn gray":"btn gray hide";
 $("wps_nt").textContent=j.avail&&j.allowed&&!netOk&&!run?t("Il WPS parte solo con il Wi-Fi acceso e non in modo aereo"):"";
 clearTimeout(WP.tm);if(run)WP.tm=setTimeout(wpsLoad,1000)}
function wpsPost(d,mid){return post("/api/wps",d).then(function(r){return r.json()}).then(function(j){if(j.ok===false){msg(mid,j.err,false);wpsLoad();return false}wpsDraw(j);return true}).catch(function(){})}
function wpsSetOn(v){return wpsPost({on:v?1:0},"wpsm").then(function(ok){if(ok)msg("wpsm",t("Salvato"),true)})}
function wpsOpt(k,e){var d={};d[k]=e.classList.contains("on")?"0":"1";wpsPost(d,"wps_om").then(function(ok){if(ok)msg("wps_om",t("Salvato"),true)})}
function wpsGo(m){var j=WP.j||{},w=j.net==3;
 dlg({title:m=="pin"?t("Collegare con il PIN?"):t("Collegare con il tasto del router?"),
  body:'<p style="margin:0">'+esc(w?t("La scheda si scollega dalla rete di casa per la prova. Se non riesce, ci torna da sola."):t("La scheda cerca il router mentre l'hotspot resta acceso."))+'</p><p style="margin:8px 0 0">'+esc(t("Se sei collegato all'hotspot la pagina puo interrompersi per qualche secondo."))+'</p>',
  ok:t("Avvia"),onOk:function(){return post("/api/wps",{start:m}).then(function(r){return r.json()}).then(function(x){if(x.ok===false)return x.err;wpsDraw(x);return""})}})}
function wpsCancel(){wpsPost({stop:1},"wpsm")}
