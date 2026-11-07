/* ===== Watchdog ===== */
var WDD=["Lun","Mar","Mer","Gio","Ven","Sab","Dom"];
function wdLoad(){
  api("/api/wd").then(function(r){return r.json()}).then(function(w){
    var h=(w.degraded?'<div class="msg ko" style="display:block">'+
      esc(tf("Modalita ridotta: servizi spenti: {0}. Pagina web, log e recupero restano attivi.",
        w.off||"-"))+'</div>':"")+
      (w.stopped?'<div class="msg ko" style="display:block">'+
        esc(t("Riavvii automatici sospesi: troppi in un'ora. Controlla il registro."))+'</div>':"")+
      (w.last?row(esc(t("Ultimo riavvio automatico")),esc(w.last)):"");
    h+='<table class="ftab"><thead><tr><th>'+esc(t("Servizio"))+'</th><th>'+
      esc(t("Ultimo segno di vita"))+'</th><th>'+esc(t("Limite"))+'</th><th>'+esc(t("Riavvii"))+
      '</th></tr></thead><tbody>';
    w.watch.forEach(function(x){
      h+='<tr><td>'+esc(x.n)+'</td><td>'+esc(durText(x.ago))+'</td><td>'+esc(durText(x.to))+
        '</td><td>'+x.r+'</td></tr>'
    });
    $("wdst").innerHTML=h+'</tbody></table>';
    $("wd_task").classList.toggle("on",w.task);
    $("wd_net").classList.toggle("on",w.net);
    $("wd_ram").classList.toggle("on",w.ram);
    $("wd_nm").value=w.netMin;
    $("wd_rk").value=w.ramKb;
    $("wd_ud").value=w.upDays;
    $("wd_at").value=w.at>=0?("0"+Math.floor(w.at/60)).slice(-2)+":"+("0"+w.at%60).slice(-2):"";
    wdTimes(w);
    var b="";
    WDD.forEach(function(d,i){
      b+='<button type="button" class="btn '+((w.days>>i)&1?"":"gray")+'" data-on="'+
        ((w.days>>i)&1)+
        '" onclick="this.dataset.on=this.dataset.on==1?0:1;this.className=\'btn \'+(this.dataset.on==1?\'\':\'gray\')">'+
        esc(t(d))+'</button>'
    });
    $("wd_days").innerHTML=b
  }).catch(function(){})
}

function wdTimes(w){
  function fill(id,o){
    var e=$(id);
    e.value=o.v;
    e.min=o.min;
    e.max=o.max;
    $(id+"_h").textContent=tf("Ora {0} s, da {1} a {2}, di base {3}",o.v,o.min,o.max,o.def)
  }
  fill("wd_tw",w.twdt);
  w.lim.forEach(function(o){fill("wd_l_"+o.n,o)});
  $("wd_int").textContent=
    tf(
      "Watchdog di interruzione: {0} ms. Si cambia solo compilando il firmware: qui e in sola lettura.",
      w.intMs)
}

function wdDefaults(){
  post("/api/wd",{defaults:1})
    .then(function(r){return r.json()}).then(function(j){
      msg("wdm",j.ok?t("Salvato"):j.err,j.ok);
      if(j.ok)wdLoad()
    })
}

function wdSave(){
  var at=$("wd_at").value,days=0;
  [].forEach.call($("wd_days").children,function(b,i){if(b.dataset.on==1)days|=1<<i});
  post("/api/wd",{task:$("wd_task").classList.contains("on")?1:0,
    net:$("wd_net").classList.contains("on")?1:0,ram:$("wd_ram").classList.contains("on")?1:0,
    netmin:$("wd_nm").value,ramkb:$("wd_rk").value,updays:$("wd_ud").value,
    at:at?+at.slice(0,2)*60+ +at.slice(3,5):-1,days:days,twdt:$("wd_tw").value,
    lim_net:$("wd_l_net").value,lim_monitor:$("wd_l_monitor").value,lim_mesh:$("wd_l_mesh").value,
    lim_mqtt:$("wd_l_mqtt").value,lim_led:$("wd_l_led").value})
    .then(function(r){return r.json()}).then(function(j){
      msg("wdm",j.ok?t("Salvato"):j.err,j.ok);
      if(j.ok)wdLoad()
    })
}
