/* ===== File: elenco, menu azioni, finestre, editor ===== */
var FS={list:[],used:0,total:0,max:32768,menu:null,lp:null};
var FTXT=
  /\.(txt|md|log|csv|html?|css|js|conf|cfg|ini|xml|ya?ml|h|c|cpp|ino|py|sh|svg|rules|vsb|vsc|vab|vap)$/i,
  FIMG=/\.(png|jpe?g|gif|svg|ico|bmp|webp)$/i;
var FCODE=/\.(html?|css|js|h|c|cpp|ino|py|sh|xml|vsb|vsc|vab|vap)$/i;
function fIco(f){
  return f.d?"i-folder":/\.json$/i.test(f.n)?"i-fjson":FIMG.test(f.n)?"i-fimg"
    :/\.vs[bc]$/i.test(f.n)?"i-fvsc":/\.va[bp]$/i.test(f.n)?"i-fvap":FCODE.test(f.n)?"i-fcode"
    :(FTXT.test(f.n)||f.n.indexOf(".")<0)?"i-ftxt":"i-fbin"
}

function fEditable(n){return /\.json$/i.test(n)||FTXT.test(n)||n.indexOf(".")<0}
function fSize(n){
  return n<1024?n+" B":n<1048576?(n/1024).toFixed(n<10240?1:0)+" KB":(n/1048576).toFixed(1)+" MB"
}

function fDir(p){
  var i=p.lastIndexOf("/");
  return i<=0?"/":p.substring(0,i)
}

function fBase(p){return p.substring(p.lastIndexOf("/")+1)}
function fName(n){
  n=(n||"").trim();
  if(!n)return t("Scrivi un nome");
  if(n.length>60)return t("Nome troppo lungo (max 60)");
  if(/[\/\\]/.test(n)||n.indexOf("..")>=0)return t("Il nome non puo contenere / \\ o ..");
  if(/[^\x20-\x7e]/.test(n))return t("Usa solo lettere senza accenti, numeri, spazio . - _");
  if(/\.tmp~$/.test(n)||n=="vesevos.conf")return t("Nome riservato al sistema");
  return""
}

function fSys(p){return p=="/rules.txt"||p.indexOf("/lang/")==0}
function fList(p){
  fp=p;
  fMenuClose();
  api("/api/fs/list?path="+enc(p)).then(function(r){return r.json()}).then(function(j){
    if(!j.ok){
      msg("fm",j.err,false);
      if(p!="/")fList("/");
      return
    }
    fp=j.path;
    FS.list=j.list;
    if(j.more)msg("fm",tf("Elenco parziale: mostrate le prime {0} voci",j.list.length),true);
    var u=j.u||1;
    j.used*=u;
    j.total*=u;
    FS.used=j.used;
    FS.total=j.total;
    if(j.max)FS.max=j.max;
    var sd=j.path=="/media/sd"||j.path.indexOf("/media/sd/")==0,base=sd?"/media/sd":"",
      parts=j.path.slice(base.length).split("/").filter(Boolean),acc=base,
      c='<button class="crb" onclick="fList(\'/\')">'+esc(t("Memoria interna"))+
      '</button><button class="crb" onclick="fList(\'/media/sd\')">'+esc(t("microSD"))+'</button>';
    parts.forEach(function(x){
      acc+="/"+x;
      c+='<span aria-hidden="true">›</span><button class="crb" onclick="fList(this.dataset.p)" data-p="'+
        esc(acc)+'">'+esc(x)+'</button>'
    });
    $("fpath").innerHTML=c;
    $("fupb").disabled=j.path=="/";
    j.list.sort(function(a,b){return (b.d-a.d)||a.n.localeCompare(b.n)});
    var h='<thead><tr><th>'+esc(t("Nome"))+'</th><th class="fsz">'+esc(t("Dimensione"))+
      '</th><th class="fac"><span class="sr">'+esc(t("Azioni"))+'</span></th></tr></thead><tbody>';
    if(!j.list.length)h+='<tr><td colspan="3" style="color:var(--mut)">'+esc(t("Cartella vuota"))+
      '</td></tr>';
    j.list.forEach(function(f,i){
      var kind=f.d?t("Cartella"):t("File");
      h+='<tr data-i="'+i+'"><td><button class="fnm" onclick="fOpen('+i+')" title="'+
        esc(f.d?t("Apri la cartella"):fEditable(f.n)?t("Apri nell'editor"):t("Scarica"))+
        '"><svg class="i fi" aria-hidden="true"><use href="#'+fIco(f)+'"/></svg><span>'+esc(f.n)+
        '</span><span class="sr"> ('+esc(kind)+')</span></button></td>'+
        '<td class="fsz">'+(f.d?(f.s?fSize(f.s):""):fSize(f.s))+
        '</td><td class="fac"><button class="ib fmb" onclick="fMenu(event,'+i+
        ')" aria-haspopup="menu" aria-label="'+esc(tf("Azioni per {0}",f.n))+'" title="'+
        esc(t("Azioni"))+'"><svg class="i"><use href="#i-more"/></svg></button></td></tr>'
    });
    $("ftab").innerHTML=h+'</tbody>';
    fPie(j);
    var pc=j.total?Math.round(100*j.used/j.total):0;
    $("fspace").innerHTML='<div class="fbar" role="img" aria-label="'+
      esc(tf("Usati {0} su {1}",kb(j.used),kb(j.total)))+'"><i style="width:'+pc+'%;background:'+
      hcol(pc,75,90)+'"></i></div>'+esc(tf("Usati {0} su {1}",kb(j.used),kb(j.total)))+" · "+
      esc(tf("liberi {0}",kb(j.total-j.used)))
  })
}
var PIEC=["#3fa7ff","#37e0b0","#ffb547","#c27cff","#ff7a90","#7bd389"];
function fPie(j){
  var e=$("fpie");
  if(!e)return;
  var tot=j.total||1,items=
    j.list.filter(function(f){return f.s>0}).sort(function(a,b){return b.s-a.s}),sl=[],sum=0;
  items.slice(0,5).forEach(function(f,i){
    sl.push({n:f.n+(f.d?"/":""),v:f.s,c:PIEC[i]});
    sum+=f.s
  });
  var rest=items.slice(5).reduce(function(a,f){return a+f.s},0);
  if(rest){
    sl.push({n:t("Altri file"),v:rest,c:PIEC[5]});
    sum+=rest
  }
  var other=Math.max(0,j.used-sum);
  if(other)sl.push({n:fp=="/"?t("Sistema (struttura del disco)"):t("Fuori da questa cartella"),
    v:other,c:"var(--mut)"});
  sl.push({n:t("Libero"),v:Math.max(0,tot-j.used),c:"var(--trk)"});
  var a=-Math.PI/2,R=46,C=60,
    svg='<svg viewBox="0 0 120 120" class="pie" role="img" aria-label="'+
    esc(tf("Spazio: usati {0} su {1}",kb(j.used),kb(tot)))+'">';
  sl.forEach(function(x){
    var f=x.v/tot;
    if(f<=0)return;
    if(f>=.9999){
      svg+='<circle cx="60" cy="60" r="'+R+'" fill="'+x.c+'"/>';
      return
    }
    var b=a+f*2*Math.PI,l=f>.5?1:0;
    svg+='<path d="M'+C+' '+C+'L'+(C+R*Math.cos(a)).toFixed(2)+' '+(C+R*Math.sin(a)).toFixed(2)+'A'+
      R+' '+R+' 0 '+l+' 1 '+(C+R*Math.cos(b)).toFixed(2)+' '+(C+R*Math.sin(b)).toFixed(2)+
      'Z" fill="'+x.c+'" stroke="var(--card)" stroke-width="1.2"><title>'+esc(x.n)+': '+fSize(x.v)+
      '</title></path>';
    a=b
  });
  svg+='<circle cx="60" cy="60" r="24" fill="var(--card)"/><text x="60" y="58" text-anchor="middle" font-size="13" font-weight="700" fill="var(--tx)">'+
    Math.round(100*j.used/tot)+
    '%</text><text x="60" y="72" text-anchor="middle" font-size="8" fill="var(--mut)">'+
    esc(t("usato"))+'</text></svg>';
  e.innerHTML=svg+'<ul class="plg">'+sl.map(function(x){
    return'<li><i style="background:'+x.c+'"></i><span>'+esc(x.n)+'</span><b>'+fSize(x.v)+' · '+
      (100*x.v/tot).toFixed(x.v/tot<.1?1:0)+'%</b></li>'
  }).join("")+'</ul>'
}

function fUp(){
  if(fp=="/")return;
  fList(fDir(fp))
}

function fOpen(i){
  var f=FS.list[i];
  if(!f)return;
  var p=join(fp,f.n);
  if(f.d)fList(p);
  else if(fEditable(f.n))fEdit(p);
  else location.href="/api/fs/get?path="+enc(p)
}

function fDo(u,d,okt){
  return post(u,d).then(function(r){return r.json()}).then(function(j){
    if(j.ok){
      msg("fm",okt||t("Fatto"),true);
      fList(fp)
    }
    return j
  })
}
/* --- menu azioni (pulsante, tasto destro, tocco lungo) --- */
function fMenu(e,i,x,y){
  e.preventDefault();
  e.stopPropagation();
  var f=FS.list[i];
  if(!f)return;
  fMenuClose();
  var it=[];
  if(f.d)it.push(["open","i-folder","Apri"]);
  else{
    if(fEditable(f.n))it.push(["edit","i-edit","Modifica"]);
    it.push(["dl","i-dl","Scarica"])
  }
  it.push(["ren","i-ren","Rinomina"]);
  if(!f.d)it.push(["dup","i-dup","Duplica"]);
  it.push(["move","i-move","Sposta"]);
  it.push(["del","i-del","Elimina"]);
  var m=document.createElement("div");
  m.className="pop fmenu";
  m.setAttribute("role","menu");
  m.setAttribute("aria-label",tf("Azioni per {0}",f.n));
  m.innerHTML=it.map(function(a){
    return'<button role="menuitem" class="'+(a[0]=="del"?"dz":"")+'" onclick="fAct(\''+a[0]+'\','+i+
      ')"><svg class="i" aria-hidden="true"><use href="#'+a[1]+'"/></svg>'+esc(t(a[2]))+'</button>'
  }).join("");
  document.body.appendChild(m);
  FS.menu={el:m,btn:e.currentTarget&&e.currentTarget.tagName=="BUTTON"?e.currentTarget:null};
  var r=e.currentTarget&&e.currentTarget.getBoundingClientRect
    ?e.currentTarget.getBoundingClientRect():null,px=x!=null?x:(r?r.right:e.clientX),
    py=y!=null?y:(r?r.bottom+4:e.clientY);
  var w=m.offsetWidth,hh=m.offsetHeight;
  px=Math.max(8,Math.min(px-(x!=null?0:w),innerWidth-w-8));
  py=Math.max(8,py+hh>innerHeight-8?py-hh-(r&&x==null?r.height+8:0):py);
  m.style.left=px+"px";
  m.style.top=py+"px";
  m.style.right="auto";
  var b=m.querySelector("button");
  if(b)b.focus();
  m.addEventListener("keydown",function(k){
    var bs=[].slice.call(m.querySelectorAll("button")),n=bs.indexOf(document.activeElement);
    if(k.key=="ArrowDown"){
      k.preventDefault();
      bs[(n+1)%bs.length].focus()
    }else if(k.key=="ArrowUp"){
      k.preventDefault();
      bs[(n-1+bs.length)%bs.length].focus()
    }
    else if(k.key=="Escape"||k.key=="Tab"){
      k.preventDefault();
      fMenuClose(true)
    }
  })
}

function fMenuClose(back){
  if(!FS.menu)return;
  var b=FS.menu.btn;
  FS.menu.el.remove();
  FS.menu=null;
  if(back&&b)b.focus()
}
document.addEventListener("click",function(e){
  if(FS.menu&&!FS.menu.el.contains(e.target))fMenuClose()
});
(function(){
  var tb=$("ftab");
  tb.addEventListener("contextmenu",function(e){
    var tr=e.target.closest("tr[data-i]");
    if(tr)fMenu(e,+tr.dataset.i,e.clientX,e.clientY)
  });
  tb.addEventListener("pointerdown",function(e){
    if(e.pointerType!="touch")return;
    var tr=e.target.closest("tr[data-i]");
    if(!tr)return;
    var x=e.clientX,y=e.clientY;
    FS.lp=setTimeout(function(){
      FS.lp="done";
      tr.classList.add("lp");
      setTimeout(function(){tr.classList.remove("lp")},300);
      fMenu({preventDefault:function(){},stopPropagation:function(){},currentTarget:null,clientX:x,
        clientY:y},+tr.dataset.i,x,y)
    },550)
  });
  ["pointerup","pointercancel","pointermove"].forEach(function(n){
    tb.addEventListener(n,function(e){
      if(n=="pointermove"&&FS.lp&&FS.lp!="done"&&Math.abs(e.movementX)+
        Math.abs(e.movementY)<6)return;
      if(FS.lp&&FS.lp!="done")clearTimeout(FS.lp);
      if(n!="pointermove"&&FS.lp=="done"){e.preventDefault()}
      if(n!="pointermove")setTimeout(function(){FS.lp=null},0)
    })
  });
  tb.addEventListener("click",function(e){
    if(FS.lp=="done"){
      e.preventDefault();
      e.stopPropagation()
    }
  },true)
})();

function fAct(a,i){
  var f=FS.list[i];
  fMenuClose(a=="dl"||a=="edit"?false:false);
  if(!f)return;
  var p=join(fp,f.n);
  if(a=="open")fList(p);
  else if(a=="edit")fEdit(p);
  else if(a=="dl")location.href="/api/fs/get?path="+enc(p);
  else if(a=="ren")fRen(p,f);
  else if(a=="dup")fDup(p);
  else if(a=="move")fMove(p,f);
  else if(a=="del")fDel(p,f)
}

function fDirsSel(id,cur,skip){
  return api("/api/fs/dirs").then(function(r){return r.json()}).then(function(L){
    var s=$(id);
    if(!s)return;
    s.innerHTML=L.filter(function(d){
      return!skip||(d!=skip&&d.indexOf(skip+"/")!=0)
    }).map(function(d){
      return'<option'+(d==cur?" selected":"")+' value="'+esc(d)+'">'+
        esc(d=="/"?"/ ("+t("Memoria interna")+")":d)+'</option>'
    }).join("")
  }).catch(function(){})
}

function fExists(n){return FS.list.some(function(f){return f.n==n})}
/* --- azioni --- */
function fMk(){
  dlg({title:t("Nuova cartella"),
    body:'<label for="dn">'+esc(t("Nome della cartella"))+
    '</label><input id="dn" autocomplete="off" maxlength="60">',ok:t("Crea"),
    onOk:function(){
      var n=dlgV("dn"),e=fName(n);
      if(e)return e;
      if(fExists(n))return t("Esiste gia");
      return fDo("/api/fs/mkdir",{path:join(fp,n)},
        t("Cartella creata")).then(function(j){return j.ok?"":j.err})
    }})
}

function fNew(){
  dlg({title:t("Nuovo file"),
    body:'<label for="dn">'+esc(t("Nome del file (es. note.txt)"))+
    '</label><input id="dn" autocomplete="off" maxlength="60" value="nuovo.txt"><label for="dd">'+
    esc(t("Cartella"))+'</label><select id="dd"><option value="'+esc(fp)+'">'+esc(fp)+
    '</option></select>',ok:t("Crea e apri"),sel:[0,5],
    onOk:function(){
      var n=dlgV("dn"),d=$("dd").value||fp,e=fName(n);
      if(e)return e;
      if(d==fp&&fExists(n))return t("Esiste gia un file con questo nome");
      return api("/api/fs/list?path="+enc(d)).then(function(r){return r.json()}).then(function(j){
        if(j.ok&&j.list.some(function(f){return f.n==n}))return t(
          "Esiste gia un file con questo nome");
        fEdOpen(join(d,n),"",true);
        return""
      })
    }});
  fDirsSel("dd",fp)
}

function fRen(p,f){
  var n=fBase(p),dot=f.d?-1:n.lastIndexOf(".");
  dlg({title:f.d?t("Rinomina cartella"):t("Rinomina file"),
    body:'<label for="dn">'+esc(t("Nuovo nome"))+
    '</label><input id="dn" autocomplete="off" maxlength="60" value="'+esc(n)+'">',ok:t("Rinomina"),
    sel:[0,dot>0?dot:n.length],
    onOk:function(){
      var m=dlgV("dn"),e=fName(m);
      if(e)return e;
      if(m==n)return"";
      if(fExists(m))return t("Esiste gia");
      return fDo("/api/fs/ren",{from:p,to:join(fp,m)},
        t("Rinominato")).then(function(j){return j.ok?"":j.err})
    }})
}

function fCopyName(n){
  var dot=n.lastIndexOf("."),b=dot>0?n.substring(0,dot):n,x=dot>0?n.substring(dot):"",
    c=b+"-copia"+x,k=2;
  while(fExists(c))c=b+"-copia"+(k++)+x;
  return c
}

function fDup(p){
  var c=fCopyName(fBase(p)),dot=c.lastIndexOf(".");
  dlg({title:t("Duplica file"),
    body:'<p style="margin:0 0 8px;color:var(--mut)">'+esc(tf("Copia di {0}",fBase(p)))+
    '</p><label for="dn">'+esc(t("Nome della copia"))+
    '</label><input id="dn" autocomplete="off" maxlength="60" value="'+esc(c)+'">',ok:t("Duplica"),
    sel:[0,dot>0?dot:c.length],
    onOk:function(){
      var m=dlgV("dn"),e=fName(m);
      if(e)return e;
      if(fExists(m))return t("Esiste gia");
      return fDo("/api/fs/copy",{from:p,to:join(fp,m)},
        t("Duplicato")).then(function(j){return j.ok?"":j.err})
    }})
}

function fMove(p,f){
  dlg({title:f.d?t("Sposta cartella"):t("Sposta file"),
    body:'<p style="margin:0 0 8px;color:var(--mut)">'+esc(p)+'</p><label for="dd">'+
    esc(t("Sposta nella cartella"))+'</label><select id="dd"><option>...</option></select>',
    ok:t("Sposta"),
    onOk:function(){
      var d=$("dd").value;
      if(!d||d==fp)return t("Scegli un'altra cartella");
      return fDo("/api/fs/ren",{from:p,to:join(d,f.n)},
        t("Spostato")).then(function(j){return j.ok?"":j.err})
    }});
  fDirsSel("dd",fp,f.d?p:null)
}

function fDel(p,f){
  dlg({title:t("Eliminare?"),
    body:'<p style="margin:0">'+
    esc(f.d?tf("La cartella {0} e tutto il suo contenuto verranno eliminati.",p)
      :tf("Il file {0} verra eliminato.",p))+'</p><p style="margin:8px 0 0;color:var(--mut)">'+
    esc(t("Non si puo annullare."))+'</p>',ok:t("Elimina"),danger:true,
    onOk:function(){
      return fDo("/api/fs/del",{path:p},t("Eliminato")).then(function(j){return j.ok?"":j.err})
    }})
}

function fUpload(){
  var f=$("fup").files[0];
  if(!f)return;
  $("fup").value="";
  var go=function(){
    var fd=new FormData();
    fd.append("file",f,f.name);
    msg("fm",tf("Carico {0}...",f.name),true);
    api("/api/fs/up?dir="+enc(fp),
      {method:"POST",body:fd}).then(function(r){return r.json()}).then(function(j){
        msgCaricato("fm",j,t("Caricato"));
        if(j.ok)fList(fp)
      })
  };
  if(f.size+8192>FS.total-FS.used){
    msg("fm",t("Spazio esaurito"),false);
    return
  }
  if(fExists(f.name))dlg({title:t("Sostituire il file?"),
    body:'<p style="margin:0">'+esc(tf("{0} esiste gia in questa cartella.",f.name))+'</p>',
    ok:t("Sostituisci"),danger:true,onOk:function(){go()}});
  else go()
}
