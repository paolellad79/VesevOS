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
    Y=d.getUTCFullYear(),M=p(d.getUTCMonth()+1),D=p(d.getUTCDate()),H=d.getUTCHours(),ap="";
  if(A.tf){
    ap=H>=12?" PM":" AM";
    H=H%12||12
  }
  var ds=A.df==1?Y+"-"+M+"-"+D:A.df==2?M+"/"+D+"/"+Y:D+"/"+M+"/"+Y,
    of=A.to/60,sg=of<0?"-":"+",ao=Math.abs(of),tz="UTC"+sg+p(Math.floor(ao/60))+":"+p(ao%60);
  var s=tf("Ora della scheda: {0} {1} ({2})",ds,
    p(H)+":"+p(d.getUTCMinutes())+":"+p(d.getUTCSeconds())+ap,(A.tn?A.tn+", ":"")+tz);
  if(Math.abs(ms-Date.now())>30000)s+=" - "+
    t("Differisce dall'ora del tuo dispositivo: controlla data e fuso.");
  e.textContent=s
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
  var n=!AUTH.set;
  $("lnew").className=n?"":"hide";
  $("lp2w").className=n?"":"hide";
  $("lbt").textContent=n?t("Salva ed entra"):t("Entra");
  $("lu").readOnly=n;
  if(n)$("lu").value=AUTH.first||"admin";
  $("lp").setAttribute("autocomplete",n?"new-password":"current-password");
  $("lhttps").textContent=AUTH.https&&!AUTH.secure
    ?t("Collegamento non cifrato: usi l'hotspot della scheda, protetto dalla sua password Wi-Fi.")
    :AUTH.secure?tf("Collegamento cifrato (HTTPS). Impronta del certificato: {0}",AUTH.fp):""
}

function showLogin(){
  MFAT=null;
  if($("lmfa")){
    $("lmfa").className="hide";
    $("lbt").className="btn";
    $("lu").disabled=false;
    $("lp").disabled=false
  }
  clearInterval(timer);
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
    dlg({title:t("Hotspot e HTTP spenti"),
      body:'<p style="margin:0">'+
      esc(t(
        "Hai raggiunto la scheda dalla rete di casa: l'hotspot, il portale automatico e HTTP sono stati spenti. Per riaccenderli: Rete. Se perdi la Wi-Fi: tasto fisico 8 secondi."))+
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
        $("lbt").className="btn";
        loginOk(j)
      }else{
        msg("lm",j.err||t("Errore"),false);
        if(/Tempo scaduto/.test(j.err||"")){setTimeout(showLogin,1500)}
      }
    }).catch(function(){})
}

function logout(){post("/api/logout",{}).then(showLogin).catch(showLogin)}
function start(){
  $("login").className="hide";
  $("app").className="";
  setRole(AUTH.role===undefined?ROLE:AUTH.role);
  USER=AUTH.user||USER;
  $("who").textContent=USER?USER+" · "+t(RNAME[ROLE]):"";
  buildNav();
  cpApply();
  poll();
  brdLoad();
  rfApply();
  auPoll();
  if(!AUTH.setup&&ROLE==2)setupOpen();
  else if(location.hash=="#aiuto")show("help")
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
