/* ===== Controlli della configurazione (allarmi) ===== */
var AU={al:[],tm:null};
function auPoll(){
  clearInterval(AU.tm);
  var go=function(){
    if(!loggedIn()||document.hidden||ROLE<1)return;
    api("/api/audit").then(function(r){return r.json()}).then(function(a){
      AU.al=a.alarms;
      AU.hist=a.history;
      if(S)alertsCalc(S);
      if(cur=="audit"||(cur=="log"&&ROLE>=1))auDraw()
    }).catch(function(){})
  };
  go();
  AU.tm=setInterval(go,15000)
}

function auLoad(){
  api("/api/audit").then(function(r){return r.json()}).then(function(a){
    AU.al=a.alarms;
    AU.hist=a.history;
    auDraw()
  }).catch(function(){})
}

function auDraw(){
  var h="";
  if(!AU.al.length)h='<p style="color:var(--okt)">'+
    esc(t("Nessun allarme: configurazione in regola"))+'</p>';
  AU.al.forEach(function(a){
    h+='<div class="alv"><span class="dot" style="background:'+(a.lv==2?"var(--ko)":"var(--wa)")+
      '"></span><div style="flex:1">'+esc(a.text)+
      (a.ack?' <span class="pill">'+esc(t("visto"))+'</span>':"")+'</div>'+
      (a.ack?"":'<button class="btn gray sm op" onclick="auAck('+a.n+')">'+esc(t("Visto"))+
        '</button>')+'</div>'
  });
  $("aulist").innerHTML=h;
  $("auhist").innerHTML=(AU.hist||[]).map(function(x){return esc(x)}).join("<br>")||
    esc(t("Nessuna modifica registrata"))
}

function auAck(n){
  post("/api/audit/ack",{n:n}).then(function(r){return r.json()}).then(function(j){
    msg("aum",j.ok?t("Fatto"):j.err,j.ok);
    auLoad()
  })
}
