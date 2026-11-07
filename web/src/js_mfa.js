/* ===== MFA ===== */
var MF=null;
function mfLoad(){
  api("/api/mfa").then(function(r){return r.json()}).then(mfDraw).catch(function(){})
}

function mfDraw(j){
  MF=j;
  var me=null;
  j.list.forEach(function(x){if(x.name==USER)me=x});
  var on=me&&me.on;
  $("mfst").className="msg";
  $("mfst").style.display="block";
  $("mfst").textContent=on?tf("MFA acceso. Codici di recupero rimasti: {0}",me.rec):t("MFA spento");
  svcSt("mfa",on);
  $("mf_rec").className=on?"btn gray":"hide";
  $("mftime").textContent=j.time?t("Ora della scheda valida.")
    :t("Ora della scheda NON valida: impostala in Sistema > Ora prima di attivare l'MFA.");
  $("mfadm").className=ROLE==2?"card":"card hide";
  if(ROLE==2){
    $("mf_nt").value=j.nt;
    $("mf_pow").value=j.pow;
    var h='<thead><tr><th>'+esc(t("Nome"))+'</th><th>MFA</th><th></th></tr></thead><tbody>';
    j.list.forEach(function(x){
      h+='<tr><td>'+esc(x.name)+'</td><td>'+esc(x.on?t("acceso"):t("spento"))+'</td><td>'+
        (x.on?'<button class="btn gray sm" onclick="mfOff('+x.i+')">'+esc(t("Spegni"))+'</button>'
          :"")+'</td></tr>'
    });
    $("mftab").innerHTML=h+'</tbody>'
  }
}

function mfBegin(){
  return post("/api/mfa/begin",{}).then(function(r){return r.json()}).then(function(j){
    if(j.ok===false){
      msg("mfm",j.err,false);
      return
    }
    $("mfqr").innerHTML=qrSvg(j.uri);
    $("mfkey").textContent=j.key;
    $("mfact").className="";
    $("mf_code").value="";
    $("mfrec").className="hide";
    $("mf_code").focus()
  }).catch(function(){})
}

function mfConfirm(){
  post("/api/mfa/confirm",
    {code:$("mf_code").value.trim()}).then(function(r){return r.json()}).then(function(j){
      if(j.ok===false){
        msg("mfm",j.err,false);
        return
      }
      $("mfact").className="hide";
      mfShowRec(j.rec);
      msg("mfm",t("MFA acceso"),true);
      mfLoad()
    }).catch(function(){})
}

function mfShowRec(r){
  $("mfqr").innerHTML="";
  $("mfkey").textContent="";
  $("mfrecl").textContent=r.split(" ").join("   ");
  $("mfrec").className="law"
}

function mfRec(){
  dlg({title:t("Nuovi codici di recupero?"),
    body:'<p style="margin:0">'+esc(t("I codici vecchi smettono di valere."))+'</p>',ok:t("Crea"),
    onOk:function(){
      return post("/api/mfa/recovery",{}).then(function(r){return r.json()}).then(function(j){
        if(j.ok===false)return j.err;
        mfShowRec(j.rec);
        mfLoad();
        return""
      })
    }})
}

function mfOff(i){
  dlg({title:t("Spegnere l'MFA?"),
    body:'<p style="margin:0">'+esc(t("L'accesso torna a chiedere solo la password."))+'</p>',
    ok:t("Spegni"),danger:true,onOk:function(){
      return post("/api/mfa/off",i<0?{}:{i:i}).then(function(r){return r.json()}).then(function(j){
        if(j.ok===false)return j.err;
        mfLoad();
        return""
      })
    }})
}

function mfSet(){
  post("/api/mfa/set",{nt:$("mf_nt").value,
    pow:$("mf_pow").value}).then(function(r){return r.json()}).then(function(j){
      if(j.ok===false){
        msg("mfm2",j.err,false);
        mfLoad();
        return
      }
      mfDraw(j);
      msg("mfm2",t("Salvato"),true)
    }).catch(function(){})
}

function mfCopy(){
  var x=$("mfrecl").textContent;
  try{
    navigator.clipboard.writeText(x);
    msg("mfm",t("Copiato"),true)
  }catch(e){msg("mfm",t("Non riesco a copiare: scrivili a mano"),false)}
}
