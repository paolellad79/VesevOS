/* ===== LED ===== */
function ledHex(n){return"#"+("000000"+n.toString(16)).slice(-6)}
/* i modi 4-6 (lampi flash, wifi, cpu) e i loro colori: li cambia solo l'Admin; gli altri li vedono ma non li toccano */
function ledShow(){
  var m=+$("l_mode").value,adm=ROLE==2;
  $("l_src").className=m>=4?"":"hide";
  for(var i=0;i<5;i++)$("l_s"+i).disabled=!adm;
  $("l_thr").disabled=!adm;
  Array.prototype.forEach.call($("l_mode").options,function(o){
    o.disabled=!adm&&+o.value>=4&&+o.value!=m
  })
}

function fillLed(){
  api("/api/settings").then(function(r){return r.json()}).then(function(c){
    $("l_mode").value=c.ledMode;
    $("l_col").value=ledHex(c.ledColor);
    $("l_br").value=c.ledBrightness;
    for(var i=0;i<5;i++)$("l_s"+i).value=ledHex(c["ledSrc"+i]);
    $("l_thr").value=c.ledCpuThr;
    ledShow()
  })
}

function saveLed(){
  var d={mode:$("l_mode").value,color:parseInt($("l_col").value.slice(1),16),br:$("l_br").value};
  if(ROLE==2){
    for(var i=0;i<5;i++)d["s"+i]=parseInt($("l_s"+i).value.slice(1),16);
    d.thr=$("l_thr").value
  }
  post("/api/led",d).then(function(r){return r.json()}).then(function(j){
    msg("lm2",j.ok?t("Salvato"):j.err,j.ok)
  })
}
