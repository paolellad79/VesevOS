/* ===== App (.vap): elenco, pagina generica con i controlli dichiarati, avvio e stop. Il testo si modifica con l'editor dei file ===== */
var AP={list:[],sel:"",info:null,tm:0,run:false,busy:false,key:"",tmr:0};
function apLoad(){apTimerStop();api("/api/apps").then(function(r){return r.json()}).then(function(j){if(!j.ok){msg("appm",j.err,false);return}AP.list=j.list;apDraw(j);if(AP.sel)apPoll();
 AP.tm=setInterval(function(){if(cur!="app"||document.hidden){if(cur!="app")apTimerStop();return}if(AP.sel)apPoll()},1200)}).catch(function(){})}
function apTimerStop(){if(AP.tm){clearInterval(AP.tm);AP.tm=0}}
function apIco(x){
 if(x.ok&&x.icon){var sv=x.icon;if(sv.indexOf("xmlns")<0)sv=sv.replace("<svg",'<svg xmlns="http://www.w3.org/2000/svg"');return'<img class="api" alt="" width="24" height="24" src="data:image/svg+xml;charset=utf-8,'+enc(sv)+'">'}
 return'<svg class="i" aria-hidden="true"><use href="#i-fvap"/></svg>'}
function apDraw(j){var h="";
 if(!j.list.length)h='<tbody><tr><td colspan="3" class="fhint">'+esc(ROLE==2?t("Nessuna app. Premi Nuova app per crearne una."):t("Nessuna app disponibile per il tuo ruolo."))+'</td></tr></tbody>';
 else{h="<tbody>";j.list.forEach(function(x){var q=esc(x.n),oq="'"+x.n+"'",nm=/^[a-z0-9_-]{1,24}$/.test(x.n);
  h+='<tr><td><span class="scn">'+apIco(x)+'<b>'+(x.ok?esc(x.t):q)+'</b>'+(ROLE==2?'<span class="pill'+(x.ok?" v":" k")+'">'+esc(x.ok?t("valida"):t("errore"))+'</span>':'')+'</span>';
  if(x.ok)h+='<div class="fhint">'+esc(x.n+" · "+tf("Versione {0}",x.v)+(x.a?" · "+x.a:"")+" · "+tf("Controlli: {0}",x.c)+" · "+tf("Permessi: {0}",x.u||"-"))+'</div>';
  else h+='<div class="fhint bad">'+esc(x.e)+'</div>';
  h+='</td><td class="fsz">'+(x.ok&&ROLE==2?fSize(x.s):"")+'</td><td class="fac tkb"><span class="svi">'+(x.ok&&nm?sibB("sm go","i-sgo",tf("Apri {0}",x.n),AP.sel==x.n,false,"apOpen("+oq+")"):"")+(ROLE==2&&nm?sibB("sm","i-edit",tf("Modifica {0}",x.n),false,false,"apEdit("+oq+")")+sibB("sm","i-del",tf("Elimina {0}",x.n),false,false,"apDel("+oq+")"):"")+'</span></td></tr>'});h+="</tbody>"}
 if(AP._h!==h){AP._h=h;$("aptab").innerHTML=h}}
function apPath(n){return"/apps/"+n+".vap"}
function apEdit(n){api("/api/fs/text?path="+enc(apPath(n))).then(function(r){return r.text().then(function(x){return{ok:r.ok,x:x}})}).then(function(o){if(!o.ok){msg("appm",o.x,false);return}fEdOpen(apPath(n),o.x,false)}).catch(function(){})}
function apTpl(n){return"[manifest]\nname="+n+"\ntitle="+n.charAt(0).toUpperCase()+n.slice(1)+"\nversion=1\nuse=led\nwho=admin\n\n[icon]\n<svg viewBox=\"0 0 24 24\"><path d=\"M5 19l4-9 3 4 3-6 4 11z\" fill=\"none\" stroke=\"#c0392b\" stroke-width=\"2\"/></svg>\n\n[page]\nswitch id=power label=\"Accesa\"\nslider id=lum label=\"Luminosita\" min=0 max=255\ntext id=info label=\"Stato\"\n\n[code]\ndef on_set(id, v)\n  if id == 'power'\n    if v == 1\n      led('ffffff')\n    else\n      led('auto')\n    end\n  end\n  if id == 'lum'\n    dev_set('info', 'Luminosita ' + str(v))\n  end\nend\n\n[data]\npower=0\n"}
function apNew(){dlg({title:t("Nuova app"),body:'<label for="dn">'+esc(t("Nome dell'app"))+'</label><input id="dn" autocomplete="off" maxlength="24" spellcheck="false" autocapitalize="off" placeholder="lampada"><p class="fhint" style="margin:0">'+esc(t("Da 1 a 24 caratteri: lettere minuscole, numeri, trattino e sottotrattino."))+'</p>',ok:t("Crea"),
 onOk:function(){var n=dlgV("dn").toLowerCase().replace(/\.vap$/,"");if(!/^[a-z0-9_-]{1,24}$/.test(n))return t("Nome non valido");if(AP.list.some(function(x){return x.n==n}))return t("Esiste gia");
  fEdOpen(apPath(n),apTpl(n),true);return""}})}
function apDel(n){dlg({title:t("Eliminare?"),body:'<p style="margin:0">'+esc(tf("L'app {0} verra eliminata.",n))+'</p><p style="margin:8px 0 0;color:var(--mut)">'+esc(t("Non si puo annullare."))+'</p>',ok:t("Elimina"),danger:true,
 onOk:function(){if(AP.sel==n)apClose();return post("/api/fs/del",{path:apPath(n)}).then(function(r){return r.json()}).then(function(j){apLoad();return j.ok?"":j.err})}})}
/* ---- la pagina generica dell'app: UN solo disegno nel firmware, legge i controlli dichiarati nel manifest (niente HTML dell'utente) ---- */
function apOpen(n){api("/api/app?name="+enc(n)).then(function(r){return r.json()}).then(function(j){if(!j.ok){msg("appm",j.err,false);return}
 AP.sel=n;AP.info=j;AP.key="";AP.run=false;apBuild();apPoll();if(AP._h){AP._h="";apLoad()}else apDraw({list:AP.list});$("apcard").scrollIntoView({block:"nearest"})}).catch(function(){})}
function apClose(){AP.sel="";AP.info=null;AP.key="";$("apcard").className="card hide";AP._h="";apDraw({list:AP.list})}
function apCtl(c){var id=esc(c.id),l=esc(c.l),dis=AP.run?"":" disabled",x='<div class="apc" id="apw_'+id+'">';
 if(c.k=="switch")x+='<span class="apl" id="apt_'+id+'">'+l+'</span><button type="button" class="sw1" id="apc_'+id+'" role="switch" aria-checked="false" aria-labelledby="apt_'+id+'"'+dis+' onclick="apTog(\''+c.id+'\')"></button>';
 else if(c.k=="slider")x+='<label class="apl" for="apc_'+id+'">'+l+'</label><input type="range" id="apc_'+id+'" min="'+(+c.mn)+'" max="'+(+c.mx)+'" value="'+(+c.mn)+'"'+dis+' oninput="apSl(\''+c.id+'\',this.value,0)" onchange="apSl(\''+c.id+'\',this.value,1)"><output class="apv" id="apo_'+id+'"></output>';
 else if(c.k=="color")x+='<label class="apl" for="apc_'+id+'">'+l+'</label><input type="color" id="apc_'+id+'" value="#000000"'+dis+' onchange="apSend(\''+c.id+'\',this.value.slice(1))">';
 else if(c.k=="text")x+='<label class="apl" for="apc_'+id+'">'+l+'</label><input type="text" id="apc_'+id+'" maxlength="40"'+dis+' onkeydown="if(event.key===\'Enter\')apSend(\''+c.id+'\',this.value)" onchange="apSend(\''+c.id+'\',this.value)">';
 else if(c.k=="button")x+='<button type="button" class="btn gray" id="apc_'+id+'"'+dis+' onclick="apSend(\''+c.id+'\',1)">'+l+'</button>';
 else x+='<span class="apl" id="apt_'+id+'">'+l+'</span><svg class="apg" id="apc_'+id+'" viewBox="0 0 200 60" preserveAspectRatio="none" role="img" aria-labelledby="apt_'+id+'"></svg><output class="apv" id="apo_'+id+'"></output>';
 return x+'</div>'}
function apBuild(){var j=AP.info;if(!j)return;$("apcard").className="card";
 $("aphead").innerHTML=apIco({ok:true,icon:j.icon})+'<b>'+esc(j.t)+'</b>';
 $("apctl").innerHTML=j.ctl.length?j.ctl.map(apCtl).join(""):'<p class="fhint">'+esc(t("Questa app non ha controlli: lavora da sola."))+'</p>'}
function apState(j){AP.run=!!j.run;AP.busy=!!j.busy;
 var k=(AP.run?1:0)+"|"+(AP.busy?1:0);
 $("appill").className="pill"+(AP.run?" a":"");$("appill").textContent=AP.run?t("in esecuzione"):t("ferma");
 $("apgo").disabled=AP.busy;$("apst").disabled=!AP.run;$("apgo").setAttribute("aria-pressed",AP.run?"true":"false");
 if(AP.key!==k){AP.key=k;[].forEach.call($("apctl").querySelectorAll("input,button"),function(e){e.disabled=!AP.run})}
 (j.vals||[]).forEach(function(v){var e=$("apc_"+v.id);if(!e)return;var f=document.activeElement===e;
  if(v.n!==undefined){if(e.getAttribute("role")=="switch"){e.className="sw1"+(v.n?" on":"");e.setAttribute("aria-checked",v.n?"true":"false")}else if(e.type=="range"){if(!f)e.value=v.n;var o=$("apo_"+v.id);if(o)o.textContent=v.n}}
  else if(v.a!==undefined)apChart(e,v.a,$("apo_"+v.id));
  else if(v.s!==undefined&&!f){if(e.type=="color")e.value="#"+(/^[0-9a-f]{6}$/.test(v.s)?v.s:"000000");else if(e.type=="text")e.value=v.s}});
 var o=$("apout");o.textContent=(j.out||"")+(j.err?(j.out?"\n":"")+j.err:"");o.className="lgv"+(j.err?" bad":"")+(o.textContent?"":" hide")}
function apChart(svg,a,out){var n=a.length,mn=1e18,mx=-1e18,p="",i;if(out)out.textContent=n?a[n-1]:"";if(!n){svg.innerHTML="";return}
 for(i=0;i<n;i++){if(a[i]<mn)mn=a[i];if(a[i]>mx)mx=a[i]}if(mx==mn){mx=mn+1}
 for(i=0;i<n;i++)p+=(n>1?i*200/(n-1):100).toFixed(1)+","+(56-(a[i]-mn)*52/(mx-mn)).toFixed(1)+" ";
 svg.innerHTML='<polyline points="'+p+'" fill="none" stroke="currentColor" stroke-width="2" vector-effect="non-scaling-stroke"/>'}
function apPoll(){if(!AP.sel)return;var n=AP.sel;api("/api/app/state?name="+enc(n)).then(function(r){return r.json()}).then(function(j){if(n!=AP.sel)return;if(!j.ok){msg("appm",j.err,false);apClose();return}apState(j)}).catch(function(){})}
function apStart(){post("/api/app/start",{name:AP.sel}).then(function(r){return r.json()}).then(function(j){msg("appm",j.ok?t("App avviata"):j.err,j.ok);apPoll()}).catch(function(){})}
function apStop(){post("/api/app/stop",{name:AP.sel}).then(function(r){return r.json()}).then(function(j){msg("appm",j.ok?t("App fermata"):j.err,j.ok);apPoll()}).catch(function(){})}
function apSend(id,v){if(!AP.run)return;post("/api/app/set",{name:AP.sel,id:id,v:v}).then(function(r){return r.json()}).then(function(j){if(!j.ok)msg("appm",j.err,false);else apPoll()}).catch(function(){})}
function apTog(id){var e=$("apc_"+id);apSend(id,e&&e.getAttribute("aria-checked")=="true"?0:1)}
function apSl(id,v,fin){var o=$("apo_"+id);if(o)o.textContent=v;if(fin){clearTimeout(AP.tmr);apSend(id,v);return}if(AP.tmr)return;AP.tmr=setTimeout(function(){AP.tmr=0;var e=$("apc_"+id);if(e)apSend(id,e.value)},250)}
