// Минифицированный скрипт веб-интерфейса
// Исправлено отображение чата и обработка кнопки загрузки ключа
#pragma once
const char WEB_SCRIPT_JS[] PROGMEM = R"rawliteral(
function appendChat(txt){const chat=document.getElementById('chat');const div=document.createElement('div');let cls='',tag='',msg=txt;if(msg.startsWith('*TX:*')){cls='tx';tag='TX';msg=msg.slice(5).trim()}else if(msg.startsWith('*RX:*')){cls='rx';tag='RX';msg=msg.slice(5).trim()}else if(msg.startsWith('*SYS:*')){cls='sys';tag='SYS';msg=msg.slice(6).trim()}const time=new Date().toLocaleTimeString();div.className=cls;div.textContent='['+time+'] '+(tag?tag+': ':'')+msg;chat.appendChild(div);while(chat.children.length>100)chat.removeChild(chat.lastChild)}
const pingHistory=[];function updatePingHistory(rssi,snr,dist,time){pingHistory.push({rssi,snr,dist,time});if(pingHistory.length>10)pingHistory.shift();const ph=document.getElementById('pingHistory');ph.textContent=pingHistory.map((p,i)=>(i+1)+'. RSSI:'+p.rssi+' dBm, SNR:'+p.snr+' dB, dist:'+p.dist+' km, time:'+p.time+' ms').join('\n')}
const ws=new WebSocket('ws://'+location.hostname+':81/');ws.onmessage=e=>{const t=e.data.trim();if(t){t.split('\n').forEach(l=>{if(!l)return;appendChat(l);if(l.indexOf('Ping>OK')!==-1){try{const m=/RSSI:([\-\d\.]+) dBm\/SNR:([\-\d\.]+) dB distance:~([\d\.]+) km time:([\d\.]+) ms/.exec(l);if(m){const rssi=parseFloat(m[1]).toFixed(1);const snr=parseFloat(m[2]).toFixed(1);const dist=parseFloat(m[3]).toFixed(3);const time=parseFloat(m[4]).toFixed(2);updatePingHistory(rssi,snr,dist,time)}}catch(e){}}})}}
function applyProfile(name){if(name==='range'){setSelect('bwSelect','7.8');setSelect('sfSelect','12');setSelect('crSelect','8');setSelect('txpSelect','12');sendParam('setbw','7.8');sendParam('setsf','12');sendParam('setcr','8');sendParam('settxp','12')}else if(name==='speed'){setSelect('bwSelect','500');setSelect('sfSelect','7');setSelect('crSelect','5');setSelect('txpSelect','20');sendParam('setbw','500');sendParam('setsf','7');sendParam('setcr','5');sendParam('settxp','20')}else if(name==='balanced'){setSelect('bwSelect','125');setSelect('sfSelect','9');setSelect('crSelect','6');setSelect('txpSelect','14');sendParam('setbw','125');sendParam('setsf','9');sendParam('setcr','6');sendParam('settxp','14')}}
function setSelect(id,val){const el=document.getElementById(id);if(el)el.value=val}
function sendParam(path,val){fetch('/'+path+'?val='+encodeURIComponent(val))}
document.getElementById('sendBtn').addEventListener('click',()=>{const m=document.getElementById('msg').value;const st=document.getElementById('sendStatus');if(m){fetch('/send?msg='+encodeURIComponent(m)).then(r=>{if(r.ok){st.textContent='\u2714';st.style.color='var(--sys-color)';document.getElementById('msg').value=''}else{st.textContent='\u2716';st.style.color='red'}}).catch(()=>{st.textContent='\u2716';st.style.color='red'});setTimeout(()=>{st.textContent=''},2000)}});
document.getElementById('bankSelect').addEventListener('change',e=>{sendParam('setbank',e.target.value)});
document.getElementById('presetSelect').addEventListener('change',e=>{sendParam('setpreset',e.target.value)});
document.getElementById('bwSelect').addEventListener('change',e=>{sendParam('setbw',e.target.value)});
document.getElementById('sfSelect').addEventListener('change',e=>{sendParam('setsf',e.target.value)});
document.getElementById('crSelect').addEventListener('change',e=>{sendParam('setcr',e.target.value)});
document.getElementById('txpSelect').addEventListener('change',e=>{sendParam('settxp',e.target.value)});
document.getElementById('ackChk').addEventListener('change',()=>{fetch('/toggleack')});
document.getElementById('retryNInput').addEventListener('change',e=>{sendParam('setretryn',e.target.value)});
document.getElementById('retryMSInput').addEventListener('change',e=>{sendParam('setretryms',e.target.value)});
document.getElementById('encChk').addEventListener('change',()=>{fetch('/toggleenc')});
document.getElementById('kidInput').addEventListener('change',e=>{sendParam('setkid',e.target.value)});
document.getElementById('keyBtn').addEventListener('click',()=>{const k=document.getElementById('keyInput').value;fetch('/setkey?val='+encodeURIComponent(k))});
document.getElementById('saveBtn').addEventListener('click',()=>{fetch('/save')});
document.getElementById('loadBtn').addEventListener('click',()=>{fetch('/load')});
document.getElementById('resetBtn').addEventListener('click',()=>{fetch('/reset')});
document.getElementById('profileSelect').addEventListener('change',e=>{applyProfile(e.target.value)});
const themeToggle=document.getElementById('themeToggle');
const fontRange=document.getElementById('fontRange');
function applyPrefs(){const th=localStorage.getItem('theme')||'dark';const fs=localStorage.getItem('font')||'16';document.body.classList.toggle('light',th==='light');themeToggle.checked=th==='light';document.documentElement.style.setProperty('--base-font',fs+'px');fontRange.value=fs}
applyPrefs();
themeToggle.addEventListener('change',()=>{const light=themeToggle.checked;document.body.classList.toggle('light',light);localStorage.setItem('theme',light?'light':'dark')});
fontRange.addEventListener('input',()=>{const v=fontRange.value;document.documentElement.style.setProperty('--base-font',v+'px');localStorage.setItem('font',v)});
document.getElementById('simpleBtn').addEventListener('click',()=>{fetch('/simple')});
document.getElementById('largeBtn').addEventListener('click',()=>{const n=document.getElementById('largeSize').value;fetch('/large?size='+encodeURIComponent(n))});
document.getElementById('encTestBtn').addEventListener('click',()=>{const n=document.getElementById('encTestSize').value;let url='/enctest';if(n)url+='?size='+encodeURIComponent(n);fetch(url)});
document.getElementById('encTestBadBtn').addEventListener('click',()=>{fetch('/enctestbad')});
document.getElementById('msgIdBtn').addEventListener('click',()=>{const n=document.getElementById('msgIdVal').value;let url='/msgid';if(n)url+='?val='+encodeURIComponent(n);fetch(url)});
const keyTestBtn=document.getElementById('keyTestBtn');if(keyTestBtn){keyTestBtn.addEventListener('click',()=>{fetch('/keytest')})}
const keyReqBtn=document.getElementById('keyReqBtn');if(keyReqBtn){keyReqBtn.addEventListener('click',()=>{fetch('/keyreq')})}
const keySendBtn=document.getElementById('keySendBtn');if(keySendBtn){keySendBtn.addEventListener('click',()=>{fetch('/keysend')})}
const keyDhBtn=document.getElementById('keyDhBtn');if(keyDhBtn){keyDhBtn.addEventListener('click',()=>{fetch('/keydh')})}
function updateKeyStatus(){fetch('/keystatus').then(r=>r.json()).then(data=>{const ind=document.getElementById('keyIndicator');const txt=document.getElementById('keyStatusText');if(!ind||!txt)return;if(data.status==='local'){ind.classList.remove('remote');ind.classList.add('local');const hash=data.hash?data.hash:'----';txt.textContent='Local '+hash}else{ind.classList.remove('local');ind.classList.add('remote');txt.textContent='Remote'}if(parseInt(data.request)===1){ind.classList.add('blink')}else{ind.classList.remove('blink')}}).catch(()=>{});setTimeout(updateKeyStatus,1000)}
updateKeyStatus();
document.getElementById('sendQBtn').addEventListener('click',()=>{const msg=document.getElementById('sendQMsg').value;const prio=document.getElementById('sendQPrio').value;if(msg){fetch('/sendq?prio='+encodeURIComponent(prio)+'&msg='+encodeURIComponent(msg));document.getElementById('sendQMsg').value=''}});
document.getElementById('largeQBtn').addEventListener('click',()=>{const sz=document.getElementById('largeQSize').value;const prio=document.getElementById('largeQPrio').value;fetch('/largeq?prio='+encodeURIComponent(prio)+'&size='+encodeURIComponent(sz))});
document.getElementById('qosModeBtn').addEventListener('click',()=>{const mode=document.getElementById('qosModeSelect').value;fetch('/qosmode?val='+encodeURIComponent(mode))});
document.getElementById('qosBtn').addEventListener('click',()=>{fetch('/qos').then(r=>r.text()).then(t=>{document.getElementById('qosStats').textContent=t})});
)rawliteral";
