/* ===== HTTPS ===== */
function tlsLoad(){
  api("/api/tls").then(function(r){return r.json()}).then(tlsDraw).catch(function(){})
}

function tlsDraw(x){
  svcSt("https",x.on,x.pending||!!x.on!=!!AUTH.https,!!AUTH.https);
  if($("certst"))$("certst").innerHTML=
    row(esc(t("Certificato")),x.custom?esc(t("tuo")):esc(t("creato dalla scheda")))+
    row(esc(t("Nome")),esc(x.name))+
    (x.from?row(esc(t("Valido dal")),esc(x.from))+row(esc(t("Scade il")),esc(x.to)):"")+
    '<div class="subt" style="margin-top:6px">'+esc(t("Impronta SHA-256"))+'</div><div class="fp">'+
    esc(x.fp||"-")+'</div>'+
    (x.pending?'<div class="law">'+esc(t("Certificato nuovo pronto: vale dal prossimo riavvio."))+
      '</div>':"");
  $("tlsst").innerHTML=row(esc(t("Stato")),
    x.on?(AUTH.https?esc(t("Attivo")):esc(t("Attivo dal prossimo riavvio"))):esc(t("Spento")))+
    row(esc(t("Nome")),esc(x.name))+
    row(esc(t("Certificato")),x.custom?esc(t("tuo")):esc(t("creato dalla scheda")))+
    '<div class="subt" style="margin-top:6px">'+esc(t("Impronta SHA-256"))+'</div><div class="fp">'+
    esc(x.fp||"-")+'</div>'+
    (x.pending?'<div class="law">'+esc(t("Certificato nuovo pronto: vale dal prossimo riavvio."))+
      '</div>':"")
}

function tlsSet(v){
  if(!v&&!confirm(t(
    "Spegnere HTTPS? Le password restano protette, ma la pagina viaggia in chiaro sulla rete di casa.")))return Promise.resolve();
  return post("/api/tls",{on:v?1:0}).then(function(r){return r.json()}).then(function(j){
    if(j.ok===false){
      msg("tlsm",j.err,false);
      return
    }
    tlsDraw(j);
    svcLoad();
    msg("tlsm",t("Salvato: vale dopo Applica"),true)
  })
}

function tlsRegen(){
  dlg({title:t("Creare un certificato nuovo?"),
    body:'<p style="margin:0">'+
    esc(t("Vale dal prossimo riavvio. Il browser avvisera di nuovo: confronta l'impronta nuova."))+
    '</p>',ok:t("Crea"),
    onOk:function(){
      return post("/api/tls",{regen:1}).then(function(r){return r.json()}).then(function(j){
        if(j.ok!==false)tlsDraw(j);
        return j.ok===false?j.err:""
      })
    }})
}

function tlsUp(){
  post("/api/tls",{cert:$("tls_c").value.trim(),
    key:$("tls_k").value.trim()}).then(function(r){return r.json()}).then(function(j){
      if(j.ok===false){
        msg("tlsm2",j.err,false);
        return
      }
      $("tls_k").value="";
      tlsDraw(j);
      msg("tlsm2",t("Salvato: vale dal prossimo riavvio"),true)
    })
}
