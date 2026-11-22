/* ===== Sistema: nome della scheda, licenze ===== */
function fillSys(){
  api("/api/settings").then(function(r){return r.json()}).then(function(c){
    $("s_host").value=c.hostname;
    $("s_dom").value=c.domain;
    $("s_ml").value=c.messaggioLogin||"";
    mlAnteprima();
    $("s_dn").textContent=c.staDhcp
      ?t("In DHCP il router non comunica il dominio alla scheda: scrivilo tu se serve."):"";
    $("s_fq").textContent=t("Nome completo:")+" "+c.hostname+(c.domain?"."+c.domain:"")
  })
}

/* Anteprima del messaggio con le variabili gia sostituite (solo testo) */
function mlAnteprima(){
  var A=fmtSrc();
  $("s_mlp").textContent=msgVar($("s_ml").value,A,dataSpostata(A),$("s_host").value)
}

function saveSys(){
  post("/api/system",{hostname:$("s_host").value,
    domain:$("s_dom").value,
    messaggiologin:$("s_ml").value}).then(function(r){return r.json()}).then(function(j){
      msg("sm",j.ok?t("Salvato"):j.err,j.ok);
      if(j.ok)fillSys()
    })
}

function loadLic(){
  fetch("/api/license").then(function(r){return r.json()}).then(function(l){
    var h="";
    l.forEach(function(x){
      h+='<button class="btn gray" onclick="showLic(\''+esc(x.id)+'\')">'+esc(x.title)+'</button>'
    });
    $("licbtn").innerHTML=h
  }).catch(function(){})
}

function showLic(id){
  fetch("/api/license?id="+
    encodeURIComponent(id)).then(function(r){return r.text()}).then(function(x){
      var e=$("licx");
      e.textContent=x;
      e.className="";
      e.scrollTop=0
    })
}
