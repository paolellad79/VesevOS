/* ===== MQTT ===== */
function mqLoad(){
  api("/api/mqtt").then(function(r){return r.json()}).then(function(m){
    svcSt("mqtt",!!m.run);
    $("mqst").innerHTML=(m.run
      ?'<span class="pill'+(m.conn?" a":"")+'">'+esc(m.conn?t("Collegato"):t("non collegato"))+
      '</span> ':"")+'<span class="fhint">'+esc(tf("Inviati {0}, ricevuti {1}",m.sent,m.recv))+
      '</span>'+(m.err?'<div class="msg ko" style="display:block;margin-top:6px">'+esc(m.err)+
        '</div>':"");
    if(document.activeElement&&document.activeElement.closest&&
      document.activeElement.closest("#t_mqtt .agrid"))return;
    $("mq_host").value=m.host;
    $("mq_port").value=m.port;
    $("mq_user").value=m.user;
    $("mq_pre").value=m.prefix;
    $("mq_pre").placeholder=m.prefixUsed;
    $("mq_ev").value=m.every;
    $("mq_pass").placeholder=m.hasPass?t("(salvata: lascia vuoto per non cambiarla)"):"";
    $("mq_auto").classList.toggle("on",m.auto);
    $("mq_ha").classList.toggle("on",m.ha);
    $("mq_tls").classList.toggle("on",!!m.tls);
    $("mqtlsw").className=(!m.tls&&(m.user||m.hasPass))?"law":"law hide";
    $("mq_ca").placeholder=m.hasCa
      ?t("(certificato salvato: incolla per sostituirlo, salva vuoto per toglierlo)")
      :"-----BEGIN CERTIFICATE-----";
    var p=m.prefixUsed;
    $("mqtopics").innerHTML=
      esc(t("Argomenti:"))+' <code>'+esc(p)+'/state</code> '+esc(t("(stato)"))+', <code>'+esc(p)+
      '/cmd</code> '+esc(t("(comandi, es. led-color ff0000, gpio 4 1, reboot)"))+', <code>'+esc(p)+
      '/status</code> (online/offline)'
  }).catch(function(){})
}

function mqSave(){
  post("/api/mqtt",{host:$("mq_host").value,port:$("mq_port").value,user:$("mq_user").value,
    pass:$("mq_pass").value,prefix:$("mq_pre").value,every:$("mq_ev").value,
    auto:$("mq_auto").classList.contains("on")?1:0,ha:$("mq_ha").classList.contains("on")?1:0,
    tls:$("mq_tls").classList.contains("on")?1:0})
    .then(function(r){return r.json()}).then(function(j){
      msg("mqm2",j.ok?t("Salvato"):j.err,j.ok);
      if(j.ok){
        $("mq_pass").value="";
        setTimeout(mqLoad,800)
      }
    })
}

function mqCa(){
  post("/api/mqtt/ca",{pem:$("mq_ca").value.trim()}).then(function(r){return r.json()}).then(
    function(j){
      msg("mqm3",j.ok?t("Salvato"):j.err,j.ok);
      if(j.ok){
        $("mq_ca").value="";
        mqLoad()
      }
    })
}

function mqRun(a){
  return post("/api/mqtt/run",{a:a}).then(function(r){return r.json()}).then(function(j){
    msg("mqm",j.ok?(a=="test"?t("Messaggio inviato"):t("Fatto")):j.err,j.ok);
    setTimeout(mqLoad,1200)
  }).catch(function(){mqLoad()})
}
