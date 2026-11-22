/* ===== LED ===== */
function ledHex(n){return"#"+("000000"+n.toString(16)).slice(-6)}
/* i modi 4-6 (lampi flash, wifi, cpu) e i loro colori: li cambia solo l'Admin; gli altri li vedono ma non li toccano */
function ledShow(){
  var m=+$("l_mode").value,adm=ROLE==2;
  for(var j=0;j<3;j++)$("l_g"+j).disabled=!adm;
  for(var j=0;j<6;j++)$("l_t"+j).disabled=!adm;
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
    for(var i=0;i<3;i++)$("l_g"+i).value=c["ledGain"+i];
    for(var i=0;i<6;i++)$("l_t"+i).value=ledHex(c["ledSt"+i]);
    ledShow()
  })
}

function saveLed(){
  var d={mode:$("l_mode").value,color:parseInt($("l_col").value.slice(1),16),br:$("l_br").value};
  if(ROLE==2){
    for(var i=0;i<5;i++)d["s"+i]=parseInt($("l_s"+i).value.slice(1),16);
    d.thr=$("l_thr").value;
    for(var i=0;i<3;i++)d["g"+i]=$("l_g"+i).value;
    for(var i=0;i<6;i++)d["t"+i]=parseInt($("l_t"+i).value.slice(1),16)
  }
  post("/api/led",d).then(function(r){return r.json()}).then(function(j){
    msg("lm2",j.ok?t("Salvato"):j.err,j.ok)
  })
}

/* Test e calibrazione: il colore si vede sul LED per 20 s senza salvare; ledProva accetta un numero o un selettore di colore */
var ledUltimo=0xFFFFFF,ledTimer=0;
function ledAnte(){
  post("/api/led/anteprima",{c:ledUltimo,g0:$("l_g0").value,g1:$("l_g1").value,g2:$("l_g2").value}).then(function(r){return r.json()}).then(function(j){
    if(!j.ok)msg("lm2",j.err,false)
  })
}
function ledProva(x){
  ledUltimo=typeof x=="number"?x:parseInt(x.value.slice(1),16);
  ledAnte()
}
function ledGuad(){
  clearTimeout(ledTimer);
  ledTimer=setTimeout(ledAnte,150)
}
function ledFineProva(){post("/api/led/anteprima",{fine:1})}
function ledCalFabbrica(){
  for(var i=0;i<3;i++)$("l_g"+i).value=255;
  var f=[0xFF7800,0x0000FF,0xFFFF00,0x00FF00,0xAA00FF,0xFF7800];
  for(var i=0;i<6;i++)$("l_t"+i).value=ledHex(f[i]);
  ledAnte()
}
