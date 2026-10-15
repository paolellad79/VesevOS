/* ===== Schema dei collegamenti di una scheda modulo: disegnato dalla pagina con i dati del file (pin, tensioni, componenti) ===== */
function modVn(v){return v=="3V3"?"3,3 V":String(v).replace("V"," V")}
function modColN(k,hex){var m={red:t("rosso"),black:t("nero"),blue:t("blu"),green:t("verde"),yellow:t("giallo"),orange:t("arancione"),white:t("bianco"),brown:t("marrone"),purple:t("viola"),gray:t("grigio"),pink:t("rosa"),cyan:t("azzurro")};return m[k]||hex}
function modSchema(j){
 var out=/^(rele|led|pin)$/.test(j.c),pw=j.p,ext=(pw=="12V"||pw=="24V"||pw=="230V"),bd=(pw=="3V3"||pw=="5V"),lv=j.lv=="5V"?"5 V":"3,3 V",parts=j.parts||[],R=[],ex=null;
 if(ext)ex={l:pw=="230V"?"230 V":"+"+modVn(pw),r:"VCC",c:"#c0392b",cn:t("rosso"),v:tf("{0} alimentazione esterna",modVn(pw)),k:"vcc"};
 else if(bd)R.push({l:pw=="3V3"?"3V3":"5V",r:"VCC",c:"#c0392b",cn:t("rosso"),v:tf("{0} alimentazione",modVn(pw)),k:"vcc"});
 R.push({l:"GND",r:"GND",c:"#222222",cn:t("nero"),v:ext?t("0 V (massa dell'alimentatore unita a GND della scheda)"):t("0 V (massa)"),k:"gnd"});
 j.sig.forEach(function(s,i){var p=parts.filter(function(x){return x.s==s}),d=[];
  p.forEach(function(x){d.push(x.v+" "+(x.k=="serie"?t("in serie"):x.k=="gnd"?t("verso GND"):t("verso l'alimentazione")))});
  var nt=[tf("segnale a {0}",lv),out?t("scheda → modulo"):t("modulo → scheda")];
  if(j.pull=="u")nt.push(t("pull-up interno acceso"));if(j.pull=="d")nt.push(t("pull-down interno acceso"));
  if(j.low)nt.push(t("attivo basso (0 = acceso)"));
  R.push({l:j.sug[i]!=null?"GPIO"+j.sug[i]:"GPIO ?",r:s,sg:s,c:j.c=="analogico"?"#d4730a":"#2d6cdf",cn:j.c=="analogico"?t("arancione"):t("blu"),v:nt.concat(d).join(" · "),k:"sig",p:p,out:out})});
 var wp=j.pins||[],cw=wp.length>0;   // [pins] del file: nome vero del pin del modulo e colore del filo (nome o #rrggbb)
 wp.forEach(function(w){var r=null;R.forEach(function(x){if((w.d=="VCC"&&x.k=="vcc")||(w.d=="GND"&&x.k=="gnd")||(x.k=="sig"&&x.sg==w.d))r=x});
  if(r){r.r=w.l;r.c=/^#[0-9a-f]{6}$/.test(w.c)?w.c:r.c;r.cn=modColN(w.cn,w.c)}});
 var y=ex?64:34,top=ex?12:12,ys=[],tot=0,svg="",tb="",h;
 var ey=ex?30:0;
 R.forEach(function(r){var vert=(r.p||[]).some(function(x){return x.k!="serie"});r.h=vert?86:46;ys.push(y+18);y+=r.h});
 var H=y+6,bx=8,bw=112,mx=360,mw=112,by=ex?58:6;
 svg='<svg viewBox="0 0 480 '+H+'" role="img" aria-label="'+esc(tf("Schema dei collegamenti di {0}",j.t))+'" style="width:100%;max-width:520px;background:#fff;border-radius:8px;font:12px sans-serif">';
 svg+='<rect x="'+bx+'" y="'+by+'" width="'+bw+'" height="'+(H-by-6)+'" rx="6" fill="#e8f1ff" stroke="#222"/><text x="'+(bx+8)+'" y="'+(by+14)+'" fill="#222" font-weight="bold">'+esc(t("Scheda"))+'</text>';
 svg+='<rect x="'+mx+'" y="6" width="'+mw+'" height="'+(H-12)+'" rx="6" fill="#e9f7ef" stroke="#222"/><text x="'+(mx+8)+'" y="20" fill="#222" font-weight="bold">'+esc(t("Modulo"))+'</text>';
 if(ex){svg+='<rect x="'+bx+'" y="6" width="'+bw+'" height="44" rx="6" fill="#fff3e0" stroke="#c0392b" stroke-width="2"/><text x="'+(bx+8)+'" y="22" fill="#222" font-weight="bold">'+esc(t("Alimentatore"))+'</text><text x="'+(bx+8)+'" y="40" fill="#c0392b" font-weight="bold">'+esc(ex.l)+'</text>';
  var ye=30;svg+='<line x1="'+(bx+bw)+'" y1="'+ye+'" x2="'+mx+'" y2="'+ye+'" stroke="#c0392b" stroke-width="3"/><circle cx="'+(bx+bw)+'" cy="'+ye+'" r="4" fill="#c0392b"/><circle cx="'+mx+'" cy="'+ye+'" r="4" fill="#c0392b"/><text x="'+(mx+8)+'" y="'+(ye+4)+'" fill="#222">VCC</text>';
  svg+='<text x="150" y="'+(ye-6)+'" fill="#c0392b" font-weight="bold">'+esc(modVn(pw))+'</text>';
  tb+='<tr><td>'+esc(ex.cn)+'</td><td>'+esc(t("Alimentatore"))+' '+esc(ex.l)+'</td><td>VCC</td><td>'+esc(ex.v)+'</td></tr>'}
 R.forEach(function(r,i){var yy=ys[i],x1=bx+bw,x2=mx,col=r.c,sw=r.k=="sig"?2.5:3;
  if(cw)svg+='<line x1="'+x1+'" y1="'+yy+'" x2="'+x2+'" y2="'+yy+'" stroke="#222" stroke-width="'+(sw+2.5)+'" stroke-linecap="round"/>';
  svg+='<line x1="'+x1+'" y1="'+yy+'" x2="'+x2+'" y2="'+yy+'" stroke="'+col+'" stroke-width="'+sw+'" stroke-linecap="round"/><circle cx="'+x1+'" cy="'+yy+'" r="4" fill="'+col+'"/><circle cx="'+x2+'" cy="'+yy+'" r="4" fill="'+col+'"/>';
  svg+='<text x="'+(bx+8)+'" y="'+(yy+4)+'" fill="#222" font-weight="bold">'+esc(r.l)+'</text><text x="'+(mx+8)+'" y="'+(yy+4)+'" fill="#222" font-weight="bold">'+esc(r.r)+'</text>';
  var vt=r.k=="vcc"?modVn(pw):r.k=="gnd"?"0 V":lv,warn=r.k=="sig"&&j.lv=="5V"&&!r.out;
  svg+='<text x="'+(x1+10)+'" y="'+(yy-7)+'" fill="'+(warn?"#c0392b":cw?"#222":col)+'" font-weight="bold">'+esc(vt+(warn?" !":"")+(cw?" · "+r.cn:""))+'</text>';
  if(r.k=="sig"){var ax=r.out?(x2-18):(x1+18);svg+='<polygon points="'+(r.out?ax+","+(yy-5)+" "+(ax+10)+","+yy+" "+ax+","+(yy+5):(ax+10)+","+(yy-5)+" "+ax+","+yy+" "+(ax+10)+","+(yy+5))+'" fill="'+col+'"/>';
   var vi=0;(r.p||[]).forEach(function(p){
    if(p.k=="serie"){svg+='<rect x="212" y="'+(yy-9)+'" width="56" height="18" fill="#fff" stroke="#222" stroke-width="2"/><text x="240" y="'+(yy-14)+'" text-anchor="middle" fill="#222" font-size="11">'+esc(p.v)+'</text>'}
    else{var px=vi++?316:164;svg+='<circle cx="'+px+'" cy="'+yy+'" r="3" fill="'+col+'"/><line x1="'+px+'" y1="'+yy+'" x2="'+px+'" y2="'+(yy+14)+'" stroke="#222" stroke-width="2"/><rect x="'+(px-8)+'" y="'+(yy+14)+'" width="16" height="26" fill="#fff" stroke="#222" stroke-width="2"/><text x="'+(px+14)+'" y="'+(yy+31)+'" fill="#222" font-size="11">'+esc(p.v)+'</text><line x1="'+px+'" y1="'+(yy+40)+'" x2="'+px+'" y2="'+(yy+52)+'" stroke="#222" stroke-width="2"/>';
     if(p.k=="gnd")svg+='<line x1="'+(px-9)+'" y1="'+(yy+52)+'" x2="'+(px+9)+'" y2="'+(yy+52)+'" stroke="#222" stroke-width="2"/><line x1="'+(px-6)+'" y1="'+(yy+56)+'" x2="'+(px+6)+'" y2="'+(yy+56)+'" stroke="#222" stroke-width="2"/><line x1="'+(px-3)+'" y1="'+(yy+60)+'" x2="'+(px+3)+'" y2="'+(yy+60)+'" stroke="#222" stroke-width="2"/>';
     else svg+='<text x="'+px+'" y="'+(yy+66)+'" text-anchor="middle" fill="#c0392b" font-weight="bold">'+esc(modVn(pw))+'</text>'}})}
  tb+='<tr><td>'+esc(r.cn)+'</td><td>'+esc(r.l)+'</td><td>'+esc(r.r)+'</td><td>'+esc(r.v)+'</td></tr>'});
 svg+='</svg>';
 h=svg+'<table class="ftab" style="margin-top:8px"><tr><th>'+esc(t("Filo"))+'</th><th>'+esc(t("Dalla scheda"))+'</th><th>'+esc(t("Al modulo"))+'</th><th>'+esc(t("Tensione e note"))+'</th></tr>'+tb+'</table>';
 if(j.lv=="5V"&&!out)h+='<p class="fhint" style="color:var(--kot)"><b>'+esc(t("Il segnale del modulo e a 5 V: i pin della scheda reggono solo 3,3 V. Metti un convertitore di livello o un partitore prima del pin."))+'</b></p>';
 else if(j.lv=="5V")h+='<p class="fhint">'+esc(t("Il modulo vuole il segnale a 5 V: con i 3,3 V della scheda potrebbe non attivarsi. Controlla il datasheet."))+'</p>';
 if(ext)h+='<p class="fhint">'+esc(t("Unisci la massa (GND) dell'alimentatore con la massa della scheda."))+'</p>';
 return h}
