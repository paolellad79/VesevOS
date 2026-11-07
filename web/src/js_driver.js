/* ===== Driver delle periferiche: UN solo elenco, UN solo Carica, le stesse azioni per i driver vdb e vde (tag piccolo). Solo Admin. ===== */
/* I pin li scegli tu (propone quelli liberi, li puoi cambiare); il sistema controlla vietati, gia usati e doppi. */
function drvTag(x){return' <span class="pill">'+esc(x.slice(1))+'</span>'}
function drvLoad(){
  var L=[],f=[api("/api/mods").then(function(r){return r.json()}).then(function(j){
    (j.list||[]).forEach(function(x){L.push({k:"b",n:x.n,x:x})})
  }).catch(function(){})];
  if(AUTH&&AUTH.elf)f.push(api("/api/vde").then(function(r){return r.json()}).then(function(j){
    (j.list||[]).forEach(function(x){L.push({k:"e",n:x.n,x:x})})
  }).catch(function(){}));
  Promise.all(f).then(function(){
    L.sort(function(a,b){return a.n<b.n?-1:a.n>b.n?1:0});
    drvDraw(L)
  })
}

function drvDraw(L){
  var el=$("drvlist");
  if(!el)return;
  if(!L.length){
    el.innerHTML='<p class="fhint">'+
      esc(t("Nessun driver installato. Caricane uno con il pulsante qui sotto."))+'</p>';
    return
  }
  var h='<table class="ftab">';
  L.forEach(function(r){
    var x=r.x,adm=ROLE>=2,btn="",st="",tit,ico="";
    if(r.k=="b"){
      tit=x.ok?x.t:x.n;
      ico=ioIco(x.ok?x.icon:"");
      st=x.ok?ioCn(x.c)+" x"+x.ns+" · "+(x.p=="-"?t("senza alimentazione"):x.p)+(x.l?" · "+x.l:"")+
        (x.app?" · "+t("con app"):""):(x.e||"");
      if(adm){
        if(x.ok)btn+='<button class="btn" onclick="ioModOpen(\''+x.n+'\')">'+esc(t("Installa"))+
          '</button> ';
        btn+='<button class="btn gray" onclick="ioModDel(\''+x.n+'\')">'+esc(t("Elimina"))+
          '</button>'
      }
    }
    else{
      tit=x.ok?x.t:x.n;
      ico=ioIco(x.ok&&x.i?x.ic:"");
      st=!x.ok?(x.e||""):x.i?(x.p.length?tf("Pin dati: {0}",x.p.join(", ")):t("Installato"))
        :t("Senza pin");
      if(adm){
        if(x.ok)btn+='<button class="btn" onclick="drvVdeOpen(\''+x.n+'\')">'+
          esc(x.i?t("Cambia pin"):t("Installa"))+'</button> ';
        btn+='<button class="btn gray" onclick="drvVdeDel(\''+x.n+'\')">'+esc(t("Elimina"))+
          '</button>'
      }
    }
    h+='<tr><td><span class="scn">'+ico+'<b>'+esc(tit)+'</b>'+drvTag(r.k=="b"?EXT.db:EXT.de)+
      '</span><div class="fhint"'+(!x.ok?' style="color:var(--kot)"':"")+'>'+esc(st)+'</div>'+
      (x.ok&&x.w?'<div class="fhint">'+esc(x.w)+'</div>':"")+
      '</td><td style="text-align:right;white-space:nowrap">'+btn+'</td></tr>'
  });
  el.innerHTML=h+'</table>'
}
/* Dialogo dei pin per un driver vde: stessi campi del vdb (un pin per segnale, quelli proposti gia scritti). */
function drvVdeOpen(n){
  api("/api/vde").then(function(r){return r.json()}).then(function(j){
    var d=(j.list||[]).filter(function(x){return x.n==n})[0];
    if(!d||!d.ok){
      msg("drvm",d?d.e:t("File non trovato"),false);
      return
    }
    var b='<p class="fhint" style="margin-bottom:2px"><b>'+esc(t("Pin"))+'</b> '+
      esc(t(
        "La scheda propone questi pin liberi. Puoi cambiarli: il sistema controlla che siano ammessi e non gia usati."))+
      '</p>';
    if(d.w)b+='<p class="fhint"><b>'+esc(d.w)+'</b></p>';
    d.s.forEach(function(s,i){
      var v=d.i&&d.p[i]!=null?d.p[i]:(d.sug&&d.sug[i]!=null?d.sug[i]:"");
      b+='<label for="drv_pn'+i+'">'+esc(s)+'</label><input id="drv_pn'+i+
        '" inputmode="numeric" maxlength="2" autocomplete="off" style="width:5em" value="'+v+'">'
    });
    dlg({title:d.t,body:b,ok:d.i?t("Cambia pin"):t("Installa"),onOk:function(){
      var pn=d.s.map(function(s,i){return dlgV("drv_pn"+i)});
      if(!pn.every(function(x){
        return /^[0-9]{1,2}$/.test(x)
      }))return t("Indica un pin per ogni segnale");
      return post("/api/vde",{act:d.i?"pins":"install",name:n,
        pins:pn.join(",")}).then(function(r){return r.json()}).then(function(k){
          if(k.ok===false)return k.err;
          msg("drvm",t("Fatto"),true);
          drvLoad();
          return""
        })
    }})
  }).catch(function(){})
}

function drvVdeDel(n){
  dlg({title:t("Eliminare?"),
    body:'<p style="margin:0">'+
    esc(tf("Il driver {0} verra eliminato e i suoi pin torneranno liberi.",n))+'</p>',
    ok:t("Elimina"),danger:true,
    onOk:function(){
      return post("/api/vde",{act:"del",
        name:n}).then(function(r){return r.json()}).then(function(j){
          if(j.ok===false)return j.err;
          drvLoad();
          return""
        })
    }})
}
/* Carica: il tipo si capisce dall'estensione. */
function drvUp(){
  var f=$("drvup").files[0];
  if(!f)return;
  $("drvup").value="";
  var low=f.name.toLowerCase(),e=low.slice(low.lastIndexOf("."));
  if(e==EXT.de){
    var fd=new FormData();
    fd.append("file",f);
    api("/api/fs/up?dir=/devs",
      {method:"POST",body:fd}).then(function(r){return r.json()}).then(function(j){
        msgCaricato("drvm",j,t("Caricato"));
        drvLoad()
      }).catch(function(){});
    return
  }
  if(e!=EXT.db&&e!=".vdp"&&e!=".txt"){
    msg("drvm",tf("Il file deve finire per {0} o {1}",EXT.db,EXT.de),false);
    return
  }
  if(f.size>16384){
    msg("drvm",t("File troppo grande (massimo 16 KB)"),false);
    return
  }
  var n=low.replace(/\.(vdb|vdp|txt)$/,"");
  f.text().then(function(x){
    return post("/api/mod/save",{name:n,text:x})
  }).then(function(r){return r.json()}).then(function(j){
    msgCaricato("drvm",j,t("Driver caricato"));
    if(j.ok)drvLoad()
  }).catch(function(){})
}
