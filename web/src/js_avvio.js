/* ===== Ordine di avvio ===== */
var BT={d:null,roots:[],ch:{}};
var BN=[["sys","Sistema e monitor"],["led","LED"],["net","Rete"],["time","Ora"],
  ["web","Pagina web e API"],["rules","Automazioni"],["mqtt","MQTT"],["mesh","Rete tra schede"],
  ["wd","Watchdog"]];
var BFIX=[["log","Registro (log)"],["pin","Pin e risorse"],["fs","File e configurazione"],
  ["lang","Lingua"]];
function bn(id){return lk(BN,id)}
function bLoad(){
  bpLoad();
  api("/api/boot").then(function(r){return r.json()}).then(function(d){
    BT.d=d;
    var pos={};
    d.order.forEach(function(x,i){pos[x]=i});
    BT.roots=[];
    BT.ch={};
    d.svc.forEach(function(x){
      if(!x.parent)BT.roots.push(x.id);
      else{
        (BT.ch[x.parent]=BT.ch[x.parent]||[]).push(x.id)
      }
    });
    BT.roots.sort(function(a,b){return pos[a]-pos[b]});
    for(var k in BT.ch)BT.ch[k].sort(function(a,b){return pos[a]-pos[b]});
    bDraw()
  }).catch(function(){})
}

function bSvc(id){return BT.d.svc.filter(function(x){return x.id==id})[0]}
function bRow(id,child){
  var x=bSvc(id),rq=x.req.length?tf("Richiede: {0}",x.req.map(function(q){return bn(q)}).join(", "))
    :t("Non richiede altri servizi");
  var tm=tf("partito a +{0} ms, ha impiegato {1} ms",x.at,x.ms);
  return'<div class="brow"><span class="grip" data-g="'+id+
    '"><svg class="i"><use href="#i-grip"/></svg></span><div class="bn"><b>'+esc(bn(id))+
    '</b><small>'+esc(rq)+'</small><small>'+esc(tm)+'</small></div>'+
    '<button class="bi" onclick="bMove(\''+id+'\',-1)" aria-label="'+esc(t("Su"))+
    '">&uarr;</button><button class="bi" onclick="bMove(\''+id+'\',1)" aria-label="'+esc(t("Giu"))+
    '">&darr;</button></div>'
}

function bNode(id,lv){
  var h='<div class="'+(lv?'bc':'bg')+'" data-id="'+id+'">'+bRow(id)+'<div class="bch">';
  (BT.ch[id]||[]).forEach(function(c){h+=bNode(c,lv+1)});
  return h+'</div></div>'
}

function bDraw(){
  var f="";
  BFIX.forEach(function(x){
    f+='<div class="bfix"><svg class="i"><use href="#i-lock"/></svg>'+esc(t(x[1]))+'</div>'
  });
  $("bfix").innerHTML=f;
  var h="";
  BT.roots.forEach(function(r){h+=bNode(r,0)});
  $("bl").innerHTML=h;
  var w=$("bwarn");
  if(BT.d.fell){
    w.className="msg ko";
    w.textContent=t("L'ordine scelto non ha funzionato: e stato ripristinato quello predefinito.")
  }else if(BT.d.custom){
    w.className="msg ok";
    w.textContent=t("Ordine personalizzato attivo.")
  }else{
    w.className="msg";
    w.textContent=""
  }
  bGrips()
}

function bFlat(){
  var o=[];
  function w(id){
    o.push(id);
    (BT.ch[id]||[]).forEach(w)
  }
  BT.roots.forEach(w);
  return o
}

function bRead(box,pid){
  var ids=[].filter.call(box.children,function(c){
    return c.hasAttribute("data-id")
  }).map(function(c){return c.getAttribute("data-id")});
  if(pid)BT.ch[pid]=ids;
  else BT.roots=ids;
  ids.forEach(function(id){bRead(box.querySelector('[data-id="'+id+'"]>.bch'),id)})
}

function bMove(id,d){
  var a=BT.roots.indexOf(id)>=0?BT.roots:BT.ch[bSvc(id).parent];
  var i=a.indexOf(id),j=i+d;
  if(j<0||j>=a.length)return;
  var x=a[i];
  a[i]=a[j];
  a[j]=x;
  bDraw()
}

function bGrips(){
  var gs=$("bl").querySelectorAll(".grip");
  for(var i=0;i<gs.length;i++)(function(g){
    g.addEventListener("pointerdown",function(e){
      var el=g.closest(".bc,.bg");
      el.classList.add("drag");
      e.preventDefault();
      function mv(ev){
        if(ev.clientY>innerHeight-70)scrollBy(0,18);
        else if(ev.clientY<130)scrollBy(0,-18);
        var par=el.parentNode,sib=[].filter.call(par.children,function(c){
          return c!==el&&c.hasAttribute("data-id")
        }),ins=null;
        for(var k=0;k<sib.length;k++){
          var r=sib[k].getBoundingClientRect();
          if(ev.clientY<r.top+r.height/2){
            ins=sib[k];
            break
          }
        }
        if(ins)par.insertBefore(el,ins);
        else par.appendChild(el)
      }
      function up(){
        document.removeEventListener("pointermove",mv);
        document.removeEventListener("pointerup",up);
        document.removeEventListener("pointercancel",up);
        el.classList.remove("drag");
        bRead($("bl"),null);
        bDraw()
      }
      document.addEventListener("pointermove",mv);
      document.addEventListener("pointerup",up);
      document.addEventListener("pointercancel",up)
    })
  })(gs[i])
}

function bSave(){
  post("/api/boot",{order:bFlat().join(",")}).then(function(r){return r.json()}).then(function(j){
    if(j.ok){
      msg("bm",t("Salvato: vale dal prossimo riavvio"),true);
      bLoad()
    }else msg("bm",j.err||t("Errore"),false)
  })
}

function bReset(){
  post("/api/boot/reset",{}).then(function(){
    msg("bm",t("Ordine predefinito (vale dal prossimo riavvio)"),true);
    bLoad()
  })
}

function bReboot(){
  if(!confirm(t("Riavviare la scheda?")))return;
  post("/api/shell",{c:"reboot"}).catch(function(){});
  msg("bm",t("Riavvio..."),true);
  setTimeout(function(){location.reload()},9000)
}
