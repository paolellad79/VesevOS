(function(){
  var of=window.fetch.bind(window),act=0,q=[],lim=2;
  function run(j){
    act++;
    var done=false,rel=function(){
      if(!done){
        done=true;
        act--;
        nx()
      }
    },tm=setTimeout(rel,15000);
    of(j.u,j.o).then(function(r){
      clearTimeout(tm);
      rel();
      j.res(r)
    },function(e){
      clearTimeout(tm);
      rel();
      j.rej(e)
    })
  }
  function nx(){while(act<lim&&q.length)run(q.shift())}
  /* Richieste in parallelo: 1 durante la guida iniziale (la scheda ha poche connessioni e poca RAM), 2 poi */
  window.fetchLimite=function(n){lim=n;nx()};
  window.fetch=function(u,o){
    return new Promise(function(res,rej){
      q.push({u:u,o:o,res:res,rej:rej});
      nx()
    })
  }
})();
var GRP=[["home","Home"],["dev","Dispositivi"],["rete","Rete"],["prog","Programmi"],
  ["sys","Sistema"],["sec","Sicurezza"],["info","Informazioni"]];
var GICO={home:"i-home",dev:"i-usb",rete:"i-wifi",prog:"i-prog",sys:"i-gear",sec:"i-shield",
  info:"i-info"};
/* UN solo punto per le estensioni dei file (come core/vos_ext.h): schema v + tipo (s script, a app, d driver) + linguaggio (b funzioni VesevOS, e ELF). Oggi script .vsb, app .vab, app ELF .vae, driver .vdb e .vde. I vecchi nomi (.vsc, .vap, .vel) si leggono ancora e non si scrivono piu */
var EXT={s:".vsb",b:".vab",e:".vae",db:".vdb",de:".vde"},EXTOLD={s:".vsc",b:".vap",e:".vel"};
/* Un file di programma: prima il nome nuovo, se non c'e il vecchio. fn(percorso) torna una promessa con {ok,...} */
function progTry(dir,n,k,fn){
  return fn(dir+n+EXT[k]).then(function(o){
    return o.ok?o:fn(dir+n+EXTOLD[k]).then(function(o2){return o2.ok?o2:o})
  })
}
/* Il file finisce per l'estensione nuova o per la vecchia di quel tipo? */
function extOk(name,k){
  var l=name.toLowerCase();
  return l.slice(-EXT[k].length)==EXT[k]||l.slice(-EXTOLD[k].length)==EXTOLD[k]
}
var SECN=["Stato","Periferiche","Interfacce","Diagnostica","Protocolli","Servizi"];
var LASTG={};
function gOf(id){
  for(var g in GMAP)if(GMAP[g].indexOf(id)>=0)return g;
  return"home"
}
var DICT={},LANG="it",LANGS=[{code:"it",name:"Italiano"}];
function t(s){
  var v=DICT[s];
  return v?v:s
}

function tf(s){
  var a=arguments;
  return t(s).replace(/\{(\d)\}/g,function(m,i){return a[+i+1]})
}
/* durata in secondi: "45 s", "12 min", "3 h 5 min", "5 g 23 h 11 min" */
function fmtDur(n){
  n=Math.max(0,Math.floor(+n||0));
  var d=Math.floor(n/86400),h=Math.floor(n%86400/3600),m=Math.floor(n%3600/60);
  return n<60?tf("{0} s",n):n<3600?tf("{0} min",m):n<86400?tf("{0} h",h)+" "+tf("{0} min",m)
    :tf("{0} g",d)+" "+tf("{0} h",h)+" "+tf("{0} min",m)
}

function applyStatic(){
  var w=document.createTreeWalker(document.body,NodeFilter.SHOW_TEXT),n,list=[];
  while((n=w.nextNode()))list.push(n);
  list.forEach(function(n){
    var p=n.parentNode;
    if(p&&(p.tagName=="SCRIPT"||p.tagName=="STYLE"))return;
    if(p&&p.closest&&p.closest("[translate=no]"))return;   /* ora e messaggio dell'accesso: mai tradotti */
    if(n.__o===undefined)n.__o=n.nodeValue;
    var k=n.__o.trim().replace(/\s+/g," ");
    if(!k)return;
    var m=n.__o.match(/^(\s*)[\s\S]*?(\s*)$/);
    n.nodeValue=(LANG!="it"&&DICT[k])?m[1]+DICT[k]+m[2]:n.__o
  });
  var els=document.querySelectorAll("[placeholder]");
  for(var i=0;i<els.length;i++){
    var e=els[i];
    if(e.__ph===undefined)e.__ph=e.getAttribute("placeholder");
    e.setAttribute("placeholder",(LANG!="it"&&DICT[e.__ph])?DICT[e.__ph]:e.__ph)
  }
    ["title","aria-label"].forEach(function(at){
      var L=document.querySelectorAll("[data-tt]["+at+"]");
      for(var i=0;i<L.length;i++){
        var e=L[i],k="__"+at;
        if(e[k]===undefined)e[k]=e.getAttribute(at);
        e.setAttribute(at,(LANG!="it"&&DICT[e[k]])?DICT[e[k]]:e[k])
      }
    });
  document.documentElement.lang=LANG;
  a11yLabels()
}

function loadDict(code){
  if(code=="it"){
    DICT={};
    return Promise.resolve()
  }
  /* il .vlp e gzip: la scheda lo serve con Content-Encoding gzip e il browser lo apre da solo. Se manca (404) o e rovinato: italiano */
  return fetch("/api/langfile?code="+encodeURIComponent(code)).then(function(r){
    if(!r.ok)throw 0;
    return r.json()
  }).then(function(d){DICT=d}).catch(function(){
    DICT={};
    LANG="it"
  })
}

function loggedIn(){return !$("app").classList.contains("hide")}
function refreshView(){
  fillLangSel();
  applyStatic();
  if(loggedIn()){
    if(PAGINA){
      buildNav();
      if(ROLE>=1)dvList().catch(function(){});
      poll();
      if(cur=="loc")loadLangs();
      if(cur=="log")loadLog()
    }
  }else showLoginText()
}

function setLang(code,noSave){
  LANG=code;
  try{localStorage.setItem("vl",code)}catch(e){}
  loadDict(code).then(function(){
    refreshView();
    if(!noSave&&loggedIn())post("/api/lang",{code:LANG}).catch(function(){})
  })
}

function initLang(dev){
  var c=dev||"it";
  try{c=localStorage.getItem("vl")||c}catch(e){}
  fetch("/api/langs").then(function(r){return r.json()}).then(function(l){
    LANGS=l;
    if(!l.some(function(x){return x.code==c}))c="it";
    setLang(c,true)
  }).catch(function(){setLang("it",true)})
}
var cur="sum",S=null,timer=null;
var CP={o:false,h:280};
try{
  CP.h=+localStorage.getItem("vph")||280;
  CP.o=localStorage.getItem("vpo")=="1"
}catch(e){}

function cpApply(){
  var mx=Math.max(140,innerHeight-130);
  CP.h=Math.max(120,Math.min(mx,CP.h));
  document.documentElement.style.setProperty("--ph",CP.h+"px");
  document.body.classList.toggle("cpo",CP.o&&loggedIn());
  try{
    localStorage.setItem("vph",CP.h);
    localStorage.setItem("vpo",CP.o?"1":"0")
  }catch(e){}
  var o=$("tbt");
  if(o)o.style.color=CP.o?"var(--ac)":""
}

function cpToggle(f){
  CP.o=f===undefined?!CP.o:f;
  cpApply();
  if(CP.o){
    shLoadCmds();
    var i=$("si");
    if(i)i.focus()
  }
}

function cpSize(k){
  CP.h=k==0?170:k==1?Math.round(innerHeight*.45):innerHeight-130;
  if(!CP.o)CP.o=true;
  cpApply()
}

function cpTitles(){
  var m={tbt:"Terminale",cps0:"Piccolo",cps1:"Medio",cps2:"Quasi a tutto schermo",cpx:"Chiudi"};
  for(var k in m){
    var e=$(k);
    if(e){
      e.title=t(m[k]);
      e.setAttribute("aria-label",t(m[k]))
    }
  }
}
  (function(){
    var h=$("cph"),y0=0,h0=0,dr=false;
    h.addEventListener("pointerdown",function(e){
      if(e.target.closest("button"))return;
      dr=true;
      y0=e.clientY;
      h0=CP.h;
      try{h.setPointerCapture(e.pointerId)}catch(x){}
    });
    h.addEventListener("pointermove",function(e){
      if(!dr)return;
      CP.h=h0+(y0-e.clientY);
      cpApply()
    });
    function up(){dr=false}
    h.addEventListener("pointerup",up);
    h.addEventListener("pointercancel",up);
    window.addEventListener("resize",cpApply)
  })();

function $(i){return document.getElementById(i)}
function api(u,o){
  return fetch(u,o).then(function(r){
    connOk();
    if(r.status==401){
      showLogin();
      throw 0
    }
    if(r.status==403&&location.protocol=="http:")return r.clone().json().then(function(j){
      if(j.err=="https"){
        location.href="https://"+location.hostname+"/";
        throw 0
      }
      return r
    },function(){return r});
    return r
  },function(e){
    connFail();
    throw e
  })
}

function post(u,d){
  if(/^\/api\/(reboot|sleep|factory|recovery|updorder|aggiorna\/installa)/.test(u))CONN.hold=
    Date.now()+120000;
  var b=new URLSearchParams(d).toString();
  return api(u,{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:b})
}

function msg(id,t,ok){
  var e=$(id);
  clearTimeout(e._tm);
  e.setAttribute("role",ok?"status":"alert");
  e.className="msg";
  void e.offsetWidth;
  e.textContent=t;
  e.className="msg "+(ok?"ok":"ko");
  if(ok)e._tm=setTimeout(function(){
    e.classList.add("out");
    e._tm=setTimeout(function(){
      e.className="msg";
      e.textContent=""
    },400)
  },3000)
}
// Esito di un caricamento: un avviso del firmware (per esempio una versione piu vecchia) si vede al posto del testo di riuscita.
function msgCaricato(id,j,fatto){msg(id,j.ok?(j.avviso||fatto):j.err,!!j.ok&&!j.avviso)}

/* ===== scheda non raggiungibile: pagina in grigio con finestra e pulsante Aggiorna ===== */
var CONN={bad:0,off:false,back:false,hold:0};
function connTxt(back){
  CONN.back=back;
  $("cnt").textContent=back?t("Scheda di nuovo raggiungibile"):t("Scheda non raggiungibile");
  $("cnd").textContent=back
    ?t("La scheda risponde di nuovo: premi Aggiorna per ricaricare la pagina.")
    :t(
      "La pagina non riesce piu a parlare con la scheda: puo essere spenta, riavviata o fuori dalla rete. Controlla l'alimentazione e il Wi-Fi, poi premi Aggiorna.")
}

function connOk(){
  CONN.bad=0;
  if(CONN.off&&!CONN.back)connTxt(true)
}

function connFail(){
  if(Date.now()<CONN.hold)return;
  if(++CONN.bad<2)return;
  if(!CONN.off){
    CONN.off=true;
    document.body.classList.add("cx");
    $("conn").className="dlgw";
    connTxt(false);
    setTimeout(function(){$("cnb").focus()},30)
  }
  else if(CONN.back)connTxt(false)
}
setInterval(function(){
  if(document.hidden||Date.now()<CONN.hold)return;
  fetch("/api/auth",{cache:"no-store"}).then(connOk,connFail)
},5000);
window.addEventListener("offline",function(){
  CONN.bad=1;
  connFail()
});
