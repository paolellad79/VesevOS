/* ===== Aiuti comuni: testi, righe, misure, percorsi ===== */
function esc(s){
  return String(s).replace(/[&<>"]/g,function(c){
    return{"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;"}[c]
  })
}

function row(a,b){return '<div class="row"><span>'+a+'</span><span>'+b+'</span></div>'}
function macRow(lab,m){
  if(!m)return"";
  return row(esc(lab),'<code class="mac">'+esc(m)+'</code>')
}

function bar(p){return '<div class="bar"><i style="width:'+p+'%"></i></div>'}
function tmp(c,u){return u?(c*9/5+32).toFixed(1)+" &deg;F":c.toFixed(1)+" &deg;C"}
function kb(n){return Math.round(n/1024)+" KB"}
function enc(s){return encodeURIComponent(s)}
function join(a,b){return a=="/"?"/"+b:a+"/"+b}
function draw(id,a,mn,mx){
  var c=$(id),x=c.getContext("2d");
  x.clearRect(0,0,c.width,c.height);
  if(!a||a.length<2)return;
  if(mn===null){
    mn=Math.min.apply(null,a)-2;
    mx=Math.max.apply(null,a)+2
  }
  x.strokeStyle="#3fa7ff";
  x.lineWidth=2;
  x.beginPath();
  for(var i=0;i<a.length;i++){
    var X=i*c.width/59,Y=c.height-(a[i]-mn)/(mx-mn)*c.height;
    if(i)x.lineTo(X,Y);
    else x.moveTo(X,Y)
  }
  x.stroke();
  x.fillStyle="#8b98a8";
  x.font="12px sans-serif";
  x.fillText(mx.toFixed(0),4,12);
  x.fillText(mn.toFixed(0),4,c.height-4)
}
