/* ===== Registratore di dati: cartella dei dati (elenco, selezione, scarico, eliminazione) ===== */
var RCSEL={},RCFL=[],RCDIR="",RCSIG="";

/* dimensione con fSize (js_file.js); la data e quella scritta nel nome (-AAAAMMGG), formattata come in Localizzazione */
function rcFData(f){return f.q?fmtData(f.q):"-"}

function rcFiles(j){
  var i,nm={};
  RCFL=j.files;
  RCDIR=j.dir;
  for(i=0;i<RCFL.length;i++)nm[RCFL[i].n]=1;
  for(var k in RCSEL)if(!nm[k])delete RCSEL[k];        /* un file sparito esce dalla selezione */
  var altri=0;
  for(i=0;i<RCFL.length;i++)if(RCFL[i].o)altri++;
  $("rc_files").textContent=(RCFL.length?tf("{0} file in {1}, {2} byte in tutto",RCFL.length,j.dir,j.bytes):tf("Nessun file in {0}",j.dir))+
    (altri?" · "+tf("{0} file con un altro nome: non si contano nel limite e si tolgono solo da qui",altri):"");
  var sig=RCDIR+"|"+RCFL.map(function(f){return f.n}).join("|");
  if(sig!=RCSIG){
    RCSIG=sig;
    rcFTab()
  }else{
    for(i=0;i<RCFL.length;i++){
      $("rc_fs"+i).textContent=fSize(RCFL[i].s);         /* stessa lista: si aggiornano solo le dimensioni, cosi il focus resta dov'e */
      $("rc_fd"+i).textContent=rcFData(RCFL[i])
    }
  }
  rcFBtn()
}

function rcFTab(){
  var tb=$("rc_ftab"),x=document.activeElement,keep=x&&tb.contains(x)?x.getAttribute("data-n"):null,h,i,f;
  if(!RCFL.length){
    tb.className="ftab hide";
    tb.innerHTML="";
    return
  }
  tb.className="ftab";
  tb.setAttribute("aria-label",t("File del registratore"));
  h='<thead><tr><th scope="col"><input type="checkbox" id="rc_fall" aria-label="'+esc(t("Seleziona tutti i file"))+
    '" onchange="rcPickAll(this.checked)"></th><th scope="col">'+esc(t("Nome"))+'</th><th scope="col" class="fsz">'+esc(t("Dimensione"))+
    '</th><th scope="col">'+esc(t("Data (dal nome)"))+"</th></tr></thead><tbody>";
  for(i=0;i<RCFL.length;i++){
    f=RCFL[i];
    h+='<tr><td><input type="checkbox" id="rc_fc'+i+'" data-n="'+esc(f.n)+'" aria-label="'+esc(tf("Seleziona {0}",f.n))+
      '"'+(RCSEL[f.n]?" checked":"")+' onchange="rcPick('+i+',this.checked)"></td><td>'+esc(f.n)+(f.o?" ("+esc(t("altro nome"))+")":"")+
      '</td><td class="fsz" id="rc_fs'+i+'">'+esc(fSize(f.s))+'</td><td id="rc_fd'+i+'">'+esc(rcFData(f))+"</td></tr>"
  }
  tb.innerHTML=h+"</tbody>";
  if(keep)for(i=0;i<RCFL.length;i++)if(RCFL[i].n==keep)$("rc_fc"+i).focus()
}

function rcPick(i,on){
  if(on)RCSEL[RCFL[i].n]=1;
  else delete RCSEL[RCFL[i].n];
  rcFBtn()
}

function rcPickAll(on){
  for(var i=0;i<RCFL.length;i++){
    $("rc_fc"+i).checked=on;
    if(on)RCSEL[RCFL[i].n]=1;
    else delete RCSEL[RCFL[i].n]
  }
  rcFBtn()
}

function rcSelNomi(){
  return RCFL.filter(function(f){return RCSEL[f.n]}).map(function(f){return f.n})
}

/* conteggio, bottoni attivi solo con una selezione, casella "tutti" (anche a meta) */
function rcFBtn(){
  var n=rcSelNomi().length,all=$("rc_fall");
  $("rc_fsel").textContent=n?tf("{0} file selezionati",n):(RCFL.length?t("Nessun file selezionato"):"");
  $("rc_fdl").disabled=!n;
  $("rc_fdel").disabled=!n;
  if(all){
    all.checked=n>0&&n==RCFL.length;
    all.indeterminate=n>0&&n<RCFL.length
  }
}

/* uno alla volta con la rotta dei file: i browser accettano lo scarico di piu file se non partono tutti insieme */
function rcScarica(){
  var nomi=rcSelNomi(),i=0,dir=RCDIR;
  if(!nomi.length)return;
  msg("rcm2",tf("Scarico {0} file",nomi.length),true);
  (function next(){
    if(i>=nomi.length)return;
    var a=document.createElement("a");
    a.href="/api/fs/get?path="+enc(dir+"/"+nomi[i]);
    a.download=nomi[i];
    a.style.display="none";
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    i++;
    setTimeout(next,700)
  })()
}

/* solo Admin: la rotta e il suo controllo di ruolo e dei percorsi protetti sono quelli della pagina File */
function rcElimina(){
  var nomi=rcSelNomi(),dir=RCDIR;
  if(!nomi.length)return;
  dlg({title:t("Eliminare i file selezionati?"),
    body:'<p style="margin:0">'+esc(tf("Si eliminano {0} file dalla cartella {1}.",nomi.length,dir))+
      '</p><p style="margin:8px 0 0;color:var(--mut)">'+esc(t("Non si puo annullare."))+"</p>",
    ok:t("Elimina"),danger:true,
    onOk:function(){
      var fatti=0,falliti=[];
      return nomi.reduce(function(p,n){
        return p.then(function(){
          return post("/api/fs/del",{path:dir+"/"+n}).then(function(r){return r.json()}).then(function(j){
            if(j.ok)fatti++;
            else falliti.push(n)
          }).catch(function(){falliti.push(n)})
        })
      },Promise.resolve()).then(function(){
        RCSEL={};
        RCSIG="";
        rcLoad();
        if(falliti.length)return tf("Non si sono eliminati {0} file: {1}",falliti.length,falliti.join(", "));
        msg("rcm2",tf("Eliminati {0} file",fatti),true);
        return ""
      })
    }})
}
