/* ===== scheda microSD (API /api/sd e /api/sd/act): riquadro nella scheda Periferiche ===== */
function sdKb(k){return k>=1048576?(k/1048576).toFixed(1)+" GB":k>=1024?Math.round(k/1024)+" MB":k+" KB"}
/* widget della Home: c'e solo se la periferica "sd" e in io.cfg. Scheda tolta o assente: resta, con l'icona sbarrata e i dati grigi */
function widSd(d){var ok=d.s==1,on=ok?"":" off",nm=t("Scheda microSD"),
 st=ok?t("Scheda montata"):d.s==3?t("Scheda non presente"):d.s==4?t("Formato non supportato: serve FAT32"):t("Scheda non montata"),
 ic=ok?'<svg class="i" aria-hidden="true"><use href="#i-sd"/></svg>':'<svg class="i" role="img" aria-label="'+esc(t("Scheda microSD non presente"))+'"><title>'+esc(t("Scheda microSD non presente"))+'</title><use href="#i-sdx"/></svg>',
 sub=ok?esc(d.fs)+" · "+esc(tf("{0} liberi su {1}",sdKb(d.free),sdKb(d.cap)))+" · "+d.khz+" kHz":"-- · "+esc(tf("{0} liberi su {1}","0 KB","0 KB"))+" · "+d.khz+" kHz";
 return'<div class="wid wsd'+on+'" role="button" tabindex="0" onclick="show(\'io\')" onkeydown="if(event.key==\'Enter\')show(\'io\')" aria-label="'+esc(nm+": "+st)+'"><div class="wt">'+ic+esc(nm)+'</div><div class="wv">'+esc(st)+'</div><div class="ws">'+sub+'</div></div>'}
function sdLoad(){var c=$("sd_card");if(!c)return;if(ROLE<1){c.className="card hide";return}
 api("/api/sd").then(function(r){if(r.status!=200){c.className="card hide";throw 0}return r.json()}).then(function(d){
  if(!d.cfg){c.className="card hide";return}   /* nessuna periferica "sd" in io.cfg: il riquadro non si vede */
  var R=d.rows||[],h='<table>',up=d.up==1,adm=ROLE>=2;
  R.forEach(function(x){h+='<tr><td>'+esc(x[0])+'</td><td style="text-align:right"><b>'+esc(x[1])+'</b></td></tr>'});
  $("sd_rows").innerHTML=h+'</table>';c.className="card";
  $("sd_mnt").className=adm&&!up?"btn":"btn hide";$("sd_umt").className=adm&&up?"btn gray":"btn gray hide";
  $("sd_tst").className=adm&&up?"btn gray":"btn gray hide";$("sd_lsb").className=up?"btn gray":"btn gray hide";
  if(!up)$("sd_ls").innerHTML=""}).catch(function(){})}
function sdAct(a,arg){var b=["sd_mnt","sd_umt","sd_tst","sd_lsb"];b.forEach(function(i){$(i).disabled=true});
 post("/api/sd/act",{a:a,arg:arg||""}).then(function(r){return r.json()}).then(function(j){
  b.forEach(function(i){$(i).disabled=false});
  if(j.ok===false){msg("sd_m",j.err,false);sdLoad();return}
  if(a=="test")msg("sd_m",t("Prova riuscita")+": "+j.bytes+" B, "+t("scrittura")+" "+j.wms+" ms, "+t("lettura")+" "+j.rms+" ms",true);
  else msg("sd_m",t("Fatto"),true);
  sdLoad()}).catch(function(){b.forEach(function(i){$(i).disabled=false})})}
function sdLs(p){post("/api/sd/act",{a:"ls",arg:p}).then(function(r){return r.json()}).then(function(j){
  if(j.ok===false){msg("sd_m",j.err,false);return}
  var L=j.list||[],h='<table class="ftab">';
  if(j.dir!="/")h+='<tr><td><button class="btn gray" onclick="sdLs(\''+esc(j.dir.replace(/\/[^\/]*$/,"")||"/")+'\')">..</button></td><td></td></tr>';
  L.forEach(function(x){var full=(j.dir=="/"?"":j.dir)+"/"+x.n;
   if(x.x){h+='<tr><td><span class="fhint" title="'+esc(t("Nome non gestibile da qui (accenti, simboli o troppo lungo)"))+'">'+esc(x.n)+(x.d?"/":"")+' *</span></td><td style="text-align:right">'+(x.d?"":esc(sdKb(Math.ceil(x.s/1024))))+'</td></tr>';return}
   h+='<tr><td>'+(x.d?'<button class="btn gray" onclick="sdLs(this.dataset.p)" data-p="'+esc(full)+'">'+esc(x.n)+'/</button>':esc(x.n))+'</td><td style="text-align:right">'+(x.d?"":esc(sdKb(Math.ceil(x.s/1024))))+'</td></tr>'});
  if(!L.length)h+='<tr><td class="fhint">'+esc(t("Nessuna voce"))+'</td></tr>';
  if(L.some(function(x){return x.x}))h+='<tr><td class="fhint" colspan="2">* '+esc(t("Nome non gestibile da qui (accenti, simboli o troppo lungo)"))+'</td></tr>';
  if(j.more)h+='<tr><td class="fhint" colspan="2">'+esc(t("Ci sono altri file: l'elenco mostra solo i primi 32"))+'</td></tr>';
  $("sd_ls").innerHTML='<p class="fhint">'+esc(j.dir)+'</p>'+h+'</table>'}).catch(function(){})}
