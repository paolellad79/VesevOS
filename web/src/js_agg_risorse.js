/* Scheda "Risorse" (Sicurezza > Aggiornamenti, solo Amministratore): stato di ogni file di /usr/share/vesevos e, su clic, nuovo scarico.
   Come per lo store: lo scarica il BROWSER da GitHub (la scheda non ha bisogno di internet), senza credenziali e senza referrer.
   Lunghezza e SHA-256 si controllano QUI prima di caricare; la scheda li ricontrolla con il suo elenco SHA256SUMS e rifiuta un file diverso.
   Stati (come RisStato nel firmware): 0 da controllare, 1 buono, 2 diverso dall'elenco, 3 mancante, 4 non verificabile (elenco non valido). */
/* identita: DUE righe distinte. Firmware = versione letta dalla scheda (/api/aggiorna, campo fw); Pagina = numero proprio di html.vrs
   (costante messa da mkweb.py, intero da 1) e prime 8 cifre dello SHA-256 di html.vrs lette dall'elenco della scheda. */
var PAG_VER="@@PAGINA_VERSIONE@@";
function pagIdRighe(){
  var d=(typeof AG!="undefined")?AG.d:null;
  var f=((d&&d.ris&&d.ris.f)||[]).filter(function(x){return x.n=="html.vrs"})[0];
  var pag=(f&&f.h)?tf("Pagina: versione {0} (impronta SHA-256 {1})",PAG_VER,f.h.slice(0,8)):tf("Pagina: versione {0}",PAG_VER);
  return [tf("Firmware: {0}",(d&&d.fw)?d.fw:"?"),pag]
}
function pagIdHtml(){
  return pagIdRighe().map(esc).join("<br>")
}
function pagIdDisegna(){
  var e=$("pag_id");
  if(!e)return;
  e.innerHTML=pagIdHtml();
  if(typeof AG!="undefined"&&!AG.d&&!AG.idCarico){
    AG.idCarico=true;
    api("/api/aggiorna").then(function(r){return r.json()}).then(function(d){
      if(d&&d.fw){AG.d=AG.d||d;e.innerHTML=pagIdHtml()}
    }).catch(function(){})
  }
}
var AR_BASE=AG_REPO.base+AG_REPO.risorse,AR_ELENCO_TETTO=1024,AR={busy:false};
function arTesto(s){
  if(s==1)return t("buono");
  if(s==2)return t("diverso dall'elenco");
  if(s==3)return t("mancante");
  if(s==4)return t("non verificabile");
  return t("da controllare")
}
/* le voci della tabella: prima l'elenco SHA256SUMS, poi ogni file (il loro stato conta solo se l'elenco e buono) */
function arVoci(d){
  var r=d&&d.ris,v=[];
  if(!r)return v;
  v.push({n:"SHA256SUMS",s:r.el==0?1:(r.el==1?3:2),h:"",t:AR_ELENCO_TETTO,e:true});
  r.f.forEach(function(f){v.push({n:f.n,s:r.el==0?f.s:4,h:f.h,t:f.t,e:false})});
  return v
}
function arDaScaricare(v){return v.filter(function(x){return x.s==2||x.s==3})}
function arDraw(d){
  var v=arVoci(d),h="";
  if(!$("ag_ris"))return;
  pagIdDisegna();
  h='<p class="fhint">'+pagIdHtml()+'</p>';
  if(v.length){
    h+='<table class="ftab" aria-label="'+esc(t("Risorse"))+'"><tr><th scope="col">'+esc(t("File"))+
      '</th><th scope="col">'+esc(t("Stato"))+'</th><th scope="col">'+esc(t("Versione"))+'</th></tr>';
    v.forEach(function(x){
      h+='<tr><td>'+esc(x.n)+'</td><td>'+(x.s==2||x.s==3?'<b>'+esc(arTesto(x.s))+'</b>':esc(arTesto(x.s)))+'</td><td>'+(x.n=="html.vrs"?esc(PAG_VER):"")+'</td></tr>'
    });
    h+='</table>'
  }
  $("ag_ris").innerHTML=h;
  $("ag_rb").disabled=AR.busy||!arDaScaricare(v).length
}
/* l'elenco scaricato: formato di sha256sum, al massimo 16 righe, solo nomi di file che la scheda conosce (nessuna ancora al firmware) */
function arElencoOk(u,ris){
  var x,righe,noti=((ris&&ris.f)||[]).map(function(f){return f.n});
  try{x=new TextDecoder("utf-8",{fatal:true}).decode(u)}catch(e){return false}
  if(!/^([0-9a-f]{64}  [A-Za-z0-9._-]{1,40}\n){1,16}$/.test(x))return false;
  righe=x.match(/[^\n]*\n/g);
  return righe.every(function(r){return noti.indexOf(r.slice(66,-1))>=0})
}
/* un file: scarica, controlla lunghezza e SHA-256 contro la riga dell'elenco della scheda, poi lo carica. Errore = testo per l'utente */
function arUno(fw,x,ris){
  return fetch(AR_BASE+fw+"/"+x.n,{credentials:"omit",referrerPolicy:"no-referrer",cache:"no-cache"}).then(function(r){
    if(!r.ok)throw tf("Scarico non riuscito: {0}",x.n);
    return r.arrayBuffer()
  }).then(function(b){
    var u=new Uint8Array(b);
    if(!u.length||u.length>x.t)throw tf("Dimensione non valida: {0}",x.n);
    if(x.e?!arElencoOk(u,ris):hexb(sha256b(u))!=x.h)throw tf("Il file scaricato non corrisponde all'elenco della scheda: {0}",x.n);
    return caricaPezzi("/api/risorse/pezzo",new Blob([u]),x.n,null,t("Caricamento non riuscito"))
  }).then(function(j){
    if(!j.ok)throw (j.err||t("Errore"))
  })
}
function arRileggi(){
  return api("/api/aggiorna").then(function(r){return r.json()}).then(function(d){AG.d=d})
}
/* dopo un caricamento i file grandi si ricontrollano nel ciclo della scheda: si aspetta che non ce ne siano piu "da controllare" (al massimo 12 giri) */
function arAttendi(n){
  var daControllare=((AG.d&&AG.d.ris&&AG.d.ris.f)||[]).some(function(f){return f.s==0});
  if(!daControllare||n>=12)return Promise.resolve();
  return new Promise(function(ok){setTimeout(ok,1500)}).then(arRileggi).then(function(){return arAttendi(n+1)})
}
function arCiclo(fw){
  var fatti=0,provati={};
  function giro(){
    var v=arDaScaricare(arVoci(AG.d)),x=v[0];
    if(!x)return Promise.resolve(fatti);
    if(provati[x.n])return Promise.reject(tf("Il file resta diverso dopo il caricamento: {0}",x.n));
    provati[x.n]=1;
    return arUno(fw,x,AG.d.ris).then(function(){
      fatti++;
      return arRileggi()
    }).then(function(){return arAttendi(0)}).then(giro)
  }
  return giro()
}
function arScarica(){
  var d=AG.d;
  if(!d||!d.ris||AR.busy)return;
  if(!/^[0-9a-z.]{1,16}$/.test(d.fw)){
    msg("ag_rm",t("Versione del firmware non valida"));
    return
  }
  AR.busy=true;
  $("ag_rb").disabled=true;
  msg("ag_rm",t("Scarico in corso..."),true);
  arCiclo(d.fw).then(function(n){
    msg("ag_rm",tf("File caricati di nuovo: {0}",n),true)
  },function(e){
    msg("ag_rm",typeof e=="string"?e:t("Scarico non riuscito"))
  }).then(function(){
    AR.busy=false;
    agLoad()
  })
}
/* la campanella: un file con problema, con il motivo; poi il rimando alla pagina */
function arNota(n){
  var m;
  if(!n||!n.k)return;
  m=tf("Risorsa {0}: {1}. Scaricala di nuovo o caricala da file: Sicurezza > Aggiornamenti",n.n,
    n.s==3?t("mancante"):t("diverso dall'elenco"));
  if(n.k>1)m+=" +"+(n.k-1);
  AL.push(["w",m,"agg"])
}
