/* ===== navigazione con ruoli ===== */
function canSee(id){
  if(AUTH&&AUTH.pk&&AUTH.pk[id]===0)return false;
  return ROLE>=(TMIN[id]||0)
}

function gTabs(g){return GMAP[g].filter(canSee)}
function buildNav(){
  var h='<svg class="i slogo" style="stroke:url(#vg)"><use href="#i-chipn"/></svg>';
  GRP.forEach(function(g){
    if(gTabs(g[0]).length)h+='<button class="nb" id="n_'+g[0]+'" onclick="gGo(\''+g[0]+
      '\')" aria-label="'+esc(t(g[1]))+'" title="'+esc(t(g[1]))+'"><svg class="i"><use href="#'+
      GICO[g[0]]+'"/></svg><span class="nl">'+esc(t(g[1]))+'</span></button>'
  });
  $("nav").innerHTML=h;
  $("bellb").title=t("Allarmi");
  $("outb").title=t("Logout, riavvia o sleep");
  $("tbt").classList.toggle("hide",ROLE<1);
  themeApply();
  cpTitles();
  skRuolo();
  show(canSee(cur)?cur:"sum")
}
function gGo(g){
  var L=gTabs(g);
  show(LASTG[g]&&canSee(LASTG[g])?LASTG[g]:L[0])
}
/* Il sottomenu del gruppo: i bottoni delle voci permesse, con i titoli delle sezioni. */
function subDraw(id,g){
  var L=gTabs(g),h="";
  if(L.length<2)return h;
  L.forEach(function(k,ix){
    var sc=TSEC[k];
    var sp=ix>0?TSEC[L[ix-1]]:undefined;
    if(sc!==undefined&&sc!==sp)h+='<span class="gsep">'+esc(t(SECN[sc]))+'</span>';
    else if(sc===undefined&&sp!==undefined)h+='<span class="gsep"></span>';
    h+='<button class="'+(k==id?"on":"")+'" onclick="show(\''+k+'\')">'+esc(t(voceDi(k).testo))+
      '</button>';
  });
  return h;
}
/* La voce con questo id, oppure null. */
function voceDi(id){
  for(var i=0;i<VOCI.length;i++){
    if(VOCI[i].id==id)return VOCI[i];
  }
  return null;
}
/* Schede dentro una voce (modello WAI-ARIA). Nella pagina c'e solo la barra sk_<voce> con i bottoni:
   dopo la barra, ogni elemento fratello e il pannello del bottone con lo stesso numero (sp_<voce><n>).
   Attributi e pannelli li prepara skPrepara. Bottone con class="adm": solo Admin; class="op": dall'Operatore in su.
   Solo la scheda attiva e nella sequenza di Tab, le frecce spostano tra le schede visibili. */
function skPrepara(){
  document.querySelectorAll(".sub[id^=sk_]").forEach(function(b){
    var v=b.id.slice(3);
    var p=b.nextElementSibling;
    b.setAttribute("role","tablist");
    b.onkeydown=function(e){skTasto(e,v)};
    [].forEach.call(b.children,function(s,i){
      s.id=b.id+i;
      p.id="sp_"+v+i;
      s.setAttribute("role","tab");
      s.setAttribute("aria-controls",p.id);
      s.onclick=function(){skApri(v,i)};
      p.setAttribute("role","tabpanel");
      p.setAttribute("aria-labelledby",s.id);
      p.tabIndex=0;
      p=p.nextElementSibling
    });
    skMostra(v,0)
  })
}
function skMostra(v,i){
  var L=$("sk_"+v).children;
  for(var k=0;k<L.length;k++){
    var on=k==i;
    L[k].classList.toggle("on",on);
    L[k].setAttribute("aria-selected",on?"true":"false");
    if(on)L[k].removeAttribute("tabindex");
    else L[k].setAttribute("tabindex","-1");
    $("sp_"+v+k).classList.toggle("hide",!on)
  }
}
/* Dopo l'accesso o il cambio lingua: nome della barra, niente barra con una sola scheda permessa,
   mai aperta una scheda che questo ruolo non vede. */
function skRuolo(){
  document.querySelectorAll(".sub[id^=sk_]").forEach(function(b){
    var v=b.id.slice(3);
    var ok=ROLE<1?"button:not(.adm):not(.op)":ROLE<2?"button:not(.adm)":"button";
    var n=b.querySelectorAll(ok).length;
    b.setAttribute("aria-label",t(voceDi(v).testo));
    b.classList.toggle("hide",n<2);
    if(!b.querySelector(".on").matches(ok))skMostra(v,0)
  })
}
function skApri(v,i,fuoco){
  skMostra(v,i);
  if(fuoco)$("sk_"+v).children[i].focus();
  if(v=="log"&&!i)$("lg").scrollTop=$("lg").scrollHeight;
  if(v=="pin"&&!i)pinSchedaDisegno()
}

function skTasto(e,v){
  var T=$("sk_"+v).children;
  var L=[].filter.call(T,function(b){return b.offsetParent});
  var n=L.length,i=L.indexOf($("sk_"+v).querySelector(".on")),k=e.key;
  if(k=="ArrowRight")i=(i+1)%n;
  else if(k=="ArrowLeft")i=(i+n-1)%n;
  else if(k=="Home")i=0;
  else if(k=="End")i=n-1;
  else return;
  e.preventDefault();
  skApri(v,[].indexOf.call(T,L[i]),true)
}
/* L'indirizzo mostra #aiuto solo sulla voce Aiuto. */
function hashAiuto(id){
  try{
    if(id=="help"&&location.hash!="#aiuto")history.replaceState(null,"","#aiuto");
    if(id!="help"&&location.hash=="#aiuto")history.replaceState(null,"",location.pathname);
  }catch(e){}
}

function show(id){
  if(!canSee(id))id="sum";
  var via=VIA[id];
  if(via)id=via[0];
  if(!canSee(id))id="sum";
  cur=id;
  var g=gOf(id);
  LASTG[g]=id;

  VOCI.forEach(function(v){
    var e=$("t_"+v.id);
    if(e)e.className=v.id==id?"":"hide";
  });
  GRP.forEach(function(x){
    var e=$("n_"+x[0]);
    if(e)e.className="nb"+(x[0]==g?" on":"");
  });
  var h=subDraw(id,g);
  $("gsub").innerHTML=h;
  $("gsub").className="sub gsub"+(h?"":" hide");
  $("alerts").className="alerts hide";

  VOCI.forEach(function(v){
    if(v.esci&&v.id!=id)v.esci();
  });
  var voce=voceDi(id);
  if(voce&&voce.apri)voce.apri(id);
  if(via&&via[1]!==undefined&&cur==via[0])skApri(id,via[1]);
  hashAiuto(id);
}
window.addEventListener("hashchange",function(){
  if(location.hash=="#aiuto"&&loggedIn())show("help")
});
skPrepara();
