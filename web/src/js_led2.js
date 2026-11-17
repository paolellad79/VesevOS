/* ===== LED1 (LED monocromatico) =====
   Stessi modi e stessa luminosita (0-255) del LED_RGB1, senza colori. Si legge e si salva con /api/dev (id led1). */
var L2={thr:70};

/* I modi 4-6 (lampi flash, wifi, carico CPU) li cambia solo l'Admin; gli altri li vedono ma non li toccano. */
function led2Show(){
  var m=+$("l2_mode").value,adm=ROLE==2;
  $("l2_src").className=m>=4?"":"hide";
  $("l2_thr").disabled=!adm;
  $("l2_thr").parentNode.className=m==6?"":"hide";
  Array.prototype.forEach.call($("l2_mode").options,function(o){
    o.disabled=!adm&&+o.value>=4&&+o.value!=m
  })
}

function apriLed2(id){
  $("l2_tit").textContent=voceTitolo(id);
  aliasPannello("al_led1",BDI[id]);
  api("/api/dev?id=led1").then(function(r){return r.json()}).then(function(j){
    var d=j.data||{};
    $("l2_mode").value=d.mode;
    $("l2_thr").value=d.thr;
    $("l2_br").value=d.br;
    led2Show()
  }).catch(function(){})
}

function led2Salva(){
  var arg=$("l2_mode").value+","+$("l2_thr").value+","+$("l2_br").value;
  post("/api/dev/act",{id:"led1",a:"set",arg:arg}).then(function(r){return r.json()}).then(function(j){
    msg("l2_m",j.mode!==undefined?t("Salvato"):j.err,j.mode!==undefined)
  }).catch(function(){})
}
