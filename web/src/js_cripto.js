/* ===== 1.7.1: impronta della password (SHA-256 / HMAC), uguale alla scheda ===== */
var SHK=[0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
  0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
  0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
  0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
  0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
  0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
  0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
  0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2];
function utf8(s){return new TextEncoder().encode(s)}
function sha256b(m){
  var l=m.length,n=((l+9+63)>>6)<<6,b=new Uint8Array(n),W=new Int32Array(64),
    H=[0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19],i,j;
  b.set(m);
  b[l]=0x80;
  var bl=l*8;
  b[n-4]=bl>>>24;
  b[n-3]=bl>>>16;
  b[n-2]=bl>>>8;
  b[n-1]=bl;
  b[n-5]=Math.floor(l/536870912);
  for(i=0;i<n;i+=64){
    for(j=0;j<16;j++)W[j]=b[i+4*j]<<24|b[i+4*j+1]<<16|b[i+4*j+2]<<8|b[i+4*j+3];
    for(j=16;j<64;j++){
      var x=W[j-15],y=W[j-2];
      W[j]=(((x>>>7|x<<25)^(x>>>18|x<<14)^(x>>>3))+W[j-7]+((y>>>17|y<<15)^(y>>>19|y<<13)^(y>>>10))+
        W[j-16])|0
    }
    var a=H[0],c=H[1],d=H[2],e=H[3],f=H[4],g=H[5],h=H[6],k=H[7];
    for(j=0;j<64;j++){
      var t1=(k+((f>>>6|f<<26)^(f>>>11|f<<21)^(f>>>25|f<<7))+((f&g)^(~f&h))+SHK[j]+W[j])|0,
        t2=(((a>>>2|a<<30)^(a>>>13|a<<19)^(a>>>22|a<<10))+((a&c)^(a&d)^(c&d)))|0;
      k=h;
      h=g;
      g=f;
      f=(e+t1)|0;
      e=d;
      d=c;
      c=a;
      a=(t1+t2)|0
    }
    H[0]=H[0]+a|0;
    H[1]=H[1]+c|0;
    H[2]=H[2]+d|0;
    H[3]=H[3]+e|0;
    H[4]=H[4]+f|0;
    H[5]=H[5]+g|0;
    H[6]=H[6]+h|0;
    H[7]=H[7]+k|0
  }
  var o=new Uint8Array(32);
  for(i=0;i<8;i++){
    o[4*i]=H[i]>>>24;
    o[4*i+1]=H[i]>>>16;
    o[4*i+2]=H[i]>>>8;
    o[4*i+3]=H[i]
  }
  return o
}

function hexb(b){
  var s="";
  for(var i=0;i<b.length;i++)s+=(b[i]<16?"0":"")+b[i].toString(16);
  return s
}

function sha256Hex(s){return hexb(sha256b(utf8(s)))}
function hmacHex(key,msg){
  var k=utf8(key);
  if(k.length>64)k=sha256b(k);
  var ip=new Uint8Array(64),op=new Uint8Array(64),m=utf8(msg),i;
  for(i=0;i<64;i++){
    ip[i]=(k[i]||0)^0x36;
    op[i]=(k[i]||0)^0x5c
  }
  var a=new Uint8Array(64+m.length);
  a.set(ip);
  a.set(m,64);
  var ih=sha256b(a),z=new Uint8Array(96);
  z.set(op);
  z.set(ih,64);
  return hexb(sha256b(z))
}

function hashPass(salt,p,it){
  var h=sha256Hex(salt+p);
  for(var i=0;i<it;i++)h=sha256Hex(h+salt+p);
  return h
}
