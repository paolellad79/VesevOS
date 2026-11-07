/* ===== Risparmio energia ===== */
var PW=null;
function pwLoad(){
  api("/api/power").then(function(r){return r.json()}).then(pwDraw).catch(function(){})
}

function pwDraw(j){
  PW=j;
  svcSt("power",j.mode!=0);
  var x=document.activeElement;
  if((!x||x.id!="pw_mode")&&j.mode!=0){$("pw_mode").value=j.mode}
  if(!x||x.id!="pw_aw")$("pw_aw").value=j.awake;
  if(!x||x.id!="pw_sl")$("pw_sl").value=j.sleep;
  pwUi();
  var h=j.mode==0?t("Risparmio energia spento"):j.mode==1?t("Wi-Fi a risparmio massimo")
    :(j.guide?tf("Sonno a cicli: dormira tra {0}",durText(j.in))
      :t("Sonno a cicli: non dorme finche la guida di configurazione non e finita"));
  $("pwst").className="msg";
  $("pwst").style.display="block";
  $("pwst").textContent=h+" · "+tf("risvegli: {0}, dormito circa {1}",j.wakes,durText(j.slept))
}

function pwUi(){
  var m=$("pw_mode").value;
  $("pw_c").className=m==2?"agrid":"agrid hide";
  $("pw_w1").className=m==1?"law":"law hide";
  $("pw_w2").className=m==2?"law":"law hide"
}

function pwSave(on){
  if(on===undefined)on=!!(SVS.power&&SVS.power.on);
  return post("/api/power",
    {mode:on?$("pw_mode").value:0,awake:$("pw_aw").value,
      sleep:$("pw_sl").value}).then(function(r){return r.json()}).then(function(j){
        if(j.ok===false){
          msg("pwm",j.err,false);
          return
        }
        pwDraw(j);
        msg("pwm",t("Salvato"),true)
      }).catch(function(){})
}

function pwNow(){
  var m=+$("pw_now").value;
  if(!(m>=1&&m<=10080)){
    msg("pwm2",t("Tempo di sonno: da 1 minuto a 7 giorni"),false);
    return
  }
  dlg({title:t("Dormire adesso?"),
    body:'<p style="margin:0">'+esc(t("La pagina non si raggiunge piu fino al risveglio."))+
    '</p><p style="margin:8px 0 0">'+
    esc(tf("La scheda riparte da sola tra {0} (o subito con il tasto RESET).",durText(m*60)))+
    '</p>',ok:t("Dormi"),danger:true,
    onOk:function(){
      return post("/api/sleep",{min:m}).then(function(r){return r.json()}).then(function(j){
        return j.ok===false?j.err:""
      })
    }})
}
