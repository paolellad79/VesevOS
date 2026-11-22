/* ===== lingua: bandiere ===== */
function flagImg(l){
  return l&&l.flag?'<img class="flag" alt="" src="data:image/svg+xml;charset=utf-8,'+
    encodeURIComponent(l.flag)+'">':""
}

function langOf(c){
  for(var i=0;i<LANGS.length;i++)if(LANGS[i].code==c)return LANGS[i];
  return{code:c,name:c}
}

function fillLangSel(){
  var l=langOf(LANG),h=flagImg(l)+'<span>'+esc(LANG.toUpperCase())+'</span>';
  var bs=document.querySelectorAll(".lbtn");
  for(var i=0;i<bs.length;i++){
    bs[i].innerHTML=h;
    bs[i].title=t("Lingua")+": "+l.name;
    bs[i].setAttribute("aria-label",bs[i].title);
    if(bs[i].closest("#login"))bs[i].classList.toggle("hide",LANGS.length<2)
  }   // al login il selettore c'e solo con piu di una lingua
  var s=$("lc_lang");
  if(s){
    var o="";
    LANGS.forEach(function(x){
      o+='<option value="'+esc(x.code)+'"'+(x.code==LANG?" selected":"")+'>'+esc(x.name)+'</option>'
    });
    s.innerHTML=o
  }
}
var LANGB=null;   // pulsante che ha aperto il menu lingue (ci si torna con Esc)
function langClose(){
  var m=$("lpop");
  if(!m||m.classList.contains("hide"))return;
  m.className="pop hide";
  if(LANGB){
    LANGB.setAttribute("aria-expanded","false");
    if(LANGB.offsetParent)LANGB.focus()
  }
}

function langPop(e){
  e.stopPropagation();
  var m=$("lpop");
  if(!m.classList.contains("hide")){
    langClose();
    return
  }
  LANGB=e.currentTarget;
  LANGB.setAttribute("aria-expanded","true");
  var h="";
  LANGS.forEach(function(x){
    h+='<button role="menuitem" class="'+(x.code==LANG?"on":"")+'" onclick="langClose();setLang(\''+
      esc(x.code)+'\')">'+flagImg(x)+'<span><b>'+esc(x.name)+'</b><small>'+esc(x.loc||x.code)+
      '</small></span><i class="ck">&#10003;</i></button>'
  });
  h+='<div class="lloc" id="hlocn"></div>';
  m.innerHTML=h;
  if(loggedIn())homeLoc();   // prima del login /api/region non e pubblica
  var r=e.currentTarget.getBoundingClientRect();
  m.style.top=(r.bottom+6)+"px";
  m.style.right=Math.max(8,innerWidth-r.right)+"px";
  m.className="pop";
  $("thm").className="pop hide";
  $("um").className="pop hide";
  var b=m.querySelector("button");
  if(b)b.focus()
}
document.addEventListener("click",function(e){
  var m=$("lpop");
  if(m&&!m.classList.contains("hide")&&!e.target.closest("#lpop")){
    m.className="pop hide";
    if(LANGB)LANGB.setAttribute("aria-expanded","false")
  }
});

function loadLangs(){
  api("/api/langs").then(function(r){return r.json()}).then(function(l){
    LANGS=l;
    fillLangSel();
    var h="<table><tr><th>"+t("Codice")+"</th><th>"+t("Lingua")+"</th><th></th></tr>";
    l.forEach(function(x){
      h+="<tr><td>"+esc(x.code)+"</td><td>"+flagImg(x)+" "+esc(x.name)+"</td><td>"+
        (x.code=="it"?esc(t("Predefinita (nel firmware)"))
          :'<button class="btn red" onclick="delLang(\''+esc(x.code)+'\')">'+esc(t("Elimina"))+
          '</button>')+"</td></tr>"
    });
    $("lglist").innerHTML=h+"</table>"
  });
  api("/api/fs/list?path=/").then(function(r){return r.json()}).then(function(j){
    if(j.ok)$("lgsp").textContent=tf("Memoria interna: usati {0} su {1}",kb(j.used),kb(j.total))
  })
}

/* file di lingua: <codice>.vld (dizionario del firmware, obbligatorio) e <codice>.vlp (frasi della pagina, gzip, facoltativo) */
var LF_VLD_MAX=196608,LF_VLP_MAX=131072;   /* come LANG_VLD_MAX e LANG_VLP_MAX del firmware */

/* Raggruppa i file scelti per lingua. Ritorna {err} oppure {lista:[{code,vld,vlp}]} */
function langGruppi(files){
  var per={},ord=[],i,m,g;
  for(i=0;i<files.length;i++){
    m=files[i].name.match(/^([a-z0-9]{2,8})\.(vld|vlp)$/);
    if(!m||m[1]=="it")return{err:t("Il nome deve essere come en.vld o en.vlp (codice di 2-8 lettere minuscole)")};
    if(!per[m[1]]){
      per[m[1]]={code:m[1],vld:null,vlp:null};
      ord.push(m[1])
    }
    g=per[m[1]];
    if(g[m[2]])return{err:t("Scegli un solo file .vld e un solo file .vlp per lingua")};
    g[m[2]]=files[i];
    if(files[i].size>(m[2]=="vld"?LF_VLD_MAX:LF_VLP_MAX)){
      return{err:files[i].name+": "+t("File di lingua troppo grande")}
    }
  }
  var lista=ord.map(function(c){return per[c]});
  for(i=0;i<lista.length;i++){
    if(!lista[i].vld&&!LANGS.some(function(x){return x.code==lista[i].code})){
      return{err:lista[i].code+": "+t("Manca il file .vld della lingua")}
    }
  }
  return{lista:lista}
}

/* Controlla l'inizio del file: "VOSL" nel .vld, intestazione gzip (1f 8b) nel .vlp. Promessa del testo dell'errore ("" = buono) */
function langInizio(f,n){
  return f.slice(0,n).arrayBuffer().then(function(b){return new Uint8Array(b)})
}

function langControlla(g){
  var p=Promise.resolve("");
  if(g.vld){
    p=langInizio(g.vld,4).then(function(b){
      var ok=b.length==4&&String.fromCharCode(b[0],b[1],b[2],b[3])=="VOSL";
      return ok?"":g.vld.name+": "+t("File di lingua non valido")
    })
  }
  if(g.vlp){
    p=p.then(function(e){
      if(e)return e;
      return langInizio(g.vlp,2).then(function(b){
        return b.length==2&&b[0]==0x1f&&b[1]==0x8b?"":g.vlp.name+": "+t("File della pagina non valido")
      })
    })
  }
  return p
}

/* Invia i file di una lingua: prima il .vld, poi il .vlp (se il secondo fallisce la lingua funziona lo stesso, in italiano nella pagina) */
function langInvia(g){
  var fl=[g.vld,g.vlp].filter(function(f){return f}),i=0;
  return new Promise(function(ok,ko){
    (function next(){
      if(i>=fl.length){
        ok();
        return
      }
      var f=fl[i++];
      caricaPezzi("/api/fs/pezzo?dir=/lang",f,f.name,null,t("Caricamento non riuscito")).then(function(j){
          if(!j.ok){
            ko(f.name+": "+(j.err||t("Errore")));
            return
          }
          next()
        }).catch(function(){ko(t("Caricamento non riuscito"))})
    })()
  })
}

/* Controlla e invia i file scelti. Promessa con la lista delle lingue inviate; err = testo del motivo */
function langCaricaFile(files){
  var gr=langGruppi(files);
  if(gr.err)return Promise.reject(gr.err);
  return Promise.all(gr.lista.map(langControlla)).then(function(es){
    for(var k=0;k<es.length;k++)if(es[k])return Promise.reject(es[k]);
    var p=Promise.resolve();
    gr.lista.forEach(function(g){p=p.then(function(){return langInvia(g)})});
    return p.then(function(){return gr.lista})
  })
}

function upLang(){
  var fs=[].slice.call($("lgf").files||[]);
  if(!fs.length){
    msg("lgm",t("Scegli un file"),false);
    return
  }
  msg("lgm",t("Carico..."),true);
  langCaricaFile(fs).then(function(lista){
    msg("lgm",t("Lingua caricata"),true);
    $("lgf").value="";
    var uso=lista.some(function(g){return g.code==LANG});
    if(uso)setLang(LANG);
    loadLangs()
  }).catch(function(e){
    msg("lgm",String(e),false)
  })
}

function delLang(c){
  if(!confirm(tf("Eliminare la lingua {0}?",c)))return;
  var go=function(){
    /* prima il .vlp (facoltativo, puo non esserci), poi il .vld: se qualcosa va storto la lingua resta installata */
    post("/api/fs/del",{path:"/lang/"+c+".vlp"}).catch(function(){}).then(function(){
      return post("/api/fs/del",{path:"/lang/"+c+".vld"})
    }).then(function(r){return r.json()}).then(function(j){
      msg("lgm",j.ok?t("Fatto"):j.err,j.ok);
      loadLangs()
    })
  };
  if(c==LANG){
    post("/api/lang",{code:"it"}).then(function(){
      LANG="it";
      DICT={};
      try{localStorage.setItem("vl","it")}catch(e){}
      refreshView();
      go()
    })
  }else go()
}

/* la campanella: il file della lingua in uso non e valido (la scheda usa l'italiano); ricaricarlo si fa in Sistema > Localizzazione */
function langNota(n){
  if(!n||!n.k)return;
  AL.push(["w",tf("Il file della lingua {0} non e valido: ricaricalo oppure usa l'italiano",n.c),"loc"])
}
