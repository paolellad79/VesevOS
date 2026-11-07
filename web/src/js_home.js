/* ===== Home: aggiornamento dei dati, avvisi, schede e widget ===== */
var PBUSY=0;
document.addEventListener("visibilitychange",function(){
  if(!document.hidden&&typeof loggedIn=="function"&&loggedIn())poll()
});
var RF=3;
try{
  var q=parseInt(localStorage.getItem("vosRf"));
  if([0,1,3,5,10,30].indexOf(q)>=0)RF=q
}catch(e){}

function rfApply(){
  clearInterval(timer);
  timer=null;
  if($("rf"))$("rf").value=String(RF);
  if(RF>0)timer=setInterval(poll,RF*1000)
}

function rfSet(v){
  RF=parseInt(v);
  try{localStorage.setItem("vosRf",String(RF))}catch(e){}
  rfApply();
  if(RF>0)poll()
}

function poll(){
  if(PBUSY&&Date.now()-PBUSY<8000)return;
  if(document.hidden)return;
  PBUSY=Date.now();
  api("/api/status").then(function(r){return r.json()}).then(function(s){
    S=s;
    if($("fver"))$("fver").textContent="VesevOS "+s.version+" \u00b7 (C) 2026 Domenico Paolella";
    hdTick();
    hwFetch();
    updCheckNew();
    $("bchip").textContent=s.net.host+" · "+boardId(s);
    var hp=Math.round(100-100*s.heapFree/s.heapTotal),
      pp=s.psramTotal?Math.round(100-100*s.psramFree/s.psramTotal):0,
      fp=s.flashTotal?Math.round(100*s.flashUsed/s.flashTotal):0;
    cpuUi(s);
    alertsCalc(s,hp,pp,fp);
    if(cur=="sum"){homeDraw()}
    if(cur=="mem")memDraw();
    PBUSY=0
  }).catch(function(){PBUSY=0})
}

function boardId(s){
  var m=(s.net.mac||"").split(":");
  return m.length==6?(m[4]+m[5]).toUpperCase():"----"
}

function hcol(p,w,k){return p>=k?"var(--ko)":p>=w?"var(--wa)":"var(--ok)"}
function hring(l,txt,p,c,icon,go,sub,ex){
  var R=38,C=2*Math.PI*R;
  var apri=canSee(go)?' onclick="'+"show('"+go+"')"+'"':' style="cursor:default"';
  return'<div class="ring"'+apri+'><svg viewBox="0 0 100 100"><circle cx="50" cy="50" r="'+R+
    '" fill="none" stroke="var(--trk)" stroke-width="9"/><circle cx="50" cy="50" r="'+R+
    '" fill="none" stroke="'+c+'" stroke-width="9" stroke-linecap="round" stroke-dasharray="'+
    (C*Math.max(0,Math.min(100,p))/100)+' '+C+
    '" transform="rotate(-90 50 50)"/><g fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" style="color:var(--mut)"><use href="#'+
    icon+'" x="35" y="35" width="30" height="30"/></g></svg><div class="v">'+txt+
    '</div><div class="l">'+esc(l)+'</div>'+(sub?'<div class="l">'+sub+'</div>':'')+
    (ex?'<div class="rx">'+ex+'</div>':'')+'</div>'
}

function hbar(l,v,p,w,k){
  var c=p>=k?" k":p>=w?" w":"";
  return'<div class="pb'+c+'"><div class="t"><span>'+esc(l)+'</span><span>'+v+
    '</span></div><div class="tr"><i style="width:'+Math.min(100,p)+'%"></i></div></div>'
}

function harea(a,mn,mx,color,id){
  var w=600,h=120,n=a.length;
  if(n<2)return"";
  if(mn===null){
    mn=Math.min.apply(null,a)-2;
    mx=Math.max.apply(null,a)+2
  }
  var pts=a.map(function(v,i){return[i*w/59,h-6-((v-mn)/(mx-mn))*(h-14)]}),l=pts.map(function(q,i){
    return(i?"L":"M")+q[0].toFixed(1)+" "+q[1].toFixed(1)
  }).join(" "),g="";
  for(var i=1;i<4;i++)g+='<line x1="0" x2="'+w+'" y1="'+i*h/4+'" y2="'+i*h/4+
    '" stroke="var(--bd)"/>';
  return'<svg viewBox="0 0 '+w+' '+h+
    '" preserveAspectRatio="none" style="width:100%;height:110px;display:block"><defs><linearGradient id="'+
    id+'" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="'+color+
    '" stop-opacity=".35"/><stop offset="1" stop-color="'+color+
    '" stop-opacity="0"/></linearGradient></defs>'+g+'<path d="'+l+" L"+(pts[n-1][0]).toFixed(1)+
    " "+h+" L0 "+h+'Z" fill="url(#'+id+')"/><path d="'+l+'" fill="none" stroke="'+color+
    '" stroke-width="2.5" vector-effect="non-scaling-stroke" stroke-linejoin="round"/></svg>'
}
var AL=[];
function alertsCalc(s,hp,pp,fp){
  AL=[];
  var n=s.net;
  if(hp===undefined){
    hp=Math.round(100-100*s.heapFree/s.heapTotal);
    fp=s.flashTotal?Math.round(100*s.flashUsed/s.flashTotal):0
  }
    (typeof AU!="undefined"?AU.al:[]).forEach(function(a){
      if(!a.ack)AL.push([a.lv==2?"k":"w",a.text,1])
    });
  agNota(s.agg);
  evNota(s.evb);
  if(s.hot)AL.push(["k",t("Temperatura CPU troppo alta")]);
  if(hp>=90)AL.push(["k",t("RAM quasi esaurita")]);
  else if(hp>=80)AL.push(["w",t("RAM molto usata")]);
  if(fp>=90)AL.push(["w",t("Spazio file quasi pieno")]);
  if(n.st==3&&n.rssi<-75)AL.push(["w",t("Segnale Wi-Fi debole")]);
  if(n.st==1&&n.staFail)AL.push(["w",
    t(
      "La scheda non e riuscita a collegarsi alla Wi-Fi di casa: controlla nome e password in Rete > Wi-Fi.")]);
  if(n.st==1)AL.push(["i",t("Modo punto di accesso: la scheda non e collegata a una rete")]);
  if(n.st==2)AL.push(["w",t("Collegamento Wi-Fi in corso")]);
  var b=$("bdg"),k=AL.filter(function(x){return x[0]!="i"}).length;
  b.textContent=AL.length;
  b.className="bdg"+(AL.some(function(x){return x[0]=="k"})?" k":"")+(AL.length?"":" hide");
  void k;
  if(!$("alerts").classList.contains("hide"))bellDraw()
}

function bellDraw(){
  var h="";
  if(!AL.length)h="<div style='color:var(--mut)'>"+esc(t("Nessun allarme"))+"</div>";
  AL.forEach(function(a){
    h+="<div"+(a[2]?" style='cursor:pointer' onclick=\"show('"+(a[2]===1?"audit":a[2])+"')\"":"")+
      "><span class='dot' style='background:"+
      (a[0]=="k"?"var(--ko)":a[0]=="w"?"var(--wa)":"var(--ac)")+"'></span>"+esc(a[1])+"</div>"
  });
  $("alerts").innerHTML=h
}

function bellTog(){
  var e=$("alerts");
  if(e.classList.contains("hide")){
    bellDraw();
    e.className="alerts"
  }else e.className="alerts hide"
}

function hlines(L,mn,mx,px){
  var w=600,h=100,g="";
  for(var i=1;i<4;i++)g+='<line x1="0" x2="'+w+'" y1="'+i*h/4+'" y2="'+i*h/4+
    '" stroke="var(--bd)"/>';
  var p=L.map(function(q){
    var a=q[0]||[];
    if(a.length<2)return"";
    var d=a.map(function(v,i){
      return(i?"L":"M")+(i*w/59).toFixed(1)+" "+(h-4-((v-mn)/(mx-mn))*(h-8)).toFixed(1)
    }).join(" ");
    return'<path d="'+d+'" fill="none" stroke="'+q[1]+
      '" stroke-width="2" vector-effect="non-scaling-stroke"/>'
  }).join("");
  return'<svg viewBox="0 0 '+w+' '+h+'" preserveAspectRatio="none" style="width:100%;height:'+
    (px||90)+'px;display:block">'+g+p+'</svg>'
}

function hleg(L){
  return'<div class="leg">'+L.map(function(q){
    return'<span><i style="background:'+q[1]+'"></i>'+esc(q[0])+' '+q[2]+'</span>'
  }).join("")+'</div>'
}

function bps(v){return v>=1024?(v/1024).toFixed(1)+' KB/s':v+' B/s'}
function memTx(u,t0){return kb(u)+' / '+kb(t0)+'<br>'+kb(t0-u)+' '+esc(t("liberi"))}
function mkb(v){return typeof v=="number"?kb(v):t("non misurato")}
function memCard(s){
  var m=s.mem;
  if(!m||!m.sram)return"";
  var pc=function(v){
    return Math.max(0,Math.min(100,100*(v||0)/m.sram)).toFixed(1)
  },hf=s.heapFree,hu=m.heapUsed,
    sub=function(l,v){
      return'<div class="row msub"><span>'+esc(l)+'</span><span>'+mkb(v)+'</span></div>'
    },
    head=function(l){return'<div class="subt mhd"><b>'+esc(l)+'</b></div>'};
  var bar='<div class="stk" role="img" aria-label="'+
    esc(tf("RAM {0}: parte fissa {1}, heap usato {2}, heap libero {3}",mkb(m.sram),mkb(m.fixed),
      mkb(hu),mkb(hf)))+'"><i style="width:'+pc(m.fixed)+
    '%;background:var(--mut)"></i><i style="width:'+pc(hu)+
    '%;background:var(--ac)"></i><i style="width:'+pc(hf)+'%;background:var(--ok)"></i></div>';
  return'<h3>'+esc(t("Memoria RAM"))+'</h3>'+bar+
    hleg([[t("Parte fissa di sistema"),"var(--mut)",mkb(m.fixed)],[t("Usato"),"var(--ac)",mkb(hu)],
      [t("Libero"),"var(--ok)",mkb(hf)]])+
    head(t("Totale (SRAM del chip)"))+row(t("Totale"),mkb(m.sram))+
    head(t("Parte fissa di sistema"))+row(t("Totale"),mkb(m.fixed))+sub(t("Codice in IRAM"),m.iram)+
    sub(t("Dati statici"),m.data)+sub(t("Dati azzerati"),m.bss)+
    sub(t("Resto (cache e ROM)"),m.rest)+
    head(t("Parte variabile (heap)"))+row(t("Totale"),mkb(s.heapTotal))+row(t("Usato"),mkb(hu))+
    row(t("Libero"),mkb(hf))+row(t("Minimo storico"),mkb(m.heapMin))+
    row(t("Blocco piu grande"),mkb(m.heapMaxBlock))
}

function homeDraw(){
  var s=S;
  if(!s||!$("home"))return;
  var n=s.net,hp=Math.round(100-100*s.heapFree/s.heapTotal),
    pp=s.psramTotal?Math.round(100-100*s.psramFree/s.psramTotal):0,
    fp=s.flashTotal?Math.round(100*s.flashUsed/s.flashTotal):0;
  var wp=n.st==3?2*(n.rssi+100):100,
    wt=n.st==3?n.rssi+" dBm":(n.st==1?""+(n.clients||0):n.st==4?t("Aereo"):"..."),
    wc=n.st==3?(n.rssi>-67?"var(--ok)":n.rssi>-75?"var(--wa)":"var(--ko)"):"var(--ac)",
    ws=esc(n.mode)+(n.st==1?" · "+esc(tf("{0} client",n.clients||0)):"")+(n.ip?"<br>"+esc(n.ip):"");
  var rv=s.rev,revs=Math.floor(rv/100)+"."+rv%100,mhz=s.cpuMhz;
  var th=(s.tempHist||[]).map(function(v){return s.tempUnit?v*9/5+32:v});
  var cx=hlines([[s.cpu0Hist,"#3fa7ff"],[s.cpu1Hist,"#b07cff"]],0,100,34)+
    hleg([["C0","#3fa7ff",s.cpu0+"%"],["C1","#b07cff",s.cpu1+"%"]])+
    hlines([[th,"#ff9f43"]],th.length?Math.min.apply(null,th)-2:0,
      th.length?Math.max.apply(null,th)+2:1,26)+
    hleg([[t("Temp."),"#ff9f43",tmp(s.temp,s.tempUnit)]]);
  var ih=s.inHist||[],oh=s.outHist||[],mx=Math.max(1024,Math.max.apply(null,ih.concat(oh,[0]))),
    wx=hlines([[ih,"#3fa7ff"],[oh,"#ff9f43"]],0,mx,34)+
    hleg([["In","#3fa7ff",bps(ih.length?ih[ih.length-1]:0)],
      ["Out","#ff9f43",bps(oh.length?oh[oh.length-1]:0)]]);
  var h='<section class="rings">'+
    hring("CPU",s.cpu+"%",s.cpu,hcol(s.cpu,60,85),"i-chipn","cpu",mhz+" MHz",cx)+
    hring("RAM",hp+"%",hp,hcol(hp,80,92),"i-mem","mem",memTx(s.heapTotal-s.heapFree,s.heapTotal),
      hlines([[s.ramHist,"#3fa7ff"]],0,100,34))+
    hring("PSRAM",pp+"%",pp,hcol(pp,80,92),"i-mem","mem",
      memTx(s.psramTotal-s.psramFree,s.psramTotal),hlines([[s.psHist,"#2ecc71"]],0,100,34))+
    hring(t("File"),fp+"%",fp,hcol(fp,75,90),"i-disk","file",memTx(s.flashUsed,s.flashTotal),
      hlines([[s.flHist,"#ffb020"]],0,100,34))+hring(t("Wi-Fi"),wt,wp,wc,"i-wifi","wifi",ws,wx)+
    '</section>';
  h+=homeWid(s,n);
  h+='<section class="card hid"><h3>'+esc(t("Sistema"))+'</h3><div class="big">'+esc(n.host)+
    '</div><div class="subt">'+esc(s.chip)+" · ID "+boardId(s)+
    '</div><div style="height:8px"></div>'+
    row(t("Chip"),esc(s.chip)+" rev "+revs)+
    row(t("Processore"),tf("{0} core a {1} MHz",s.cores,mhz))+
    row(t("Memoria"),tf("{0} MB flash · {1} MB PSRAM",Math.round(s.flashChip/1048576),
      Math.round(s.psramTotal/1048576)))+
    row(t("Acceso da"),esc(s.uptime))+row(t("Ore di vita"),fmtDur(s.lifeSec))+
    row(t("Avvii totali"),s.boots)+row(t("Ultimo reset"),esc(s.reset))+
    row(t("Versione"),esc(s.name+" "+s.version))+
    '<button class="btn" style="margin-top:10px" onclick="identify()">'+
    esc(t("Trova questa scheda"))+'</button><div id="idm" class="msg"></div></section>';
  $("home").innerHTML=h
}
var HW=null,HWT=0,HWU=null;
function hdTick(){
  var e=$("hd");
  if(!e)return;
  if(HW&&HW.tv){
    var c=hClock();
    e.textContent=c[0]+" · "+c[1]
  }else e.textContent=""
}

function hwUsers(){
  api("/api/svc").then(function(r){return r.json()}).then(function(j){
    if(typeof j.usersHttp!="number"||typeof j.usersHttps!="number")return;
    var n=j.usersHttp+j.usersHttps;
    if(n!==HWU){
      HWU=n;
      if(cur=="sum")homeDraw()
    }
  }).catch(function(){})
}

function hwFetch(){
  if(!document.hidden&&Date.now()-HWT>8000){
    HWT=Date.now();
    hwUsers();
    api("/api/home").then(function(r){return r.json()}).then(function(j){
      j.t0=Date.now();
      HW=j;
      hdTick();
      if(cur=="sum")homeDraw()
    }).catch(function(){})
  }
}

function hClock(){
  var A=HW;
  if(!A||!A.tv)return["--:--:--",t("Ora non impostata")];
  var p=function(x){return(x<10?"0":"")+x},ms=(A.te+(Date.now()-A.t0)/1000)*1000,
    d=new Date(ms+A.to*1000),H=d.getUTCHours(),ap="";
  if(A.tf){
    ap=H>=12?" PM":" AM";
    H=H%12||12
  }
  var Y=d.getUTCFullYear(),M=p(d.getUTCMonth()+1),D=p(d.getUTCDate());
  return[p(H)+":"+p(d.getUTCMinutes())+":"+p(d.getUTCSeconds())+ap,
    A.df==1?Y+"-"+M+"-"+D:A.df==2?M+"/"+D+"/"+Y:D+"/"+M+"/"+Y]
}
setInterval(hdTick,1000);
function wid(tt,v,sub,go,ic){
  return'<div class="wid" role="button" tabindex="0" onclick="show(\''+go+
    '\')" onkeydown="if(event.key==\'Enter\')show(\''+go+
    '\')"><div class="wt"><svg class="i" aria-hidden="true"><use href="#'+ic+'"/></svg>'+esc(tt)+
    '</div><div class="wv">'+v+'</div>'+(sub?'<div class="ws">'+sub+'</div>':"")+'</div>'
}

function widCol(n){
  var many=n>1,l=tf("Collegati: {0}",n),adm=ROLE==2,
    sub=many?'<svg class="i" aria-hidden="true"><use href="#i-bell"/></svg>'+
    esc(t("Attenzione: piu di un collegato")):esc(t("Sessioni web attive negli ultimi 2 minuti"));
  return'<div class="wid wcol'+(many?" warn":"")+'" '+
    (adm
      ?'role="button" tabindex="0" onclick="whoShow()" onkeydown="if(event.key==\'Enter\'||event.key==\' \')whoShow()" aria-label="'+
      esc(l+". "+t("Chi e collegato"))+'"':'role="group" aria-label="'+esc(l)+'"')+
    '><div class="wt"><svg class="i" aria-hidden="true"><use href="#i-shield"/></svg>'+
    esc(t("Collegati"))+'</div><div class="wv wbig">'+n+'</div><div class="ws">'+sub+'</div></div>'
}

function homeWid(s,n){
  hwFetch();
  if(!SV.httpPort)svcLoad();
  var W=HW||{},pill=function(on,a,b){
    return'<span class="pill'+(on?" a":"")+'">'+esc(on?a:b)+'</span>'
  },c=[],now=Math.floor(Date.now()/1000);
  if(HWU!==null)c.push(widCol(HWU));
  if(SV.httpOn)c.push(wid("HTTP",esc(tf("Porta: {0}",SV.httpPort)),"","http","i-globe"));
  if(SV.httpsOn)c.push(wid("HTTPS",esc(tf("Porta: {0}",SV.httpsPort)),"","https","i-lock"));
  if(s.mqtt&&canSee("mqtt"))c.push(wid("MQTT",pill(s.mqtt==2,t("collegato"),t("non collegato")),"",
    "mqtt","i-mqtt"));
  if(W.mesh&&canSee("mesh"))c.push(wid("ESP-NOW",esc(t(MROLE[W.role]||"Nodo")),
    esc(t("Schede vicine"))+": "+W.nodes,"mesh","i-mesh"));
  if(n.mdnsRun)c.push(wid("mDNS",esc(n.fqdn),"","mdns","i-tag"));
  if(W.ntp)c.push(wid("NTP",
    W.last?esc(tf("{0} fa",durText(Math.max(1,now-W.last))))
    :esc(t("Nessuna sincronizzazione ancora")),"","ntp","i-clock"));
  if(n.dhcpRun)c.push(wid("DHCP",pill(1,t("Attivo"),""),
    esc(t("Dispositivi collegati"))+": "+(n.clients||0),"dhcp","i-net"));
  if(n.st==1)c.push(wid(t("Punto di accesso (AP)"),(n.clients||0)+"",
    esc(t("Dispositivi collegati")),"ap","i-ant"));
  if(W.pw)c.push(wid(t("Risparmio energia"),esc(W.pw==1?t("Risparmio Wi-Fi"):t("Sonno a cicli")),"",
    "power","i-leaf"));
  if(W.w)W.w.forEach(function(x){c.push(widDrv(x))});
  if(W.stat)c.push(wid(t("Statistiche"),pill(1,t("Attivo"),""),"","stat","i-chart"));
  return c.length?'<div class="wg">'+c.join("")+'</div>':""
}

function identify(){
  post("/api/identify",{}).then(function(r){return r.json()}).then(function(j){
    msg("idm",j.ok?t("Guarda il LED: lampeggia a colori per 10 secondi"):j.err,j.ok);
    setTimeout(function(){
      var e=$("idm");
      if(e)e.className="msg"
    },5000)
  }).catch(function(){})
}
