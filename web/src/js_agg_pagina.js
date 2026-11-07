/* Pagina Aggiornamenti (Sicurezza, solo Amministratore): quattro schede con tastiera a frecce, verifica online, controllo programmato.
   Il controllo gira sulla scheda in un suo task: la pagina lo avvia e rilegge lo stato finche non finisce. */
/* store: elenco pubblicato, letto dal browser e confrontato qui. Si legge solo a controllo acceso o dopo Verifica ora: mai da spento senza richiesta. */
var AG={d:null,sch:0,tm:0,st:null,pre:false,vec:0,prima:true,pac:null};
var AG_STORE="https://raw.githubusercontent.com/paolellad79/VesevOS/main/store/index.json",
  AG_STORE_TETTO=32768,AG_STORE_VOCI=40;
function agRiga(e,v){return v?row(e,v):""}
function agQuando(n){return n?new Date(n*1000).toLocaleString():t("mai")}
function agLoad(){
  clearTimeout(AG.tm);
  api("/api/aggiorna").then(function(r){return r.json()}).then(function(d){
    if(cur!="agg")return;
    AG.d=d;
    agDraw(d);
    if(d.corso)AG.tm=setTimeout(agLoad,2000);
    else if(AG.pre&&!AG.st)agConfronta(d.modo)
  }).catch(function(){})
}

function agDraw(d){
  $("ag_stato").innerHTML=
    row(t("Firmware installato"),esc(d.fw))+
    row(t("Recovery installato"),d.recOk?esc(d.rec||t("versione sconosciuta")):t("non presente"))
    +row(t("Ultimo controllo"),esc(agQuando(d.ult)))+agRiga(t("Esito"),esc(d.esito))+
    agRiga(t("Installazione automatica"),esc(d.motivo))
    +(d.oraOk?"":row(t("Ora"),t("non disponibile: il controllo programmato salta")));
  $("ag_ver").disabled=d.corso;
  msg("ag_m",d.corso?t("Controllo in corso..."):"",true);
  var fw=d.trovato?row(t("Versione pubblicata"),esc(d.ver))+agRiga(t("Data"),esc(d.data))+
    agRiga(t("Dimensione"),d.dim?kb(d.dim):"")+agRiga(t("Pubblicato da"),esc(d.chi))
    +row(t("Canale"),d.uff?t("Ufficiale"):t("Non ufficiale"))+'<p class="fhint">'+
    esc(t(
      "Nome e canale sono scritte dell'elenco, non firme: lo SHA-256 dice solo che il file non e rotto."))+
    '</p>'
    :'<p class="fhint">'+esc(
      t("Premi Verifica ora nella scheda Stato per vedere la versione pubblicata."))+'</p>';
  $("ag_fw").innerHTML=row(t("Firmware installato"),esc(d.fw))+fw+
    (d.trovato&&!d.nuovo?'<p>'+esc(t("Hai gia l'ultima versione."))+'</p>':"");
  $("ag_ins").className="btn"+(d.nuovo&&d.uff?"":" hide");
  $("ag_rec").innerHTML=row(t("Recovery installato"),
    d.recOk?esc(d.rec||t("versione sconosciuta")):t("non presente"))+
    agRiga(t("Versione pubblicata"),esc(d.verRec));
  if(document.activeElement&&document.activeElement.closest&&
    document.activeElement.closest("#ag_quando,#ag_modo"))return;
  $("ag_modo").value=d.modo;
  $("ag_freq").value=d.freq;
  $("ag_ora").value=("0"+Math.floor(d.ora/60)).slice(-2)+":"+("0"+d.ora%60).slice(-2);
  agGiorni(d.giorno);
  agScelta()
}

function agGiorni(g){
  var f=$("ag_freq").value,h="",i;
  if(f=="1")[t("lunedi"),t("martedi"),t("mercoledi"),t("giovedi"),t("venerdi"),t("sabato"),
    t("domenica")].forEach(function(n,k){
      h+='<option value="'+(k+1)+'">'+esc(n)+'</option>'
    });
  if(f=="2")for(i=1;i<=28;i++)h+='<option value="'+i+'">'+i+'</option>';
  var c=g||$("ag_gg").value;
  $("ag_gg").innerHTML=h;
  $("ag_gg").value=c;
  if($("ag_gg").selectedIndex<0)$("ag_gg").selectedIndex=0;
  $("ag_ggw").className=f=="0"?"hide":""
}

function agScelta(){
  var m=$("ag_modo").value;
  $("ag_quando").className=m=="0"?"hide":"";
  $("ag_auto").className="law"+(m=="2"?"":" hide")
}
/* le schede seguono il modello WAI-ARIA: solo quella attiva e nella sequenza di Tab, le frecce spostano tra le schede */
function agTab(i,fuoco){
  AG.sch=i;
  [0,1,2,3].forEach(function(k){
    var b=$("ag_t"+k),on=k==i;
    b.className=on?"on":"";
    b.setAttribute("aria-selected",on?"true":"false");
    if(on)b.removeAttribute("tabindex");
    else b.setAttribute("tabindex","-1");
    $("ag_p"+k).className=on?"":"hide"
  });
  if(fuoco)$("ag_t"+i).focus();
  if(i==3)agPac()
}

function agTasto(e){
  var i=AG.sch,k=e.key;
  if(k=="ArrowRight")i=(i+1)%4;
  else if(k=="ArrowLeft")i=(i+3)%4;
  else if(k=="Home")i=0;
  else if(k=="End")i=3;
  else return;
  e.preventDefault();
  agTab(i,true)
}

function agVerifica(){
  AG.pre=true;
  AG.st=null;
  $("ag_ver").disabled=true;
  msg("ag_m",t("Controllo in corso..."),true);
  post("/api/aggiorna/verifica",{}).then(function(r){return r.json()}).then(function(j){
    if(!j.ok){
      $("ag_ver").disabled=false;
      msg("ag_m",j.err||t("Errore"));
      return
    }
    setTimeout(agLoad,1500)
  },function(){
    $("ag_ver").disabled=false;
    msg("ag_m",t("Richiesta non riuscita"))
  })
}

function agInvia(conferma){
  var o=($("ag_ora").value||"03:00").split(":"),m=$("ag_modo").value,f=$("ag_freq").value,
    v={modo:m,freq:f,giorno:f=="0"?1:$("ag_gg").value,ora:o[0]*60+ +o[1]};
  if(conferma)v.confirm="1";
  return post("/api/aggiorna/imposta",v).then(function(r){return r.json()}).then(function(j){
    msg("ag_ms",j.ok?t("Salvato"):(j.err||t("Errore")),j.ok);
    if(j.ok)agLoad();
    return j.ok?"":(j.err||t("Errore"))
  },function(){return t("Richiesta non riuscita")})
}

function agSalva(){
  if($("ag_modo").value=="0"||(AG.d&&AG.d.modo)){
    agInvia(false);
    return
  }
  dlg({title:t("Attivare il controllo online?"),
    body:'<p style="margin:0 0 8px">'+
    esc(t(
      "Gli aggiornamenti vengono dalla pagina ufficiale del progetto su GitHub. Il controllo verifica che il file sia integro. L'attivazione e facoltativa e puoi spegnerla quando vuoi."))+
    '</p><p class="law" style="margin:0">'+
    esc(t("A ogni controllo GitHub vede l'indirizzo IP pubblico della tua rete."))+'</p>',
    ok:t("Attiva"),onOk:function(){return agInvia(true)}})
}

function agOrdine(fn){
  api("/api/updorder").then(function(r){return r.json()}).then(function(j){
    if(!j.present){
      dlg({title:t("Aggiornare tramite recovery?"),
        body:'<p style="margin:0">'+
        esc(t("Il recovery non e presente su questa scheda: si installa solo con il cavo USB."))+
        '</p>',buttons:[["ok",t("Chiudi"),""]]});
      return
    }
    fn(j)
  }).catch(function(){})
}

function agInstalla(){
  var d=AG.d;
  if(!d)return;
  updDlg({title:tf("Installare la versione {0}?",d.ver),
    body:'<p style="margin:0 0 8px">'+
    esc(t(
      "La scheda si riavvia nel recovery, che scarica il file da GitHub, ne controlla lunghezza e SHA-256 e lo installa. Se non riesce, la scheda resta nel recovery."))+
    '</p>'
    +'<label class="chk" for="updc"><input type="checkbox" id="updc"><span>'+
    esc(t("Ho capito: la scheda sara fuori servizio per alcuni minuti"))+'</span></label>',
    ok:t("Riavvia e aggiorna"),
    onOk:function(){
      if(!$("updc").checked)return t("Spunta la casella per continuare");
      return post("/api/aggiorna/installa",
        {confirm:"1"}).then(function(r){return r.json()}).then(function(k){
          if(k.ok)setTimeout(updWaitBack,50);
          return k.ok?"":(k.err||t("Errore"))
        },function(){return t("Richiesta non riuscita")})
    }})
}
/* dati da internet: si tengono solo voci con nome, tipo e versione validi, senza eredita (la chiave __proto__ resta una chiave qualunque) */
function agStoreVoci(x){
  if(x.length>AG_STORE_TETTO)throw 0;
  var v=JSON.parse(x).items,m=Object.create(null),n=0;
  if(!Array.isArray(v))throw 0;
  v.forEach(function(p){
    if(n>=AG_STORE_VOCI||!p||typeof p.name!="string"||!p.name||p.name.length>24||
      typeof p.version!="string"||!/^[0-9.]{1,10}$/.test(p.version)||
      ["vab","vae","vdb","vde"].indexOf(p.ext)<0)return;
    m[p.ext+"/"+p.name]=p.version;
    n++
  });
  return m
}

function agStoreLeggi(){
  if(AG.st)return Promise.resolve();
  return fetch(AG_STORE,{credentials:"omit",referrerPolicy:"no-referrer",
    cache:"no-cache"}).then(function(r){
      if(!r.ok)throw 0;
      return r.text()
    }).then(function(x){AG.st=agStoreVoci(x)})
}
/* come pacVersionePiuBassa nel firmware: numero per numero, 1.10 > 1.9; false se una delle due manca */
function agPiuBassa(a,b){
  if(!a||!b)return false;
  var x=a.split("."),y=b.split("."),i,p,q;
  for(i=0;i<x.length||i<y.length;i++){
    p=+x[i]||0;
    q=+y[i]||0;
    if(p!=q)return p<q
  }
  return false
}

function agPubblicata(p){return AG.st&&AG.st[p.e+"/"+p.n]||""}
function agVecchio(p){return agPiuBassa(p.v,agPubblicata(p))}
function agPacDraw(){
  var h="",l=AG.pac||[];
  if(!$("ag_pac"))return;
  if(!AG.st)h+='<p class="fhint">'+
    esc(t("Store non ancora letto: premi Verifica ora nella scheda Stato."))+'</p>';
  if(!l.length)h+='<p>'+esc(t("Nessuna app e nessun driver installati."))+'</p>';
  else{
    h+='<table class="ftab" aria-label="'+esc(t("App e driver installati"))+
      '"><tr><th scope="col">'+esc(t("Nome"))+'</th><th scope="col">'+esc(t("Tipo"))+
      '</th><th scope="col">'+esc(t("Installata"))+'</th><th scope="col">'+esc(t("Pubblicata"))+
      '</th></tr>';
    l.forEach(function(x){
      var d=agPubblicata(x);
      h+='<tr><td>'+esc(x.n)+'</td><td>.'+esc(x.e)+'</td><td>'+esc(x.v||t("non dichiarata"))+
        '</td><td>'+(agVecchio(x)?'<b>'+esc(d)+'</b>':esc(d||"-"))+'</td></tr>'
    });
    h+='</table>'
  }
  $("ag_pac").innerHTML=h
}

function agPacLista(){
  return api("/api/aggiorna/pacchetti").then(function(r){return r.json()}).then(function(j){
    AG.pac=j.lista||[];
    AG.vec=AG.st?AG.pac.filter(agVecchio).length:0;
    agPacDraw()
  })
}
/* un errore di rete lascia lo store non letto: la lista degli installati resta comunque */
function agConfronta(modo){
  if(!modo&&!AG.pre)return Promise.resolve();
  return agStoreLeggi().then(agPacLista).catch(function(){})
}

function agPac(){
  agPacLista().catch(function(){});
  agConfronta(AG.d&&AG.d.modo)
}
/* la campanella: il primo tipo utile (vedi aggiornaNotaJson nel firmware) */
function agNota(n){
  if(!n)return;
  if(n.m&&AG.prima){
    AG.prima=false;
    agConfronta(n.m)
  }
  if(!n.m&&!AG.pre)AG.vec=0;
  if(!n.k){
    if(AG.vec)AL.push(["i",tf("Aggiornamenti disponibili per {0} pacchetti",AG.vec),"agg"]);
    return
  }
  if(n.k==1)AL.push(["i",tf("Aggiornamento disponibile: firmware {0}",n.v),"agg"]);
  else if(n.k==2)AL.push(["i",tf("Aggiornamento installato: {0}",n.v),"agg"]);
  else if(n.k==3)AL.push(["w",t("Aggiornamento non riuscito"),"agg"])
}
