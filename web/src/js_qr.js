/* ===== QR (scritto da noi: modo byte, versioni 1-10) ===== */
var QRE={L:[7,10,15,20,26,18,20,24,30,18],M:[10,16,26,18,24,16,18,22,22,26],
  Q:[13,22,18,26,18,24,18,22,20,24],H:[17,28,22,16,22,28,26,26,24,28]},
  QRB={L:[1,1,1,1,1,2,2,2,2,4],M:[1,1,1,2,2,4,4,4,5,5],Q:[1,1,2,2,4,4,6,6,8,8],
    H:[1,1,2,4,4,4,5,6,8,8]},QRF={L:1,M:0,Q:3,H:2};
function qrRaw(v){
  var r=(16*v+128)*v+64;
  if(v>=2){
    var n=Math.floor(v/7)+2;
    r-=(25*n-10)*n-55;
    if(v>=7)r-=36
  }
  return r
}

function qrDc(v,e){return Math.floor(qrRaw(v)/8)-QRE[e][v-1]*QRB[e][v-1]}
function gfMul(x,y){
  var z=0;
  for(var i=7;i>=0;i--){
    z=(z<<1)^((z>>7)*0x11D);
    z^=((y>>i)&1)*x
  }
  return z&255
}

function rsDiv(d){
  var r=[],i,j,root=1;
  for(i=0;i<d-1;i++)r.push(0);
  r.push(1);
  for(i=0;i<d;i++){
    for(j=0;j<d;j++){
      r[j]=gfMul(r[j],root);
      if(j+1<d)r[j]^=r[j+1]
    }
    root=gfMul(root,2)
  }
  return r
}

function rsRem(data,div){
  var r=div.map(function(){return 0});
  data.forEach(function(b){
    var f=b^r.shift();
    r.push(0);
    div.forEach(function(c,i){r[i]^=gfMul(c,f)})
  });
  return r
}

function qrAlign(v){
  if(v==1)return[];
  var n=Math.floor(v/7)+2,st=Math.floor((v*4+n*2+1)/(n*2-2))*2,sz=v*4+17,r=[6];
  for(var i=0;i<n-1;i++)r.push(sz-7-i*st);
  return r.sort(function(a,b){return a-b})
}

function qrMake(text,ecl){
  ecl=ecl||"M";
  var data=utf8(text),v;
  for(v=1;v<=10;v++)if(4+(v<10?8:16)+data.length*8<=qrDc(v,ecl)*8)break;
  if(v>10)return null;
  var cap=qrDc(v,ecl)*8,bits=[];
  function put(val,n){for(var i=n-1;i>=0;i--)bits.push((val>>>i)&1)}
  put(4,4);
  put(data.length,v<10?8:16);
  data.forEach(function(b){put(b,8)});
  put(0,Math.min(4,cap-bits.length));
  put(0,(8-bits.length%8)%8);
  for(var pad=0xEC;bits.length<cap;pad^=0xEC^0x11)put(pad,8);
  var cw=[];
  for(var i=0;i<bits.length;i+=8){
    var x=0;
    for(var j=0;j<8;j++)x=x<<1|bits[i+j];
    cw.push(x)
  }
  var nb=QRB[ecl][v-1],eb=QRE[ecl][v-1],raw=Math.floor(qrRaw(v)/8),sh=nb-raw%nb,
    sl=Math.floor(raw/nb),dv=rsDiv(eb),bl=[],k=0;
  for(i=0;i<nb;i++){
    var dl=sl-eb+(i<sh?0:1),d=cw.slice(k,k+dl);
    k+=dl;
    var e=rsRem(d,dv);
    if(i<sh)d.push(null);
    bl.push(d.concat(e))
  }
  var fin=[];
  for(i=0;i<bl[0].length;i++)bl.forEach(function(b){if(b[i]!==null)fin.push(b[i])});
  var S=v*4+17,m=[],f=[];
  for(i=0;i<S;i++){
    m.push(new Array(S).fill(false));
    f.push(new Array(S).fill(false))
  }
  function sf(x,y,d){
    m[y][x]=d;
    f[y][x]=true
  }
  for(i=0;i<S;i++){
    sf(6,i,i%2==0);
    sf(i,6,i%2==0)
  }
  function fd(x,y){
    for(var dy=-4;dy<=4;dy++)for(var dx=-4;dx<=4;dx++){
      var xx=x+dx,yy=y+dy;
      if(xx>=0&&xx<S&&yy>=0&&yy<S){
        var q=Math.max(Math.abs(dx),Math.abs(dy));
        sf(xx,yy,q!=2&&q!=4)
      }
    }
  }
  fd(3,3);
  fd(S-4,3);
  fd(3,S-4);
  var ap=qrAlign(v);
  ap.forEach(function(a){
    ap.forEach(function(b){
      if((a==6&&b==6)||(a==6&&b==S-7)||(a==S-7&&b==6))return;
      for(var dy=-2;dy<=2;dy++)for(var dx=-2;dx<=2;dx++)sf(a+dx,b+dy,
        Math.max(Math.abs(dx),Math.abs(dy))!=1)
    })
  });
  function fmt(mk){
    var d=QRF[ecl]<<3|mk,r=d;
    for(var i=0;i<10;i++)r=(r<<1)^((r>>9)*0x537);
    var b=(d<<10|r)^0x5412,g=function(i){return((b>>i)&1)!=0};
    for(i=0;i<6;i++)sf(8,i,g(i));
    sf(8,7,g(6));
    sf(8,8,g(7));
    sf(7,8,g(8));
    for(i=9;i<15;i++)sf(14-i,8,g(i));
    for(i=0;i<8;i++)sf(S-1-i,8,g(i));
    for(i=8;i<15;i++)sf(8,S-15+i,g(i));
    sf(8,S-8,true)
  }
  fmt(0);
  if(v>=7){
    var r=v;
    for(i=0;i<12;i++)r=(r<<1)^((r>>11)*0x1F25);
    var b=v<<12|r;
    for(i=0;i<18;i++){
      var bt=((b>>i)&1)!=0,a=S-11+i%3,c=Math.floor(i/3);
      sf(a,c,bt);
      sf(c,a,bt)
    }
  }
  var all=[];
  fin.forEach(function(c){for(var j=0;j<8;j++)all.push((c>>(7-j))&1)});
  var n=0;
  for(var rt=S-1;rt>=1;rt-=2){
    if(rt==6)rt=5;
    for(var vt=0;vt<S;vt++)for(j=0;j<2;j++){
      var x=rt-j,upw=((rt+1)&2)==0,y=upw?S-1-vt:vt;
      if(!f[y][x]&&n<all.length){
        m[y][x]=all[n]==1;
        n++
      }
    }
  }
  var MK=[function(x,y){return(x+y)%2==0},function(x,y){return y%2==0},function(x){return x%3==0},
    function(x,y){return(x+y)%3==0},function(x,y){
      return(Math.floor(x/3)+Math.floor(y/2))%2==0
    },function(x,y){return x*y%2+x*y%3==0},function(x,y){return(x*y%2+x*y%3)%2==0},function(x,y){
      return((x+y)%2+x*y%3)%2==0
    }];
  function apl(q){
    for(var y=0;y<S;y++)for(var x=0;x<S;x++)if(!f[y][x]&&MK[q](x,y))m[y][x]=!m[y][x]
  }
  function pen(){
    var p=0,cols=[];
    for(var x=0;x<S;x++){
      var c=[];
      for(var y=0;y<S;y++)c.push(m[y][x]);
      cols.push(c)
    }
      [m,cols].forEach(function(rows){
        rows.forEach(function(row){
          var run=1;
          for(var a=1;a<S;a++){
            if(row[a]==row[a-1])run++;
            else{
              if(run>=5)p+=run-2;
              run=1
            }
          }
          if(run>=5)p+=run-2;
          var s=row.map(function(c){return c?"1":"0"}).join("");
          p+=40*(s.split("10111010000").length-1+s.split("00001011101").length-1)
        })
      });
    for(var y=0;y<S-1;y++)for(x=
      0;x<S-1;x++)if(m[y][x]==m[y][x+1]&&m[y][x]==m[y+1][x]&&m[y][x]==m[y+1][x+1])p+=3;
    var dk=0;
    m.forEach(function(r){r.forEach(function(c){if(c)dk++})});
    var tot=S*S;
    return p+10*(Math.floor((Math.abs(dk*20-tot*10)+tot-1)/tot)-1)
  }
  var best=-1,bp=0;
  for(var q=0;q<8;q++){
    apl(q);
    fmt(q);
    var pp=pen();
    if(best<0||pp<bp){
      bp=pp;
      best=q
    }
    apl(q)
  }
  apl(best);
  fmt(best);
  return m
}

function qrSvg(text){
  var m=qrMake(text,"M");
  if(!m)return"";
  var n=m.length,d="";
  for(var y=0;y<n;y++)for(var x=0;x<n;x++)if(m[y][x])d+="M"+(x+2)+" "+(y+2)+"h1v1h-1z";
  return'<svg viewBox="0 0 '+(n+4)+' '+(n+4)+
    '" shape-rendering="crispEdges" role="img" aria-label="QR"><rect width="100%" height="100%" fill="#fff"/><path d="'+
    d+'" fill="#000"/></svg>'
}

function wifiQr(ss,pw){
  var e=function(s){return String(s).replace(/([\\;,:"])/g,"\\$1")};
  return"WIFI:T:WPA;S:"+e(ss)+";P:"+e(pw)+";;"
}
