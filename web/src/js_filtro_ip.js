/* ===== Filtro IP ===== */
var FW={tm:null,you:""};
function fwLoad(){
  api("/api/fw").then(function(r){return r.json()}).then(fwDraw).catch(function(){})
}

function fwDraw(f){
  FW.you=f.you;
  $("fwyou").textContent=tf("Il tuo indirizzo: {0}",f.you)+" · "+
    (f.youOk?t("con questa regola puoi entrare")
      :t("ATTENZIONE: con questa regola resteresti fuori"));
  $("fwyou").style.borderColor=f.youOk?"":"var(--ko)";
  if(document.activeElement!=$("fw_r")){
    $("fw_m").value=f.mode;
    $("fw_ntp").classList.toggle("on",f.ntp);
    $("fw_r").value=f.rules.map(function(x){
      return(x.from==x.to?x.from:x.from+"-"+x.to)+"|"+(x.on?1:0)+(x.name?"|"+x.name:"")
    }).join("\n")
  }
  var h='<thead><tr><th>'+esc(t("Indirizzo"))+'</th><th>'+esc(t("Tentativi"))+
    '</th></tr></thead><tbody>';
  if(!f.rejected.length)h+='<tr><td colspan="2" style="color:var(--mut)">'+esc(t("Nessuno"))+
    '</td></tr>';
  f.rejected.forEach(function(x){
    h+='<tr><td><code>'+esc(x.ip)+'</code></td><td>'+x.n+'</td></tr>'
  });
  $("fwrej").innerHTML=h+'</tbody>';
  clearInterval(FW.tm);
  $("fwok").className=f["try"]?"btn":"btn hide";
  if(f["try"]){
    var left=f["try"];
    var tick=function(){
      if(cur!="fw"){
        clearInterval(FW.tm);
        return
      }
      var e=$("fwtry");
      e.className="msg ko";
      e.textContent=tf("Regola in prova: se non confermi torna quella di prima tra {0} secondi",
        left);
      if(--left<0){
        clearInterval(FW.tm);
        fwLoad()
      }
    };
    tick();
    FW.tm=setInterval(tick,1000)
  }else{
    $("fwtry").className="msg";
    $("fwtry").textContent=""
  }
}

function fwMine(){
  var v=$("fw_r").value.trim();
  $("fw_r").value=(v?v+"\n":"")+FW.you+"|1|"+t("io")
}

function fwSave(){
  post("/api/fw",{mode:$("fw_m").value,ntp:$("fw_ntp").classList.contains("on")?1:0,
    rules:$("fw_r").value}).then(function(r){return r.json()}).then(function(j){
      if(j.ok===false){
        msg("fwm",j.err,false);
        return
      }
      fwDraw(j);
      msg("fwm",t("Regola in prova per 2 minuti: se la pagina risponde ancora premi Conferma"),true)
    }).catch(function(){
      msg("fwm",t("Nessuna risposta: aspetta 2 minuti, torna la regola di prima"),false)
    })
}

function fwConfirm(){
  post("/api/fw/confirm",{}).then(function(r){return r.json()}).then(function(j){
    msg("fwm",j.ok?t("Regola confermata"):j.err,j.ok);
    fwLoad()
  })
}

function banLoad(){
  api("/api/ban").then(function(r){return r.json()}).then(function(j){
    $("b_f").value=j.fails;
    $("b_s").value=j.secs;
    var h='<thead><tr><th>'+esc(t("Indirizzo"))+'</th><th>'+esc(t("Stato"))+
      '</th><th class="fac"></th></tr></thead><tbody>';
    if(!j.list.length)h+='<tr><td colspan="3" style="color:var(--mut)">'+
      esc(t("Nessun indirizzo bloccato o sospetto"))+'</td></tr>';
    j.list.forEach(function(b){
      h+='<tr><td><code>'+esc(b.ip)+'</code></td><td>'+
        (b.wait?'<span class="pill k">'+esc(tf("bloccato ancora {0}",durText(b.wait)))+'</span>'
          :esc(tf("{0} errori",b.fails)))+
        '</td><td class="fac"><button class="btn gray sm" onclick="banUn(\''+esc(b.ip)+'\')">'+
        esc(t("Sblocca"))+'</button></td></tr>'
    });
    $("bntab").innerHTML=h+'</tbody>'
  }).catch(function(){})
}

function banUn(ip){
  post("/api/ban/unban",{ip:ip}).then(function(r){return r.json()}).then(function(j){
    msg("bnm",j.ok?t("Sbloccato"):j.err,j.ok);
    banLoad()
  })
}

function banSave(){
  post("/api/ban/set",{fails:$("b_f").value,
    secs:$("b_s").value}).then(function(r){return r.json()}).then(function(j){
      msg("bnm",j.ok?t("Salvato"):j.err,j.ok)
    })
}
