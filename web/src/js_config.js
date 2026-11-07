/* ===== Configurazione: ripristino, copia, azzeramento ===== */
function restoreCfg(){
  var f=$("cf").files[0];
  if(!f){
    msg("cm",t("Scegli un file"),false);
    return
  }
  f.text().then(function(x){
    return post("/api/config/restore",{t:x,pw:$("bk_rp").value})
  }).then(function(r){return r.json()}).then(function(j){
    msg("cm",j.ok?tf("Ripristinato: {0} righe cambiate",j.changed):j.err,j.ok);
    if(j.ok)$("bk_rp").value=""
  })
}

function bkSec(){$("bk_box").className=$("bk_sec").checked?"":"hide"}
function bkExport(){
  var p=$("bk_pw").value;
  if(p.length<10){
    msg("cm",t("La frase deve avere almeno 10 caratteri"),false);
    return
  }
  if(p!==$("bk_pw2").value){
    msg("cm",t("Le due frasi non sono uguali"),false);
    return
  }
  if(!$("bk_acc").checked){
    msg("cm",t("Devi accettare l'avviso per esportare i segreti"),false);
    return
  }
  post("/api/config/export",{acc:"1",pw:p}).then(function(r){return r.text()}).then(function(x){
    if(x.charAt(0)=="{"){
      var j={};
      try{j=JSON.parse(x)}catch(e){}
      msg("cm",j.err||t("Errore"),false);
      return
    }
    var a=document.createElement("a");
    a.href=URL.createObjectURL(new Blob([x],{type:"text/plain"}));
    a.download="vesevos-segreti.conf";
    document.body.appendChild(a);
    a.click();
    setTimeout(function(){
      URL.revokeObjectURL(a.href);
      a.remove()
    },500);
    $("bk_pw").value="";
    $("bk_pw2").value="";
    $("bk_acc").checked=false;
    msg("cm",t("Backup cifrato scaricato. Custodisci il file e la frase."),true)
  }).catch(function(){})
}

function factory(){
  dlg({title:t("Azzerare TUTTO (anche la password)?"),
    body:'<p style="margin:0 0 8px">'+
    esc(t(
      "Tutte le impostazioni, gli utenti e le password vengono cancellati. Per confermare scrivi la parola AZZERA."))+
    '</p><input id="fz_t" autocomplete="off" aria-label="'+esc(t("Parola di conferma"))+'">'+
    '<label for="fz_p">'+esc(t("Password"))+'</label>'+
    '<input id="fz_p" type="password" autocomplete="current-password">',
    ok:t("Azzera tutto"),danger:true,
    onOk:function(){
      if(dlgV("fz_t").toUpperCase()!==t("AZZERA"))return t("La parola non coincide");
      var p=dlgV("fz_p");
      if(!p)return t("Scrivi la password");
      return post("/api/login/start",{u:USER}).then(function(r){return r.json()}).then(function(s){
        var mac=hmacHex(hashPass(s.salt,p,s.iter),s.nonce);
        return post("/api/factory",{nonce:s.nonce,mac:mac})
      }).then(function(r){return r.json()}).then(function(j){
        if(!j.ok)return j.err;
        msg("fzm",t("Riavvio in corso..."),true);
        return ""
      })
    }})
}
