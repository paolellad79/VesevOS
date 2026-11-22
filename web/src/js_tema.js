/* ===== Tema della pagina: un file di stile caricato dall'amministratore (/etc/tema.css), servito da /tema.css ===== */
/* Le stesse parole vietate del firmware (core/vos_tema_rule.h): si controlla qui per non salvare un file che la scheda poi rifiuterebbe. */
var TEMA_VIETATO=/\\|<|@import|url\(|image-set\(|src\(|expression\(|javascript:|-moz-binding/i;
function temaStato(){
  fetch("/tema.css",{cache:"no-store"}).then(function(r){
    var s=r.headers.get("X-VOS-Tema")||"";
    return r.text().then(function(x){
      var e=$("tm_st");
      if(!e)return;
      if(s=="ok")e.textContent=t("Personalizzato")+" ("+x.length+" byte)";
      else if(s&&s!="fabbrica")e.textContent=t("Rifiutato dalla scheda: resta quello di fabbrica");
      else e.textContent=t("Di fabbrica")
    })
  }).catch(function(){})
}
function temaRicarica(testo){
  msg("tm_m",testo,true);
  setTimeout(function(){location.reload()},900)
}
function temaCarica(inp){
  var f=inp.files[0];
  inp.value="";
  if(!f)return;
  if(f.size>32768){msg("tm_m",t("Il tema e troppo grande (32 KB al massimo)"),false);return}
  f.text().then(function(x){
    if(!x.length||x.indexOf("\0")>=0||TEMA_VIETATO.test(x)){
      msg("tm_m",t("Il tema contiene qualcosa di non permesso (@import, url(), barra rovesciata o <)"),false);
      return
    }
    return caricaPezzi("/api/fs/pezzo?dir=/etc",new Blob([x]),"tema.css",null,t("Caricamento non riuscito")).then(function(j){
      if(!j.ok)msg("tm_m",j.err,false);
      else temaRicarica(t("Tema caricato: ricarico la pagina"))
    })
  })
}
function temaRipristina(){
  post("/api/fs/del",{path:"/etc/tema.css"}).then(function(r){return r.json()}).then(function(j){
    if(j.ok||/non esiste|not found/i.test(j.err||""))temaRicarica(t("Tema di fabbrica: ricarico la pagina"));
    else msg("tm_m",j.err,false)
  })
}
