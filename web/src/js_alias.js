/* ===== alias e descrizione delle periferiche =====
   Si leggono da GET /api/settings (campo "alias": nome vero -> {a: alias, d: descrizione}) e si scrivono con POST /api/settings (solo Admin).
   L'alias e solo un nome MOSTRATO (menu, titoli, elenchi, schema dei pin): il nome vero resta quello dei comandi, delle funzioni e delle regole.
   Il testo e dell'utente: qui entra sempre in pagina con esc() o textContent. */
var ALIAS={};
function aliasLeggi(){
  return api("/api/settings").then(function(r){return r.json()}).then(function(c){
    ALIAS=c.alias||{}
  }).catch(function(){})
}
function aliasDi(id){
  var x=ALIAS[id];
  return x&&x.a?x.a:""
}
function descrDi(id){
  var x=ALIAS[id];
  return x&&x.d?x.d:""
}
/* Il testo da mostrare: l'alias se c'e, altrimenti l'etichetta di prima. */
function etichetta(id,label){
  return aliasDi(id)||label
}
/* Lo stesso in HTML (gia con esc), con il nome vero in piccolo accanto all'alias. */
function etichettaHtml(id,label){
  var a=aliasDi(id);
  return a?esc(a)+' <small class="alnome">'+esc(id)+'</small>':esc(label)
}
/* Attributo title con la descrizione (vuoto se non c'e). */
function descrTitle(id){
  var d=descrDi(id);
  return d?' title="'+esc(d)+'"':""
}
/* Alias e nome vero per un titolo scritto con textContent. */
function titoloAlias(id,label){
  var a=aliasDi(id);
  return a?a+" ("+id+")":label
}
function aliasByte(s){
  return typeof TextEncoder!="undefined"?new TextEncoder().encode(s).length:s.length
}
/* Il riquadro "Alias e descrizione" di una periferica, dentro l'elemento box. */
function aliasPannello(box,id){
  var e=$(box);
  if(!e||!/^[a-z0-9_]{1,31}$/.test(id||"")){
    if(e)e.innerHTML="";
    return
  }
  var adm=ROLE>=2,dis=adm?"":" disabled";
  e.innerHTML='<h4 style="margin:14px 0 6px">'+esc(t("Alias e descrizione"))+'</h4>'+
    '<label for="alA_'+id+'">'+esc(t("Alias"))+'</label><input id="alA_'+id+
    '" maxlength="24" autocomplete="off" spellcheck="false"'+dis+'>'+
    '<label for="alD_'+id+'">'+esc(t("Descrizione"))+'</label><input id="alD_'+id+
    '" maxlength="120" autocomplete="off"'+dis+'>'+
    '<p class="fhint">'+esc(tf(
      "L'alias si vede nei menu e nei titoli. Comandi, funzioni e regole usano sempre il nome vero: {0}.",
      id))+'</p>';
  $("alA_"+id).value=aliasDi(id);
  $("alD_"+id).value=descrDi(id);
  if(adm)e.insertAdjacentHTML("beforeend",
    '<button class="btn" onclick="aliasSalva(\''+id+'\')">'+esc(t("Salva"))+
    '</button><div id="alM_'+id+'" class="msg" role="status"></div>');
  else e.insertAdjacentHTML("beforeend",'<p class="fhint">'+
    esc(t("Alias e descrizione li cambia solo l'amministratore."))+'</p>')
}
function aliasSalva(id){
  var a=$("alA_"+id).value.trim(),d=$("alD_"+id).value.trim(),m="alM_"+id;
  if(aliasByte(a)>24){
    msg(m,t("L'alias puo avere al massimo 24 byte (una lettera accentata ne vale 2)"),false);
    return
  }
  if(aliasByte(d)>120){
    msg(m,t("La descrizione puo avere al massimo 120 byte (una lettera accentata ne vale 2)"),false);
    return
  }
  post("/api/settings",{n:id,alias:a,descrizione:d}).then(function(r){return r.json()}).then(function(j){
    if(!j.ok){
      msg(m,j.err,false);
      return
    }
    if(a||d)ALIAS[id]={a:a,d:d};
    else delete ALIAS[id];
    $("alA_"+id).value=a;
    $("alD_"+id).value=d;
    msg(m,j.dup?tf("Salvato. Attenzione: lo stesso alias e gia usato da {0}",j.dup):t("Salvato"),true);
    dvList().then(aliasTitoli).catch(function(){})
  }).catch(function(){})
}
/* Dopo un cambio: titoli delle pagine di bordo e della periferica aperta. */
function aliasTitoli(){
  if($("l_tit"))$("l_tit").textContent=voceTitolo("led_rgb1");
  if($("l2_tit"))$("l2_tit").textContent=voceTitolo("led1");
  var h=$("iod_h"),d=typeof IOS!="undefined"&&IOS.sel?ioFind(IOS.sel):null;
  if(h&&d)h.textContent=titoloAlias(d.n,d.n)
}
