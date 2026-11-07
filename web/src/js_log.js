/* ===== Registro e diario dei riavvii ===== */
var LG={txt:"",tm:null};
function loadLog(){
  api("/api/log").then(function(r){return r.text()}).then(function(x){
    LG.txt=x;
    lgDraw()
  }).catch(function(){})
}
var LGL={E:0,W:1,I:2,D:3},LGC=["lk","lw","",""];
function lgDraw(){
  var e=$("lg");
  if(!e)return;
  var q=($("lgq").value||"").toLowerCase(),mx=+$("lgsh").value,
    all=LG.txt.split("\n").filter(Boolean),L=all.filter(function(l){
      var m=l.match(/\[([EWID])\] /),lv=m?LGL[m[1]]:2;
      return lv<=mx&&(!q||l.toLowerCase().indexOf(q)>=0)
    });
  var stick=e.scrollTop+e.clientHeight>=e.scrollHeight-20;
  e.innerHTML=L.map(function(l){
    var m=l.match(/\[([EWID])\] /),c=m?LGC[LGL[m[1]]]:"";
    return c?'<span class="'+c+'">'+esc(l)+'</span>':esc(l)
  }).join("\n");
  if(stick)e.scrollTop=e.scrollHeight;
  $("lginfo").textContent=L.length!=all.length?tf("{0} righe su {1}",L.length,all.length)
    :tf("{0} righe",all.length)
}

function lgInit(){
  api("/api/log/level").then(function(r){return r.json()}).then(function(j){
    $("lglv").value=j.lv;
    $("lgdw").className=j.lv>=3?"fhint":"fhint hide"
  }).catch(function(){});
  $("lglv").disabled=ROLE<2;
  $("btclr").disabled=ROLE<2;
  loadBoots()
}

function lgSetLv(){
  var v=+$("lglv").value;
  post("/api/log/level",{lv:v}).then(function(r){return r.json()}).then(function(j){
    if(j.ok){
      $("lgdw").className=v>=3?"fhint":"fhint hide";
      loadLog()
    }
  })
}

function loadBoots(){
  api("/api/boots").then(function(r){return r.text()}).then(function(x){
    $("bt").textContent=x||t("Nessun riavvio registrato")
  }).catch(function(){})
}

function btClear(){
  dlg({title:t("Azzerare il diario dei riavvii?"),body:"",ok:t("Azzera il diario"),danger:true,
    onOk:function(){
      return post("/api/boots/clear",{}).then(function(r){return r.json()}).then(function(j){
        if(j.ok)loadBoots();
        return j.ok?"":j.err
      })
    }})
}

function lgAuto(){
  clearInterval(LG.tm);
  if($("lgauto").checked&&cur=="log")LG.tm=setInterval(loadLog,3000)
}

function lgDl(){
  var a=document.createElement("a");
  a.href=URL.createObjectURL(new Blob([LG.txt],{type:"text/plain"}));
  a.download="vesevos-log.txt";
  document.body.appendChild(a);
  a.click();
  setTimeout(function(){
    URL.revokeObjectURL(a.href);
    a.remove()
  },500)
}

function lgClear(){
  dlg({title:t("Svuotare il registro?"),
    body:'<p style="margin:0">'+
    esc(t(
      "Le righe vengono cancellate dalla memoria della scheda. Se ti servono, prima premi Scarica."))+
    '</p>',ok:t("Svuota"),danger:true,onOk:function(){
      return post("/api/log/clear",{}).then(function(r){return r.json()}).then(function(j){
        if(j.ok)loadLog();
        return j.ok?"":j.err
      })
    }})
}
