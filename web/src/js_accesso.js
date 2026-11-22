/* ===== utente, ruolo, accesso ===== */
var AUTH={set:true,setup:true},USER="",ROLE=2;
var RNAME=["Ospite","Operatore","Amministratore"];
function setRole(r){
  ROLE=+r;
  document.body.setAttribute("data-role",ROLE)
}

function authLoad(){
  return fetch("/api/auth").then(function(r){return r.json()}).then(function(j){
    AUTH=j;
    AUTHSET=j.set;
    return j
  })
}
var CLK={tm:0,t0:0};
/* Ora lunga: i due punti diventano un elemento che lampeggia ogni secondo (secondi dispari = spento); il resto del testo e quello del formato */
function clkOra(testo,secondi){
  var cl=secondi%2?"off":"",pezzi=testo.split(":"),o="";
  for(var i=0;i<pezzi.length;i++){
    if(i)o+='<i class="'+cl+'">:</i>';
    o+=esc(pezzi[i])
  }
  return o
}

function clkDraw(){
  var A=AUTH,e=$("lclk");
  if(!e)return;
  if(!A||A.te===undefined){
    e.textContent="";
    return
  }
  if(!A.tv){
    e.textContent=t("Ora della scheda non impostata. Si imposta dopo l'accesso (Sistema > Ora).");
    return
  }
  var ms=(A.te+(Date.now()-CLK.t0)/1000)*1000,d=new Date(ms+A.to*1000),
    p=function(n){return(n<10?"0":"")+n},
    im=dfImpostDa(A),pd=dfDaData(d),nm=dfNomi(),
    ora=dfOraCorta(im,pd),                       /* accesso: solo ore e minuti, con i due punti che lampeggiano */
    dataLunga=fmtLungo(d,A)[1],                  /* telefono: data lunga scelta in Localizzazione */
    dataEstesa=dfFormatta(DF_PRONTA_LUNGA,pd,nm),/* desktop: data per esteso, con i nomi completi */
    z=fusoTesto(A),
    s='<div class="lt1" translate="no">'+clkOra(ora,d.getUTCSeconds())+'</div><div class="lt2"><span class="dn">'+
    esc(dataLunga)+'</span><span class="de">'+esc(dataEstesa)+'</span><span class="dz"><span class="ds"> &middot; </span>'+
    z.replace(/&/g,"&amp;").replace(/</g,"&lt;")+"</span></div>";
  if(Math.abs(ms-Date.now())>30000)s+='<div class="lt3">'+
    t("Differisce dall'ora del tuo dispositivo: controlla data e fuso.")+"</div>";
  e.innerHTML=s;
  if(AUTH.ml)$("lmsg").textContent=msgVar(AUTH.ml,A,d,A.nm)   /* le variabili con l'ora si aggiornano ogni secondo */
}

function pwEye(){
  var i=$("lp"),on=i.type=="password";
  i.type=on?"text":"password";
  $("leyes").setAttribute("href",on?"#i-occhio-barrato":"#i-occhio");   /* stessa icona, nel disegno con o senza la barra */
  $("leye").setAttribute("aria-pressed",on?"true":"false");
  $("leye").setAttribute("aria-label",on?t("Nascondi la password"):t("Mostra la password"))
}

function clkStart(){
  CLK.t0=Date.now();
  clearInterval(CLK.tm);
  clkDraw();
  CLK.tm=setInterval(clkDraw,1000)
}

function showLoginText(){
  clkStart();
  $("lt").textContent=AUTH.set?t("Accesso"):t("Primo accesso");
  var ml=$("lmsg");
  ml.textContent=msgVar(AUTH.ml,AUTH,dataSpostata(AUTH),AUTH.nm);   /* testo pubblico scritto dall'Admin: sempre textContent, mai HTML */
  ml.className=AUTH.ml?"lmsg":"lmsg hide";
  var n=!AUTH.set;
  $("lnew").className=n?"":"hide";
  $("lp2w").className=n?"":"hide";
  $("lbt").textContent=n?t("Salva ed entra"):t("Entra");
  $("lu").readOnly=n;
  if(n)$("lu").value=AUTH.first||"admin";
  $("lp").setAttribute("autocomplete",n?"new-password":"current-password")
}

function showLogin(){
  MFAT=null;
  if($("lmfa")){
    $("lmfa").className="hide";
    $("lbt").className="btn lgo";
    $("lu").disabled=false;
    $("lp").disabled=false
  }
  clearInterval(timer);
  clearInterval(AU.tm);
  PAGINA=false;
  $("app").className="hide";
  $("login").className="card";
  document.body.classList.remove("cpo");
  setupClose();
  authLoad().then(function(){
    showLoginText();
    try{
      var u=localStorage.getItem("vu");
      if(u&&AUTH.set)$("lu").value=u
    }catch(e){}($("lu").value&&AUTH.set?$("lp"):$("lu")).focus()
  }).catch(function(){})
}

function lKey(e){
  if(e.key=="Enter"){
    e.preventDefault();
    doLogin()
  }
}

function loginOk(j){
  clearInterval(CLK.tm);
  if(j.clean)setTimeout(function(){
    dlg({title:t("Hotspot e DHCP spenti"),
      body:'<p style="margin:0">'+
      esc(t(
        "Hai raggiunto la scheda dalla rete di casa: l'hotspot, il portale automatico e il DHCP sono stati spenti. Per riaccenderli: Rete. Se perdi la Wi-Fi: tasto fisico 8 secondi."))+
      '</p>',buttons:[["ok",t("OK"),""]],onOk:function(){return""}})
  },800);
  $("lp").value="";
  $("lp2").value="";
  USER=j.user;
  setRole(j.role);
  try{localStorage.setItem("vu",USER)}catch(e){}
  authLoad().then(function(){start()}).catch(function(){start()})
}

function doLogin(){
  var u=$("lu").value.trim(),p=$("lp").value;
  if(!p){
    msg("lm",t("Scrivi la password"),false);
    return
  }
  if(!AUTH.set){
    if(p.length<6){
      msg("lm",t("Password troppo corta (min 6)"),false);
      return
    }
    if(p!=$("lp2").value){
      msg("lm",t("Le due password non sono uguali"),false);
      return
    }
    post("/api/firstpass",{p:p}).then(function(r){return r.json()}).then(function(j){
      if(j.ok)loginOk(j);
      else msg("lm",j.err||t("Errore"),false)
    }).catch(function(){});
    return
  }
  if(!u){
    msg("lm",t("Scrivi il nome utente"),false);
    return
  }
  msg("lm",t("Controllo..."),true);
  post("/api/login/start",{u:u}).then(function(r){return r.json()}).then(function(s){
    if(s.ok===false){
      msg("lm",s.err,false);
      return
    }
    var mac=hmacHex(hashPass(s.salt,p,s.iter),s.nonce);
    msg("lm",s.pow?t("Verifico che sei una persona..."):t("Controllo..."),true);
    return powSolve(s.nonce,s.pow||0).then(function(pw){
      return post("/api/login",
        {u:u,nonce:s.nonce,mac:mac,pw:pw,
          hp:$("lhp").value}).then(function(r){return r.json()}).then(function(j){
            if(j.ok)loginOk(j);
            else if(j.mfa)mfaStep(j);
            else msg("lm",j.err||t("Errore"),false)
          })
    })
  }).catch(function(){})
}
/* prova di lavoro anti-bot: sha256(nonce:n) con N zeri iniziali (a pezzi, la pagina non si blocca) */
function powSolve(nonce,bits){
  return new Promise(function(res){
    if(!bits){
      res("");
      return
    }
    var n=0,hx=Math.floor(bits/4),rest=bits%4;
    function ok(h){
      for(var i=0;i<hx;i++)if(h.charAt(i)!="0")return false;
      if(rest&&parseInt(h.charAt(hx),16)>=(16>>rest))return false;
      return true
    }
      (function step(){
        var e=n+1500;
        for(;n<e;n++){
          if(ok(sha256Hex(nonce+":"+n))){
            res(String(n));
            return
          }
        }
        setTimeout(step,0)
      })()
  })
}
var MFAT=null;
function mfaStep(j){
  MFAT=j.tok;
  $("lmfa").className="";
  $("lbt").className="hide";
  $("lu").disabled=true;
  $("lp").disabled=true;
  $("lcode").value="";
  var n=$("lmfan");
  if(j.recOnly){
    n.className="law";
    n.textContent=t("L'ora della scheda non e valida: serve un codice di recupero.")
  }
  else if(j.notime){
    n.className="law";
    n.textContent=t(
      "L'ora della scheda non e valida: con il codice giusto la scheda prende l'ora di questo dispositivo. Controlla che l'ora del dispositivo sia esatta.")
  }else n.className="hide";
  msg("lm","",true);
  setTimeout(function(){$("lcode").focus()},50)
}

function doMfa(){
  var c=$("lcode").value.trim();
  if(!c){
    msg("lm",t("Scrivi il codice"),false);
    return
  }
  post("/api/login/mfa",{tok:MFAT,code:c,
    now:Math.floor(Date.now()/1000)}).then(function(r){return r.json()}).then(function(j){
      if(j.ok){
        $("lu").disabled=false;
        $("lp").disabled=false;
        $("lmfa").className="hide";
        $("lbt").className="btn lgo";
        loginOk(j)
      }else{
        msg("lm",j.err||t("Errore"),false);
        if(/Tempo scaduto/.test(j.err||"")){setTimeout(showLogin,1500)}
      }
    }).catch(function(){})
}

function logout(){post("/api/logout",{}).then(showLogin).catch(showLogin)}
/* La pagina vera (menu, home, aggiornamenti) parte una volta per accesso. Con la guida da fare parte solo a guida chiusa:
   nella finestra di prima configurazione la scheda ha poca RAM e pochi percorsi, la home non serve. */
var PAGINA=false;
function start(){
  $("login").className="hide";
  $("app").className="";
  setRole(AUTH.role===undefined?ROLE:AUTH.role);
  USER=AUTH.user||USER;
  $("who").textContent=USER?USER+" \u00b7 "+t(RNAME[ROLE]):"";
  if(!AUTH.setup&&ROLE==2){
    fetchLimite(1);
    setupOpen();
    return
  }
  paginaAvvia()
}

function paginaAvvia(){
  fetchLimite(2);
  if(PAGINA){
    show("sum");
    return
  }
  PAGINA=true;
  buildNav();
  if(ROLE>=1)dvList().catch(function(){});
  cpApply();
  poll();
  brdLoad();
  rfApply();
  auPoll();
  if(location.hash=="#aiuto")show("help")
}

function chPass(){
  if($("p_new").value!=$("p_new2").value){
    msg("pm",t("Le due password non coincidono"),false);
    return
  }
  post("/api/passwd",{o:$("p_old").value,
    n:$("p_new").value}).then(function(r){return r.json()}).then(function(j){
      msg("pm",j.ok?t("Password cambiata"):j.err,j.ok);
      if(j.ok)pwVuota()
    })
}
/* voce Password: menu in alto e riga "tu" */
function passApri(){
  show("pass");
  $("p_old").focus();
}
