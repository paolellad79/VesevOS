/* ===== Localizzazione ===== */
var LOC={cc:"",r:null,mini:null};
function lcLoad(){
  Promise.all([api("/api/region").then(function(r){return r.json()}),
    commonLoad()]).then(function(a){
      var r=a[0];
      LOC.r=r;
      LOC.cc=r.country;
      fillLangSel();
      $("lc_lang").value=r.lang||LANG;
      LOC.ntp=r.ntp;
      $("lc_df").value=r.dateFmt;
      $("lc_dft").value=r.dataCortaTesto||"";
      $("lc_dl").value=r.dataLungaFmt;
      $("lc_dlt").value=r.dataLungaTesto||"";
      $("lc_tf").value=r.timeFmt;
      $("lc_ts").value=r.oraLungaSec;
      $("lc_tl").value=r.oraLungaFmt;
      $("lc_tlt").value=r.oraLungaTesto||"";
      $("lc_ds").value=r.decSep;
      $("lc_ws").value=r.weekStart;
      $("lc_tu").value=r.tempUnit;
      $("lc_tarsoc").value=(r.tarSoc/10).toFixed(1);
      $("lc_tarext").value=(r.tarExt/10).toFixed(1);
      lcTarHint();
      $("lc_ant").value=r.antExt;
      $("lc_gain").value=r.gain;
      lcAnt();
      lcTzFill(r.tz,r.tzName);
      lcCountry();
      lcFmt();
      lcRadio(r)
    }).catch(function(){})
}

/* Formati di data e ora: scelte del modulo (non ancora salvate) */
function lcImpost(){
  return{
    dataCorta:+$("lc_df").value,dataCortaTesto:$("lc_dft").value,
    dataLunga:+$("lc_dl").value,dataLungaTesto:$("lc_dlt").value,
    ampm:+$("lc_tf").value==1,oraLungaSec:+$("lc_ts").value==1,
    oraLunga:+$("lc_tl").value,oraLungaTesto:$("lc_tlt").value
  }
}

/* Primo errore dei formati scritti a mano: un testo scelto deve essere valido; se non e scelto, quando c'e, va controllato lo stesso */
function lcFmtErr(im){
  var v=[[im.dataCorta==3,im.dataCortaTesto],[im.dataLunga==2,im.dataLungaTesto],[im.oraLunga==1,im.oraLungaTesto]];
  for(var i=0;i<3;i++){
    if(!v[i][0]&&!v[i][1])continue;
    var e=dfControlla(v[i][1]);
    if(e)return dfMessaggio(e)
  }
  return""
}

/* Anteprima accanto a ogni formato, con l'ora della scheda adesso. Un testo scritto a mano non valido mostra l'errore */
function lcAnteprima(id,scritta,testo,fa){
  var e=scritta?dfControlla(testo):0;
  $(id).textContent=e?dfMessaggio(e):fa()
}

function lcFmt(){
  var im=lcImpost(),d=dfDaData(new Date(Date.now()+(fmtSrc().to||0)*1000)),n=dfNomi();
  $("lc_dft").className=im.dataCorta==3?"":"hide";
  $("lc_dlt").className=im.dataLunga==2?"":"hide";
  $("lc_tlt").className=im.oraLunga==1?"":"hide";
  lcAnteprima("lc_dfp",im.dataCorta==3,im.dataCortaTesto,function(){return dfDataCorta(im,d,n)});
  lcAnteprima("lc_dlp",im.dataLunga==2,im.dataLungaTesto,function(){return dfDataLunga(im,d,n)});
  lcAnteprima("lc_tlp",im.oraLunga==1,im.oraLungaTesto,function(){return dfOraLunga(im,d,n)});
  $("lc_tfp").textContent=dfOraCorta(im,d)
}

/* Taratura: in decimi di grado Celsius (come la scheda). Fahrenheit: la correzione si moltiplica per 9/5, senza +32 */
function lcTarDecimi(id){
  var v=parseFloat($(id).value);
  return isNaN(v)?0:Math.round(v*10)
}

function lcTarHint(){
  var e=$("lc_tarf");
  if(+$("lc_tu").value!=1){
    e.textContent="";
    return
  }
  var a=(lcTarDecimi("lc_tarsoc")*9/50).toFixed(1),
    b=(lcTarDecimi("lc_tarext")*9/50).toFixed(1);
  e.textContent=tf("Con i gradi Fahrenheit la correzione vale {0} F (chip) e {1} F (esterno): si moltiplica per 9/5, senza aggiungere 32",a,b)
}

function lcCountry(){
  var c=COMMON.by[LOC.cc];
  $("lcc").textContent=c?cName(c)+" ("+c.cc+")"
    :t("Paese non scelto: la radio usa le regole piu prudenti (canali 1-11)");
}

function lcTzFill(tz,name){
  var c=COMMON.by[LOC.cc],L=c?c.tz.slice():[],h="",f=-1;
  if(!L.some(function(z){return z[0]=="UTC"}))L.push(["UTC","UTC0"]);
  L.forEach(function(z,i){
    h+='<option value="'+i+'">'+esc(z[0])+'</option>';
    if(z[1]==tz&&(f<0||z[0]==name))f=i
  });
  h+='<option value="-1">'+esc(t("Personalizzato"))+'</option>';
  $("lc_tz").innerHTML=h;
  $("lc_tz").value=f;
  LOC.tzl=L;
  $("lc_posix").value=tz||"";
  LOC.tzn=name||"";
  lcTz()
}

function lcTz(){
  var v=+$("lc_tz").value;
  $("lc_tzc").className=v<0?"":"hide";
  if(v>=0){
    $("lc_posix").value=LOC.tzl[v][1];
    LOC.tzn=LOC.tzl[v][0]
  }
}

function lcAnt(){$("lc_gw").className=$("lc_ant").value=="1"?"":"hide"}
function lcRadio(r){
  var c=COMMON.by[r.country],g=regRule(c);
  var ant=+r.antExt?t("esterna")+" ("+r.gain+" dBi)":t("interna");
  $("lcpaese").innerHTML=row(esc(t("Paese")),esc(c?cName(c)+" ("+c.cc+")":t("non scelto")));
  $("lcradio").innerHTML=row(esc(t("Canali permessi")),"1-"+r.ch)+
    row(esc(t("Limite del paese")),r.limit+" dBm EIRP")+
    row(esc(t("Potenza massima")),r.max+" dBm")+
    row(esc(t("Potenza in uso")),r.tx+" dBm")+
    row(esc(t("Antenna")),esc(ant))+
    (g?row(esc(t("Regole")),esc(g.rule)+(g.mark?" · "+esc(g.mark):"")):"");
  var x=$("lc_tx");
  x.max=r.max;
  x.value=r.txSet&&r.txSet<=r.max?r.txSet:r.max;
  lcTx()
}

function lcTx(){
  var x=$("lc_tx");
  $("lc_txv").textContent=tf("{0} dBm (massimo {1})",x.value,x.max)
}

function locPick(cc){
  LOC.cc=cc;
  lcDefaults(true)
}

function lcDefaults(keep){
  var c=COMMON.by[LOC.cc];
  if(!c){
    msg("lcm",t("Prima scegli il paese"),false);
    return
  }
  $("lc_df").value=c.f.d;
  $("lc_tf").value=c.f.h;
  lcFmt();
  $("lc_ds").value=c.f.s;
  $("lc_ws").value=c.f.w;
  $("lc_tu").value=c.f.t;
  LOC.ntp=c.ntp||"pool.ntp.org";
  lcTzFill(c.tz[0][1],c.tz[0][0]);
  lcCountry();
  if(LANGS.some(function(l){return l.code==c.f.l}))$("lc_lang").value=c.f.l;
  msg("lcm",t("Valori del paese caricati: premi Salva per applicarli"),true)
}

function lcSave(mid){
  mid=typeof mid=="string"?mid:"lcm";
  var fe=lcFmtErr(lcImpost());
  if(fe){
    msg(mid,fe,false);
    return
  }
  var d={country:LOC.cc,tz:$("lc_posix").value.trim(),
    tzname:+$("lc_tz").value<0?t("Personalizzato"):LOC.tzn,ntp:LOC.ntp,
    datefmt:$("lc_df").value,datacortatesto:$("lc_dft").value,
    datalungafmt:$("lc_dl").value,datalungatesto:$("lc_dlt").value,
    timefmt:$("lc_tf").value,oralungasec:$("lc_ts").value,
    oralungafmt:$("lc_tl").value,oralungatesto:$("lc_tlt").value,decsep:$("lc_ds").value,
    weekstart:$("lc_ws").value,tempunit:$("lc_tu").value,antenna:$("lc_ant").value,
    gain:$("lc_gain").value||0,txpower:$("lc_tx").value,
    tarsoc:lcTarDecimi("lc_tarsoc"),tarext:lcTarDecimi("lc_tarext")};
  var lg=$("lc_lang").value;
  post("/api/region",d).then(function(r){return r.json()}).then(function(j){
    if(j.ok===false){
      msg(mid,j.err,false);
      return
    }
    LOC.r=j;
    lcRadio(j);
    msg(mid,t("Salvato"),true);
    if(lg&&lg!=LANG)setLang(lg)
  }).catch(function(){})
}
