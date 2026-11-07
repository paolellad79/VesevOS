/* --- finestra di dialogo generica (accessibile) --- */
var DLG=null;
function dlg(o){
  dlgClose(true);
  var w=$("dlg");
  DLG={o:o,back:document.activeElement};
  $("dlgt").textContent=o.title;
  $("dlgb").innerHTML=o.body||"";
  $("dlgm").className="msg";
  var bs=o.buttons||[["cancel",t("Annulla"),"gray"],["ok",o.ok||t("OK"),o.danger?"red":""]];
  $("dlga").innerHTML=bs.map(function(b){
    return'<button type="'+(b[0]=="ok"?"submit":"button")+'" class="btn '+(b[2]||"")+'" data-k="'+
      b[0]+'">'+esc(b[1])+'</button>'
  }).join("");
  [].forEach.call($("dlga").querySelectorAll("button[type=button]"),function(b){
    b.onclick=function(){dlgBtn(b.dataset.k)}
  });
  w.className="dlgw";
  var f=w.querySelector("input,select,textarea")||w.querySelector(".btn:not(.gray)")||
    w.querySelector(".btn");
  setTimeout(function(){
    if(!f)return;
    f.focus();
    if(o.sel&&f.setSelectionRange)f.setSelectionRange(o.sel[0],o.sel[1])
  },30)
}

function dlgBtn(k){
  if(!DLG)return;
  var o=DLG.o;
  if(k=="cancel"){
    dlgClose();
    return
  }
  var r=o.onOk?o.onOk(k):null;
  if(r&&r.then){
    [].forEach.call($("dlga").querySelectorAll("button"),function(b){b.disabled=true});
    r.then(function(e){
      [].forEach.call($("dlga").querySelectorAll("button"),function(b){b.disabled=false});
      if(e)msg("dlgm",e,false);
      else dlgClose()
    })
  }
  else if(typeof r=="string"&&r)msg("dlgm",r,false);
  else dlgClose()
}

function dlgClose(quiet){
  var w=$("dlg");
  if(!DLG){
    w.className="dlgw hide";
    return
  }
  var b=DLG.back,o=DLG.o;
  DLG=null;
  w.className="dlgw hide";
  if(!quiet){
    if(o.onCancel&&!o._done)o.onCancel();
    if(b&&b.focus)b.focus()
  }
}
$("dlg").addEventListener("keydown",function(e){
  if(e.key=="Escape"){
    e.preventDefault();
    dlgClose()
  }else if(e.key=="Tab"){
    var fs=[].slice.call($("dlg").querySelectorAll("input,select,textarea,button:not([disabled])")),
      a=fs[0],z=fs[fs.length-1];
    if(e.shiftKey&&document.activeElement==a){
      e.preventDefault();
      z.focus()
    }else if(!e.shiftKey&&document.activeElement==z){
      e.preventDefault();
      a.focus()
    }
  }
});

function dlgV(id){return($(id).value||"").trim()}
