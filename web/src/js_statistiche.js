/* ===== Statistiche d'uso ===== */
var STT=0;
function stLoad(){
  api("/api/stats").then(function(r){return r.json()}).then(stDraw).catch(function(){})
}

function stDraw(j){
  svcSt("stat",j.on);
  $("st_csv").className=j.on?"btn gray":"btn gray hide";
  $("st_rst").className=j.on?"btn gray adm":"btn gray adm hide";
  if(!j.on){
    $("stbody").innerHTML='<p class="fhint">'+esc(t("Statistiche spente"))+'</p>';
    return
  }
  var h=row(esc(t("Conteggio da")),esc(durText(j.sec)))+
    row(esc(t("Wi-Fi: collegamenti / cadute")),j.wifiUp+" / "+j.wifiDown)+
    row(esc(t("Wi-Fi: tempo collegato")),esc(durText(j.wifiSec)))+
    row(esc(t("Segnale medio / minimo")),j.rssiAvg+" / "+j.rssiMin+" dBm")+
    row(esc(t("Hotspot: telefoni insieme (massimo)")),j.apMax)+
    row(esc(t("Bluetooth: accensioni / collegamenti")),j.bleStart+" / "+j.bleConn)+
    row(esc(t("Tasto fisico: brevi / 2 s / 8 s / 20 s")),j.boot.join(" / "))+
    (j.bootEver?row(esc(t("Ultima pressione del tasto fisico")),esc(tf("{0} fa",durText(j.bootLast)))):"")+
    row(esc(t("LED: stato / battito / fisso / spento")),j.led.map(durText).join(" / "));
  $("stbody").innerHTML=h
}

function stSet(on){
  return post("/api/stats",{on:on?1:0}).then(function(r){return r.json()}).then(function(j){
    if(j.ok===false){
      msg("stm",j.err,false);
      return
    }
    stDraw(j);
    msg("stm",t("Salvato"),true)
  }).catch(function(){})
}

function stReset(){
  post("/api/stats",{reset:1}).then(function(r){return r.json()}).then(function(j){
    stDraw(j);
    msg("stm",t("Azzerate"),true)
  }).catch(function(){})
}
