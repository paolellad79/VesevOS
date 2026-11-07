/* Pannello Pin come schema elettrico (1.8.14). Sul fronte, a lato della scheda, un blocco per ogni periferica con
   icona, nome, segnali e alimentazione; fili colorati dai pin della scheda ai pin del blocco. I dati: /api/pins (chi usa il
   pin e con quale segnale), /api/io, /api/mods e /api/vde (titolo, icona, alimentazione). Dove un dato manca il filo
   e tratteggiato ("da verificare"): niente viene inventato. */
var PDEV={io:{io:[]},mods:{list:[]},vde:{list:[]}},PBLK=[];
var PCOL=["#3fa7ff","#2ec4b6","#f4a261","#c77dff","#ff6b9d","#9ad65b"];
var PWCOL={"3V3":"#e08a2c","5V":"#d64545","GND":"#8b98a8"};
var PBW=186,PZONE=104,PROW=17,
  PHEAD=44;   // larghezza del blocco, spazio dei fili, altezza di una riga e dell'intestazione
var PGEN={rele:'<rect x="3" y="7" width="18" height="10" rx="2"/><path d="M7 12h3l2-3 2 6 2-3h1"/>',
  led:'<circle cx="12" cy="10" r="5"/><path d="M9 17h6M10 20h4"/>',
  pin:'<circle cx="12" cy="12" r="3"/><path d="M12 3v6M12 15v6"/>',
  ingresso:'<path d="M3 12h10M9 8l4 4-4 4"/><rect x="15" y="6" width="6" height="12"/>',
  analogico:'<path d="M3 16c3-8 5-8 8 0s5 8 10-4"/>',
  i2c:'<rect x="6" y="6" width="12" height="12" rx="2"/><path d="M9 3v3M15 3v3M9 18v3M15 18v3"/>',
  sd:'<path d="M7 3h8l3 3v15H7z"/><path d="M10 3v4M13 3v4"/>',
  altro:'<path d="M5 6h14M5 12h14M5 18h9"/>'};
function pinInk(){
  return document.documentElement.getAttribute("data-theme")=="light"?"#566676":"#8b98a8"
}

function pinClsName(c){
  return c=="rele"?t("Rele"):c=="led"?t("LED"):c=="pin"?t("Uscita"):c=="ingresso"?t("Ingresso")
    :c=="analogico"?t("Ingresso analogico"):c=="i2c"?t("Sensore I2C"):c=="sd"?t("Lettore microSD"):c
}

function pinDevLoad(){
  if(ROLE<1)return;
  var get=function(u,k,v){
    return api(u).then(function(r){return r.json()}).then(function(j){
      if(j&&!j.err)PDEV[k]=j
    }).catch(function(){PDEV[k]=v})
  };
  var p=[get("/api/io","io",{io:[]}),get("/api/mods","mods",{list:[]})];
  if(AUTH&&AUTH.elf)p.push(get("/api/vde","vde",{list:[]}));
  Promise.all(p).then(function(){if(cur=="pin")drawPins()})
}

function pinFind(a,k,v){
  for(var i=0;i<a.length;i++)if(a[i][k]==v)return a[i];
  return null
}

function pinPwrKey(p){return p=="3V3"||p=="5V"?p:""}
/* titolo, icona e alimentazione del blocco: prima il driver ELF installato, poi la scheda modulo della stessa classe */
function pinBlockInfo(b){
  var v=pinFind(PDEV.vde.list||[],"n",b.name),d=pinFind(PDEV.io.io||[],"n",b.name);
  if(v&&v.i){
    b.kind="vde";
    b.title=v.t;
    b.icon=v.ic;
    b.power=v.pw;
    b.warn=v.w;
    b.tab="drv";
    b.sigs=v.s||[];
    b.pins.forEach(function(k){
      var i=(v.p||[]).indexOf(k.g);
      if(i>=0&&v.s&&v.s[i])k.sig=v.s[i]
    });
    return
  }
  if(d){
    b.kind="io";
    b.cls=d.c;
    b.tab="io";
    var m=null,ms=(PDEV.mods.list||[]).filter(function(x){return x.ok&&x.c==d.c});
    if(d.c=="sd"){
      m=pinFind(PDEV.vde.list||[],"b","sd");
      if(m&&!m.i)m=null;
      if(m){
        b.title=m.t;
        b.icon=m.ic;
        b.power=m.pw;
        b.warn=m.w;
        return
      }
    }
    // il legame periferica-scheda non e salvato: si usa solo quando e certo (driver I2C uguale, o un'unica scheda microSD)
    m=d.c=="i2c"&&d.d?pinFind(ms,"drv",d.d):d.c=="sd"&&ms.length==1?ms[0]:null;
    b.title=m?m.t:pinClsName(d.c);
    b.icon=m?m.icon:"";
    b.power=m?m.p:"";
    b.warn=m?m.w:"";
    return
  }
  b.kind="altro";
  b.title=b.note;
  b.tab=b.name=="Automazioni"?"auto":""
}

function pinBlocks(){
  var by={},ord=[];
  PUSE.forEach(function(u){
    if(u.fixed||!u.owner||u.owner=="TEST")return;
    if(!by[u.owner]){
      by[u.owner]={name:u.owner,note:u.note,pins:[]};
      ord.push(u.owner)
    }
    by[u.owner].pins.push({g:u.gpio,sig:u.sig||""})
  });
  return ord.map(function(n,i){
    var b=by[n];
    pinBlockInfo(b);
    b.col=PCOL[i%PCOL.length];
    return b
  })
}
/* icona del blocco come immagine (nessuno script): quella del driver, o una generica per classe */
function pinIconUri(b){
  var s=b.icon;
  if(!s){
    s=
      '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="#2c6fbb" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">'+
      (PGEN[b.cls]||PGEN.altro)+'</svg>'
  }
  else if(s.indexOf("xmlns")<0)s=s.replace("<svg",'<svg xmlns="http://www.w3.org/2000/svg"');
  return "data:image/svg+xml;charset=utf-8,"+encodeURIComponent(s)
}
/* Dove stanno i blocchi e i fili. L e R sono i pin del fronte (sinistra e destra), top e st la riga del primo pin e il passo. */
function pinSchemaPlan(L,R,top,st,bw){
  var side=function(l){return L.indexOf(l)>=0?-1:R.indexOf(l)>=0?1:0},
    yOf=function(l){
      var i=L.indexOf(l);
      if(i<0)i=R.indexOf(l);
      return i<0?null:top+PY0+i*st
    },P={blocks:[],pinBlk:{},pinSig:{}},hasL=false,hasR=false;
  pinBlocks().forEach(function(b){
    var rows=[],cnt=[0,0];
    b.pins.forEach(function(k){
      var l=String(k.g),sd=side(l);
      if(!sd)return;
      rows.push({sig:k.sig,g:k.g,ps:sd,py:yOf(l),color:b.col});
      cnt[sd<0?0:1]++
    });
    if(!rows.length)return;
    var pk=pinPwrKey(b.power);
    if(pk){
      [[pk,"VCC",pk],["GND","GND","GND"]].forEach(function(w){
        var sd=side(w[0]);
        if(sd)rows.push({sig:w[1],ps:sd,py:yOf(w[0]),color:PWCOL[w[2]],pwr:w[2]})
      })
    }
    rows.sort(function(a,c){return a.py-c.py});
    var un=(b.sigs||[]).slice(b.pins.length);   // segnali del driver senza pin
    b.note=pk?"":(b.power&&b.power!="-"?tf("Alimentazione {0}: esterna",b.power)
      :t("Alimentazione: da verificare"));
    b.rows=rows;
    b.un=un;
    b.h=PHEAD+(rows.length+un.length+(b.note?1:0))*PROW+8;
    b.side=cnt[0]>cnt[1]?-1:1;
    b.my=rows.reduce(function(a,r){return a+r.py},0)/rows.length;
    if(b.side<0)hasL=true;
    else hasR=true;
    b.pins.forEach(function(k){
      P.pinBlk[k.g]=b;
      P.pinSig[k.g]=k.sig
    });
    P.blocks.push(b)
  });
  if(!P.blocks.length)return null;
  var bx=hasL?4+PBW+PZONE+11:66,rx=bx+bw+11+PZONE;
  [-1,1].forEach(function(sd){
    var y=8;
    P.blocks.filter(function(b){return b.side==sd}).sort(function(a,
      c){return a.my-c.my}).forEach(function(b){
        b.x=sd<0?4:rx;
        b.y=Math.max(y,b.my-b.h/2);
        y=b.y+b.h+10;
        P.bottom=Math.max(P.bottom||0,b.y+b.h)
      })
  });
  P.bx=bx;
  P.xMin=0;
  P.w=Math.max(hasR?rx+PBW+4:bx+bw+130,440);
  pinWires(P,bx,bw);
  return P
}
/* fili: un tratto che esce dal pin, sale o scende lungo una corsia propria fino alla riga del blocco. Le corsie si
   assegnano in modo che i fili non si incrocino; se il blocco sta dall'altra parte della scheda il filo passa sopra. */
function pinWires(P,bx,bw){
  var W=[],yc=0;
  P.blocks.forEach(function(b){
    var nb=0;
    b.rows.forEach(function(r,i){
      r.ry=b.y+PHEAD+i*PROW+PROW/2;
      r.b=b;
      r.px=r.ps<0?bx:bx+bw;
      r.cross=r.ps!=b.side;
      r.ex=b.side<0?b.x+PBW:b.x;
      if(r.cross){
        r.yc=8+(yc++)*6;
        r.dx=r.ex-b.side*(10+(nb++)*6)
      }
      W.push(r)
    })
  });
  [-1,1].forEach(function(sd){
    var S=W.filter(function(r){return r.ps==sd&&(r.cross||Math.abs(r.ry-r.py)>=2)}),
      dn=S.filter(function(r){return !r.cross&&r.ry>r.py}).sort(function(a,c){return c.py-a.py}),
      up=S.filter(function(r){return r.cross||r.ry<r.py}).sort(function(a,c){return a.py-c.py}),n=0;
    dn.concat(up).forEach(function(r){r.lx=r.px+sd*(21+(n++)*7)})
  });
  W.forEach(function(r){
    var x0=r.px+r.ps*11,d;
    if(r.cross)d="M"+x0+" "+r.py+"H"+r.lx+"V"+r.yc+"H"+r.dx+"V"+r.ry+"H"+r.ex;
    else if(r.lx==null)d="M"+x0+" "+r.py+"L"+r.ex+" "+r.ry;
    else d="M"+x0+" "+r.py+"H"+r.lx+"V"+r.ry+"H"+r.ex;
    r.d=d
  });
  P.wires=W
}

function pinSchemaSvg(P){
  var s="",ink=pinInk();
  PBLK=P.blocks;
  P.wires.forEach(function(r){
    s+='<path d="'+r.d+'" fill="none" stroke="'+r.color+'" stroke-width="1.8"/>'
  });
  P.blocks.forEach(function(b,bi){
    var rs=[],x=b.x,y=b.y;
    b.rows.forEach(function(r){rs.push(r.sig+(r.pwr?"":" GPIO"+r.g))});
    var lab=tf("Periferica {0}: {1}. Pin: {2}. Premi per aprire la pagina.",b.name,b.title||"",
      rs.join(", "));
    s+='<g class="pblk" tabindex="0" role="link" aria-label="'+esc(lab)+
      '" style="cursor:pointer" onclick="pinGoBlk('+bi+
      ')" onkeydown="if(event.key==\'Enter\'||event.key==\' \'){event.preventDefault();pinGoBlk('+
      bi+')}">';
    s+='<title>'+esc(lab)+'</title><rect x="'+x+'" y="'+y+'" width="'+PBW+'" height="'+b.h+
      '" rx="9" fill="#1f2a37" stroke="'+b.col+'" stroke-width="1.6"/>';
    s+='<rect x="'+(x+8)+'" y="'+(y+7)+
      '" width="30" height="30" rx="7" fill="#e9eef3"/><image href="'+pinIconUri(b)+'" x="'+(x+10)+
      '" y="'+(y+9)+'" width="26" height="26"/>';
    s+='<text x="'+(x+46)+'" y="'+(y+21)+'" font-size="11" font-weight="700" fill="#e6edf3">'+
      esc(b.name)+'</text>';
    s+='<text x="'+(x+46)+'" y="'+(y+34)+'" font-size="8.5" fill="#aab6c4">'+
      esc((b.title||"").length>27?b.title.slice(0,26)+"…":b.title||"")+'</text>';
    b.rows.forEach(function(r){
      var left=b.side<0,ex=left?x+PBW:x,tx=left?x+PBW-12:x+12;
      s+='<circle cx="'+ex+'" cy="'+r.ry+'" r="3.5" fill="'+r.color+'"/>';
      s+='<text x="'+tx+'" y="'+(r.ry+3)+'" font-size="9" font-weight="700" text-anchor="'+
        (left?"end":"start")+'" fill="#e6edf3">'+esc(r.sig)+'</text>';
      s+='<text x="'+(left?x+12:x+PBW-12)+'" y="'+(r.ry+3)+'" font-size="8" text-anchor="'+
        (left?"start":"end")+'" fill="#aab6c4">'+(r.pwr?esc(r.pwr=="GND"?"GND":b.power):"GPIO"+r.g)+
        '</text>'
    });
    var k=b.rows.length;
    b.un.forEach(function(sg){
      var ry=y+PHEAD+k*PROW+PROW/2,left=b.side<0,ex=left?x+PBW:x;
      k++;
      s+='<path d="M'+ex+' '+ry+'h'+(left?-14:14)+'" stroke="'+b.col+
        '" stroke-dasharray="3 2" fill="none"/><text x="'+(left?x+PBW-20:x+20)+'" y="'+(ry+3)+
        '" font-size="9" text-anchor="'+(left?"end":"start")+'" fill="#e6edf3">'+esc(sg)+
        '</text><text x="'+(left?x+12:x+PBW-12)+'" y="'+(ry+3)+'" font-size="8" text-anchor="'+
        (left?"start":"end")+'" fill="#aab6c4">'+esc(t("non assegnato"))+'</text>'
    });
    if(b.note){
      var ry=y+PHEAD+k*PROW+PROW/2;
      s+='<path d="M'+x+' '+ry+'h12" stroke="'+PWCOL["3V3"]+
        '" stroke-dasharray="3 2" fill="none"/><text x="'+(x+18)+'" y="'+(ry+3)+
        '" font-size="8.5" fill="#f0c27a">'+esc(b.note)+'</text>'
    }
    s+='</g>'
  });
  return s
}

function pinGoBlk(i){
  var b=PBLK[i];
  if(b&&b.tab&&canSee(b.tab))show(b.tab)
}

function pinKey(e,g){
  if(e.key=="Enter"||e.key==" "){
    e.preventDefault();
    pinSel(g)
  }
}
/* nel pannello del pin: chi lo usa e con quale segnale */
function pinWho(g){
  var u=pinOwner(g);
  if(!u||u.fixed)return"";
  var h='<div class="fhint">'+esc(tf("Usato da: {0}{1}",u.owner,u.sig?" ("+u.sig+")":""))+'</div>';
  for(var i=0;i<PBLK.length;i++)if(PBLK[i].name==u.owner&&PBLK[i].tab&&
    canSee(PBLK[i].tab))h+='<button class="btn gray" onclick="pinGoBlk('+i+')">'+
    esc(t("Apri la periferica"))+'</button>';
  return h
}
