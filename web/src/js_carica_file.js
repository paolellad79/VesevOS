/* Componente unico "Carica da file" della pagina Sicurezza > Aggiornamenti (solo Amministratore).
   Lo stesso codice serve le quattro schede; cambia solo la riga della tabella CF_TIPI:
   - fw e rec: apre il dialogo che c'e gia (firmware: riavvio nel recovery; recovery: scrittura diretta con i suoi controlli). Nessun controllo cambiato.
   - pac: uno o piu file, il tipo si capisce dall'estensione (.vsb script, .vab e .vae app, .vdb e .vde driver, e i vecchi .vsc .vap .vel .vdp).
   - ris: uno o piu file di /usr/share/vesevos; SHA256SUMS per primo, poi gli altri. Nome, lunghezza e SHA-256 si controllano QUI
     contro l'elenco della scheda, poi la scheda li ricontrolla. Un file rifiutato non ferma gli altri.
   Nessuna rotta nuova: pac usa /api/fs/up e /api/mod/save, ris usa /api/risorse/carica. */
var CF={busy:{}};
var CF_TIPI={
  fw:{
    az:function(){agOrdine(updFile)},
    cls:"btn gray",
    et:function(){return t("Carica un file dal computer")}
  },
  rec:{
    az:function(){uRecUp()},
    cls:"btn",
    et:function(){return t("Carica da file")},
    nota:function(){
      return t(
        "Il recovery serve a rimettere a posto la scheda: e l'operazione piu delicata. La scheda controlla tutto il file prima di scriverlo e, se e sbagliato, il recovery attuale resta com'e.")
    }
  },
  pac:{
    file:true,
    acc:".vsb,.vab,.vae,.vdb,.vde,.vsc,.vap,.vel,.vdp",
    cls:"btn",
    et:function(){return t("Carica da file")},
    nota:function(){
      return t(
        "Scegli uno o piu file: script (.vsb), app (.vab, .vae) o driver (.vdb, .vde). Il tipo si capisce dall'estensione.")
    },
    prepara:function(){return Promise.resolve({})},
    ordina:function(l){return l},
    controlla:function(f,cx){return cfPacControlla(f,cx)},
    invia:function(f,cx){return cfPacInvia(f,cx)},
    dopo:function(){return agPacLista().catch(function(){})}
  },
  ris:{
    file:true,
    acc:"",
    cls:"btn",
    et:function(){return t("Carica da file")},
    nota:function(){
      return t(
        "Scegli uno o piu file di risorse (anche dalla cartella delle risorse sul computer). SHA256SUMS si carica per primo; gli altri si riconoscono dal nome e dallo SHA-256 dell'elenco della scheda.")
    },
    prepara:function(){return cfRisPrepara()},
    ordina:function(l){
      return l.filter(function(f){return f.name=="SHA256SUMS"}).concat(
        l.filter(function(f){return f.name!="SHA256SUMS"}))
    },
    controlla:function(f,cx){return cfRisControlla(f,cx)},
    invia:function(f,cx){return cfRisInvia(f,cx)},
    dopo:function(){
      return arRileggi().then(function(){return arAttendi(0)}).then(agLoad,function(){})
    }
  }
};

function cfByte(n){return n<1024?n+" B":kb(n)}
/* il pulsante e il resto si disegnano di nuovo solo se cambia la lingua: i risultati a video restano */
function cfMonta(k){
  var c=$("cf_"+k),T=CF_TIPI[k],h;
  if(!c||c.getAttribute("data-l")==LANG)return;
  h='<button type="button" class="'+T.cls+'" id="cf_b_'+k+'" data-k="'+k+
    '" onclick="cfClic(this.dataset.k)">'+esc(T.et())+'</button>';
  if(T.file)h+='<input type="file" id="cf_f_'+k+'" hidden multiple'+(T.acc?' accept="'+T.acc+'"':'')+
    ' data-k="'+k+'" aria-label="'+esc(T.et())+'" onchange="cfScelti(this.dataset.k)">';
  if(T.nota)h+='<p class="fhint" id="cf_n_'+k+'">'+esc(T.nota())+'</p>';
  if(T.file)h+='<div class="fhint" id="cf_p_'+k+'" role="status"></div>'+
    '<ul class="cfr" id="cf_r_'+k+'" aria-live="polite" aria-relevant="additions"></ul>';
  c.innerHTML=h;
  c.setAttribute("data-l",LANG)
}

function cfMontaTutti(){["fw","rec","pac","ris"].forEach(cfMonta)}

function cfClic(k){
  var T=CF_TIPI[k];
  if(CF.busy[k])return;
  if(T.az)T.az();
  else $("cf_f_"+k).click()
}

function cfScelti(k){
  var inp=$("cf_f_"+k),l=Array.prototype.slice.call(inp.files);
  inp.value="";
  if(l.length)cfEsegui(k,l)
}

function cfAggiungi(k,nome,ok,m){
  var li=document.createElement("li"),b=document.createElement("b");
  b.textContent=nome;
  li.appendChild(b);
  li.appendChild(document.createTextNode(": "+(ok?"":t("Rifiutato")+". ")+m));
  if(!ok)li.className="ko";
  $("cf_r_"+k).appendChild(li)
}
/* un file alla volta, nell'ordine: controlla, invia, scrive l'esito. Un errore di un file non ferma i successivi */
function cfUno(k,l,cx,i,n){
  var T=CF_TIPI[k],f;
  if(i>=l.length)return Promise.resolve(n);
  f=l[i];
  $("cf_p_"+k).textContent=tf("Invio di {0} ({1} di {2})...",f.name,i+1,l.length);
  return T.controlla(f,cx).then(function(e){
    if(e)throw e;
    return T.invia(f,cx)
  }).then(function(m){
    cfAggiungi(k,f.name,true,m);
    return 1
  },function(e){
    cfAggiungi(k,f.name,false,typeof e=="string"?e:t("Caricamento non riuscito"));
    return 0
  }).then(function(v){return cfUno(k,l,cx,i+1,n+v)})
}

function cfEsegui(k,l){
  var T=CF_TIPI[k],b=$("cf_b_"+k),r=$("cf_r_"+k),p=$("cf_p_"+k);
  CF.busy[k]=true;
  b.disabled=true;
  b.setAttribute("aria-busy","true");
  r.innerHTML="";
  p.textContent=t("Controllo in corso...");
  T.prepara().then(function(cx){
    return cfUno(k,T.ordina(l),cx,0,0)
  }).then(function(n){
    p.textContent=tf("Caricati {0} file su {1}",n,l.length)
  },function(e){
    p.textContent=typeof e=="string"?e:t("Caricamento non riuscito")
  }).then(function(){
    return T.dopo()
  }).then(function(){
    CF.busy[k]=false;
    b.disabled=false;
    b.removeAttribute("aria-busy");
    b.focus()
  })
}
/* --- pacchetti: la rotta si sceglie dall'estensione (come js_script.js, js_app.js, js_driver.js) --- */
function cfPacRotta(f){
  var n=f.name.toLowerCase(),i=n.lastIndexOf("."),e=i<0?"":n.slice(i);
  if(e==EXT.s||e==EXTOLD.s)return{dir:"/sbin"};
  if(e==EXT.b||e==EXTOLD.b)return{dir:"/bin"};
  if(e==EXT.e||e==EXTOLD.e){
    if(!(AUTH&&AUTH.elf))return{err:t("Le app in codice nativo non sono abilitate su questa scheda")};
    return{dir:"/bin"}
  }
  if(e==EXT.de)return{dir:"/devs"};
  if(e==EXT.db||e==".vdp"){
    if(f.size>16384)return{err:t("File troppo grande (massimo 16 KB)")};
    return{driver:n.slice(0,i)}
  }
  return{err:tf("Estensione non ammessa: {0}",e||"-")}
}

function cfPacControlla(f,cx){
  var r=cfPacRotta(f);
  cx.r=r;
  return Promise.resolve(r.err||(f.size?"":tf("Dimensione non valida: {0}",f.name)))
}

function cfPacInvia(f,cx){
  var r=cx.r,fd,p;
  if(r.driver)p=f.text().then(function(x){return post("/api/mod/save",{name:r.driver,text:x})});
  else{
    fd=new FormData();
    fd.append("file",f);
    p=api("/api/fs/up?dir="+r.dir,{method:"POST",body:fd})
  }
  return p.then(function(x){return x.json()}).then(function(j){
    if(!j.ok)throw (j.err||t("Errore"));
    return j.avviso||t("Caricato")
  },function(){throw t("Caricamento non riuscito")})
}
/* --- risorse: l'elenco SHA256SUMS della scheda dice quali nomi sono ammessi, quanto sono grandi e che SHA-256 devono avere --- */
function cfRisPrepara(){
  return arRileggi().then(function(){
    var r=AG.d&&AG.d.ris;
    if(!r)throw t("La scheda non ha dato l'elenco dei file di risorse");
    return{ris:r}
  },function(){throw t("Richiesta non riuscita")})
}

function cfRisVoce(r,n){
  var v=null;
  r.f.forEach(function(x){if(x.n==n)v=x});
  return v
}
/* risposta "" = file buono; altrimenti il motivo del rifiuto (nulla parte verso la scheda) */
function cfRisControlla(f,cx){
  var r=cx.ris,n=f.name,v=null,tetto=AR_ELENCO_TETTO;
  if(n!="SHA256SUMS"){
    if(r.el!=0)return Promise.resolve(t(
      "L'elenco SHA256SUMS della scheda non e valido: carica prima SHA256SUMS"));
    v=cfRisVoce(r,n);
    if(!v)return Promise.resolve(tf("Nome sconosciuto: {0} non e nell'elenco della scheda",n));
    tetto=v.t
  }
  if(!f.size)return Promise.resolve(tf("Dimensione non valida: {0}",n));
  if(f.size>tetto)return Promise.resolve(tf("File troppo grande: {0} (massimo {1})",n,cfByte(tetto)));
  return f.arrayBuffer().then(function(b){
    var u=new Uint8Array(b);
    cx.dati=u;
    if(n=="SHA256SUMS")return arElencoOk(u,r)?"":tf(
      "Questo SHA256SUMS non e valido (formato o nomi): {0}",n);
    return hexb(sha256b(u))==v.h?"":tf("SHA-256 diverso dall'elenco della scheda: {0}",n)
  })
}
/* Spazio insufficiente (409): la scheda non da numeri, quindi si legge lo spazio libero della memoria interna e si confronta con la dimensione del file */
function cfRisSpazio(dim){
  return api("/api/fs/list?path=/").then(function(r){return r.json()}).then(function(j){
    var u=j.u||1;
    return tf("Spazio insufficiente per sostituire il file: servono circa {0}, liberi {1}",
      cfByte(dim),cfByte(Math.max(0,(j.total-j.used)*u)))
  }).catch(function(){return t("Spazio insufficiente per sostituire il file")})
}

function cfRisInvia(f,cx){
  var fd=new FormData();
  fd.append("file",new Blob([cx.dati]),f.name);
  return api("/api/risorse/carica",{method:"POST",body:fd}).then(function(r){
    return r.json().then(function(j){
      j.s=r.status;
      return j
    })
  },function(){throw t("Caricamento non riuscito")}).then(function(j){
    if(j.ok){
      if(f.name!="SHA256SUMS")return t("Caricato");
      return arRileggi().then(function(){
        cx.ris=AG.d.ris;
        return t("Caricato")
      },function(){return t("Caricato")})
    }
    if(j.s==409||j.code=="state")return cfRisSpazio(f.size).then(function(m){throw m});
    throw (j.err||t("Errore"))
  })
}
/* i pulsanti ci sono subito (in italiano); agDraw li ridisegna se la lingua cambia */
cfMontaTutti();
