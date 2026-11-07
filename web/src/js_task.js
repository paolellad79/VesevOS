/* ===== Task ===== */
var TK={d:[],prev:null,col:"n",asc:true,tm:null,total:0};
var TKCOLS=[["n","Nome"],["t","Tipo"],["s","Stato"],["p","Priorita"],["stk","Stack libero"],
  ["cpu","CPU"]];
function loadTasks(){
  clearInterval(TK.tm);
  if(cur!="task")return;
  tkPoll();
  TK.tm=setInterval(function(){
    if(cur!="task"){
      clearInterval(TK.tm);
      TK.prev=null;
      return
    }
    tkPoll()
  },2000)
}

function tkPoll(){
  if(document.hidden)return;
  api("/api/tasklist").then(function(r){return r.json()}).then(function(j){
    if(!j.ok){
      $("tkinfo").textContent=t("(dettagli non disponibili in questa build)");
      $("tktab").innerHTML="";
      return
    }
    var dt=TK.prev?j.total-TK.prev.total:0,hasRt=j.total>0;
    j.tasks.forEach(function(x){
      x.cpu=-1;
      if(hasRt&&TK.prev&&dt>0&&TK.prev.m[x.id]!==undefined){
        x.cpu=Math.min(100,Math.max(0,100*(x.rt-TK.prev.m[x.id])/dt))
      }
    });
    var m={};
    j.tasks.forEach(function(x){m[x.id]=x.rt});
    TK.prev={total:j.total,m:m};
    TK.hasCpu=hasRt;
    TK.d=j.tasks;
    TK.total=j.total;
    tkDraw();
    if(typeof tkProgs=="function")tkProgs(j)
  }).catch(function(){})
}

function tkSort(c){
  if(TK.col==c)TK.asc=!TK.asc;
  else{
    TK.col=c;
    TK.asc=(c=="n"||c=="t"||c=="s")
  }
  tkDraw()
}

function tkState(s){
  return s==9?t("Fermato")
    :[t("In esecuzione"),t("Pronto"),t("In attesa"),t("Sospeso"),t("Eliminato"),"?"][s]||"?"
}

function tkType(x){return x.t==2?t("App"):x.t==1?t("VesevOS"):t("Sistema")}
function tkDraw(){
  var c=TK.col,k=TK.asc?1:-1,d=TK.d.slice();
  d.sort(function(a,b){
    var x=a[c],y=b[c];
    if(c=="n"){
      x=a.n.toLowerCase();
      y=b.n.toLowerCase()
    }
    return(x<y?-1:x>y?1:0)*k||(a.id-b.id)
  });
  var h="<tr>";
  TKCOLS.forEach(function(x){
    if(x[0]=="cpu"&&!TK.hasCpu)return;
    h+='<th onclick="tkSort(\''+x[0]+'\')">'+esc(t(x[1]))+
      (TK.col==x[0]?(TK.asc?" &#9650;":" &#9660;"):"")+"</th>"
  });
  h+="<th></th></tr>";
  d.forEach(function(x){
    h+="<tr><td>"+esc(x.n)+"</td><td><span class='pill "+(x.t==2?"a":x.t==1?"v":"")+"'>"+
      esc(tkType(x))+"</span></td><td>"+esc(tkState(x.s))+"</td><td>"+x.p+"</td><td>"+x.stk+
      " B</td>"+
      (TK.hasCpu?"<td>"+(x.cpu<0?"-"
        :"<span class='cbar'><i style='width:"+x.cpu+"%'></i></span>"+x.cpu.toFixed(0)+"%")+
        "</td>":"")+
      "<td class='tkb'>"+(x.k?"<span class='svi'>"+
        sibB("sm go","i-sgo",t("Avvia"),x.s!=9,0,x.s==9?"tkRestart('"+esc(x.n)+"')":"")+
        sibB("sm st","i-sst",t("Ferma"),x.s==9,0,x.s==9?"":"tkKill('"+esc(x.n)+"')")+
        sibB("sm rs","i-srs",t("Riavvia"),0,x.s==9,"tkRestart('"+esc(x.n)+"')")+
        "</span>":"<span class='lk' title='"+esc(t("Protetto"))+"'>&#128274;</span>")+"</td></tr>"
  });
  $("tktab").innerHTML=h;
  $("tkinfo").textContent=tf("{0} task. Clicca un titolo per ordinare.",d.length)
}

function tkKill(n){
  dlg({title:tf("Fermare il task {0}?",n),
    body:'<p style="margin:0">'+
    esc(t(
      "Alcune funzioni smettono di funzionare finche non lo riavvii (pulsante Avvia o comando start)."))+
    '</p>',ok:t("Ferma"),danger:true,
    onOk:function(){
      return post("/api/taskkill",{name:n}).then(function(r){return r.json()}).then(function(j){
        if(j.ok){
          msg("tkm",tf("Task '{0}' fermato",n),true);
          tkPoll()
        }
        return j.ok?"":j.err
      })
    }})
}

function tkRestart(n){
  post("/api/taskrestart",{name:n}).then(function(r){return r.json()}).then(function(j){
    msg("tkm",j.ok?tf("Task '{0}' avviato",n):j.err,j.ok);
    setTimeout(tkPoll,300)
  }).catch(function(){})
}
