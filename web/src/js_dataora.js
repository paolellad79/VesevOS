/* ===== Data e ora della scheda nei formati scelti in Localizzazione =====
   Gemella di core/vos_datefmt.cpp e core/vos_timefmt.cpp: le due devono dare lo stesso testo (prova di parita:
   tools/hosttest/prova_formato_data.sh). Non usare toLocaleString: segue il computer e non la scheda.
   CORTA (tabelle, registro, elenchi): ore e minuti, mai secondi. LUNGA (testata, accesso, riquadri grandi).
   Formato scritto a mano, come il comando date: %d %-d %m %-m %Y %y %b %B %a %A %H %-H %I %-I %M %S %p %% (massimo 32 caratteri). */
var DF_TESTO_MAX=32,
  DF_PRONTA_CORTA=["%-d/%-m/%Y","%Y-%-m-%-d","%-m/%-d/%Y"],
  DF_PRONTA_LUNGA="%A %-d %B %Y";

/* 0 = va bene, 1 vuoto, 2 troppo lungo, 3 carattere non ammesso, 4 segno sconosciuto, 5 percentuale isolata in fondo */
function dfControlla(f){
  if(!f)return 1;
  if(f.length>DF_TESTO_MAX)return 2;
  for(var i=0;i<f.length;i++){
    var c=f.charCodeAt(i);
    if(c<32||c>126||c==34||c==92)return 3;
    if(c!=37)continue;
    i++;
    var senzaZero=false;
    if(f.charAt(i)=="-"){
      senzaZero=true;
      i++
    }
    if(i>=f.length)return 5;
    if((senzaZero?"dmHI":"dmHIMSYybBaAp%").indexOf(f.charAt(i))<0)return 4
  }
  return 0
}

function dfMessaggio(e){
  return[
    "",
    t("Formato vuoto: scrivi il formato"),
    t("Formato troppo lungo (massimo 32 caratteri)"),
    t("Caratteri non ammessi nel formato (solo ASCII, senza virgolette e senza barra rovesciata)"),
    t("Segno non riconosciuto dopo il segno di percentuale"),
    t("Segno di percentuale isolato in fondo al formato")
  ][e]
}

function dfTaglia(s){return s.replace(/^[ .,]+/,"").replace(/[ ,]+$/,"")}

/* Mese abbreviato ricavato dalla frase del mese ("%d Nov. %d"): il testo tra i due %d */
function dfMeseBreve(frase){
  var a=frase.indexOf("%d"),b=a<0?-1:frase.indexOf("%d",a+2);
  if(a>=0&&b>=0){
    var r=dfTaglia(frase.substring(a+2,b));
    if(r)return r
  }
  return dfTaglia(frase.split("%d").join(""))
}

function dfNum(v,duecifre){return duecifre&&v<10?"0"+v:String(v)}

function dfOre12(o){return o%12||12}

function dfSegno(c,senzaZero,d,n){
  var z=!senzaZero;
  switch(c){
    case"d":return dfNum(d.giorno,z);
    case"m":return dfNum(d.mese,z);
    case"H":return dfNum(d.ore,z);
    case"I":return dfNum(dfOre12(d.ore),z);
    case"M":return dfNum(d.minuti,true);
    case"S":return dfNum(d.secondi,true);
    case"Y":return String(d.anno);
    case"y":return dfNum(d.anno%100,true);
    case"p":return d.ore>=12?"PM":"AM";
    case"%":return"%";
    case"a":return n?n.giornoBreve[d.giornoSett]:"";
    case"A":return n?n.giornoLungo[d.giornoSett]:"";
    case"B":return n?n.meseLungo[d.mese-1]:"";
    case"b":return n?dfMeseBreve(n.fraseMese[d.mese-1]):""
  }
  return""
}

/* d = {anno,mese(1-12),giorno,giornoSett(0=domenica),ore,minuti,secondi}; n = nomi (o null: i nomi escono vuoti) */
function dfFormatta(f,d,n){
  var o="";
  for(var i=0;i<f.length;i++){
    if(f.charAt(i)!="%"){
      o+=f.charAt(i);
      continue
    }
    i++;
    var senzaZero=false;
    if(f.charAt(i)=="-"){
      senzaZero=true;
      i++
    }
    if(i>=f.length)break;
    o+=dfSegno(f.charAt(i),senzaZero,d,n)
  }
  return o
}

/* "giorno mese anno": il primo %d e il giorno, il secondo l'anno */
function dfFraseMese(frase,giorno,anno){
  var k=0,o="";
  for(var i=0;i<frase.length;i++){
    if(frase.charAt(i)=="%"&&frase.charAt(i+1)=="d"){
      o+=k?anno:giorno;
      k++;
      i++
    }
    else o+=frase.charAt(i)
  }
  return o
}

function dfDataCorta(im,d,n){
  var scritta=im.dataCorta==3&&dfControlla(im.dataCortaTesto)==0;
  return dfFormatta(scritta?im.dataCortaTesto:DF_PRONTA_CORTA[im.dataCorta>=0&&im.dataCorta<=2?im.dataCorta:0],d,n)
}

function dfDataLunga(im,d,n){
  if(im.dataLunga==2&&dfControlla(im.dataLungaTesto)==0)return dfFormatta(im.dataLungaTesto,d,n);
  if(im.dataLunga==1)return dfFormatta(DF_PRONTA_LUNGA,d,n);
  return n.giornoBreve[d.giornoSett]+" "+dfFraseMese(n.fraseMese[d.mese-1],d.giorno,d.anno)
}

function dfOraCorta(im,d){return dfFormatta(im.ampm?"%-I:%M %p":"%-H:%M",d,null)}

function dfOraLunga(im,d,n){
  if(im.oraLunga==1&&dfControlla(im.oraLungaTesto)==0)return dfFormatta(im.oraLungaTesto,d,n);
  var s=im.oraLungaSec;
  return dfFormatta(im.ampm?(s?"%-I:%M:%S %p":"%-I:%M %p"):(s?"%-H:%M:%S":"%-H:%M"),d,null)
}

/* Nomi nella lingua in uso, rifatti solo quando cambia il dizionario */
var DF_NOMI=null,DF_DIZ=null;
function dfFrase(s){return s.replace("{0}","%d").replace("{1}","%d")}

function dfNomi(){
  if(DF_NOMI&&DF_DIZ===DICT)return DF_NOMI;
  DF_DIZ=DICT;
  DF_NOMI={
    giornoBreve:[t("Dom."),t("Lun."),t("Mar."),t("Mer."),t("Giov."),t("Ven."),t("Sab.")],
    giornoLungo:[t("Domenica"),t("Lunedi"),t("Martedi"),t("Mercoledi"),t("Giovedi"),t("Venerdi"),t("Sabato")],
    meseLungo:[
      t("gennaio"),t("febbraio"),t("marzo"),t("aprile"),t("maggio"),t("giugno"),
      t("luglio"),t("agosto"),t("settembre"),t("ottobre"),t("novembre"),t("dicembre")
    ],
    fraseMese:[
      dfFrase(t("{0} Gen. {1}")),dfFrase(t("{0} Feb. {1}")),dfFrase(t("{0} Mar. {1}")),dfFrase(t("{0} Apr. {1}")),
      dfFrase(t("{0} Mag. {1}")),dfFrase(t("{0} Giu. {1}")),dfFrase(t("{0} Lug. {1}")),dfFrase(t("{0} Ago. {1}")),
      dfFrase(t("{0} Set. {1}")),dfFrase(t("{0} Ott. {1}")),dfFrase(t("{0} Nov. {1}")),dfFrase(t("{0} Dic. {1}"))
    ]
  };
  return DF_NOMI
}

/* Scelte della scheda (da /api/auth, /api/home o dal modulo di Localizzazione) */
function fmtSrc(){
  if(typeof HW!="undefined"&&HW&&HW.tf!==undefined&&HW.to!==undefined)return HW;
  return typeof AUTH!="undefined"&&AUTH&&AUTH.to!==undefined?AUTH:{tf:0,df:0,to:0}
}

function dfImpostDa(A){
  return{
    dataCorta:A.df||0,dataCortaTesto:A.dft||"",
    dataLunga:A.dl||0,dataLungaTesto:A.dlt||"",
    ampm:!!A.tf,oraLungaSec:A.ts!==0,oraLunga:A.tl||0,oraLungaTesto:A.tlt||""
  }
}

/* d = Date gia spostata nel fuso della scheda (si leggono i campi UTC) */
function dfDaData(d){
  return{
    anno:d.getUTCFullYear(),mese:d.getUTCMonth()+1,giorno:d.getUTCDate(),giornoSett:d.getUTCDay(),
    ore:d.getUTCHours(),minuti:d.getUTCMinutes(),secondi:d.getUTCSeconds()
  }
}

function dfDaEpoca(ep){return dfDaData(new Date((ep+(fmtSrc().to||0))*1000))}

/* CORTA: ep = secondi dal 1970 (UTC) */
function fmtData(ep){return dfDataCorta(dfImpostDa(fmtSrc()),dfDaEpoca(ep),dfNomi())}
function fmtOra(ep){return dfOraCorta(dfImpostDa(fmtSrc()),dfDaEpoca(ep))}
function fmtOraData(ep){return fmtData(ep)+" "+fmtOra(ep)}

/* LUNGA, per testata e accesso: [ora, data] di una Date spostata nel fuso della scheda, con le scelte A */
function fmtLungo(d,A){
  var im=dfImpostDa(A),p=dfDaData(d),n=dfNomi();
  return[dfOraLunga(im,p,n),dfDataLunga(im,p,n)]
}
