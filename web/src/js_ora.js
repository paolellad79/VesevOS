/* ===== Ora (qui resta anche fp, cartella della pagina File: l'ordine delle var non cambia) ===== */
var fp="/";
var OTZ=[];
function oraTz(){
  Promise.all([commonLoad(),
    api("/api/region").then(function(r){return r.json()})]).then(function(a){
      var r=a[1],c=COMMON.by[r.country],L=c?c.tz.slice():[],h="",f=-1;
      if(!L.some(function(z){return z[0]=="UTC"}))L.push(["UTC","UTC0"]);
      L.forEach(function(z,i){if(z[1]==r.tz&&(f<0||z[0]==r.tzName))f=i});
      if(f<0){
        L.push([r.tzName,r.tz]);
        f=L.length-1
      }
      L.forEach(function(z,i){h+='<option value="'+i+'">'+esc(z[0])+'</option>'});
      OTZ=L;
      $("o_tz").innerHTML=h;
      $("o_tz").value=f
    }).catch(function(){})
}

function oraTzSave(){
  var z=OTZ[+$("o_tz").value];
  if(!z)return;
  post("/api/region",{tz:z[1],tzname:z[0]}).then(function(r){return r.json()}).then(function(j){
    msg("om4",j.ok===false?j.err:t("Salvato"),j.ok!==false);
    loadTime();
    oraTz()
  }).catch(function(){})
}

function loadTime(){
  if(!$("o_tz").options.length)oraTz();
  api("/api/time").then(function(r){return r.json()}).then(function(t){
    $("oranow").textContent=t.valid?t.now:window.t("Ora non impostata");
    svcSt("ntp",t.ntp);
    $("o_ev").value=t.every;
    var ld=new Date((t.last||0)*1000);
    $("olast").textContent=t.last?tf("Ultima sincronizzazione: {0}",ld.toLocaleString())
      :(t.ntp?window.t("Nessuna sincronizzazione ancora")
        :window.t("Sincronizzazione spenta: ora a mano"));
    if(t.valid&&!$("o_md").value){
      var e=new Date(t.epoch*1000);
      $("o_md").value=e.getFullYear()+"-"+("0"+(e.getMonth()+1)).slice(-2)+"-"+
        ("0"+e.getDate()).slice(-2);
      $("o_mt").value=("0"+e.getHours()).slice(-2)+":"+("0"+e.getMinutes()).slice(-2)+":00"
    }
    $("o_srv").value=t.server;
    $("o_srv2").value=t.server2||"";
    $("ontpwho").textContent=
      tf(
        "Ogni richiesta NTP mostra ai server l'indirizzo IP pubblico della tua rete. Server usati: {0}",
        t.server+(t.server2?", "+t.server2:""));
    if($("ontpst"))$("ontpst").textContent=t.ntp
      ?(t.last?tf("Ultima sincronizzazione: {0}",ld.toLocaleString())
        :window.t("Nessuna sincronizzazione ancora")):window.t(
          "Sincronizzazione spenta: ora a mano");
    $("o_serve").value=t.serve?"1":"0";
    $("ontpsv").innerHTML=row(esc(window.t("Questa scheda come server NTP per altri dispositivi")),
      esc(t.serve?window.t("Si (UDP 123)"):window.t("No")));
  })
}

function setManual(){
  var d=$("o_md").value,h=$("o_mt").value;
  if(!d||!h){
    msg("om3",t("Scegli data e ora"),false);
    return
  }
  if(h.length==5)h+=":00";
  post("/api/time/set",{local:d+" "+h}).then(function(r){return r.json()}).then(function(j){
    msg("om3",j.ok?t("Ora impostata"):j.err,j.ok);
    if(j.ok)loadTime()
  })
}

function saveTime(on,mid){
  if(on===undefined)on=!!(SVS.ntp&&SVS.ntp.on);
  return post("/api/time",
    {ntp:on?"1":"0",every:$("o_ev").value,server:$("o_srv").value,server2:$("o_srv2").value,
      serve:$("o_serve").value})
    .then(function(r){return r.json()}).then(function(j){
      msg(mid||"om",j.ok?t("Salvato"):j.err,j.ok);
      if(j.ok)setTimeout(loadTime,1500)
    })
}

function syncNow(){
  post("/api/time/sync",{}).then(function(r){return r.json()}).then(function(j){
    msg("om2",j.ok?t("Sincronizzazione richiesta: controllo l'ora tra pochi secondi"):j.err,j.ok);
    if(j.ok){
      setTimeout(loadTime,3000);
      setTimeout(loadTime,8000)
    }
  })
}

function setFromBrowser(){
  post("/api/time/set",{epoch:Math.floor(Date.now()/1000)}).then(function(r){return r.json()}).then(
    function(j){
      msg("om2",j.ok?t("Ora impostata"):j.err,j.ok);
      if(j.ok)loadTime()
    })
}
