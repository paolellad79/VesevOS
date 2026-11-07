/* ===== Terminale ===== */
var SH={h:[],i:0,q:[],busy:false,loaded:false,p:"vesevos> ",tabBusy:false,tabT:0,tabLine:null,
  ask:null};
/* Righe con una password dentro (vecchia sintassi: passwd utente pwd, user add nome ruolo pwd, wifi set rete pwd): il terminale le mostra con ****
   e NON le salva in cronologia (localStorage). Stessa cosa per vesevos-backup create|restore|delete <nome> (backup con segreti) e
   vesevos-update net <url> <sha> (l'indirizzo puo contenere un codice): la frase del backup non e mai nella riga, la chiede il campo di tipo password. Il firmware, senza password nella riga, la chiede: vedi shAskStart (campo di tipo password). */
function shMask(v){
  return v.replace(/^(\s*(?:passwd|password)\s+\S+)\s+\S.*$/i,
    "$1 ****").replace(/^(\s*users?\s+add\s+\S+\s+\S+)\s+\S.*$/i,"$1 ****")
    .replace(/^(\s*wifi\s+set\s+(?:"(?:[^"\\]|\\.)*"|\S+))\s+\S.*$/i,"$1 ****")
    .replace(/^(\s*vesevos-backup\s+(?:create|restore|delete))\s+\S.*$/i,
      "$1 ****").replace(/^(\s*vesevos-update\s+net)\s+\S.*$/i,"$1 ****")
}
try{
  SH.h=JSON.parse(localStorage.getItem("vos_hist")||"[]").filter(function(x){
    return typeof x=="string"&&shMask(x)===x
  })
}catch(e){SH.h=[]}

function shSave(){
  try{localStorage.setItem("vos_hist",JSON.stringify(SH.h.slice(-100)))}catch(e){}
}

function shOut(txt,cls){
  var o=$("so"),d=document.createElement("div");
  d.textContent=txt===""?" ":txt;
  if(cls)d.className=cls;
  o.appendChild(d);
  while(o.childNodes.length>600)o.removeChild(o.firstChild);
  o.scrollTop=o.scrollHeight
}

function shKind(l){
  if(
    /errore|error|non valid|invalid|non autorizzat|unauthor|fallit|failed|sconosciut|unknown|nicht|ung.ltig|desconocid/i.test(
      l))return"e";
  if(/attenzione|avviso|warning|achtung|aviso/i.test(l))return"w";
  if(/^\s*(ok|fatto|done|salvato|saved|gespeichert|guardado)\b/i.test(l))return"g";
  return""
}

function shPrompt(r){
  var p=r.headers.get("X-VOS-Prompt");
  if(!p)return;
  SH.p=p;
  var e=$("sp");
  if(e)e.textContent=p.replace(/\s+$/,"")
}

function shLoadCmds(){
  if(SH.loaded)return;
  SH.loaded=true;
  post("/api/shell",{c:"help"}).then(function(r){shPrompt(r)}).catch(function(){SH.loaded=false})
}

function shPipe(txt,parts){
  for(var i=0;i<parts.length;i++){
    var p=parts[i].trim(),m=/^(grep|head|tail)\s*(-i\s+|-v\s+)?(.*)$/.exec(p);
    if(!m)continue;
    var ls=txt.split("\n");
    if(m[1]=="grep"){
      var f=m[2]?m[2].trim():"",q=m[3].trim().toLowerCase();
      ls=ls.filter(function(l){
        var h=l.toLowerCase().indexOf(q)>=0;
        return f=="-v"?!h:h
      })
    }
    else{
      var n=parseInt(m[3])||10;
      ls=m[1]=="head"?ls.slice(0,n):ls.slice(-n)
    }
    txt=ls.join("\n")
  }
  return txt
}

function shRun(v){
  v=v.replace(/\s+$/,"");
  if(!v)return Promise.resolve();
  var parts=v.split("|"),cmd=parts[0].trim(),w=cmd.split(/\s+/)[0].toLowerCase(),shown=shMask(v);
  shOut(SH.p+shown,"c");
  if(shown===v){
    var k=SH.h.indexOf(v);
    if(k>=0)SH.h.splice(k,1);
    SH.h.push(v);
    shSave()
  }
  SH.i=SH.h.length;
  if(w=="clear"){
    shClear();
    return Promise.resolve()
  }
  if(w=="history"){
    SH.h.forEach(function(x,i){shOut((i+1)+"  "+x,"m")});
    return Promise.resolve()
  }
  if(w=="reboot"&&!confirm(t("Riavviare la scheda?"))){
    shOut(t("Annullato"),"w");
    return Promise.resolve()
  }
  return shSend(cmd,parts.slice(1),null)
}
/* Manda il comando. pw = password dal campo di tipo password (campo "pw" a parte, non nella riga), oppure null.
   Se il firmware risponde X-VOS-Ask: confirm = chiede conferma (finestra, poi rimanda il comando con " yes"); secret / secret2 = chiede una password. */
function shSend(cmd,pipes,pw){
  var d={c:cmd},ask=null;
  if(pw!==null)d.pw=pw;
  return post("/api/shell",d).then(function(r){
    shPrompt(r);
    ask=r.headers.get("X-VOS-Ask");
    return r.text()
  }).then(function(x){
    x=shPipe(x.replace(/\n+$/,""),pipes);
    x.split("\n").forEach(function(l){shOut(l,shKind(l))});
    if(ask)shAskStart(ask,cmd)
  }).catch(function(){shOut(t("Errore di rete"),"e")})
}

function shAskStart(kind,cmd){
  if(kind=="confirm"){
    if(confirm(t("Confermi? Questa azione non si annulla.")))SH.q.unshift(cmd+" yes");
    else shOut(t("Annullato"),"w");
    return
  }
  var i=$("si");
  SH.ask={c:cmd,two:kind=="secret2",step:0,first:""};
  i.type="password";
  i.setAttribute("autocomplete","new-password");
  i.value="";
  i.focus()
}

function shAskEnd(){
  SH.ask=null;
  var i=$("si");
  i.type="text";
  i.setAttribute("autocomplete","off");
  i.value=""
}

function shAnswer(v){
  var a=SH.ask;
  if(a.two&&a.step==0){
    a.first=v;
    a.step=1;
    shOut(t("Ripeti la password:"),"m");
    return
  }
  if(a.two&&v!==a.first){
    shOut(t("Le password non coincidono: non cambia niente"),"e");
    shAskEnd();
    return
  }
  shAskEnd();
  shOut("****","c");
  SH.busy=true;
  /* risposta vuota (rete Wi-Fi aperta): niente campo pw, si usa la forma esplicita "rete aperta" del comando */
  (v===""?shSend(a.c+' ""',[],null):shSend(a.c,[],v)).then(function(){
    SH.busy=false;
    shNext()
  })
}

function shNext(){
  if(SH.busy||SH.ask||!SH.q.length)return;
  SH.busy=true;
  shRun(SH.q.shift()).then(function(){
    SH.busy=false;
    shNext()
  })
}

function runCmd(){
  var i=$("si"),v=i.value;
  i.value="";
  SH.i=SH.h.length;
  if(SH.ask){
    shAnswer(v);
    return
  }
  if(!v.trim())return;
  SH.q.push(v);
  shNext()
}

function shHist(d){
  var i=$("si");
  if(!SH.h.length)return;
  SH.i=Math.max(0,Math.min(SH.h.length,SH.i+d));
  i.value=SH.i>=SH.h.length?"":SH.h[SH.i];
  i.focus()
}
/* Tab: il firmware completa (comandi, sottocomandi, percorsi secondo il ruolo). Una richiesta alla volta e almeno 250 ms tra due Tab;
   il secondo Tab di fila, se restano piu candidati, li elenca nel terminale. */
function shTab(){
  var i=$("si");
  i.focus();
  if(SH.tabBusy||Date.now()-SH.tabT<250)return;
  var pos=i.selectionStart==null?i.value.length:i.selectionStart,head=i.value.slice(0,pos),
    again=SH.tabLine!==null&&SH.tabLine===i.value;
  if(head.length>200)return;
  SH.tabBusy=true;
  SH.tabT=Date.now();
  post("/api/shell",{c:head,tab:1}).then(function(r){return r.json()}).then(function(j){
    if(!j||!j.ok||i.value.slice(0,pos)!==head)return;
    var add=j.add||"",tail=i.value.slice(pos);
    if(add){
      i.value=head+add+tail;
      var p=(head+add).length;
      i.setSelectionRange(p,p)
    }
    else if(again&&j.list&&j.list.length>1){
      shOut(SH.p+head,"c");
      shOut(j.list.join("   ")+(j.more?"   ...":""),"m")
    }
    SH.tabLine=i.value
  }).catch(function(){}).then(function(){SH.tabBusy=false})
}

function shKey(e){
  var i=$("si");
  if(e.key!="Tab"&&e.key!="Shift"&&e.key!="Control"&&e.key!="Alt"&&e.key!="Meta")SH.tabLine=null;
  if(e.key=="Enter"){
    runCmd()
  }
  else if(SH.ask&&(e.key=="ArrowUp"||e.key=="ArrowDown"||e.key=="Tab")){
    e.preventDefault()
  }
  else if(e.key=="ArrowUp"){
    e.preventDefault();
    shHist(-1)
  }
  else if(e.key=="ArrowDown"){
    e.preventDefault();
    shHist(1)
  }
  else if(e.key=="Tab"){
    e.preventDefault();
    shTab()
  }
  else if(e.ctrlKey&&(e.key=="l"||e.key=="L")){
    e.preventDefault();
    shClear()
  }
  else if(e.ctrlKey&&(e.key=="c"||e.key=="C")&&i.selectionStart==i.selectionEnd){
    e.preventDefault();
    shCancel()
  }
}

function shPaste(e){
  var x=(e.clipboardData||window.clipboardData).getData("text");
  if(x.indexOf("\n")<0)return;
  e.preventDefault();
  var ls=x.split(/\r?\n/).filter(function(l){return l.trim()});
  if(ls.length>20){
    shOut(t("Troppe righe incollate (massimo 20)"),"e");
    return
  }
  if(ls.length&&!confirm(t("Eseguire queste righe?")+"\n\n"+ls.join("\n")))return;
  ls.forEach(function(l){SH.q.push(l)});
  shNext()
}

function shCancel(){
  var i=$("si");
  if(SH.ask){
    shAskEnd();
    shOut(t("Annullato"),"w");
    shNext();
    return
  }
  if(i.value)shOut(SH.p+i.value+"^C","m");
  i.value="";
  SH.q=[];
  SH.i=SH.h.length
}

function shClear(){$("so").textContent=""}
function shCopy(){
  var x=$("so").innerText;
  var ok=function(){
    msg("shm",t("Copiato"),true);
    setTimeout(function(){$("shm").className="msg"},3000)
  };
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
}
