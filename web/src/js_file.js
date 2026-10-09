/* ===== File: elenco, menu azioni, finestre, editor ===== */
var FS={list:[],used:0,total:0,max:32768,menu:null,lp:null};
var FTXT=/\.(txt|md|log|csv|html?|css|js|conf|cfg|ini|xml|ya?ml|h|c|cpp|ino|py|sh|svg|rules)$/i,FIMG=/\.(png|jpe?g|gif|svg|ico|bmp|webp)$/i;
var FCODE=/\.(html?|css|js|h|c|cpp|ino|py|sh|xml)$/i;
function fIco(f){return f.d?"i-folder":/\.json$/i.test(f.n)?"i-fjson":FIMG.test(f.n)?"i-fimg":FCODE.test(f.n)?"i-fcode":(FTXT.test(f.n)||f.n.indexOf(".")<0)?"i-ftxt":"i-fbin"}
function fEditable(n){return /\.json$/i.test(n)||FTXT.test(n)||n.indexOf(".")<0}
function fSize(n){return n<1024?n+" B":n<1048576?(n/1024).toFixed(n<10240?1:0)+" KB":(n/1048576).toFixed(1)+" MB"}
function fDir(p){var i=p.lastIndexOf("/");return i<=0?"/":p.substring(0,i)}
function fBase(p){return p.substring(p.lastIndexOf("/")+1)}
function fName(n){n=(n||"").trim();if(!n)return t("Scrivi un nome");if(n.length>60)return t("Nome troppo lungo (max 60)");
 if(/[\/\\]/.test(n)||n.indexOf("..")>=0)return t("Il nome non puo contenere / \\ o ..");if(/[^\x20-\x7e]/.test(n))return t("Usa solo lettere senza accenti, numeri, spazio . - _");
 if(/\.tmp~$/.test(n)||n=="vesevos.conf")return t("Nome riservato al sistema");return""}
function fSys(p){return p=="/rules.txt"||p.indexOf("/lang/")==0}
function fList(p){fp=p;fMenuClose();api("/api/fs/list?path="+enc(p)).then(function(r){return r.json()}).then(function(j){
 if(!j.ok){msg("fm",j.err,false);if(p!="/")fList("/");return}fp=j.path;FS.list=j.list;if(j.more)msg("fm",tf("Elenco parziale: mostrate le prime {0} voci",j.list.length),true);FS.used=j.used;FS.total=j.total;if(j.max)FS.max=j.max;
 var parts=j.path.split("/").filter(Boolean),acc="",c='<button class="crb" onclick="fList(\'/\')">'+esc(t("Memoria interna"))+'</button>';
 parts.forEach(function(x){acc+="/"+x;c+='<span aria-hidden="true">›</span><button class="crb" onclick="fList(this.dataset.p)" data-p="'+esc(acc)+'">'+esc(x)+'</button>'});
 $("fpath").innerHTML=c;$("fupb").disabled=j.path=="/";
 j.list.sort(function(a,b){return (b.d-a.d)||a.n.localeCompare(b.n)});
 var h='<thead><tr><th>'+esc(t("Nome"))+'</th><th class="fsz">'+esc(t("Dimensione"))+'</th><th class="fac"><span class="sr">'+esc(t("Azioni"))+'</span></th></tr></thead><tbody>';
 if(!j.list.length)h+='<tr><td colspan="3" style="color:var(--mut)">'+esc(t("Cartella vuota"))+'</td></tr>';
 j.list.forEach(function(f,i){var kind=f.d?t("Cartella"):t("File");
  h+='<tr data-i="'+i+'"><td><button class="fnm" onclick="fOpen('+i+')" title="'+esc(f.d?t("Apri la cartella"):fEditable(f.n)?t("Apri nell'editor"):t("Scarica"))+'"><svg class="i fi" aria-hidden="true"><use href="#'+fIco(f)+'"/></svg><span>'+esc(f.n)+'</span><span class="sr"> ('+esc(kind)+')</span></button></td>'+
   '<td class="fsz">'+(f.d?(f.s?fSize(f.s):""):fSize(f.s))+'</td><td class="fac"><button class="ib fmb" onclick="fMenu(event,'+i+')" aria-haspopup="menu" aria-label="'+esc(tf("Azioni per {0}",f.n))+'" title="'+esc(t("Azioni"))+'"><svg class="i"><use href="#i-more"/></svg></button></td></tr>'});
 $("ftab").innerHTML=h+'</tbody>';
 fPie(j);var pc=j.total?Math.round(100*j.used/j.total):0;$("fspace").innerHTML='<div class="fbar" role="img" aria-label="'+esc(tf("Usati {0} su {1}",kb(j.used),kb(j.total)))+'"><i style="width:'+pc+'%;background:'+hcol(pc,75,90)+'"></i></div>'+esc(tf("Usati {0} su {1}",kb(j.used),kb(j.total)))+" · "+esc(tf("liberi {0}",kb(j.total-j.used)))})}
var PIEC=["#3fa7ff","#37e0b0","#ffb547","#c27cff","#ff7a90","#7bd389"];
function fPie(j){var e=$("fpie");if(!e)return;var tot=j.total||1,items=j.list.filter(function(f){return f.s>0}).sort(function(a,b){return b.s-a.s}),sl=[],sum=0;
 items.slice(0,5).forEach(function(f,i){sl.push({n:f.n+(f.d?"/":""),v:f.s,c:PIEC[i]});sum+=f.s});
 var rest=items.slice(5).reduce(function(a,f){return a+f.s},0);if(rest){sl.push({n:t("Altri file"),v:rest,c:PIEC[5]});sum+=rest}
 var other=Math.max(0,j.used-sum);if(other)sl.push({n:fp=="/"?t("Sistema (struttura del disco)"):t("Fuori da questa cartella"),v:other,c:"var(--mut)"});
 sl.push({n:t("Libero"),v:Math.max(0,tot-j.used),c:"var(--trk)"});
 var a=-Math.PI/2,R=46,C=60,svg='<svg viewBox="0 0 120 120" class="pie" role="img" aria-label="'+esc(tf("Spazio: usati {0} su {1}",kb(j.used),kb(tot)))+'">';
 sl.forEach(function(x){var f=x.v/tot;if(f<=0)return;if(f>=.9999){svg+='<circle cx="60" cy="60" r="'+R+'" fill="'+x.c+'"/>';return}
  var b=a+f*2*Math.PI,l=f>.5?1:0;svg+='<path d="M'+C+' '+C+'L'+(C+R*Math.cos(a)).toFixed(2)+' '+(C+R*Math.sin(a)).toFixed(2)+'A'+R+' '+R+' 0 '+l+' 1 '+(C+R*Math.cos(b)).toFixed(2)+' '+(C+R*Math.sin(b)).toFixed(2)+'Z" fill="'+x.c+'" stroke="var(--card)" stroke-width="1.2"><title>'+esc(x.n)+': '+fSize(x.v)+'</title></path>';a=b});
 svg+='<circle cx="60" cy="60" r="24" fill="var(--card)"/><text x="60" y="58" text-anchor="middle" font-size="13" font-weight="700" fill="var(--tx)">'+Math.round(100*j.used/tot)+'%</text><text x="60" y="72" text-anchor="middle" font-size="8" fill="var(--mut)">'+esc(t("usato"))+'</text></svg>';
 e.innerHTML=svg+'<ul class="plg">'+sl.map(function(x){return'<li><i style="background:'+x.c+'"></i><span>'+esc(x.n)+'</span><b>'+fSize(x.v)+' · '+(100*x.v/tot).toFixed(x.v/tot<.1?1:0)+'%</b></li>'}).join("")+'</ul>'}
function fUp(){if(fp=="/")return;fList(fDir(fp))}
function fOpen(i){var f=FS.list[i];if(!f)return;var p=join(fp,f.n);if(f.d)fList(p);else if(fEditable(f.n))fEdit(p);else location.href="/api/fs/get?path="+enc(p)}
function fDo(u,d,okt){return post(u,d).then(function(r){return r.json()}).then(function(j){if(j.ok){msg("fm",okt||t("Fatto"),true);fList(fp)}return j})}
/* --- menu azioni (pulsante, tasto destro, tocco lungo) --- */
function fMenu(e,i,x,y){e.preventDefault();e.stopPropagation();var f=FS.list[i];if(!f)return;fMenuClose();
 var it=[];if(f.d)it.push(["open","i-folder","Apri"]);else{if(fEditable(f.n))it.push(["edit","i-edit","Modifica"]);it.push(["dl","i-dl","Scarica"])}
 it.push(["ren","i-ren","Rinomina"]);if(!f.d)it.push(["dup","i-dup","Duplica"]);it.push(["move","i-move","Sposta"]);it.push(["del","i-del","Elimina"]);
 var m=document.createElement("div");m.className="pop fmenu";m.setAttribute("role","menu");m.setAttribute("aria-label",tf("Azioni per {0}",f.n));
 m.innerHTML=it.map(function(a){return'<button role="menuitem" class="'+(a[0]=="del"?"dz":"")+'" onclick="fAct(\''+a[0]+'\','+i+')"><svg class="i" aria-hidden="true"><use href="#'+a[1]+'"/></svg>'+esc(t(a[2]))+'</button>'}).join("");
 document.body.appendChild(m);FS.menu={el:m,btn:e.currentTarget&&e.currentTarget.tagName=="BUTTON"?e.currentTarget:null};
 var r=e.currentTarget&&e.currentTarget.getBoundingClientRect?e.currentTarget.getBoundingClientRect():null,px=x!=null?x:(r?r.right:e.clientX),py=y!=null?y:(r?r.bottom+4:e.clientY);
 var w=m.offsetWidth,hh=m.offsetHeight;px=Math.max(8,Math.min(px-(x!=null?0:w),innerWidth-w-8));py=Math.max(8,py+hh>innerHeight-8?py-hh-(r&&x==null?r.height+8:0):py);
 m.style.left=px+"px";m.style.top=py+"px";m.style.right="auto";var b=m.querySelector("button");if(b)b.focus();
 m.addEventListener("keydown",function(k){var bs=[].slice.call(m.querySelectorAll("button")),n=bs.indexOf(document.activeElement);
  if(k.key=="ArrowDown"){k.preventDefault();bs[(n+1)%bs.length].focus()}else if(k.key=="ArrowUp"){k.preventDefault();bs[(n-1+bs.length)%bs.length].focus()}
  else if(k.key=="Escape"||k.key=="Tab"){k.preventDefault();fMenuClose(true)}})}
function fMenuClose(back){if(!FS.menu)return;var b=FS.menu.btn;FS.menu.el.remove();FS.menu=null;if(back&&b)b.focus()}
document.addEventListener("click",function(e){if(FS.menu&&!FS.menu.el.contains(e.target))fMenuClose()});
(function(){var tb=$("ftab");
 tb.addEventListener("contextmenu",function(e){var tr=e.target.closest("tr[data-i]");if(tr)fMenu(e,+tr.dataset.i,e.clientX,e.clientY)});
 tb.addEventListener("pointerdown",function(e){if(e.pointerType!="touch")return;var tr=e.target.closest("tr[data-i]");if(!tr)return;var x=e.clientX,y=e.clientY;
  FS.lp=setTimeout(function(){FS.lp="done";tr.classList.add("lp");setTimeout(function(){tr.classList.remove("lp")},300);fMenu({preventDefault:function(){},stopPropagation:function(){},currentTarget:null,clientX:x,clientY:y},+tr.dataset.i,x,y)},550)});
 ["pointerup","pointercancel","pointermove"].forEach(function(n){tb.addEventListener(n,function(e){if(n=="pointermove"&&FS.lp&&FS.lp!="done"&&Math.abs(e.movementX)+Math.abs(e.movementY)<6)return;if(FS.lp&&FS.lp!="done")clearTimeout(FS.lp);if(n!="pointermove"&&FS.lp=="done"){e.preventDefault()}if(n!="pointermove")setTimeout(function(){FS.lp=null},0)})});
 tb.addEventListener("click",function(e){if(FS.lp=="done"){e.preventDefault();e.stopPropagation()}},true)})();
function fAct(a,i){var f=FS.list[i];fMenuClose(a=="dl"||a=="edit"?false:false);if(!f)return;var p=join(fp,f.n);
 if(a=="open")fList(p);else if(a=="edit")fEdit(p);else if(a=="dl")location.href="/api/fs/get?path="+enc(p);
 else if(a=="ren")fRen(p,f);else if(a=="dup")fDup(p);else if(a=="move")fMove(p,f);else if(a=="del")fDel(p,f)}
/* --- finestra di dialogo generica (accessibile) --- */
var DLG=null;
function dlg(o){dlgClose(true);var w=$("dlg");DLG={o:o,back:document.activeElement};
 $("dlgt").textContent=o.title;$("dlgb").innerHTML=o.body||"";$("dlgm").className="msg";
 var bs=o.buttons||[["cancel",t("Annulla"),"gray"],["ok",o.ok||t("OK"),o.danger?"red":""]];
 $("dlga").innerHTML=bs.map(function(b){return'<button type="'+(b[0]=="ok"?"submit":"button")+'" class="btn '+(b[2]||"")+'" data-k="'+b[0]+'">'+esc(b[1])+'</button>'}).join("");
 [].forEach.call($("dlga").querySelectorAll("button[type=button]"),function(b){b.onclick=function(){dlgBtn(b.dataset.k)}});
 w.className="dlgw";var f=w.querySelector("input,select,textarea")||w.querySelector(".btn:not(.gray)")||w.querySelector(".btn");
 setTimeout(function(){if(!f)return;f.focus();if(o.sel&&f.setSelectionRange)f.setSelectionRange(o.sel[0],o.sel[1])},30)}
function dlgBtn(k){if(!DLG)return;var o=DLG.o;if(k=="cancel"){dlgClose();return}
 var r=o.onOk?o.onOk(k):null;if(r&&r.then){[].forEach.call($("dlga").querySelectorAll("button"),function(b){b.disabled=true});
  r.then(function(e){[].forEach.call($("dlga").querySelectorAll("button"),function(b){b.disabled=false});if(e)msg("dlgm",e,false);else dlgClose()})}
 else if(typeof r=="string"&&r)msg("dlgm",r,false);else dlgClose()}
function dlgClose(quiet){var w=$("dlg");if(!DLG){w.className="dlgw hide";return}var b=DLG.back,o=DLG.o;DLG=null;w.className="dlgw hide";if(!quiet){if(o.onCancel&&!o._done)o.onCancel();if(b&&b.focus)b.focus()}}
$("dlg").addEventListener("keydown",function(e){if(e.key=="Escape"){e.preventDefault();dlgClose()}else if(e.key=="Tab"){var fs=[].slice.call($("dlg").querySelectorAll("input,select,textarea,button:not([disabled])")),a=fs[0],z=fs[fs.length-1];
 if(e.shiftKey&&document.activeElement==a){e.preventDefault();z.focus()}else if(!e.shiftKey&&document.activeElement==z){e.preventDefault();a.focus()}}});
function dlgV(id){return($(id).value||"").trim()}
function fDirsSel(id,cur,skip){return api("/api/fs/dirs").then(function(r){return r.json()}).then(function(L){var s=$(id);if(!s)return;
 s.innerHTML=L.filter(function(d){return!skip||(d!=skip&&d.indexOf(skip+"/")!=0)}).map(function(d){return'<option'+(d==cur?" selected":"")+' value="'+esc(d)+'">'+esc(d=="/"?"/ ("+t("Memoria interna")+")":d)+'</option>'}).join("")}).catch(function(){})}
function fExists(n){return FS.list.some(function(f){return f.n==n})}
/* --- azioni --- */
function fMk(){dlg({title:t("Nuova cartella"),body:'<label for="dn">'+esc(t("Nome della cartella"))+'</label><input id="dn" autocomplete="off" maxlength="60">',ok:t("Crea"),
 onOk:function(){var n=dlgV("dn"),e=fName(n);if(e)return e;if(fExists(n))return t("Esiste gia");return fDo("/api/fs/mkdir",{path:join(fp,n)},t("Cartella creata")).then(function(j){return j.ok?"":j.err})}})}
function fNew(){dlg({title:t("Nuovo file"),body:'<label for="dn">'+esc(t("Nome del file (es. note.txt)"))+'</label><input id="dn" autocomplete="off" maxlength="60" value="nuovo.txt"><label for="dd">'+esc(t("Cartella"))+'</label><select id="dd"><option value="'+esc(fp)+'">'+esc(fp)+'</option></select>',ok:t("Crea e apri"),sel:[0,5],
 onOk:function(){var n=dlgV("dn"),d=$("dd").value||fp,e=fName(n);if(e)return e;if(d==fp&&fExists(n))return t("Esiste gia un file con questo nome");
  return api("/api/fs/list?path="+enc(d)).then(function(r){return r.json()}).then(function(j){if(j.ok&&j.list.some(function(f){return f.n==n}))return t("Esiste gia un file con questo nome");fEdOpen(join(d,n),"",true);return""})}});fDirsSel("dd",fp)}
function fRen(p,f){var n=fBase(p),dot=f.d?-1:n.lastIndexOf(".");dlg({title:f.d?t("Rinomina cartella"):t("Rinomina file"),body:'<label for="dn">'+esc(t("Nuovo nome"))+'</label><input id="dn" autocomplete="off" maxlength="60" value="'+esc(n)+'">',ok:t("Rinomina"),sel:[0,dot>0?dot:n.length],
 onOk:function(){var m=dlgV("dn"),e=fName(m);if(e)return e;if(m==n)return"";if(fExists(m))return t("Esiste gia");return fDo("/api/fs/ren",{from:p,to:join(fp,m)},t("Rinominato")).then(function(j){return j.ok?"":j.err})}})}
function fCopyName(n){var dot=n.lastIndexOf("."),b=dot>0?n.substring(0,dot):n,x=dot>0?n.substring(dot):"",c=b+"-copia"+x,k=2;while(fExists(c))c=b+"-copia"+(k++)+x;return c}
function fDup(p){var c=fCopyName(fBase(p)),dot=c.lastIndexOf(".");dlg({title:t("Duplica file"),body:'<p style="margin:0 0 8px;color:var(--mut)">'+esc(tf("Copia di {0}",fBase(p)))+'</p><label for="dn">'+esc(t("Nome della copia"))+'</label><input id="dn" autocomplete="off" maxlength="60" value="'+esc(c)+'">',ok:t("Duplica"),sel:[0,dot>0?dot:c.length],
 onOk:function(){var m=dlgV("dn"),e=fName(m);if(e)return e;if(fExists(m))return t("Esiste gia");return fDo("/api/fs/copy",{from:p,to:join(fp,m)},t("Duplicato")).then(function(j){return j.ok?"":j.err})}})}
function fMove(p,f){dlg({title:f.d?t("Sposta cartella"):t("Sposta file"),body:'<p style="margin:0 0 8px;color:var(--mut)">'+esc(p)+'</p><label for="dd">'+esc(t("Sposta nella cartella"))+'</label><select id="dd"><option>...</option></select>',ok:t("Sposta"),
 onOk:function(){var d=$("dd").value;if(!d||d==fp)return t("Scegli un'altra cartella");return fDo("/api/fs/ren",{from:p,to:join(d,f.n)},t("Spostato")).then(function(j){return j.ok?"":j.err})}});fDirsSel("dd",fp,f.d?p:null)}
function fDel(p,f){dlg({title:t("Eliminare?"),body:'<p style="margin:0">'+esc(f.d?tf("La cartella {0} e tutto il suo contenuto verranno eliminati.",p):tf("Il file {0} verra eliminato.",p))+'</p><p style="margin:8px 0 0;color:var(--mut)">'+esc(t("Non si puo annullare."))+'</p>',ok:t("Elimina"),danger:true,
 onOk:function(){return fDo("/api/fs/del",{path:p},t("Eliminato")).then(function(j){return j.ok?"":j.err})}})}
function fUpload(){var f=$("fup").files[0];if(!f)return;$("fup").value="";
 var go=function(){var fd=new FormData();fd.append("file",f,f.name);msg("fm",tf("Carico {0}...",f.name),true);
  api("/api/fs/up?dir="+enc(fp),{method:"POST",body:fd}).then(function(r){return r.json()}).then(function(j){msg("fm",j.ok?t("Caricato"):j.err,j.ok);if(j.ok)fList(fp)})};
 if(f.size+8192>FS.total-FS.used){msg("fm",t("Spazio esaurito"),false);return}
 if(fExists(f.name))dlg({title:t("Sostituire il file?"),body:'<p style="margin:0">'+esc(tf("{0} esiste gia in questa cartella.",f.name))+'</p>',ok:t("Sostituisci"),danger:true,onOk:function(){go()}});else go()}
/* --- editor --- */
var FE={p:"",isNew:false,orig:"",tabSp:true};
function fEdit(p){api("/api/fs/text?path="+enc(p)).then(function(r){return r.text().then(function(x){return{ok:r.ok,x:x}})}).then(function(o){if(!o.ok){msg("fm",o.x,false);return}fEdOpen(p,o.x,false)}).catch(function(){})}
function fEdOpen(p,x,isNew){FE.p=p;FE.isNew=isNew;FE.orig=isNew?null:x;FE.back=document.activeElement;var e=$("fedx");e.value=x;e.scrollTop=0;e.scrollLeft=0;
 $("fed").className="fed";document.body.classList.add("fedo");fEdTitle();$("fedw").className=fSys(p)?"fedw":"fedw hide";
 $("fedpaste").hidden=!(navigator.clipboard&&navigator.clipboard.readText&&window.isSecureContext);fEdUpd();setTimeout(function(){e.focus();e.setSelectionRange(0,0)},30)}
function fEdDirty(){return FE.isNew||$("fedx").value!==FE.orig}
function fEdTitle(){var d=fEdDirty();$("fedt").textContent=fBase(FE.p);$("fedp").textContent=fDir(FE.p);$("feds").textContent=FE.isNew?t("Nuovo, non ancora salvato"):d?t("Modificato"):t("Salvato");$("feds").className="feds"+(d?" mod":"")}
function fEdUpd(){var e=$("fedx"),v=e.value,ln=v.split("\n").length,h="";for(var i=1;i<=ln;i++)h+=i+"\n";if($("fedn").textContent!=h)$("fedn").textContent=h;$("fedn").scrollTop=e.scrollTop;
 var b=new TextEncoder().encode(v).length,pre=v.substring(0,e.selectionStart),r=pre.split("\n").length,c=pre.length-pre.lastIndexOf("\n");
 $("fedpos").textContent=tf("Riga {0}, colonna {1}",r,c);var free=FS.total-FS.used,over=b>FS.max,nf=FS.total&&b+8192>free+(FE.orig!=null?new TextEncoder().encode(FE.orig).length:0);
 $("fedsz").textContent=fSize(b)+" / "+fSize(FS.max)+(FS.total?" · "+tf("liberi {0}",kb(free)):"");$("fedsz").className=over||nf?"bad":"";
 var jm="";if(/\.json$/i.test(FE.p)&&v.trim()){var je=fJsonErr(v);jm=je?"⚠ "+je:"✓ "+t("JSON corretto")}$("fedjs").textContent=jm;$("fedjs").className=jm.charAt(0)=="⚠"?"bad":"good";
 $("fedtab").textContent=FE.tabSp?t("Tab: inserisce spazi (Ctrl+M per cambiare)"):t("Tab: passa al pulsante successivo (Ctrl+M per cambiare)");fEdTitle()}
function fJsonErr(v){try{JSON.parse(v);return""}catch(x){var m=String(x.message),p=m.match(/position (\d+)/),l=m.match(/line (\d+) column (\d+)/);
 if(l)return tf("errore alla riga {0}, colonna {1}",l[1],l[2]);if(p){var pre=v.substring(0,+p[1]);return tf("errore alla riga {0}, colonna {1}",pre.split("\n").length,pre.length-pre.lastIndexOf("\n"))}return t("errore nel JSON")}}
function fEdSave(asNew,path,force){var v=$("fedx").value,p=path||FE.p,b=new TextEncoder().encode(v).length;
 if(b>FS.max){msg("fedm",tf("Troppo grande: massimo {0}",fSize(FS.max)),false);return Promise.resolve(false)}
 if(!force&&/\.json$/i.test(p)&&v.trim()&&fJsonErr(v)){dlg({title:t("Il JSON ha un errore"),body:'<p style="margin:0">'+esc(fJsonErr(v))+'</p><p style="margin:8px 0 0;color:var(--mut)">'+esc(t("Se lo salvi cosi, chi lo legge potrebbe non funzionare."))+'</p>',ok:t("Salva lo stesso"),danger:true,onOk:function(){fEdSave(asNew,path,true);return""}});return Promise.resolve(false)}
 return post("/api/fs/save",{path:p,text:v,"new":(FE.isNew||asNew)?"1":"0"}).then(function(r){return r.json()}).then(function(j){
  if(!j.ok&&(FE.isNew||asNew)&&j.err==t("Esiste gia")){dlg({title:t("Sostituire il file?"),body:'<p style="margin:0">'+esc(tf("{0} esiste gia.",p))+'</p>',ok:t("Sostituisci"),danger:true,onOk:function(){FE.isNew=false;return post("/api/fs/save",{path:p,text:v,"new":"0"}).then(function(r){return r.json()}).then(function(j){if(j.ok)fEdSaved(p,v);return j.ok?"":j.err})}});return false}
  if(!j.ok){msg("fedm",j.err,false);return false}fEdSaved(p,v);return true})}
function fEdSaved(p,v){FE.p=p;FE.isNew=false;FE.orig=v;msg("fedm",t("Salvato"),true);var lv=$("live");if(lv)lv.textContent=t("Salvato");fEdUpd();
 api("/api/fs/list?path="+enc(fp)).then(function(r){return r.json()}).then(function(j){if(j.ok){FS.used=j.used;FS.total=j.total;fEdUpd()}}).catch(function(){})}
function fEdSaveAs(){dlg({title:t("Salva come"),body:'<label for="dn">'+esc(t("Nome del file"))+'</label><input id="dn" autocomplete="off" maxlength="60" value="'+esc(fBase(FE.p))+'"><label for="dd">'+esc(t("Cartella"))+'</label><select id="dd"><option value="'+esc(fDir(FE.p))+'">'+esc(fDir(FE.p))+'</option></select>',ok:t("Salva"),
 onOk:function(){var n=dlgV("dn"),d=$("dd").value,e=fName(n);if(e)return e;fEdSave(true,join(d,n));return""}});fDirsSel("dd",fDir(FE.p))}
function fEdPaste(){navigator.clipboard.readText().then(function(x){var e=$("fedx");e.focus();e.setRangeText(x,e.selectionStart,e.selectionEnd,"end");fEdUpd()}).catch(function(){msg("fedm",t("Il browser non permette di leggere gli appunti: usa Ctrl+V"),false)})}
function fEdClear(){dlg({title:t("Svuotare il file?"),body:'<p style="margin:0">'+esc(t("Il testo viene cancellato dall'editor. Il file cambia solo quando premi Salva."))+'</p>',ok:t("Svuota"),danger:true,onOk:function(){var e=$("fedx");e.value="";fEdUpd();setTimeout(function(){e.focus()},40);return""}})}
function fEdDl(){var a=document.createElement("a");a.href=URL.createObjectURL(new Blob([$("fedx").value],{type:"text/plain"}));a.download=fBase(FE.p);document.body.appendChild(a);a.click();setTimeout(function(){URL.revokeObjectURL(a.href);a.remove()},500)}
function fEdClose(force){if(!force&&fEdDirty()&&!(FE.isNew&&!$("fedx").value)){dlg({title:t("Modifiche non salvate"),body:'<p style="margin:0">'+esc(tf("Vuoi salvare {0} prima di chiudere?",fBase(FE.p)))+'</p>',
  buttons:[["cancel",t("Annulla"),"gray"],["drop",t("Non salvare"),"red"],["ok",t("Salva"),""]],
  onOk:function(k){if(k=="drop"){fEdClose(true);return""}return fEdSave().then(function(ok){if(ok)fEdClose(true);return""})}});return}
 $("fed").className="fed hide";document.body.classList.remove("fedo");if(FE.back&&FE.back.focus&&document.body.contains(FE.back))FE.back.focus();fList(fp)}
(function(){var e=$("fedx");["input","click","keyup","select"].forEach(function(n){e.addEventListener(n,fEdUpd)});
 e.addEventListener("scroll",function(){$("fedn").scrollTop=e.scrollTop});
 e.addEventListener("keydown",function(k){
  if(k.key=="Tab"&&FE.tabSp&&!k.ctrlKey&&!k.altKey){k.preventDefault();var s=e.selectionStart,v=e.value;
   if(k.shiftKey){var ls=v.lastIndexOf("\n",s-1)+1,n=v.substr(ls,2)=="  "?2:v.charAt(ls)==" "?1:0;if(n){e.setRangeText("",ls,ls+n,"preserve");e.selectionStart=e.selectionEnd=Math.max(ls,s-n)}}
   else e.setRangeText("  ",s,e.selectionEnd,"end");fEdUpd()}
  else if((k.ctrlKey||k.metaKey)&&(k.key=="m"||k.key=="M")){k.preventDefault();FE.tabSp=!FE.tabSp;fEdUpd();var lv=$("live");if(lv)lv.textContent=$("fedtab").textContent}})})();
$("fed").addEventListener("keydown",function(k){if((k.ctrlKey||k.metaKey)&&(k.key=="s"||k.key=="S")){k.preventDefault();fEdSave()}else if(k.key=="Escape"&&!DLG){k.preventDefault();fEdClose()}});
window.addEventListener("beforeunload",function(e){if($("fed").className=="fed"&&fEdDirty()&&$("fedx").value){e.preventDefault();e.returnValue=""}});
function esc(s){return String(s).replace(/[&<>"]/g,function(c){return{"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;"}[c]})}
var RN={c:null,n:null,s:"wifi"};
function mqRun(a){return post("/api/mqtt/run",{a:a}).then(function(r){return r.json()}).then(function(j){msg("mqm",j.ok?(a=="test"?t("Messaggio inviato"):t("Fatto")):j.err,j.ok);setTimeout(mqLoad,1200)}).catch(function(){mqLoad()})}
function airUi(){var v=$("air_ex").value;$("air_t").className=v=="1"?"agrid":"agrid hide";$("air_h").className=v=="2"?"":"hide";
 var n=RN.n||{};$("airst").innerHTML=n.air?'<span class="pill a">'+esc(t("Attivo"))+'</span>':'<span class="pill">'+esc(t("Spento: la rete funziona"))+'</span>'}
function airOn(){var ex=+$("air_ex").value,par=0;if(ex==1){par=Math.round(+$("air_n").value*+$("air_u").value);if(!(par>=10)){msg("airm",t("Tempo minimo 10 secondi"),false);return}}
 if(ex==2){var h=$("air_at").value;if(!h){msg("airm",t("Scegli l'orario"),false);return}par=+h.slice(0,2)*60+ +h.slice(3,5)}
 var how=ex==0?t("al prossimo avvio"):ex==1?tf("dopo {0}",durText(par)):ex==2?tf("alle {0}",$("air_at").value):t("solo a mano");
 dlg({title:t("Attivare il modo aereo?"),body:'<p style="margin:0">'+esc(t("Il Wi-Fi si spegne subito e questa pagina non si raggiunge piu."))+'</p><p style="margin:8px 0 0">'+esc(tf("La rete torna {0}.",how))+'</p><p style="margin:8px 0 0;color:var(--mut)">'+esc(t("Per tornare prima: airplane off dalla seriale o tasto BOOT."))+'</p>',ok:t("Attiva"),danger:true,
  onOk:function(){return post("/api/airplane",{on:1,exit:ex,param:par}).then(function(r){return r.json()}).then(function(j){if(j.ok)setTimeout(function(){dlg({title:t("Modo aereo attivo"),body:'<p style="margin:0">'+esc(tf("La rete torna {0}. Poi ricarica la pagina.",how))+'</p>',buttons:[["ok",t("Ricarica"),""]],onOk:function(){location.reload();return""}})},50);return j.ok?"":j.err})}})}
function ipv(id,v,on){var e=$(id);e.value=v||"";e.disabled=!on}
function ipFill(ch){var n=RN.n,c=RN.c,ap=n.st==1,dh=$("w_dhcp").value=="1",ids=["w_ip","w_mask","w_gw","w_d1","w_d2"],v;
 $("w_dhcp").disabled=ap;$("ipsave").disabled=ap;
 if(ap){v=[n.ip,n.mask,n.gw,"",""];$("ipnote").textContent=t("Punto di accesso attivo: indirizzo fisso, non modificabile. Per cambiarlo collega la scheda a una rete (scheda Wi-Fi).")}
 else if(dh){v=n.st==3?[n.ip,n.mask,n.gw,n.dns,n.dns2]:["","","","",""];$("ipnote").textContent=n.st==3?t("Valori assegnati dal router (DHCP): non modificabili."):t("Non connesso: nessun indirizzo assegnato.")}
 else{v=(ch&&n.st==3&&(c.staDhcp||!c.ip))?[n.ip,n.mask,n.gw,n.dns,n.dns2]:[c.ip,c.mask,c.gw,c.dns1,c.dns2];$("ipnote").textContent=t("Indirizzo fisso: scrivi i valori.")}
 ids.forEach(function(id,i){ipv(id,v[i],!ap&&!dh)});$("ipmac").innerHTML=macRow(t("MAC client"),n.mac)+macRow(t("MAC punto di accesso"),n.apMac)}
function dh(){ipFill(true)}
function fillNet(){Promise.all([api("/api/settings").then(function(r){return r.json()}),api("/api/status").then(function(r){return r.json()})]).then(function(a){
 RN.c=a[0];RN.n=a[1].net;$("w_ssid").value=RN.c.staSsid;$("w_dhcp").value=RN.c.staDhcp?"1":"0";ipFill(false);drawAp();buildSub();fillSys()}).catch(function(){})}
function saveWifi(){var mid=RN.s=="ip"?"wm2":"wm";if(!$("w_ssid").value){msg(mid,t("Prima scegli la rete nella scheda Wi-Fi"),false);return}
 post("/api/wifi/save",{ssid:$("w_ssid").value,pass:$("w_pass").value,dhcp:$("w_dhcp").value,ip:$("w_ip").value,mask:$("w_mask").value,gw:$("w_gw").value,d1:$("w_d1").value,d2:$("w_d2").value})
 .then(function(r){return r.json()}).then(function(j){msg(mid,j.ok?t("Salvato. La scheda si sta collegando: se cambia indirizzo riconnettiti."):j.err,j.ok);if(j.ok)setTimeout(fillNet,9000)}).catch(function(){})}
function apOnly(){post("/api/wifi/ap",{}).then(function(r){return r.json()}).then(function(j){msg("wm3",j.ok?t("Modo AP attivo (192.168.4.1)"):j.err,j.ok);if(j.ok)setTimeout(fillNet,5000)}).catch(function(){})}
var SH={h:[],i:0,cmds:[],q:[],busy:false,loaded:false};
var SHOPT={led:["state","heartbeat","fixed","off"],cpu:["auto","80","160","240"],ntp:["sync"],pin:["high","low","blink","read","off"],license:["notice","gpl3","lgpl21","apache2","mit","bsd3"],help:[],wifi:[]};
try{SH.h=JSON.parse(localStorage.getItem("vos_hist")||"[]")}catch(e){SH.h=[]}
function shSave(){try{localStorage.setItem("vos_hist",JSON.stringify(SH.h.slice(-100)))}catch(e){}}
function shOut(txt,cls){var o=$("so"),d=document.createElement("div");d.textContent=txt===""?" ":txt;if(cls)d.className=cls;o.appendChild(d);while(o.childNodes.length>600)o.removeChild(o.firstChild);o.scrollTop=o.scrollHeight}
function shKind(l){if(/errore|error|non valid|invalid|non autorizzat|unauthor|fallit|failed|sconosciut|unknown|nicht|ung.ltig|desconocid/i.test(l))return"e";
 if(/attenzione|avviso|warning|achtung|aviso/i.test(l))return"w";if(/^\s*(ok|fatto|done|salvato|saved|gespeichert|guardado)\b/i.test(l))return"g";return""}
function shLoadCmds(){if(SH.loaded)return;SH.loaded=true;post("/api/shell",{c:"help"}).then(function(r){return r.text()}).then(function(x){var m={};
 x.split("\n").forEach(function(l){var k=/^ {2}([a-z0-9-]+)/.exec(l);if(k)m[k[1]]=1});SH.cmds=Object.keys(m).concat(["clear","history"]).sort()}).catch(function(){SH.loaded=false})}
function shPipe(txt,parts){for(var i=0;i<parts.length;i++){var p=parts[i].trim(),m=/^(grep|head|tail)\s*(-i\s+|-v\s+)?(.*)$/.exec(p);if(!m)continue;var ls=txt.split("\n");
 if(m[1]=="grep"){var f=m[2]?m[2].trim():"",q=m[3].trim().toLowerCase();ls=ls.filter(function(l){var h=l.toLowerCase().indexOf(q)>=0;return f=="-v"?!h:h})}
 else{var n=parseInt(m[3])||10;ls=m[1]=="head"?ls.slice(0,n):ls.slice(-n)}txt=ls.join("\n")}return txt}
function shRun(v){v=v.replace(/\s+$/,"");if(!v)return Promise.resolve();
 var parts=v.split("|"),cmd=parts[0].trim(),w=cmd.split(/\s+/)[0].toLowerCase();
 shOut("vesevos> "+(/^passwd\b/i.test(cmd)?"passwd ****":v),"c");
 if(!/^(passwd|password)\b/i.test(cmd)){var k=SH.h.indexOf(v);if(k>=0)SH.h.splice(k,1);SH.h.push(v);shSave()}SH.i=SH.h.length;
 if(w=="clear"){shClear();return Promise.resolve()}
 if(w=="history"){SH.h.forEach(function(x,i){shOut((i+1)+"  "+x,"m")});return Promise.resolve()}
 if((w=="reboot"||w=="factory-reset")&&!confirm(w=="reboot"?t("Riavviare la scheda?"):t("Azzerare TUTTO e riavviare?"))){shOut(t("Annullato"),"w");return Promise.resolve()}
 return post("/api/shell",{c:cmd}).then(function(r){return r.text()}).then(function(x){x=shPipe(x.replace(/\n+$/,""),parts.slice(1));
  x.split("\n").forEach(function(l){shOut(l,shKind(l))})}).catch(function(){shOut(t("Errore di rete"),"e")})}
function shNext(){if(SH.busy||!SH.q.length)return;SH.busy=true;shRun(SH.q.shift()).then(function(){SH.busy=false;shNext()})}
function runCmd(){var i=$("si"),v=i.value;i.value="";SH.i=SH.h.length;if(!v.trim())return;SH.q.push(v);shNext()}
function shHist(d){var i=$("si");if(!SH.h.length)return;SH.i=Math.max(0,Math.min(SH.h.length,SH.i+d));i.value=SH.i>=SH.h.length?"":SH.h[SH.i];i.focus()}
function shTab(){var i=$("si"),v=i.value,sp=v.lastIndexOf(" "),head=v.slice(0,sp+1),w=v.slice(sp+1),words=v.trim().split(/\s+/),pool;
 if(sp<0)pool=SH.cmds;else pool=SHOPT[words[0].toLowerCase()]||[];
 var m=pool.filter(function(x){return x.indexOf(w)==0});if(!m.length)return;
 if(m.length==1){i.value=head+m[0]+" ";return}
 var c=m[0];m.forEach(function(x){while(x.indexOf(c)!=0)c=c.slice(0,-1)});i.value=head+c;shOut(m.join("   "),"m")}
function shKey(e){var i=$("si");
 if(e.key=="Enter"){runCmd()}
 else if(e.key=="ArrowUp"){e.preventDefault();shHist(-1)}
 else if(e.key=="ArrowDown"){e.preventDefault();shHist(1)}
 else if(e.key=="Tab"){e.preventDefault();shTab()}
 else if(e.ctrlKey&&(e.key=="l"||e.key=="L")){e.preventDefault();shClear()}
 else if(e.ctrlKey&&(e.key=="c"||e.key=="C")&&i.selectionStart==i.selectionEnd){e.preventDefault();shCancel()}}
function shPaste(e){var x=(e.clipboardData||window.clipboardData).getData("text");if(x.indexOf("\n")<0)return;e.preventDefault();
 var ls=x.split(/\r?\n/).filter(function(l){return l.trim()});if(ls.length>20){shOut(t("Troppe righe incollate (massimo 20)"),"e");return}
 if(ls.length&&!confirm(t("Eseguire queste righe?")+"\n\n"+ls.join("\n")))return;ls.forEach(function(l){SH.q.push(l)});shNext()}
function shCancel(){var i=$("si");if(i.value)shOut("vesevos> "+i.value+"^C","m");i.value="";SH.q=[];SH.i=SH.h.length}
function shClear(){$("so").textContent=""}
function shCopy(){var x=$("so").innerText;var ok=function(){msg("shm",t("Copiato"),true);setTimeout(function(){$("shm").className="msg"},3000)};
 if(navigator.clipboard&&navigator.clipboard.writeText)navigator.clipboard.writeText(x).then(ok).catch(function(){});
 else{var a=document.createElement("textarea");a.value=x;document.body.appendChild(a);a.select();try{document.execCommand("copy");ok()}catch(e){}document.body.removeChild(a)}}
var TK={d:[],prev:null,col:"n",asc:true,tm:null,total:0};
var TKCOLS=[["n","Nome"],["t","Tipo"],["s","Stato"],["p","Priorita"],["stk","Stack libero"],["cpu","CPU"]];
function loadTasks(){clearInterval(TK.tm);if(cur!="task")return;tkPoll();TK.tm=setInterval(function(){if(cur!="task"){clearInterval(TK.tm);TK.prev=null;return}tkPoll()},2000)}
function tkPoll(){if(document.hidden)return;api("/api/tasklist").then(function(r){return r.json()}).then(function(j){
 if(!j.ok){$("tkinfo").textContent=t("(dettagli non disponibili in questa build)");$("tktab").innerHTML="";return}
 var dt=TK.prev?j.total-TK.prev.total:0,hasRt=j.total>0;
 j.tasks.forEach(function(x){x.cpu=-1;if(hasRt&&TK.prev&&dt>0&&TK.prev.m[x.id]!==undefined){x.cpu=Math.min(100,Math.max(0,100*(x.rt-TK.prev.m[x.id])/dt))}});
 var m={};j.tasks.forEach(function(x){m[x.id]=x.rt});TK.prev={total:j.total,m:m};TK.hasCpu=hasRt;TK.d=j.tasks;TK.total=j.total;tkDraw()}).catch(function(){})}
function tkSort(c){if(TK.col==c)TK.asc=!TK.asc;else{TK.col=c;TK.asc=(c=="n"||c=="t"||c=="s")}tkDraw()}
function tkState(s){return s==9?t("Fermato"):[t("In esecuzione"),t("Pronto"),t("In attesa"),t("Sospeso"),t("Eliminato"),"?"][s]||"?"}
function tkType(x){return x.t==2?t("App"):x.t==1?t("VesevOS"):t("Sistema")}
function tkDraw(){var c=TK.col,k=TK.asc?1:-1,d=TK.d.slice();
 d.sort(function(a,b){var x=a[c],y=b[c];if(c=="n"){x=a.n.toLowerCase();y=b.n.toLowerCase()}return(x<y?-1:x>y?1:0)*k||(a.id-b.id)});
 var h="<tr>";TKCOLS.forEach(function(x){if(x[0]=="cpu"&&!TK.hasCpu)return;h+='<th onclick="tkSort(\''+x[0]+'\')">'+esc(t(x[1]))+(TK.col==x[0]?(TK.asc?" &#9650;":" &#9660;"):"")+"</th>"});
 h+="<th></th></tr>";
 d.forEach(function(x){h+="<tr><td>"+esc(x.n)+"</td><td><span class='pill "+(x.t==2?"a":x.t==1?"v":"")+"'>"+esc(tkType(x))+"</span></td><td>"+esc(tkState(x.s))+"</td><td>"+x.p+"</td><td>"+x.stk+" B</td>"+
  (TK.hasCpu?"<td>"+(x.cpu<0?"-":"<span class='cbar'><i style='width:"+x.cpu+"%'></i></span>"+x.cpu.toFixed(0)+"%")+"</td>":"")+
  "<td class='tkb'>"+(x.k?"<span class='svi'>"+sibB("sm go","i-sgo",t("Avvia"),x.s!=9,0,x.s==9?"tkRestart('"+esc(x.n)+"')":"")+sibB("sm st","i-sst",t("Ferma"),x.s==9,0,x.s==9?"":"tkKill('"+esc(x.n)+"')")+sibB("sm rs","i-srs",t("Riavvia"),0,x.s==9,"tkRestart('"+esc(x.n)+"')")+"</span>":"<span class='lk' title='"+esc(t("Protetto"))+"'>&#128274;</span>")+"</td></tr>"});
 $("tktab").innerHTML=h;$("tkinfo").textContent=tf("{0} task. Clicca un titolo per ordinare.",d.length)}
function tkKill(n){dlg({title:tf("Fermare il task {0}?",n),body:'<p style="margin:0">'+esc(t("Alcune funzioni smettono di funzionare finche non lo riavvii (pulsante Avvia o comando start)."))+'</p>',ok:t("Ferma"),danger:true,
 onOk:function(){return post("/api/taskkill",{name:n}).then(function(r){return r.json()}).then(function(j){if(j.ok){msg("tkm",tf("Task '{0}' fermato",n),true);tkPoll()}return j.ok?"":j.err})}})}
function tkRestart(n){post("/api/taskrestart",{name:n}).then(function(r){return r.json()}).then(function(j){msg("tkm",j.ok?tf("Task '{0}' avviato",n):j.err,j.ok);setTimeout(tkPoll,300)}).catch(function(){})}
function fillLed(){api("/api/settings").then(function(r){return r.json()}).then(function(c){$("l_mode").value=c.ledMode;
 $("l_col").value="#"+("000000"+c.ledColor.toString(16)).slice(-6);$("l_br").value=c.ledBrightness})}
function saveLed(){post("/api/led",{mode:$("l_mode").value,color:parseInt($("l_col").value.slice(1),16),br:$("l_br").value})
 .then(function(r){return r.json()}).then(function(j){msg("lm2",j.ok?t("Salvato"):j.err,j.ok)})}
var PM=[],PSEL=-1,PWARN=false,PTM=null,PT={gpio:-1};
var PBI=null,PV="f",PNOTE={};
var PWR={"5V":"#d64545","3V3":"#e08a2c","GND":"#222"};
var PFL=["43","44","1","2","3","4","5","6","7"],PFR=["5V","GND","3V3","13","12","11","10","9","8"],PIL=[38,37,34,33,21,18,17,16,15,14],PIR=[36,35,48,47,46,45,42,41,40,39];
