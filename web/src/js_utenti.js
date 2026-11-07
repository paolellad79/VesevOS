/* ===== Utenti ===== */
function usLoad(){
  api("/api/users").then(function(r){return r.json()}).then(function(L){
    var h='<thead><tr><th>'+esc(t("Nome"))+'</th><th>'+esc(t("Ruolo"))+'</th><th>'+esc(t("Attivo"))+
      '</th><th class="fac"></th></tr></thead><tbody>';
    L.forEach(function(u){
      var apri=u.name==USER?"passApri()"
        :'usPass('+u.i+',\''+esc(u.name)+'\')';   // la propria passa dalla vecchia password
      h+='<tr><td><b>'+esc(u.name)+'</b>'+
        (u.name==USER?' <span class="pill">'+esc(t("tu"))+'</span>':"")+
        (u.pass?"":' <span class="pill k">'+esc(t("senza password"))+'</span>')+
        '</td><td><select onchange="usSet('+u.i+',\'role\',this.value)">'+[0,1,2].map(function(r){
          return'<option value="'+r+'"'+(r==u.role?" selected":"")+'>'+esc(t(RNAME[r]))+'</option>'
        }).join("")+'</select></td>'+
        '<td><button class="sw1'+(u.on?" on":"")+'" onclick="usSet('+u.i+',\'on\','+(u.on?0:1)+
        ')" aria-label="'+esc(t("Attivo"))+
        '"></button></td><td class="fac"><button class="btn gray sm" onclick="'+apri+'">'+
        esc(t("Password"))+'</button><button class="btn red sm" onclick="usDel('+u.i+',\''+
        esc(u.name)+'\')">'+esc(t("Elimina"))+'</button></td></tr>'
    });
    $("ustab").innerHTML=h+'</tbody>'
  }).catch(function(){})
}

function usSet(i,k,v){
  var d={i:i};
  d[k]=v;
  post("/api/users/set",d).then(function(r){return r.json()}).then(function(j){
    msg("usm",j.ok?t("Salvato"):j.err,j.ok);
    usLoad()
  })
}

function usDel(i,n){
  dlg({title:tf("Eliminare l'utente {0}?",n),body:"",ok:t("Elimina"),danger:true,onOk:function(){
    return post("/api/users/del",{i:i}).then(function(r){return r.json()}).then(function(j){
      if(j.ok)usLoad();
      return j.ok?"":j.err
    })
  }})
}

function usPass(i,n){
  dlg({title:tf("Nuova password per {0}",n),
    body:'<label for="dp1">'+esc(t("Password (min 6)"))+
    '</label><input type="password" id="dp1" autocomplete="new-password">',ok:t("Salva"),
    onOk:function(){
      return post("/api/users/pass",
        {i:i,pass:$("dp1").value}).then(function(r){return r.json()}).then(function(j){
          if(j.ok){
            msg("usm",t("Password cambiata"),true);
            usLoad()
          }
          return j.ok?"":j.err
        })
    }})
}

function uAdd(){
  post("/api/users/add",{name:$("u_n").value.trim(),role:$("u_r").value,
    pass:$("u_p").value}).then(function(r){return r.json()}).then(function(j){
      msg("usm2",j.ok?t("Utente aggiunto"):j.err,j.ok);
      if(j.ok){
        $("u_n").value="";
        $("u_p").value="";
        usLoad()
      }
    })
}

function fillPass(){
  $("pwho").textContent=tf("Utente: {0} ({1})",USER,t(RNAME[ROLE]))
}
/* Svuota i campi e li nasconde: dopo il cambio e quando si lascia la voce. */
function pwVuota(){
  ["p_old","p_new","p_new2"].forEach(function(i){$(i).value=""});
  pwVedi(false);
  pwMisura()
}
/* Solo indicazione, nel browser: la regola che vale e quella del firmware (6-64 caratteri). */
function pwForza(p){
  if(p.length<6)return t("Troppo corta");
  var n=p.length>=12?2:p.length>=8?1:0;
  [/[a-z]/,/[A-Z]/,/[0-9]/,/[^a-zA-Z0-9]/].forEach(function(r){if(r.test(p))n++});
  if(n<3||/^(.)\1*$/.test(p))return t("Debole");
  return n<5?t("Media"):t("Forte")
}

function pwMisura(){
  var p=$("p_new").value,x=p?tf("Robustezza: {0}",pwForza(p)):"";
  if($("p_mis").textContent!=x)$("p_mis").textContent=x
}

function pwVedi(on){
  if(on===undefined)on=$("p_new").type=="password";
  $("p_new").type=$("p_new2").type=on?"text":"password";
  $("p_ved").textContent=on?t("Nascondi"):t("Mostra");
  $("p_ved").setAttribute("aria-pressed",on?"true":"false")
}
/* Un numero da 0 a n-1 senza distorsione: i valori oltre l'ultimo multiplo intero di n si scartano. */
function pwCaso(n){
  var b=new Uint32Array(1),lim=Math.floor(4294967296/n)*n;
  do crypto.getRandomValues(b);
  while(b[0]>=lim);
  return b[0]%n
}

function pwRiempi(o){
  $("p_new").value=$("p_new2").value=o;
  pwVedi(true);
  pwMisura()
}
/* 64 caratteri senza quelli che si confondono: un byte &63 ne sceglie uno senza distorsione. */
var PWA="abcdefghijkmnpqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789-_.!?+=@";
function pwGenera(){
  var v=$("p_liv").value,o="";
  if(v=="0")return pwRicorda();
  if(v=="2")return pwMassima();
  while(o.length<16)o+=PWA.charAt(pwCaso(64));
  pwRiempi(o)
}
/* Massima: 32 byte del browser, in XOR con quelli della scheda. Chi legge la risposta su HTTP non conosce il
   risultato; se la scheda non risponde resta il solo generatore del browser, che basta. */
function pwMassima(){
  var c=new Uint8Array(32),o="";
  crypto.getRandomValues(c);
  api("/api/casuale").then(function(r){return r.json()}).then(function(j){
    for(var i=0;i<32;i++)c[i]^=parseInt(j.rnd.substr(i*2,2),16)
  }).catch(function(){}).then(function(){
    c.forEach(function(b){o+=PWA.charAt(b&63)});
    pwRiempi(o)
  })
}
/* Le parole italiane dei testi fissi della pagina: __o e il testo originale, anche con un'altra lingua. */
function pwParole(){
  var w=document.createTreeWalker($("t_help").parentNode,NodeFilter.SHOW_TEXT),n,V={};
  while(n=w.nextNode()){
    if(n.__o===undefined||n.parentNode.closest("pre,code,table,select"))continue;
    n.__o.split(/[\s.,:;!?()'"<>=\/+-]+/).forEach(function(x){
      if(/^[a-z]{5,9}$/.test(x))V[x]=1
    })
  }
  return Object.keys(V)
}
/* Da ricordare: 4 parole con una maiuscola ciascuna, tra l'una e l'altra un numero da 1 a 99. */
function pwRicorda(){
  var L=pwParole(),o="";
  for(var i=0;i<4;i++){
    var x=L[pwCaso(L.length)],k=pwCaso(x.length);
    o+=x.slice(0,k)+x.charAt(k).toUpperCase()+x.slice(k+1);
    if(i<3)o+=pwCaso(99)+1
  }
  pwRiempi(o)
}
