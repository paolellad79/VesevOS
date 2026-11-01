/* ===== Avvio: programmi con avvio automatico (script, app Berry, app ELF). Elenco e ordine; solo l'Admin cambia ===== */
var BP={list:[],cand:[],max:8};
function bpLoad(){var cs=[];
 var a=api("/api/progboot").then(function(r){return r.json()}).then(function(j){BP.list=j.list||[];BP.max=j.max||8}).catch(function(){});
 var s=ROLE==2?api("/api/script").then(function(r){return r.json()}).then(function(j){(j.list||[]).forEach(function(x){cs.push({k:"s",n:x.n})})}).catch(function(){}):Promise.resolve();
 var b=ROLE==2?api("/api/apps").then(function(r){return r.json()}).then(function(j){(j.list||[]).forEach(function(x){if(x.ok&&x.as!="never")cs.push({k:"b",n:x.n})})}).catch(function(){}):Promise.resolve();
 var e=ROLE==2&&AUTH&&AUTH.elf?api("/api/elf").then(function(r){return r.json()}).then(function(j){(j.mods||[]).forEach(function(x){if(x.ok)cs.push({k:"e",n:x.n})})}).catch(function(){}):Promise.resolve();
 Promise.all([a,s,b,e]).then(function(){BP.cand=cs;bpDraw()})}
function bpHas(k,n){return BP.list.some(function(x){return x.k==k&&x.n==n})}
function bpDraw(){var h="",adm=ROLE==2;
 if(!BP.list.length)h='<p class="fhint">'+esc(t("Nessun programma parte da solo."))+'</p>';
 BP.list.forEach(function(x,i){var oq=i;
  h+='<div class="row"><span><b>'+esc(x.n)+'</b> <span class="pill">'+esc(progExt(x.k).slice(1))+'</span>'+(x.f?' <span class="pill a">'+esc(t("obbligatorio"))+'</span>':"")+(x.ok===false?' <span class="pill k">'+esc(t("manca il file"))+'</span>':"")+'</span>'+
   (adm&&!x.f?'<span class="svi">'+bpArrow(i,-1,"&#9650;",tf("Sposta su {0}",x.n),i==0)+bpArrow(i,1,"&#9660;",tf("Sposta giu {0}",x.n),i==BP.list.length-1)+sibB("sm","i-del",tf("Togli {0}",x.n),false,false,"bpDel("+oq+")")+'</span>':"")+'</div>'});
 $("bpl").innerHTML=h;
 var o='';BP.cand.forEach(function(x,i){if(!bpHas(x.k,x.n))o+='<option value="'+i+'">'+esc(x.n+progExt(x.k))+'</option>'});
 $("bpsel").innerHTML=o||'<option value="">'+esc(t("Nessun altro programma"))+'</option>'}
function bpArrow(i,d,sym,lab,dis){return'<button type="button" class="btn gray" aria-label="'+esc(lab)+'" title="'+esc(lab)+'"'+(dis?" disabled":"")+' onclick="bpMove('+i+','+d+')">'+sym+'</button>'}
function bpMove(i,d){var j=i+d;if(j<0||j>=BP.list.length||BP.list[i].f||BP.list[j].f)return;var x=BP.list[i];BP.list[i]=BP.list[j];BP.list[j]=x;bpDraw()}
function bpDel(i){BP.list.splice(i,1);bpDraw()}
function bpAdd(){var v=$("bpsel").value;if(v==="")return;var x=BP.cand[+v];if(!x||BP.list.length>=BP.max)return;BP.list.push({k:x.k,n:x.n,f:false});bpDraw()}
function bpSave(){var csv=BP.list.filter(function(x){return !x.f}).map(function(x){return x.k+":"+x.n}).join(",");
 post("/api/progboot",{list:csv}).then(function(r){return r.json()}).then(function(j){if(j.ok===false){msg("bpm",j.err,false);return}BP.list=j.list||[];bpDraw();msg("bpm",t("Salvato"),true)}).catch(function(){})}
