/* ===== periferiche ed Elenco dispositivi (API /api/dev) ===== */
var DV=[],DVT={btn:"boot"};
function dvList(){
  return api("/api/dev").then(function(r){return r.json()}).then(function(j){DV=j.dev||[]})
}

function dmLoad(){
  dvList().then(function(){dvDraw("dm_list","dm_m")}).catch(function(){})
}

/* Lo stato si scrive a parole: il colore da solo non basta. */
function dvStato(d){
  if(d.state==2)return t("con problema");
  if(d.state!=1)return t("spento");
  return d.sw?t("acceso"):t("presente")
}

/* Interruttore unico (componente sw1): on = acceso, label = nome per chi legge, act = JS da eseguire al click */
function dvSw(on,label,act){
  return '<button class="sw1'+(on?" on":"")+'" role="switch" aria-checked="'+
    (on?"true":"false")+'" aria-label="'+esc(label)+'" onclick="'+act+'"></button>'
}

function dvRiga(d,box,mm){
  var cl=d.state==1?"v":d.state==2?"k":"",
    h='<li><button class="dvn" onclick="show(\''+d.tab+'\')"><span>'+esc(d.label)+
    '</span><span class="pill dvp '+cl+'">'+esc(dvStato(d))+'</span></button><span class="dvsw">';
  if(d.sw&&ROLE==2)h+=dvSw(d.state,d.label,"dvSet('"+d.id+"',"+(d.state?0:1)+",'"+box+"','"+mm+"')");
  return h+'</span></li>'
}
/* Albero del solo hardware: radice con nome scheda e chip, un ramo per ogni gruppo "cap". */
function dvDraw(box,mm){
  var L=DV.filter(function(d){return d.kind==1&&canSee(d.tab)}),caps=[],h="";
  L.forEach(function(d){if(caps.indexOf(d.cap)<0)caps.push(d.cap)});
  caps.forEach(function(c){
    h+='<details open><summary>'+esc(c)+'</summary><ul class="dvl">';
    L.filter(function(d){return d.cap==c}).forEach(function(d){h+=dvRiga(d,box,mm)});
    h+='</ul></details>'
  });
  if(h)h='<details class="dvt" open><summary>'+esc(S?S.net.host+" · "+S.chip:"")+'</summary>'+h+
    '</details>';
  $(box).innerHTML=h||'<p class="fhint">'+esc(t("Nessuna voce"))+'</p>'
}

function dvSet(id,on,box,mm){
  post("/api/dev/set",{id:id,on:on}).then(function(r){return r.json()}).then(function(j){
    msg(mm,j.ok?t("Fatto"):j.err,j.ok);
    setTimeout(dmLoad,600)
  }).catch(function(){})
}

function dvRows(tab){
  var id=DVT[tab]||tab;
  api("/api/dev?id="+id).then(function(r){return r.json()}).then(function(j){
    var R=(j.data&&j.data.rows)||[],h='<table>';
    R.forEach(function(x){
      h+='<tr><td>'+esc(x[0])+'</td><td style="text-align:right"><b>'+esc(x[1])+'</b></td></tr>'
    });
    $("dv_"+tab).innerHTML=h+'</table>'
  }).catch(function(){})
}

/* --- voce Sensori: temperatura del chip piu i valori gia esposti dalle periferiche (/api/io) --- */
var SENS={tm:0};
function sensLoad(){
  clearInterval(SENS.tm);
  sensDraw();
  SENS.tm=setInterval(sensDraw,5000)
}

function sensDraw(){
  Promise.all([api("/api/dev?id=temp").then(function(r){return r.json()}),
    api("/api/io").then(function(r){return r.json()})]).then(function(a){
      var R=((a[0].data&&a[0].data.rows)||[]).slice(0,1);
      (a[1].io||[]).forEach(function(d){
        if(d.c=="i2c")R.push([d.n,ioI2cVal(d)]);
        if(d.c=="analogico")R.push([d.n,d.v])
      });
      $("sens_t").innerHTML=cpuTab(R)
    }).catch(function(){})
}

/* --- voce unica Processore (include la temperatura): righe 0-1 = temperatura, le altre = processore; "lv" = stato termico --- */
var CPU={tm:0,lv:-1};
function cpuTab(R){
  var h="<table>";
  R.forEach(function(x){
    h+='<tr><td>'+esc(x[0])+'</td><td style="text-align:right"><b>'+esc(x[1])+'</b></td></tr>'
  });
  return h+"</table>"
}

function cpuLoad(){
  clearInterval(CPU.tm);
  CPU.lv=-1;
  cpuDraw();
  CPU.tm=setInterval(function(){
    if(cur!="cpu"){
      clearInterval(CPU.tm);
      return
    }
    cpuDraw()
  },5000)
}

function cpuDraw(){
  api("/api/dev?id=cpu").then(function(r){return r.json()}).then(function(j){
    var R=(j.data&&j.data.rows)||[],lv=(j.data&&j.data.lv)||0,e=$("cpu_st");
    $("cpu_t").innerHTML=cpuTab(R.slice(0,1));
    $("cpu_p").innerHTML=cpuTab(R.slice(2));
    if(lv!=CPU.lv){
      CPU.lv=lv;
      e.className="pill dvp "+(lv?"w":"v");
      e.textContent=lv==2?t("Temperatura alta: velocita ridotta"):lv==1?t("Temperatura calda")
        :t("Temperatura normale")
    }
  }).catch(function(){})
}

function cpuDesc(g){
  return g=="ondemand"?t(
    "ondemand: sale subito al massimo quando il carico supera la soglia di salita, scende dopo l'attesa.")
    :g=="conservative"?t("conservative: sale e scende un gradino alla volta (80, 160, 240 MHz).")
    :g=="schedutil"?t("schedutil: segue il lavoro svolto, con un quarto di margine.")
    :t("Velocita fissa: la CPU non cambia da sola (solo il caldo puo abbassarla).")
}

function cpuUi(s){
  var m=$("cpumode");
  if(!m)return;
  if(document.activeElement!=m){
    m.value=s.cpuGov;
    $("cpudesc").textContent=cpuDesc(s.cpuGov);
    $("cpupar").className=isNaN(+s.cpuGov)?"":"hide"
  }
    [["cpu_up",s.cpuUp],["cpu_down",s.cpuDown],["cpu_hold",s.cpuHold]].forEach(function(x){
      if($(x[0])&&document.activeElement!=$(x[0]))$(x[0]).value=x[1]
    })
}

function setCpu(){
  post("/api/cpu",{gov:$("cpumode").value}).then(function(r){return r.json()}).then(function(j){
    msg("cpum",j.ok?t("Salvato"):j.err,j.ok);
    if(j.ok){
      $("cpudesc").textContent=cpuDesc($("cpumode").value);
      $("cpupar").className=isNaN(+$("cpumode").value)?"":"hide"
    }
  })
}

function setCpuPar(){
  post("/api/cpu",{up:$("cpu_up").value,down:$("cpu_down").value,
    hold:$("cpu_hold").value}).then(function(r){return r.json()}).then(function(j){
      msg("cpum",j.ok?t("Salvato"):j.err,j.ok)
    })
}
