/* --- editor --- */
var FE={p:"",isNew:false,orig:"",tabSp:true};
function fEdit(p){
  api("/api/fs/text?path="+enc(p)).then(function(r){
    return r.text().then(function(x){return{ok:r.ok,x:x}})
  }).then(function(o){
    if(!o.ok){
      msg("fm",o.x,false);
      return
    }
    fEdOpen(p,o.x,false)
  }).catch(function(){})
}

function fEdOpen(p,x,isNew){
  FE.p=p;
  FE.isNew=isNew;
  FE.orig=isNew?null:x;
  FE.back=document.activeElement;
  var e=$("fedx");
  e.value=x;
  e.scrollTop=0;
  e.scrollLeft=0;
  $("fed").className="fed";
  document.body.classList.add("fedo");
  fEdTitle();
  $("fedw").className=fSys(p)?"fedw":"fedw hide";
  $("fedrun").hidden=!fIsScript(p);
  $("fedpaste").hidden=!(navigator.clipboard&&navigator.clipboard.readText&&window.isSecureContext);
  fEdUpd();
  setTimeout(function(){
    e.focus();
    e.setSelectionRange(0,0)
  },30)
}

function fIsScript(p){return /^\/sbin\/[a-z0-9_-]{1,24}\.vs[bc]$/.test(p)}
function fEdRun(){
  var n=FE.p.replace(/^.*\//,"").replace(/\.vs[bc]$/,"");
  fEdSave().then(function(ok){
    if(!ok)return;
    fEdClose(true);
    show("script");
    skApri("script",0);
    scRun(n)
  })
}

function fMax(){
  return /\.vs[bc]$/i.test(FE.p)?8192:/\.va[bp]$/i.test(FE.p)?16384:FS.max
}

function fEdDirty(){return FE.isNew||$("fedx").value!==FE.orig}
function fEdTitle(){
  var d=fEdDirty();
  $("fedt").textContent=fBase(FE.p);
  $("fedp").textContent=fDir(FE.p);
  $("feds").textContent=FE.isNew?t("Nuovo, non ancora salvato"):d?t("Modificato"):t("Salvato");
  $("feds").className="feds"+(d?" mod":"")
}

function fEdUpd(){
  var e=$("fedx"),v=e.value,ln=v.split("\n").length,h="";
  for(var i=1;i<=ln;i++)h+=i+"\n";
  if($("fedn").textContent!=h)$("fedn").textContent=h;
  $("fedn").scrollTop=e.scrollTop;
  var b=new TextEncoder().encode(v).length,pre=v.substring(0,e.selectionStart),
    r=pre.split("\n").length,c=pre.length-pre.lastIndexOf("\n");
  $("fedpos").textContent=tf("Riga {0}, colonna {1}",r,c);
  var free=FS.total-FS.used,over=b>fMax(),
    nf=FS.total&&b+8192>free+(FE.orig!=null?new TextEncoder().encode(FE.orig).length:0);
  $("fedsz").textContent=fSize(b)+" / "+fSize(fMax())+(FS.total?" · "+tf("liberi {0}",kb(free)):"");
  $("fedsz").className=over||nf?"bad":"";
  var jm="";
  if(/\.json$/i.test(FE.p)&&v.trim()){
    var je=fJsonErr(v);
    jm=je?"⚠ "+je:"✓ "+t("JSON corretto")
  }
  $("fedjs").textContent=jm;
  $("fedjs").className=jm.charAt(0)=="⚠"?"bad":"good";
  $("fedtab").textContent=FE.tabSp?t("Tab: inserisce spazi (Ctrl+M per cambiare)")
    :t("Tab: passa al pulsante successivo (Ctrl+M per cambiare)");
  fEdTitle()
}

function fJsonErr(v){
  try{
    JSON.parse(v);
    return""
  }catch(x){
    var m=String(x.message),p=m.match(/position (\d+)/),l=m.match(/line (\d+) column (\d+)/);
    if(l)return tf("errore alla riga {0}, colonna {1}",l[1],l[2]);
    if(p){
      var pre=v.substring(0,+p[1]);
      return tf("errore alla riga {0}, colonna {1}",pre.split("\n").length,
        pre.length-pre.lastIndexOf("\n"))
    }
    return t("errore nel JSON")
  }
}

function fEdSave(asNew,path,force){
  var v=$("fedx").value,p=path||FE.p,b=new TextEncoder().encode(v).length;
  if(b>fMax()){
    msg("fedm",tf("Troppo grande: massimo {0}",fSize(fMax())),false);
    return Promise.resolve(false)
  }
  if(!force&&/\.json$/i.test(p)&&v.trim()&&fJsonErr(v)){
    dlg({title:t("Il JSON ha un errore"),
      body:'<p style="margin:0">'+esc(fJsonErr(v))+
      '</p><p style="margin:8px 0 0;color:var(--mut)">'+
      esc(t("Se lo salvi cosi, chi lo legge potrebbe non funzionare."))+'</p>',
      ok:t("Salva lo stesso"),danger:true,onOk:function(){
        fEdSave(asNew,path,true);
        return""
      }});
    return Promise.resolve(false)
  }
  return post("/api/fs/save",
    {path:p,text:v,"new":(FE.isNew||asNew)?"1"
      :"0"}).then(function(r){return r.json()}).then(function(j){
        if(!j.ok&&(FE.isNew||asNew)&&j.err==t("Esiste gia")){
          dlg({title:t("Sostituire il file?"),
            body:'<p style="margin:0">'+esc(tf("{0} esiste gia.",p))+'</p>',ok:t("Sostituisci"),
            danger:true,onOk:function(){
              FE.isNew=false;
              return post("/api/fs/save",
                {path:p,text:v,"new":"0"}).then(function(r){return r.json()}).then(function(j){
                  if(j.ok)fEdSaved(p,v);
                  return j.ok?"":j.err
                })
            }});
          return false
        }
        if(!j.ok){
          msg("fedm",j.err,false);
          return false
        }
        fEdSaved(p,v);
        return true
      })
}

function fEdSaved(p,v){
  FE.p=p;
  FE.isNew=false;
  FE.orig=v;
  msg("fedm",t("Salvato"),true);
  var lv=$("live");
  if(lv)lv.textContent=t("Salvato");
  fEdUpd();
  api("/api/fs/list?path="+enc(fp)).then(function(r){return r.json()}).then(function(j){
    if(j.ok){
      var u=j.u||1;
      FS.used=j.used*u;
      FS.total=j.total*u;
      fEdUpd()
    }
  }).catch(function(){})
}

function fEdSaveAs(){
  dlg({title:t("Salva come"),
    body:'<label for="dn">'+esc(t("Nome del file"))+
    '</label><input id="dn" autocomplete="off" maxlength="60" value="'+esc(fBase(FE.p))+
    '"><label for="dd">'+esc(t("Cartella"))+'</label><select id="dd"><option value="'+
    esc(fDir(FE.p))+'">'+esc(fDir(FE.p))+'</option></select>',ok:t("Salva"),
    onOk:function(){
      var n=dlgV("dn"),d=$("dd").value,e=fName(n);
      if(e)return e;
      fEdSave(true,join(d,n));
      return""
    }});
  fDirsSel("dd",fDir(FE.p))
}

function fEdPaste(){
  navigator.clipboard.readText().then(function(x){
    var e=$("fedx");
    e.focus();
    e.setRangeText(x,e.selectionStart,e.selectionEnd,"end");
    fEdUpd()
  }).catch(function(){
    msg("fedm",t("Il browser non permette di leggere gli appunti: usa Ctrl+V"),false)
  })
}

function fEdClear(){
  dlg({title:t("Svuotare il file?"),
    body:'<p style="margin:0">'+
    esc(t("Il testo viene cancellato dall'editor. Il file cambia solo quando premi Salva."))+'</p>',
    ok:t("Svuota"),danger:true,onOk:function(){
      var e=$("fedx");
      e.value="";
      fEdUpd();
      setTimeout(function(){e.focus()},40);
      return""
    }})
}

function fEdDl(){
  var a=document.createElement("a");
  a.href=URL.createObjectURL(new Blob([$("fedx").value],{type:"text/plain"}));
  a.download=fBase(FE.p);
  document.body.appendChild(a);
  a.click();
  setTimeout(function(){
    URL.revokeObjectURL(a.href);
    a.remove()
  },500)
}

function fEdClose(force){
  if(!force&&fEdDirty()&&!(FE.isNew&&!$("fedx").value)){
    dlg({title:t("Modifiche non salvate"),
      body:'<p style="margin:0">'+esc(tf("Vuoi salvare {0} prima di chiudere?",fBase(FE.p)))+'</p>',
      buttons:[["cancel",t("Annulla"),"gray"],["drop",t("Non salvare"),"red"],["ok",t("Salva"),""]],
      onOk:function(k){
        if(k=="drop"){
          fEdClose(true);
          return""
        }
        return fEdSave().then(function(ok){
          if(ok)fEdClose(true);
          return""
        })
      }});
    return
  }
  $("fed").className="fed hide";
  document.body.classList.remove("fedo");
  if(FE.back&&FE.back.focus&&document.body.contains(FE.back))FE.back.focus();
  fList(fp)
}
  (function(){
    var e=$("fedx");
    ["input","click","keyup","select"].forEach(function(n){e.addEventListener(n,fEdUpd)});
    e.addEventListener("scroll",function(){$("fedn").scrollTop=e.scrollTop});
    e.addEventListener("keydown",function(k){
      if(k.key=="Tab"&&FE.tabSp&&!k.ctrlKey&&!k.altKey){
        k.preventDefault();
        var s=e.selectionStart,v=e.value;
        if(k.shiftKey){
          var ls=v.lastIndexOf("\n",s-1)+1,n=v.substr(ls,2)=="  "?2:v.charAt(ls)==" "?1:0;
          if(n){
            e.setRangeText("",ls,ls+n,"preserve");
            e.selectionStart=e.selectionEnd=Math.max(ls,s-n)
          }
        }
        else e.setRangeText("  ",s,e.selectionEnd,"end");
        fEdUpd()
      }
      else if((k.ctrlKey||k.metaKey)&&(k.key=="m"||k.key=="M")){
        k.preventDefault();
        FE.tabSp=!FE.tabSp;
        fEdUpd();
        var lv=$("live");
        if(lv)lv.textContent=$("fedtab").textContent
      }
    })
  })();
$("fed").addEventListener("keydown",function(k){
  if((k.ctrlKey||k.metaKey)&&(k.key=="s"||k.key=="S")){
    k.preventDefault();
    fEdSave()
  }else if(k.key=="Escape"&&!DLG){
    k.preventDefault();
    fEdClose()
  }
});
window.addEventListener("beforeunload",function(e){
  if($("fed").className=="fed"&&fEdDirty()&&$("fedx").value){
    e.preventDefault();
    e.returnValue=""
  }
});
