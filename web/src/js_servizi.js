/* ===== Servizi di rete: hotspot, HTTP, HTTPS, porte ===== */
var SV={};
function svcLoad(){
  return api("/api/svc").then(function(r){return r.json()}).then(svcDraw).catch(function(){})
}

function svcDraw(j){
  if(!j||j.apOn===undefined)return;
  SV=j;
  svcSt("ap",j.apOn);
  $("sv_cap").classList.toggle("on",j.captive);
  $("apdep").className=j.apOn?"":"dim";
  var dp=document.querySelectorAll(".dpap");
  for(var i=0;i<dp.length;i++)dp[i].classList.toggle("dim",!j.apOn);
  $("sv_apn").textContent=j.apOn
    ?(j.httpOn&&j.httpPort==80?"":t("Il portale automatico serve HTTP acceso sulla porta 80."))
    :t(
      "Punto di accesso spento: se la Wi-Fi di casa non e raggiungibile la scheda non si vede. Recupero: tasto fisico 8 secondi o cavo USB.");
  if(j.dhcpOn!==undefined){
    $("sv_lease").value=j.lease;
    svcSt("dhcp",j.dhcpOn);
    svcSt("mdns",j.mdnsOn);
    svcSt("dns",j.captive)
  }
  $("sv_hp").value=j.httpPort;
  $("sv_sp").value=j.httpsPort;
  var pd=!!j.pending;
  svcSt("http",j.httpOn,pd,!!j.runHttp,j.usersHttp);
  svcSt("https",j.httpsOn,pd||!!j.httpsOn!=!!AUTH.https,!!AUTH.https,j.usersHttps);
  $("sv_hw").className=j.httpOn&&j.clientOk?"law":"hide";
  $("sv_hw").textContent=j.httpOn&&j.clientOk
    ?t("Sei collegato alla tua Wi-Fi: puoi spegnere HTTP e usare solo HTTPS (consigliato)."):"";
  var nh=!j.httpsOn;
  $("sv_sw").className=nh?"law":"hide";
  $("sv_sw").textContent=nh
    ?t(
      "Senza HTTPS il traffico non e cifrato: il login non manda la password (HMAC), ma sessione e dati si leggono sulla rete. Scelta tua: usa una rete di cui ti fidi.")
    :"";
  var pe=j.pending?t("Le modifiche valgono dopo Applica (riavvia la scheda)."):"";
  ["sv_hpend","sv_spend"].forEach(function(id){
    $(id).className=pe?"law":"hide";
    $(id).textContent=pe
  })
}

function svcPost(d,mid){
  return post("/api/svc",d).then(function(r){return r.json()}).then(function(j){
    if(j.ok===false){
      msg(mid,j.err,false);
      svcLoad();
      return false
    }
    svcDraw(j);
    msg(mid,t("Salvato"),true);
    return true
  }).catch(function(){})
}

function svcTog(k,e,mid){
  var d={};
  d[k]=e.classList.contains("on")?"0":"1";
  svcPost(d,mid||(k=="apOn"||k=="captive"?"svm_ap":"svm_http"))
}

function svcSet(k,mid){
  return function(v){
    var d={};
    d[k]=v?"1":"0";
    return svcPost(d,mid)
  }
}
/* ===== Servizi: UN componente per tutti (banner Avviato/Fermo + icone) ===== */
var SVT={ap:{set:svcSet("apOn","svm_ap")},dhcp:{set:svcSet("dhcpOn","svm_dhcp")},
  dns:{set:svcSet("captive","svm_dns")},mdns:{set:svcSet("mdnsOn","svm_mdns")},
  http:{set:svcSet("httpOn","svm_http"),ap:1},https:{set:function(v){return tlsSet(v)},ap:1},
  mqtt:{set:function(v){
    return mqRun(v?"start":"stop")
  },rs:function(){return mqRun("restart")},cls:"op"},
  mesh:{set:function(v){return msRun(v?"start":"stop")},rs:function(){return msRun("restart")}},
  stat:{set:function(v){return stSet(v)}},
  ntp:{set:function(v){return saveTime(v)}},
  power:{set:function(v){return pwSave(v)}},
  mfa:{set:function(v){return v?mfBegin():mfOff(-1)},cls:""}},SVS={};
function svcSt(id,on,pend,run,n){
  SVS[id]={on:!!on,pend:!!pend,run:run===undefined?!!on:!!run,busy:"",n:n};
  svcCtl(id)
}
/* quanti sono collegati (sessioni attive negli ultimi 2 minuti); l'amministratore apre l'elenco (who) */
function svcN(n){
  var l=tf("Collegati: {0}",n);
  return ROLE==2?'<button type="button" class="pill svn" onclick="whoShow()" title="'+
    esc(t("Chi e collegato"))+'">'+esc(l)+'</button>':'<span class="pill svn">'+esc(l)+'</span>'
}

function whoShow(){
  api("/api/who").then(function(r){return r.json()}).then(function(j){
    var L=j.list||[];
    var h=L.length?'<table class="ftab"><tr><th>'+esc(t("Utente"))+'</th><th>'+esc(t("Via"))+
      '</th><th>'+esc(t("Indirizzo"))+'</th><th>'+esc(t("Collegato da"))+'</th><th>'+
      esc(t("Ultima azione"))+'</th></tr>'+
      L.map(function(u){
        return'<tr><td><b>'+esc(u.user)+'</b>'+
          (u.user==USER?' <span class="pill">'+esc(t("tu"))+'</span>':"")+'<br><small>'+
          esc(t(RNAME[u.role]||""))+'</small></td><td>'+esc(u.via.toUpperCase())+'</td><td>'+
          esc(u.ip)+'</td><td>'+esc(fmtDur(u.age))+
          (u.live?"":'<br><small>'+esc(tf("ferma da {0}",fmtDur(u.idle)))+'</small>')+
          '</td><td><code>'+esc(u.act)+'</code><br><small>'+esc(tf("{0} fa",fmtDur(u.actAge)))+
          '</small></td></tr>'
      }).join("")+'</table>'
      :'<p style="margin:0">'+esc(t("Nessuno collegato dal web"))+'</p>';
    dlg({title:t("Chi e collegato"),
      body:h+'<p style="margin:8px 0 0;color:var(--mut)">'+
      esc(t(
        "Le letture automatiche della pagina non contano come azioni. Dalla seriale o dalla shell: comando who."))+
      '</p>',buttons:[["ok",t("Chiudi"),""]]})
  }).catch(function(){})
}

function sibB(cls,ic,lab,act,dis,oc){
  return'<button type="button" class="sib '+cls+(act?" act":"")+'"'+(oc?' onclick="'+oc+'"':"")+
    (dis?" disabled":"")+' aria-label="'+esc(lab)+'" title="'+esc(lab)+'" aria-pressed="'+
    (act?"true":"false")+'"><svg viewBox="0 0 24 24" aria-hidden="true"><use href="#'+ic+
    '"/></svg></button>'
}

function svcCtl(id){
  var e=$("sc_"+id),d=SVT[id],s=SVS[id];
  if(!e||!d||!s)return;
  var b=s.busy,
    B=function(k,cls,ic,lab,act,dis){return sibB(cls,ic,lab,act,dis,"svcAct('"+id+"','"+k+"')")};
  e.innerHTML='<div class="svb '+(s.run?"ok":"ko")+'" role="status">'+
    esc(s.run?t("Avviato"):t("Fermo"))+'</div>'+(s.n!==undefined&&s.run?svcN(s.n):"")+
    '<div class="svi '+(d.cls||"adm")+'" role="group" aria-label="'+
    esc(t("Controlli del servizio"))+'">'+
    B("go","go","i-sgo",t("Avvia"),b?b=="go":s.on)+B("st","st","i-sst",t("Ferma"),b?b=="st":!s.on)+
    B("rs","rs","i-srs",t("Riavvia"),b=="rs",!d.rs||!s.on)+
    (d.ap?B("ap","ap","i-sap",t("Applica"),b?b=="ap":s.pend,!s.pend):"")+'</div>'+
    (d.ap&&s.pend?'<div class="law" role="status">'+
      esc(t("In attesa: premi Applica (riavvia la scheda) per rendere effettiva la modifica."))+
      '</div>':"")
}

function svcAct(id,k){
  var d=SVT[id],s=SVS[id];
  if(!d||!s||s.busy)return;
  if(k=="go"&&s.on||k=="st"&&!s.on)return;
  s.busy=k;
  svcCtl(id);
  var p=k=="rs"?d.rs():k=="ap"?uReboot():d.set(k=="go");
  Promise.resolve(p).catch(function(){}).then(function(){
    setTimeout(function(){
      var x=SVS[id];
      if(x&&x.busy){
        x.busy="";
        svcCtl(id)
      }
    },k=="ap"?300:1400)
  })
}

function svcPort(k,id,mid){
  var d={};
  d[k]=$(id).value;
  svcPost(d,mid)
}
