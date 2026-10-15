/* ===== Script (Berry): elenco, avvio/stop, uscita in diretta. Il testo si modifica con l'editor dei file ===== */
var SC={tm:0,list:[],busy:false,bg:false,name:"",out:null,fsOk:false};
function scLoad(){scTimerStop();scPoll();SC.tm=setInterval(function(){if(cur!="script"||document.hidden){if(cur!="script")scTimerStop();return}scPoll()},1500)}
function scTimerStop(){if(SC.tm){clearInterval(SC.tm);SC.tm=0}}
function scPoll(){api("/api/script").then(function(r){return r.json()}).then(function(j){if(!j.ok){msg("scm",j.err,false);return}scDraw(j)}).catch(function(){})}
function scFsInfo(){if(SC.fsOk)return;api("/api/fs/list?path=/scripts").then(function(r){return r.json()}).then(function(j){if(j.ok){FS.used=j.used;FS.total=j.total;SC.fsOk=true}}).catch(function(){})}
function scDraw(j){SC.list=j.list;SC.busy=j.busy;SC.bg=j.bg;SC.name=j.name;scFsInfo();
 var run=j.busy&&j.bg,st;
 if(run)st=tf("In corso: {0}",j.name);else if(j.busy)st=t("Berry sta eseguendo un comando");else if(j.last==1)st=tf("Fermo. Ultima esecuzione finita senza errori ({0} ms).",j.ms);else if(j.last==0)st=t("Fermo. L'ultima esecuzione e finita con un errore (vedi l'uscita).");else st=t("Fermo. Nessuno script eseguito dall'accensione.");
 var se=$("scst");if(se.textContent!=st)se.textContent=st;
 $("scstop").disabled=!j.busy;
 var h="";
 if(!j.list.length)h='<tbody><tr><td colspan="3" class="fhint">'+esc(t("Nessuno script. Premi Nuovo script per crearne uno."))+'</td></tr></tbody>';
 else{h="<tbody>";j.list.forEach(function(x){var on=run&&j.name==x.n,q=esc(x.n),oq="'"+x.n+"'";
  h+='<tr class="'+(on?"lp":"")+'"><td><span class="scn"><svg class="i" aria-hidden="true"><use href="#i-fvsc"/></svg><b>'+q+'</b>'+(on?'<span class="pill">'+esc(t("in corso"))+'</span>':"")+'</span></td><td class="fsz">'+fSize(x.s)+'</td>'+
   '<td class="fac tkb"><span class="svi">'+sibB("sm go","i-sgo",tf("Esegui {0}",x.n),on,j.busy,j.busy?"":"scRun("+oq+")")+sibB("sm st","i-sst",tf("Ferma {0}",x.n),false,!on,on?"scStopRun()":"")+
   sibB("sm","i-edit",tf("Modifica {0}",x.n),false,false,"scEdit("+oq+")")+sibB("sm","i-del",tf("Elimina {0}",x.n),false,on,on?"":"scDel("+oq+")")+'</span></td></tr>'});h+="</tbody>"}
 if(SC._h!==h){SC._h=h;$("sctab").innerHTML=h}
 var o=(j.trunc?"... ":"")+j.out+(j.err?(j.out?"\n":"")+j.err:""),pre=$("scout");if(!o)o=t("Nessuna uscita");
 if(pre.textContent!==o){var bot=pre.scrollTop+pre.clientHeight>=pre.scrollHeight-24;pre.textContent=o;if(bot)pre.scrollTop=pre.scrollHeight}
 $("scinfo").textContent=tf("Esecuzioni dall'accensione: {0}. Limiti: {1} di testo, memoria 64 KB, un solo script alla volta.",j.runs,fSize(j.max))}
function scRun(n){post("/api/script/run",{name:n}).then(function(r){return r.json()}).then(function(j){msg("scm",j.ok?tf("Avviato: {0}",n):j.err,j.ok);scPoll()}).catch(function(){})}
function scStopRun(){post("/api/script/stop",{}).then(function(r){return r.json()}).then(function(j){msg("scm",j.ok?t("Script fermato"):j.err,j.ok);scPoll()}).catch(function(){})}
function scPath(n){return"/scripts/"+n+".vsc"}
function scEdit(n){api("/api/fs/text?path="+enc(scPath(n))).then(function(r){return r.text().then(function(x){return{ok:r.ok,x:x}})}).then(function(o){if(!o.ok){msg("scm",o.x,false);return}fEdOpen(scPath(n),o.x,false)}).catch(function(){})}
function scNew(){dlg({title:t("Nuovo script"),body:'<label for="dn">'+esc(t("Nome dello script"))+'</label><input id="dn" autocomplete="off" maxlength="24" spellcheck="false" autocapitalize="off" placeholder="lampeggio"><p class="fhint" style="margin:0">'+esc(t("Da 1 a 24 caratteri: lettere minuscole, numeri, trattino e sottotrattino."))+'</p>',ok:t("Crea"),
 onOk:function(){var n=dlgV("dn").toLowerCase().replace(/\.vsc$/,"");if(!/^[a-z0-9_-]{1,24}$/.test(n))return t("Nome non valido");if(SC.list.some(function(x){return x.n==n}))return t("Esiste gia");
  fEdOpen(scPath(n),"# "+n+"\nprint('ciao')\n",true);return""}})}
function scDel(n){dlg({title:t("Eliminare?"),body:'<p style="margin:0">'+esc(tf("Lo script {0} verra eliminato.",n))+'</p><p style="margin:8px 0 0;color:var(--mut)">'+esc(t("Non si puo annullare."))+'</p>',ok:t("Elimina"),danger:true,
 onOk:function(){return post("/api/fs/del",{path:scPath(n)}).then(function(r){return r.json()}).then(function(j){scPoll();return j.ok?"":j.err})}})}
