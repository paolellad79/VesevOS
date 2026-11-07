/* disegno della scheda (fronte e retro), nostro e originale: proporzioni reali (circa 18 x 24 mm), componenti e piazzole in posizione relativa (u,v da 0 a 1) */
var PY0=22,PBR=1.3;   // y del primo pin dal bordo alto della scheda, rapporto altezza/larghezza
/* piazzole del retro, viste da sotto con la USB-C in alto: due file sfalsate vicino al centro a sinistra e una colonna a destra */
var PBPOS={38:[.35,.32],37:[.35,.40],34:[.35,.475],33:[.35,.55],36:[.21,.32],35:[.21,.40],
  21:[.21,.475],18:[.21,.55],17:[.21,.62],16:[.21,.70],15:[.21,.77],14:[.21,.85],
  48:[.79,.32],47:[.79,.40],46:[.79,.475],45:[.79,.55],42:[.79,.62],41:[.79,.70],40:[.79,.77],
  39:[.79,.85]};
function pinBoardH(bw,rows){return Math.max(Math.round(bw*PBR),PY0+(rows-1)*26+20)}
function pinPadXY(g,i,side,bx,top,bw,bh){
  var p=PBPOS[g];
  return p?[bx+p[0]*bw,top+p[1]*bh]:[bx+(side<0?.3:.7)*bw,top+36+i*26]
}

function pinBoardArt(fr,bx,top,bw,bh,ink,LED){
  var s='',X=function(u){return bx+u*bw},Y=function(v){return top+v*bh},cx=bx+bw/2;
  s+='<rect x="'+bx+'" y="'+top+'" width="'+bw+'" height="'+bh+'" rx="12" fill="'+
    (fr?"#1f2a37":"#243246")+'" stroke="#3a4a5e"/>';
  s+='<rect x="'+(cx-30)+'" y="'+(top-20)+
    '" width="60" height="30" rx="7" fill="#aab4bf" stroke="#5c6670"/><rect x="'+(cx-21)+'" y="'+
    (top-9)+'" width="42" height="10" rx="5" fill="#1f2a37"/><text x="'+cx+'" y="'+(top-11)+
    '" font-size="6.5" text-anchor="middle" fill="#1f2a37">USB-C</text>';
  s+='<text x="'+cx+'" y="'+(top-26)+
    '" font-size="10" font-weight="700" text-anchor="middle" fill="'+ink+'">'+
    esc(fr?t("FRONTE"):t("RETRO (vista da sotto)"))+'</text>';
  if(fr){
    [["BOOT",.244],["RESET",.73]].forEach(function(b){
      var x=X(b[1]);
      s+='<rect x="'+(x-16)+'" y="'+(Y(.37)-7)+
        '" width="32" height="14" rx="3" fill="#46566a"/><text x="'+x+'" y="'+(Y(.37)+3)+
        '" font-size="7" text-anchor="middle" fill="#fff">'+b[0]+'</text>'
    });
    if(LED!=null){
      var iL=pinInfo(LED),cL=LED==PT.gpio?"#e0a020":(iL&&iL.owner)?"#3fa7ff":"#3a4352",lx=X(.82),
        ly0=Y(.766);
      s+='<g style="cursor:pointer" onclick="pinSel('+LED+
        ')" role="button" tabindex="0" onkeydown="pinKey(event,'+LED+')" aria-label="GPIO '+LED+
        ' LED RGB"><rect x="'+(lx-7)+'" y="'+(ly0-7)+
        '" width="14" height="14" rx="3" fill="#e8ecf0" stroke="'+(PSEL==LED?"#fff":cL)+
        '" stroke-width="'+(PSEL==LED?2.5:1.5)+'"/><circle cx="'+(lx-3)+'" cy="'+(ly0-2.5)+
        '" r="1.6" fill="#d64545"/><circle cx="'+(lx+3)+'" cy="'+(ly0-2.5)+
        '" r="1.6" fill="#2e9e5b"/><circle cx="'+lx+'" cy="'+(ly0+3)+
        '" r="1.6" fill="#3a7bd5"/><title>GPIO'+LED+' - LED RGB</title></g><text x="'+(lx-14)+
        '" y="'+(ly0+3)+'" font-size="7" text-anchor="end" fill="'+ink+'">'+LED+'</text>'
    }
    s+='<circle cx="'+X(.146)+'" cy="'+Y(.054)+'" r="3" fill="#c0392b"/><circle cx="'+X(.84)+
      '" cy="'+Y(.054)+'" r="3" fill="#3a7bd5"/><text x="'+X(.84)+'" y="'+(Y(.054)+13)+
      '" font-size="6" text-anchor="middle" fill="'+ink+'">BAT</text>';
    var h=Math.round(bw*.21),mx=X(.495),my=Y(.6);
    s+='<path d="M'+mx+' '+(my-h)+'L'+(mx+h)+' '+my+'L'+mx+' '+(my+h)+'L'+(mx-h)+' '+my+
      'Z" fill="#2b3644" stroke="#5c6670"/><text x="'+mx+'" y="'+(my+3)+
      '" font-size="8" text-anchor="middle" fill="'+ink+'">'+esc((PBI&&PBI.chip)||"ESP32-S3")+
      '</text>';
    s+='<rect x="'+X(.2)+'" y="'+Y(.9)+'" width="'+(bw*.35)+
      '" height="17" rx="4" fill="#e58fb3"/><path d="M'+(X(.2)+8)+' '+(Y(.9)+13)+
      'h6v-9h6v9h6v-9h6v9h6v-9h6v9h6v-9h6v9h6" fill="none" stroke="#a24a6f" stroke-width="1.3"/><text x="'+
      (X(.2)+bw*.175)+'" y="'+(Y(.9)-4)+'" font-size="7" text-anchor="middle" fill="'+ink+'">'+
      esc(t("ANTENNA"))+'</text>'
  }
  else{
    [["B-",.40,"#222",28],["B+",.60,"#d64545",28],["BOOST",.80,"#7a6a2a",36]].forEach(function(b){
      var x=X(b[1]);
      s+='<rect x="'+(x-b[3]/2)+'" y="'+(Y(.075)-7)+'" width="'+b[3]+'" height="14" rx="3" fill="'+
        b[2]+'" stroke="#555"/><text x="'+x+'" y="'+(Y(.075)+3)+
        '" font-size="7" text-anchor="middle" fill="#fff">'+b[0]+'</text>'
    })
  }
  return s
}
