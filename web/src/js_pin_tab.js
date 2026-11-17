/* Tabella dei piedini dichiarata nel file di scheda (/api/pins/info, campo "tab"): una riga per piedino o piazzola,
   [lato, posizione, gpio, tipo, bit]. Da qui nascono il disegno della scheda, i fili, i suggerimenti e le funzioni:
   nella pagina non ci sono elenchi di pin ne rami per chip. Lati: 0 sinistra, 1 destra, 2 basso (bordo corto),
   3 retro esterno sinistro, 4 retro destro, 5 retro alto, 6 retro interno sinistro. */
var PTAB=[],PTIPO=["","5V","3V3","GND","B+","B-","BOOST"];
var PBIT={adc:1,tocco:2,uart:4,spi:8,jtag:16,usb:32,rtc:64,avvio:128,led:256,tasto:512};
var PFNOMI=[[1,"ADC"],[2,"Touch"],[4,"UART0"],[8,"SPI"],[16,"JTAG"],[32,"USB"],[64,"RTC"]];
var PRETRO_DY=20;   // passo delle piazzole del retro

function pinTabLoad(rows){
  PTAB=(rows||[]).filter(function(r){return r&&r.length>=5}).map(function(r){
    return {lato:r[0],pos:r[1],g:r[2],tipo:r[3],bit:r[4],lab:r[2]>=0?String(r[2]):PTIPO[r[3]]||"?"}
  })
}

function pinTabLato(l){
  return PTAB.filter(function(r){return r.lato==l}).sort(function(a,c){return a.pos-c.pos})
}

function pinTabGpio(g){
  for(var i=0;i<PTAB.length;i++)if(PTAB[i].g==g)return PTAB[i];
  return null
}

/* funzioni fisse del pin, dal file di scheda (es. "ADC, Touch, RTC") */
function pinFnTxt(g){
  var r=pinTabGpio(g),w=[];
  if(PBI&&g==PBI.boot)w.push("BOOT");
  if(PBI&&g==PBI.led)w.push("LED");
  if(r)PFNOMI.forEach(function(f){
    if(r.bit&f[0])w.push(f[1])
  });
  return w.join(", ")
}

function pinTabMax(l){
  var n=0;
  pinTabLato(l).forEach(function(r){n=Math.max(n,r.pos+1)});
  return n
}

/* altezza della scheda: serve a far stare file di pin, bordo in basso e piazzole del retro */
function pinBoardH(bw,st){
  var righe=Math.max(pinTabMax(0),pinTabMax(1)),
    h=Math.max(Math.round(bw*PBR),PY0+(righe-1)*st+20+(pinTabMax(2)?28:0)),
    rr=Math.max(pinTabMax(3),pinTabMax(4),pinTabMax(6));
  if(rr)h=Math.max(h,PY0+22+(rr-1)*PRETRO_DY+24);
  return h
}

/* Posto di ogni piedino nella vista scelta: {r,x,y,sd,pad}. sd = lato da cui esce il filo (-1 sinistra, 1 destra),
   pad = piazzola rettangolare (retro). Nel retro i piedini del bordo si vedono specchiati. */
function pinGeom(fr,bx,top,bw,bh,st){
  var o=[],mir=fr?1:-1,nb=pinTabMax(2),nt=pinTabMax(5),inn=pinTabMax(6)>0;
  PTAB.forEach(function(r){
    var x,y,sd=0,pad=0;
    if(r.lato<=1){
      sd=(r.lato?1:-1)*mir;
      x=sd<0?bx:bx+bw;
      y=top+PY0+r.pos*st
    }
    else if(r.lato==2){
      x=bx+bw*(fr?(r.pos+1)/(nb+1):1-(r.pos+1)/(nb+1));
      y=top+bh-16;
      sd=x<bx+bw/2?-1:1
    }
    else{
      pad=1;
      if(r.lato==5){
        x=bx+bw*(r.pos+1)/(nt+1);
        y=top+PY0
      }
      else{
        x=bx+bw*(r.lato==4?.79:r.lato==6?.36:inn?.21:.28);
        y=top+PY0+22+r.pos*PRETRO_DY
      }
    }
    o.push({r:r,x:x,y:y,sd:sd,pad:pad,fronte:r.lato<=2})
  });
  return o
}

/* punti di partenza dei fili, solo sul fronte: etichetta -> {dx dal bordo sinistro, y, lato} (il primo pin con quel nome) */
function pinAnchors(items,bx){
  var a={};
  items.forEach(function(it){
    if(it.fronte&&!a[it.r.lab])a[it.r.lab]={dx:it.x-bx,y:it.y,sd:it.sd}
  });
  return a
}
