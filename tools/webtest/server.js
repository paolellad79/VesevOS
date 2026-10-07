// Finto server della scheda (1.7.2): serve index.html e le API, con le lingue vere da lang/*.json
// Avvio: VOS_REPO=<cartella del repo> [FIRST=1] node server.js   (porta 8099)
// FIRST=1 = scheda nuova: nessuna password, prima configurazione da fare.
// Utente di prova: admin / prova123
const http=require('http'),fs=require('fs'),crypto=require('crypto');
const R=''+process.env.VOS_REPO+'';
let logged=false,devLang='it',authSet=!process.env.FIRST,setupDone=!process.env.FIRST,role=2;
const SALT='0123456789abcdef0123456789abcdef',ITER=3000;
const sha=s=>crypto.createHash('sha256').update(s,'utf8').digest('hex');
let HASH=(()=>{let h=sha(SALT+'prova123');for(let i=0;i<ITER;i++)h=sha(h+SALT+'prova123');return h})();
const NONCES={};
let ST={on:false,sec:600,wifiUp:2,wifiDown:1,wifiSec:500,rssiAvg:-61,rssiMin:-80,apMax:2,bleStart:1,bleConn:1,boot:[1,0,0,0],bootLast:30,bootEver:true,led:[600,0,0,0]};
let PWR={mode:0,awake:15,sleep:10,in:0,wakes:2,slept:1200,guide:true};
let POW=12,MFA={on:false,pend:false,rec:0,nt:0},MTOK='';
const powOk=(n,a)=>{if(!POW)return true;const h=sha(n+':'+a);let b=0;for(const c of h){const v=parseInt(c,16);if(v==0){b+=4;continue}b+=v<2?3:v<4?2:v<8?1:0;break}return b>=POW};
// lingue: it ed en nel firmware, le altre dai file
const src=JSON.parse(fs.readFileSync(R+'/tools/lang_src.json','utf8'));
const FL=src.flags||{},LOCS=src.locales||{};
const langs=[{code:'it',name:'Italiano',loc:'it-IT',flag:FL.it||''},{code:'en',name:'English',loc:'en-GB',flag:FL.en||''}];
for(const f of fs.readdirSync(R+'/lang')){const c=f.replace('.json','');if(c=='en')continue;const L=JSON.parse(fs.readFileSync(R+'/lang/'+f,'utf8'));langs.push({code:c,name:L._name,loc:L._locale||LOCS[c]||'',flag:L._flag||''})}
// dati comuni: dal file generato del firmware
const COMMON=(()=>{const h=fs.readFileSync(R+'/VesevOS/vos_common_data.h','utf8');const m=h.match(/COMMON_GZ\[\][^{]*\{([\s\S]*?)\};/);return Buffer.from(m[1].match(/0x[0-9a-f]{2}/g).map(x=>parseInt(x,16)))})();
const NET={mode:"AP",st:1,host:"vesevos",mac:"AA:BB:CC:DD:A4:F2",apMac:"AA:BB:CC:DD:A4:F3",captive:true,dhcpRun:true,mdnsRun:true,air:false,airExit:0,airAt:0,ch:1,clients:0,rssi:0,apSsid:"VesevOS",ssid:"VesevOS",fqdn:"vesevos",ip:"192.168.4.1",gw:"192.168.4.1",mask:"255.255.255.0",dns:"",dns2:""};
let NTPON=true,TASKLED=2;
let STST=0,LOGLV=2,BOOTS=true;
var SRD={baud:115200,usb:false,eol:'crlf',echo:true,input:true,log:true,banner:true,tx:10,trial:false};
const status={name:"VesevOS",version:"1.7.9",uptime:"1h 2min 3s",lifeSec:93784,chip:"ESP32-S3",rev:2,cores:2,flashChip:4194304,idf:"v5.5",serialAuth:true,reset:"Accensione",boots:5,net:NET,mqtt:0,cpu:12,cpu0:15,cpu1:9,cpuMhz:160,cpuMode:0,temp:41.2,tempUnit:0,hot:false,heapTotal:300000,heapFree:150000,psramTotal:2000000,psramFree:1900000,flashTotal:190000,flashUsed:20000,cpuHist:[1,2,3,4,5,6,7,8,9,10],tempHist:[40,41,42]};
let REGION={country:process.env.FIRST?'':'IT',ch:13,limit:20,max:20,tx:20,txSet:0,antExt:0,gain:0,weekStart:0,decSep:0,dateFmt:0,timeFmt:0,tempUnit:0,tz:'CET-1CEST,M3.5.0,M10.5.0/3',tzName:'Europe/Rome',ntp:'it.pool.ntp.org',lang:'it'};
let USERS=[{i:0,name:'admin',role:2,on:true,pass:true},{i:1,name:'ospite',role:0,on:true,pass:true}];
const AP={ssid:'VesevOS',pass:'kd7mq4xnb3ph',qr:true};
const SVC={apOn:true,captive:true,httpOn:true,httpPort:80,httpsOn:true,httpsPort:443,runHttp:80,runHttps:443,pending:false,sta:false,clientOk:false,dhcpOn:true,lease:120,mdnsOn:true};
const PNOTE={};
const WT={state:0};
const AUDIT={alarms:process.env.FIRST?[{n:1,lv:1,ack:false,key:'nocountry',text:'Paese non scelto: la radio usa le regole piu prudenti'}]:[{n:1,lv:1,ack:false,key:'serial',text:'La seriale non e protetta da password'}],history:['03/10 19:40 region.country = IT (web admin 192.168.4.2)']};
const FW={mode:0,ntp:false,try:0,you:'192.168.4.2',youOk:true,rules:[],rejected:[{ip:'10.0.0.9',n:3}]};
const WD={task:true,net:false,ram:true,netMin:10,ramKb:30,upDays:0,at:-1,days:127,stopped:false,last:'',watch:[{n:'net',to:90,ago:1,r:0},{n:'monitor',to:20,ago:0,r:0}]};
const TLS={on:true,custom:false,pending:false,fp:'AB:CD:EF:01:23:45:67:89:AB:CD:EF:01:23:45:67:89:AB:CD:EF:01:23:45:67:89:AB:CD:EF:01:23:45:67:89',name:'vesevos.local',from:'2026-10-04',to:'2036-10-01'};
const MESH={run:false,auto:false,role:0,hasKey:false,ch:1,chMax:13,me:'AA:BB:CC:DD:A4:F3',sent:0,recv:0,bad:0,nodes:[{mac:'AA:BB:CC:00:00:01',name:'cucina',role:2,hops:1,rssi:-60,ago:3,ccx:false,state:'24.1C'}],msgs:[{from:'cucina',text:'ciao',when:'19:40'}]};
const MQS={run:false,conn:false};
const HOMEJ={mesh:0,role:0,nodes:2,ntp:1,last:0,ble:0,bleLeft:0,pw:0,stat:0,tv:true,te:1791049200,to:7200,tn:'Roma',df:0,tf:0};
const BLE={have:true,on:false,pin:'',conn:false,left:0,last:''};
const j=(res,o,code=200)=>{res.writeHead(code,{'Content-Type':'application/json'});res.end(typeof o=='string'?o:JSON.stringify(o))};
const body=(q,cb)=>{let b='';q.on('data',d=>b+=d);q.on('end',()=>cb(new URLSearchParams(b),b))};
global.POSTS=[];
// ---- finta memoria file (come LittleFS) ----
const FSM={'/':{d:1},'/lang':{d:1},'/a.txt':{t:'ciao mondo\n'},'/rules.txt':{t:'1|Sveglia|time 07:00 1111100|-|note x\n'},'/logo.bin':{t:'\u0000\u0001',bin:1}};
const FT=190000,MAXT=32768,used=()=>Object.values(FSM).reduce((a,f)=>a+(f.t?Buffer.byteLength(f.t):0),0)+20000;
const clean=p=>{p=(p||'').trim();if(!p)p='/';if(p[0]!='/')p='/'+p;if(p.length>100||p.includes('..')||p.includes('//')||p.includes('\\')||/[^\x20-\x7e]/.test(p))return'';if(p.length>1&&p.endsWith('/'))p=p.slice(0,-1);return p};
const par=p=>{const i=p.lastIndexOf('/');return i<=0?'/':p.slice(0,i)};
const kids=p=>Object.keys(FSM).filter(k=>k!='/'&&par(k)==p);
function fsApi(u,q,res){const P=u.pathname;
 if(P=='/api/fs/list'){const p=clean(u.searchParams.get('path'));if(!p||!FSM[p]||!FSM[p].d)return j(res,{ok:false,err:'Cartella non trovata'});
  return j(res,{ok:true,path:p,list:kids(p).map(k=>({n:k.slice(k.lastIndexOf('/')+1),d:!!FSM[k].d,s:FSM[k].t?Buffer.byteLength(FSM[k].t):0})),used:used(),total:FT,max:MAXT})}
 if(P=='/api/fs/dirs')return j(res,Object.keys(FSM).filter(k=>FSM[k].d).sort());
 if(P=='/api/fs/text'){const p=clean(u.searchParams.get('path'));const f=FSM[p];res.writeHead(f&&!f.d&&!f.bin?200:404,{'Content-Type':'text/plain; charset=utf-8'});return res.end(!f||f.d?'File non trovato':f.bin?'File binario':f.t)}
 body(q,()=>j(res,{ok:true}))}
const login=(res,o)=>{logged=true;res.writeHead(200,{'Content-Type':'application/json','Set-Cookie':'vos=1; Path=/; HttpOnly; SameSite=Strict'});res.end(JSON.stringify(o))};
http.createServer((q,res)=>{
  const u=new URL(q.url,'http://x'),P=u.pathname,G=q.method=='GET';
  if(P=='/'){res.writeHead(200,{'Content-Type':'text/html; charset=utf-8'});return res.end(fs.readFileSync(R+'/web/index.html'))}
  if(P=='/api/common'){res.writeHead(200,{'Content-Type':'application/json','Content-Encoding':'gzip'});return res.end(COMMON)}
  if(P=='/api/auth'){const o={set:authSet,setup:setupDone,lang:devLang,secure:false,https:false,fp:'',ble:true,hp:80,sp:443,tv:true,te:Math.floor(Date.now()/1000)-(process.env.SKEW?3600:0),to:7200,tn:'Europe/Rome',df:0,tf:0};if(!authSet)o.first='admin';if(logged){o.user='admin';o.role=role}return j(res,o)}
  if(P=='/api/langs')return j(res,langs);
  if(P=='/api/langfile'){const c=u.searchParams.get('code');const p=R+'/lang/'+c+'.json';if(!fs.existsSync(p))return j(res,{},c=='en'?200:404);res.writeHead(200,{'Content-Type':'application/json; charset=utf-8'});return res.end(fs.readFileSync(p))}
  if(P=='/api/license'){const id=u.searchParams.get('id');if(!id)return j(res,[{id:'notice',title:'Note legali',size:100}]);res.writeHead(200,{'Content-Type':'text/plain; charset=utf-8'});return res.end('...')}
  if(P=='/api/login/start'){return body(q,p=>{const n=crypto.randomBytes(16).toString('hex');NONCES[n]=p.get('u');j(res,{salt:SALT,nonce:n,iter:ITER,pow:POW})})}
  if(P=='/api/login'){return body(q,p=>{const n=p.get('nonce');if(p.get('hp'))return j(res,{ok:false,err:'Nome o password errati'});if(!powOk(n,p.get('pw')))return j(res,{ok:false,err:'Prova di lavoro non valida'});const ok=NONCES[n]==p.get('u')&&p.get('u')=='admin'&&crypto.createHmac('sha256',HASH).update(n).digest('hex')==p.get('mac');delete NONCES[n];
    if(!ok)return j(res,{ok:false,err:'Nome o password errati'});if(MFA.on){MTOK=crypto.randomBytes(8).toString('hex');return j(res,{ok:false,mfa:true,tok:MTOK,notime:false,recOnly:false})}login(res,{ok:true,user:'admin',role})})}
  if(P=='/api/login/mfa'){return body(q,p=>{if(p.get('tok')!=MTOK||!/^(123456|ABCD-EFGH)$/.test(p.get('code')))return j(res,{ok:false,err:'Codice errato'});login(res,{ok:true,user:'admin',role})})}
  if(P=='/api/stats'){if(q.method=='POST')return body(q,p=>{if(p.get('on')!==null)ST.on=p.get('on')=='1';j(res,ST)});return j(res,ST)}
  if(P=='/api/power'){if(q.method=='POST')return body(q,p=>{const m=+p.get('mode'),a=+p.get('awake'),sl=+p.get('sleep');if(m==2&&(a<10||a>1440))return j(res,{ok:false,err:'Tempo da sveglia: da 10 minuti a 24 ore'});PWR.mode=m;if(m==2){PWR.awake=a;PWR.sleep=sl;PWR.in=600}j(res,PWR)});return j(res,PWR)}
  if(P=='/api/mfa/begin'&&q.method=='POST'){MFA.pend=true;return j(res,{uri:'otpauth://totp/VesevOS:admin@vesevos?secret=JBSWY3DPEHPK3PXP&issuer=VesevOS',key:'JBSW Y3DP EHPK 3PXP'})}
  if(P=='/api/mfa/confirm'){return body(q,p=>{if(p.get('code')!='123456')return j(res,{ok:false,err:'Codice errato'});MFA.on=true;MFA.rec=8;return j(res,{ok:true,rec:'AAAA-BBBB CCCC-DDDD'})})}
  if(P=='/api/mfa/off'){MFA.on=false;MFA.rec=0;return j(res,{ok:true})}
  if(P=='/api/mfa/set'){return body(q,p=>{if(p.get('pow')!==null)POW=+p.get('pow');MFA.nt=+p.get('nt');return j(res,{time:true,nt:MFA.nt,pow:POW,list:[{i:0,name:'admin',role:2,on:MFA.on,rec:MFA.rec}]})})}
  if(P=='/api/mfa')return j(res,{time:true,nt:MFA.nt,pow:POW,list:[{i:0,name:'admin',role:2,on:MFA.on,rec:MFA.rec}]});
  if(P=='/api/firstpass'){return body(q,p=>{if(authSet)return j(res,{ok:false,err:'La password e gia impostata'});const pw=p.get('p')||'';if(pw.length<6)return j(res,{ok:false,err:'Password troppo corta (min 6)'});
    let h=sha(SALT+pw);for(let i=0;i<ITER;i++)h=sha(h+SALT+pw);HASH=h;authSet=true;login(res,{ok:true,user:'admin',role:2})})}
  if(P=='/api/logout'){logged=false;return j(res,{ok:true})}
  if(P=='/__posts')return j(res,{posts:global.POSTS});
  if(!logged)return j(res,{ok:false,err:'non autorizzato'},401);
  if(P.startsWith('/api/fs/'))return fsApi(u,q,res);
  if(!G&&P!='/api/shell')return body(q,p=>{global.POSTS.push(P+' '+[...p].map(x=>x[0]+'='+(x[1].length>40?x[1].slice(0,40)+'…':x[1])).join('&'));
    if(P=='/api/region'){for(const [k,v] of p){const K={country:'country',tz:'tz',tzname:'tzName',ntp:'ntp',datefmt:'dateFmt',timefmt:'timeFmt',tempunit:'tempUnit',weekstart:'weekStart',decsep:'decSep',antenna:'antExt',gain:'gain',txpower:'txSet'}[k];if(K)REGION[K]=/^(country|tz|tzName|ntp)$/.test(K)?v:+v}return j(res,REGION)}
    if(P=='/api/setup/done'){setupDone=true;return j(res,{ok:true})}
    if(P=='/api/ap'){if(p.get('qr')!==null&&p.get('ssid')===null){AP.qr=p.get('qr')=='1';return j(res,{ok:true})}if(p.get('new')=='1')AP.pass='wxyzhjk56789';AP.ssid=p.get('ssid');return j(res,{ok:true,pass:AP.pass})}
    if(P=='/api/svc'){const n=Object.assign({},SVC);for(const [k,v] of p){if(['apOn','captive','httpOn','httpsOn','dhcpOn','mdnsOn'].includes(k))n[k]=v=='1';if(['httpPort','httpsPort','lease'].includes(k))n[k]=+v}
      if(n.lease<10||n.lease>1440)return j(res,{ok:false,err:"Durata dell'indirizzo: da 10 a 1440 minuti"});
      if(!n.dhcpOn&&!SVC.clientOk)return j(res,{ok:false,err:'Il DHCP si spegne solo con la Wi-Fi di casa collegata'});
      if(!n.httpOn&&!n.httpsOn)return j(res,{ok:false,err:'Deve restare acceso almeno un protocollo web (HTTP o HTTPS), altrimenti resti chiuso fuori'});
      if(n.httpOn&&n.httpsOn&&n.httpPort==n.httpsPort)return j(res,{ok:false,err:'HTTP e HTTPS non possono usare la stessa porta'});
      if(!n.apOn&&!SVC.sta)return j(res,{ok:false,err:'Per spegnere il Punto di accesso serve prima la Wi-Fi di casa configurata'});
      Object.assign(SVC,n);SVC.pending=(SVC.httpOn!=true||SVC.httpsOn!=true||SVC.httpPort!=80||SVC.httpsPort!=443);return j(res,SVC)}
    if(P=='/api/pinnotes'){const g=p.get('g'),nm=(p.get('name')||'').trim();if(nm)PNOTE[g]=nm;else delete PNOTE[g];return j(res,{ok:true})}
    if(P=='/api/wifi/test'){WT.state=1;const bad=(p.get('pass')||'')=='sbagliata1';setTimeout(()=>{if(bad){WT.state=3;WT.err='Collegamento non riuscito: controlla la password'}else{WT.state=2;WT.ip='192.168.1.57'}},1200);return j(res,{ok:true})}
    if(P=='/api/wifi/save'){SVC.sta=true;return j(res,{ok:true})}
    if(P=='/api/fw')return j(res,Object.assign(FW,{mode:+p.get('mode'),try:120}));
    if(P=='/api/tls')return j(res,TLS);
    if(P=='/api/selftest'){if(p.get('clear')=='1')STST=0;else{STST=2}return j(res,{ok:true})}
    if(P=='/api/log/level'){LOGLV=+p.get('lv');return j(res,{ok:true})}
    if(P=='/api/serial'){var k=p.get('k'),v=p.get('v');if(k=='baud'&&v=='1234')return j(res,{ok:false,err:'Velocita non valida'});if(k=='keep')SRD.trial=false;else if(k=='baud'){SRD.baud=+v;SRD.trial=!SRD.usb}else if(k=='eol')SRD.eol=v;else if(k=='tx')SRD.tx=+v;else SRD[k]=(v=='on');return j(res,SRD)}
    if(P=='/api/boots/clear'){BOOTS=false;return j(res,{ok:true})}
    if(P=='/api/taskkill'){TASKLED=9;return j(res,{ok:true})}
    if(P=='/api/taskrestart'){TASKLED=2;return j(res,{ok:true})}
    if(P=='/api/time'){NTPON=p.get('ntp')=='1';return j(res,{ok:true})}
    if(P=='/api/ble'){BLE.on=p.get('a')=='start';BLE.pin=BLE.on?'123456':'';BLE.left=BLE.on?600:0;return j(res,BLE)}
    if(P=='/api/mqtt/run'){const a=p.get('a');if(a=='start'||a=='restart'){MQS.run=true;MQS.conn=true;status.mqtt=2}if(a=='stop'){MQS.run=false;MQS.conn=false;status.mqtt=0}return j(res,{ok:true})}
    if(P=='/api/mesh/run'){const a=p.get('a');if(a=='start'||a=='restart'){MESH.run=true;MESH.hasKey=true;HOMEJ.mesh=1}if(a=='stop'){MESH.run=false;HOMEJ.mesh=0}return j(res,{ok:true})}
    if(P=='/api/mesh')return j(res,{ok:true,key:'a'.repeat(64)});
    if(P=='/api/lang'){devLang=p.get('code');REGION.lang=devLang}
    j(res,{ok:true})});
  if(P=='/api/shell'){body(q,(p)=>{res.writeHead(200,{'Content-Type':'text/plain; charset=utf-8'});res.end(p.get('c')=='help'?'Comandi:\n  help   questo elenco\n':'Comando sconosciuto.')});return}
  if(P=='/api/status')return j(res,status);
  if(P=='/api/settings')return j(res,{serialAuth:true,hostname:'vesevos',domain:'',staSsid:'',staDhcp:true,ip:'',mask:'',gw:'',dns1:'',dns2:'',apSsid:'VesevOS',ledMode:0,ledColor:255,ledBrightness:40});
  if(P=='/api/region')return j(res,REGION);
  if(P=='/api/audit')return j(res,AUDIT);
  if(P=='/api/users')return j(res,USERS);
  if(P=='/api/ap')return j(res,{ssid:AP.ssid,pass:AP.qr||role>=2?AP.pass:'',qr:AP.qr});
  if(P=='/api/svc')return j(res,SVC);
  if(P=='/api/pinnotes')return j(res,PNOTE);
  if(P=='/api/fw')return j(res,FW);
  if(P=='/api/wd')return j(res,WD);
  if(P=='/api/tls')return j(res,TLS);
  if(P=='/api/mesh')return j(res,MESH);
  if(P=='/api/mesh/key')return j(res,{key:''});
  if(P=='/api/ble')return j(res,BLE);
  if(P=='/api/rules')return j(res,{text:'1|Sveglia|time 07:00 1111100|-|led-color ffcc00;led fixed;note Sveglia',st:[{i:0,last:'',n:0,off:false,run:false}]});
  if(P=='/api/boot')return j(res,{order:['sys','led','net','time','web','rules','mqtt','mesh','wd'],custom:false,fell:false,stable:true,svc:[{id:'sys',req:[],parent:null,at:0,ms:12},{id:'led',req:[],parent:null,at:12,ms:3},{id:'net',req:[],parent:null,at:15,ms:40},{id:'time',req:['net'],parent:'net',at:55,ms:2},{id:'web',req:['net','sys'],parent:'net',at:57,ms:90},{id:'rules',req:['time','sys'],parent:'time',at:147,ms:5},{id:'mqtt',req:['net'],parent:'net',at:150,ms:1},{id:'mesh',req:['net'],parent:'net',at:151,ms:1},{id:'wd',req:['sys'],parent:'sys',at:152,ms:1}]});
  if(P=='/api/tasklist')return j(res,{ok:true,total:1000,tasks:[{n:"IDLE0",id:1,s:0,p:0,stk:900,rt:800,t:0,k:false},{n:"led",id:3,s:TASKLED,p:1,stk:2100,rt:10,t:1,k:true}]});
  if(P=='/api/log'){res.writeHead(200,{'Content-Type':'text/plain; charset=utf-8'});return res.end('03/10 19:40:00 [I] NET: client connesso\n03/10 19:40:01 [W] ATTENZIONE: riavvio anomalo (Task watchdog)\n03/10 19:40:02 [D] TOP: task piu attivo httpd 16%\n03/10 19:40:03 [E] ERRORE: prova\n')}
  if(P=='/api/log/level')return j(res,{lv:LOGLV});
  if(P=='/api/serial')return j(res,SRD);
  if(P=='/api/boots'){res.writeHead(200,{'Content-Type':'text/plain; charset=utf-8'});return res.end(BOOTS?'Avvio n.64 - 07/10 20:02:15 - Task watchdog (!)\n    era acceso da 1h 3min, RAM minima 42 KB; task piu attivo: httpd\n':'')}
  if(P=='/api/home')return j(res,HOMEJ);
  if(P=='/api/mqtt')return j(res,{run:MQS.run,conn:MQS.conn,auto:false,host:'',port:1883,user:'',hasPass:false,prefix:'',prefixUsed:'vesevos/vesevos',every:30,ha:false,tls:false,hasCa:false,sent:0,recv:0,err:''});
  if(P=='/api/ban')return j(res,{fails:5,secs:60,list:[]});
  if(P=='/api/pins/info')return j(res,{chip:'ESP32-S3',rev:2,cores:2,gpioCount:49,gpioValid:45,gpioOut:45,board:'ESP32-S3 SuperMini',flash:4194304,psram:2097152});
  if(P=='/api/selftest/report'){res.writeHead(200,{'content-type':'text/plain'});return res.end('VesevOS report\n01 [OK] Sistema (30 ms)\n02 [WARN] Rete (30 ms)\n')}
  if(P=='/api/selftest'){const L=[];for(let i=0;i<(STST?16:0);i++)L.push({s:i==1?2:1,n:'Prova '+(i+1),ms:30,d:'dettaglio '+i});return j(res,{state:STST,cur:STST==1?3:16,total:16,list:L})}
  if(P=='/api/time')return j(res,{valid:true,now:'03/10/2026 19:40:00',epoch:1791049200,ntp:NTPON,every:60,last:1791049000,server:'pool.ntp.org',serve:false,tz:REGION.tz,dateFmt:0,timeFmt:0,tempUnit:0,tzName:REGION.tzName});
  if(P=='/api/pins')return j(res,[{gpio:0,owner:'Sistema',note:'Pulsante BOOT',fixed:true},{gpio:48,owner:'LED',note:'LED RGB WS2812',fixed:true}]);
  if(P=='/api/pinmap'){const m=[];for(let g=0;g<=48;g++){if(g>21&&g<33)continue;const ok=![0,3,19,20,45,46,48].includes(g);m.push({g,ok,why:ok?'':'Pin di avvio',owner:'',note:PNOTE[g]||''})}return j(res,m)}
  if(P=='/api/pintest')return j(res,{gpio:-1});
  if(P=='/api/wifi/test')return j(res,WT);
  if(P=='/api/wifi/scan')return j(res,{running:false,list:[{ssid:'CasaMia',rssi:-55,ch:6,open:false}]});
  j(res,{ok:true});
}).listen(8099,()=>console.log('server su 8099'));
