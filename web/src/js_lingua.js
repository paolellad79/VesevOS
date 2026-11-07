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

function upLang(){
  var f=$("lgf").files[0];
  if(!f){
    msg("lgm",t("Scegli un file"),false);
    return
  }
  var m=f.name.match(/^([a-z0-9]{2,8})\.json$/);
  if(!m||m[1]=="it"){
    msg("lgm",t("Il nome deve essere come en.json (codice di 2-8 lettere minuscole)"),false);
    return
  }
  var code=m[1],fd=new FormData();
  fd.append("file",f,f.name);
  api("/api/fs/up?dir=/lang",
    {method:"POST",body:fd}).then(function(r){return r.json()}).then(function(j){
      if(!j.ok){
        msg("lgm",j.err,false);
        return
      }
      msg("lgm",t("Lingua caricata"),true);
      $("lgf").value="";
      if(code==LANG)setLang(code);
      else loadLangs()
    })
}

function delLang(c){
  if(!confirm(tf("Eliminare la lingua {0}?",c)))return;
  var go=function(){
    post("/api/fs/del",{path:"/lang/"+c+
      ".json"}).then(function(r){return r.json()}).then(function(j){
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
