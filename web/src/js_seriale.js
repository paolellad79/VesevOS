/* ===== Seriale ===== */
function fillSer(){
  api("/api/settings").then(function(r){return r.json()}).then(function(c){
    SER=c.serialAuth;
    serDraw();
  });
  srLoad();
}
var SRD={};
function srOpts(){
  if($("sr_baud").options.length)return;
  [9600,19200,38400,57600,115200,230400,460800,921600].forEach(function(b){
    var o=document.createElement("option");
    o.value=b;
    o.textContent=b;
    $("sr_baud").appendChild(o)
  });
  [["crlf","CR+LF"],["lf","LF"],["cr","CR"]].forEach(function(e){
    var o=document.createElement("option");
    o.value=e[0];
    o.textContent=e[1];
    $("sr_eol").appendChild(o)
  })
}

function srLoad(){
  srOpts();
  api("/api/serial").then(function(r){return r.json()}).then(function(j){
    SRD=j;
    srDraw()
  }).catch(function(){})
}

function srDraw(){
  $("sr_baud").value=SRD.baud;
  $("sr_eol").value=SRD.eol;
  $("sr_tx").value=SRD.tx;
  ["echo","input","log","banner"].forEach(function(k){
    $("sw_sr_"+k).className="sw1"+(SRD[k]?" on":"")
  });
  $("sr_usb").textContent=SRD.usb?t("USB nativa: la velocita non conta."):"";
  $("sr_trial").className=SRD.trial?"msg ko":"msg ko hide";
  clearTimeout(srDraw._t);
  if(SRD.trial)srDraw._t=setTimeout(srLoad,21000)
}

function srSet(k,v){
  post("/api/serial",{k:k,v:v}).then(function(r){return r.json()}).then(function(j){
    if(j.err){
      msg("srm",j.err,false);
      srLoad()
    }else{
      SRD=j;
      srDraw();
      msg("srm",t("Salvato"),true)
    }
  }).catch(function(){})
}

function srTog(k){
  if(k==="input"&&SRD.input&&
    !confirm(t(
      "La seriale non accettera piu comandi. Si riattiva da questa pagina o con il tasto fisico. Continuare?")))return;
  srSet(k,SRD[k]?"off":"on")
}
var SER=true;
function serDraw(){
  $("sw_ser").className="sw1"+(SER?" on":"");
  var w=$("serwarn");
  if(SER){
    w.className="msg";
    w.textContent=""
  }else{
    w.className="msg ko";
    w.textContent=t(
      "Attenzione: la seriale e aperta, chi ha il cavo USB puo usare la shell senza password.")
  }
}

function serToggle(){
  var nv=!SER;
  if(!nv&&!confirm(t(
    "Spegnere la password sulla seriale? Chi collega il cavo USB potra usare la shell senza password.")))return;
  post("/api/serialauth",{on:nv?"1":"0"}).then(function(r){return r.json()}).then(function(j){
    if(j.ok){
      SER=nv;
      serDraw();
      msg("serm",t("Salvato"),true);
      setTimeout(function(){$("serm").className="msg"},3000)
    }else msg("serm",j.err,false)
  }).catch(function(){})
}
