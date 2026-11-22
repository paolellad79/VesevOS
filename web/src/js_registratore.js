/* ===== Registratore di dati ===== */
var RC=null,RCT=0,RCCOL=["var(--ac)","var(--ac2)","var(--wa)","var(--ko)","var(--ok)","var(--mut)"];

function apriRegistratore(){
  rcLoad();
  RCT=setInterval(rcLoad,5000)
}

function fermaRegistratore(){
  if(RCT)clearInterval(RCT);
  RCT=0
}

function rcLoad(){
  api("/api/rec").then(function(r){return r.json()}).then(rcDraw).catch(function(){})
}

function rcVal(c,v){
  if(v===null)return "-";
  return c.k==1?(v/100).toFixed(2)+" °C":c.k==2?v+" KB":""+v
}

function rcTime(p){
  return p[1]?fmtOraData(p[0]):"+"+durText(p[0])      /* ora e data nel formato scelto in Localizzazione */
}

function rcYn(v){return v?t("si"):t("no")}

function rcDraw(j){
  RC=j;
  var st=j.on?tf("Acceso: intervallo predefinito {0} secondi, {1} punti in memoria",j.sec,j.count):t("Spento");
  $("rcst").className="msg "+(j.on?"ok":"");
  $("rcst").style.display="block";
  $("rcst").textContent=st+(j.err?" · "+j.err:"");
  if(j.err)$("rcst").className="msg ko";
  $("rc_cap").textContent=t("Motore: task del firmware")+" · "+
    tf("Capacita dichiarate del chip: coprocessore ULP {0}, CPU a basso consumo {1}, contatore di impulsi {2}, memoria che resta accesa {3} byte",
      rcYn(j.caps.ulp),rcYn(j.caps.lp),rcYn(j.caps.pcnt),j.caps.rtc);
  rcChart(j);
  rcFiles(j);
  rcAzDraw(j);
  rcForm(j)
}

function rcChart(j){
  var svg=$("rc_svg"),n=j.pts.length,leg="",h="",i,k;
  var cols=j.cols,out="";
  for(k=0;k<cols.length;k++){
    var mn=1e18,mx=-1e18,p="",last=null;
    for(i=0;i<n;i++){
      var v=j.pts[i][2+k];
      if(v===null)continue;
      if(v<mn)mn=v;
      if(v>mx)mx=v
    }
    if(mx==mn){mx=mn+1}
    for(i=0;i<n;i++){
      var w=j.pts[i][2+k];
      if(w===null)continue;
      last=w;
      p+=(n>1?i*300/(n-1):150).toFixed(1)+","+(112-(w-mn)*104/(mx-mn)).toFixed(1)+" "
    }
    if(p)out+='<polyline points="'+p+'" fill="none" style="stroke:'+RCCOL[k%6]+'" stroke-width="2" vector-effect="non-scaling-stroke"/>';
    leg+='<span style="margin-right:14px"><b style="color:'+RCCOL[k%6]+'">&#9632;</b> '+esc(cols[k].n)+": "+
      esc(rcVal(cols[k],last))+"</span>"
  }
  svg.innerHTML=out;
  $("rc_leg").innerHTML=leg||esc(t("Nessun punto ancora"));
  h="<tr><th scope=\"col\">"+esc(t("Quando"))+"</th>";
  for(k=0;k<cols.length;k++)h+="<th scope=\"col\">"+esc(cols[k].n)+"</th>";
  h+="</tr>";
  for(i=n-1;i>=Math.max(0,n-10);i--){
    h+="<tr><td>"+esc(rcTime(j.pts[i]))+"</td>";
    for(k=0;k<cols.length;k++)h+="<td>"+esc(rcVal(cols[k],j.pts[i][2+k]))+"</td>";
    h+="</tr>"
  }
  $("rc_tab").innerHTML=h
}

function rcSel(id,list,cur){
  var h='<option value="">'+esc(t("(nessuna)"))+"</option>",i;
  for(i=0;i<list.length;i++)h+='<option value="'+esc(list[i])+'"'+(list[i]==cur?" selected":"")+">"+esc(list[i])+"</option>";
  $(id).innerHTML=h
}

function rcForm(j){
  var x=document.activeElement;
  if(x&&$("rc_form").contains(x))return;
  $("rc_on").value=j.on?"1":"0";
  $("rc_sec").value=j.sec;
  $("rc_where").value=j.where;
  $("rc_max").value=j.maxkb;
  $("rc_lim").value=j.limite;
  $("rc_nome").value=j.nome;
  $("rc_t").checked=j.t;
  $("rc_r").checked=j.r;
  for(var i=0;i<4;i++){
    rcSel("rc_d"+(i+1),j.fonti,j.dev[i]);
    $("rc_s"+(i+1)).value=j.secdev[i]
  }
  $("rc_st").value=j.sect;
  $("rc_sr").value=j.secr;
  rcAzForm(j);
  rcWhereUi()
}

function rcWhereUi(){
  $("rc_minh").textContent=$("rc_where").value=="1"?t("Sulla microSD: da 1 secondo a 24 ore, spazio fino a 65535 KB."):t("Sulla memoria interna: da 10 secondi a 24 ore, spazio fino a 1024 KB.")
}

function rcSave(){
  var d={on:$("rc_on").value,sec:$("rc_sec").value,where:$("rc_where").value,maxkb:$("rc_max").value,
    t:$("rc_t").checked?1:0,r:$("rc_r").checked?1:0,nome:$("rc_nome").value,limite:$("rc_lim").value,sect:$("rc_st").value,secr:$("rc_sr").value,
    azioni:rcAzMask(),evmax:$("rc_evmax").value};
  for(var i=1;i<=4;i++){
    d["dev"+i]=$("rc_d"+i).value;
    d["sec"+i]=$("rc_s"+i).value
  }
  post("/api/rec/set",d).then(function(r){return r.json()}).then(function(j){
    if(j.ok===false){
      msg("rcm",j.err,false);
      return
    }
    msg("rcm",t("Salvato"),true);
    rcLoad()
  }).catch(function(){})
}
