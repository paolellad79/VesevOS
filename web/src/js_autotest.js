/* ===== Autotest ===== */
var SL={tm:null,seq:0,run:false};
function stlGo(){
  $("stl_go").disabled=true;
  post("/api/selftest",{active:$("stl_act").classList.contains("on")?1:0,
    names:$("stl_nm").classList.contains("on")?1
    :0}).then(function(r){return r.json()}).then(function(j){
      if(j.ok===false){
        msg("stlm",j.err,false);
        $("stl_go").disabled=false;
        return
      }
      $("stlm").className="msg";
      SL.run=true;
      stlPoll()
    }).catch(function(){$("stl_go").disabled=false})
}

function stlPoll(){
  clearTimeout(SL.tm);
  var id=++SL.seq;
  api("/api/selftest").then(function(r){return r.json()}).then(function(j){
    if(id!=SL.seq)return;
    stlDraw(j)
  }).catch(function(){
    if(id!=SL.seq||cur!="selftest")return;
    if(SL.run){
      $("stl_sum").textContent=t("La scheda non risponde, riprovo...");
      SL.tm=setTimeout(stlPoll,1500)
    }
  })
}
var STLW=["Saltata","OK","Avviso","Errore","Saltata"];
var STLC=["","a","w","k",""];
function stlDraw(j){
  if(cur!="selftest")return;
  var run=j.state==1,fin=j.state==2,c=[0,0,0,0,0];
  SL.run=run;
  $("stl_go").disabled=run;
  $("stl_dl").className=fin?"btn gray":"btn gray hide";
  $("stl_cp").className=fin?"btn gray":"btn gray hide";
  $("stl_cl").className=j.state?"btn gray":"btn gray hide";
  $("stl_bar").className=run?"":"hide";
  if(run)$("stl_bar").innerHTML=bar(Math.round(100*j.list.length/j.total));
  var h="";
  j.list.forEach(function(x){
    c[x.s]++;
    h+='<div class="row" style="align-items:flex-start;gap:10px"><span class="pill '+STLC[x.s]+
      '" style="min-width:64px;text-align:center">'+esc(t(STLW[x.s]))+
      '</span><span style="flex:1"><b>'+esc(x.n)+'</b> <span class="fhint">('+x.ms+
      ' ms)</span><br><span>'+esc(x.d)+'</span></span></div>'
  });
  $("stl_list").innerHTML=h;
  $("stl_sum").textContent=run?tf("In corso: prova {0} di {1}",j.cur+1,j.total):fin
    ?tf("Finita: {0} OK, {1} avvisi, {2} errori, {3} saltate",c[1],c[2],c[3],c[4]):"";
  if(run)SL.tm=setTimeout(stlPoll,700)
}

function stlCopy(){
  api("/api/selftest/report").then(function(r){return r.text()}).then(function(x){
    var ok=function(){msg("stlm",t("Report copiato"),true)};
    if(navigator.clipboard&&
      navigator.clipboard.writeText)navigator.clipboard.writeText(x).then(ok).catch(function(){});
    else{
      var a=document.createElement("textarea");
      a.value=x;
      document.body.appendChild(a);
      a.select();
      try{
        document.execCommand("copy");
        ok()
      }catch(e){}
      document.body.removeChild(a)
    }
  })
}

function stlClear(){post("/api/selftest",{clear:1}).then(function(){stlPoll()})}
