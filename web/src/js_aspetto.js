/* ===== Aspetto della pagina: accessibilita (testo, contrasto, daltonici, animazioni) e tema chiaro/scuro =====
   Spostato da js_base.js il 20/11/2026 (il file superava le 500 righe), senza cambiare il comportamento. */
var A11Y={fs:0,hc:"auto",cb:0,rm:"auto"};
try{
  var _a=JSON.parse(localStorage.getItem("va11y")||"{}");
  for(var _k in A11Y)if(_a[_k]!==undefined)A11Y[_k]=_a[_k]
}catch(e){}

function a11yApply(){
  var d=document.documentElement,mm=function(q){return!!(window.matchMedia&&matchMedia(q).matches)};
  d.setAttribute("data-fs",A11Y.fs);
  d.setAttribute("data-hc",
    (A11Y.hc=="on"||(A11Y.hc=="auto"&&mm("(prefers-contrast: more)")))?"1":"0");
  d.setAttribute("data-cb",A11Y.cb?"1":"0");
  d.setAttribute("data-rm",
    (A11Y.rm=="on"||(A11Y.rm=="auto"&&mm("(prefers-reduced-motion: reduce)")))?"1":"0");
  ["fs","hc","cb","rm"].forEach(function(k){
    var e=$("a_"+k),l=$("l_"+k);   /* a_ = scheda Sistema, l_ = finestra dell'accesso */
    if(e)e.value=A11Y[k];
    if(l)l.value=A11Y[k]
  });
  if(typeof cpApply=="function")try{cpApply()}catch(e){}
}

function a11ySet(k,v){
  A11Y[k]=v;
  try{localStorage.setItem("va11y",JSON.stringify(A11Y))}catch(e){}
  a11yApply();
  live(t("Salvato"))
}

function a11yReset(){
  A11Y={fs:0,hc:"auto",cb:0,rm:"auto"};
  try{localStorage.removeItem("va11y")}catch(e){}
  a11yApply();
  live(t("Ripristinato"))
}

function a11yOpen(){
  var lg=$("login");
  if(lg&&!lg.classList.contains("hide")){   /* nella pagina di accesso non c'e la scheda Sistema: si apre la finestra */
    thOpen(false);
    var f=$("a11p");
    popSotto(f,{currentTarget:$("lab")});
    f.className="pop";
    $("lab").setAttribute("aria-expanded","true");
    $("l_fs").focus();
    return
  }
  $("thm").className="pop hide";
  show("acc");
  setTimeout(function(){
    var e=$("a_fs");
    if(e){
      e.scrollIntoView({block:"center"});
      e.focus()
    }
  },60)
}

function live(x){
  var e=$("live");
  if(!e)return;
  e.textContent="";
  setTimeout(function(){e.textContent=x},30)
}

function a11yLabels(){
  [].forEach.call(document.querySelectorAll("button[title],a[title],label.btn"),function(b){
    if(!b.getAttribute("aria-label")&&!b.textContent.trim()&&b.title)b.setAttribute("aria-label",
      b.title)
  });
  [].forEach.call(document.querySelectorAll("svg.i:not([aria-hidden])"),function(s){
    s.setAttribute("aria-hidden","true")
  });
  [].forEach.call(document.querySelectorAll(".msg:not([role])"),function(m){
    m.setAttribute("role","status")
  });
  [].forEach.call(document.querySelectorAll("label:not([for])"),function(l){
    var n=l.nextElementSibling;
    if(n&&/^(INPUT|SELECT|TEXTAREA)$/.test(n.tagName)&&n.id&&
      !l.querySelector("input,select,textarea"))l.htmlFor=n.id
  });
  [].forEach.call(document.querySelectorAll("input[placeholder]:not([aria-label])"),function(i){
    if(!(i.labels&&i.labels.length))i.setAttribute("aria-label",i.placeholder)
  });
  [].forEach.call(document.querySelectorAll(
    "[onclick]:not(button):not(a):not(input):not(select):not(textarea):not(label):not([role]):not([tabindex])"),
    function(e){
      if(e.closest("svg"))return;
      e.setAttribute("role","button");
      e.tabIndex=0;
      e.setAttribute("data-kb","1")
    });
  [].forEach.call(document.querySelectorAll("svg:not([role]):not([aria-label]):not([aria-hidden])"),
    function(s){
      s.setAttribute("aria-hidden","true")
    })
}
document.addEventListener("keydown",function(e){
  var x=e.target;
  if((e.key=="Enter"||e.key==" ")&&x&&x.getAttribute&&x.getAttribute("data-kb")){
    e.preventDefault();
    x.click()
  }
});
(function(){
  var tm=0;
  new MutationObserver(function(){
    clearTimeout(tm);
    tm=setTimeout(a11yLabels,120)
  }).observe(document.body,{childList:true,subtree:true});
  if(window.matchMedia){
    ["(prefers-contrast: more)","(prefers-reduced-motion: reduce)"].forEach(function(q){
      var m=matchMedia(q);
      if(m.addEventListener)m.addEventListener("change",a11yApply)
    })
  }
})();
a11yApply();
a11yLabels();
var THEME="auto";
try{THEME=localStorage.getItem("vth")||"auto"}catch(e){}
function themeApply(){
  var m=THEME;
  if(m=="auto")m=(window.matchMedia&&matchMedia("(prefers-color-scheme: light)").matches)?"light"
    :"dark";
  document.documentElement.setAttribute("data-theme",m);
  var nm=THEME=="auto"?t("Automatico"):THEME=="light"?t("Chiaro"):t("Scuro");
  var u=$("thu");
  if(u)u.setAttribute("href",THEME=="auto"?"#i-auto":THEME=="light"?"#i-sun":"#i-moon");
  var lu=$("lthu");
  if(lu)lu.setAttribute("href",THEME=="auto"?"#i-auto":THEME=="light"?"#i-sun":"#i-moon");
  ["thb","lthb"].forEach(function(id){
    var b=$(id);
    if(!b)return;
    var nt=tf("Tema: {0}",nm);
    b.title=nt;
    b.setAttribute("aria-label",nt)
  });
    ["auto","light","dark"].forEach(function(k){
      var e=$("tm_"+k);
      if(e)e.className=k==THEME?"on":""
    });
  if(typeof cur!="undefined"&&cur=="pin"&&typeof PM!="undefined"&&PM.length)drawPins()
}

function thOpen(on){
  var m=$("thm");
  if(m)m.className=on?"pop":"pop hide";
  ["thb","lthb"].forEach(function(id){
    var b=$(id);
    if(b)b.setAttribute("aria-expanded",on?"true":"false")
  })
}

function themeSet(m){
  THEME=m;
  try{localStorage.setItem("vth",THEME)}catch(e){}
  themeApply();
  thOpen(false)
}

/* Menu a comparsa sotto il pulsante premuto, solo nella pagina di accesso (nella testata restano al loro posto) */
function popSotto(m,e){
  var l=$("login");
  if(!l||l.classList.contains("hide"))return;
  var r=e.currentTarget.getBoundingClientRect();
  m.style.top=(r.bottom+6)+"px";
  m.style.right=Math.max(8,innerWidth-r.right)+"px"
}

function thMenu(e){
  e.stopPropagation();
  var apri=$("thm").classList.contains("hide");
  $("a11p").className="pop hide";
  $("lab").setAttribute("aria-expanded","false");
  if(apri)popSotto($("thm"),e);
  thOpen(apri)
}

function a11yPop(e){
  e.stopPropagation();
  var m=$("a11p"),apri=m.classList.contains("hide");
  thOpen(false);
  langClose();
  if(apri)popSotto(m,e);
  m.className=apri?"pop":"pop hide";
  $("lab").setAttribute("aria-expanded",apri?"true":"false");
  if(apri)$("l_fs").focus()
}

function a11yPopChiudi(){
  var m=$("a11p");
  if(!m||m.classList.contains("hide"))return;
  m.className="pop hide";
  $("lab").setAttribute("aria-expanded","false")
}
document.addEventListener("click",function(e){
  if(!e.target.closest("#a11p")&&!e.target.closest("#thm"))a11yPopChiudi()
});
document.addEventListener("click",function(e){
  var m=$("thm");
  if(m&&!m.classList.contains("hide")&&!e.target.closest("#thm"))thOpen(false)
});
document.addEventListener("keydown",function(e){
  if(e.key=="Escape"){
    thOpen(false);
    a11yPopChiudi();
    var um=$("um");
    if(um)um.className="pop hide";
    var al=$("alerts");
    if(al&&!al.classList.contains("hide"))al.className="alerts hide";
    langClose()
  }
  if(e.ctrlKey&&(e.key=="`"||e.code=="Backquote")&&loggedIn()){
    e.preventDefault();
    cpToggle()
  }
});
if(window.matchMedia)try{
  matchMedia("(prefers-color-scheme: light)").addEventListener("change",function(){
    if(THEME=="auto")themeApply()
  })
}catch(e){}
