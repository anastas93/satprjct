// Скрипт веб-интерфейса
#pragma once
// Сохранение настроек и истории чата, статус Simple/Large, прочерки вместо хеша ключей, кнопка просмотра хеша
const char WEB_SCRIPT_JS[] PROGMEM = R"rawliteral(
function appendChat(t){
  const chat=document.getElementById('chat');
  const n=document.createElement('div');
  let cls='',tag='',txt=t;
  if(txt.startsWith('*TX:*')){cls='tx';tag='TX';txt=txt.slice(5).trim();}
  else if(txt.startsWith('*RX:*')){cls='rx';tag='RX';txt=txt.slice(5).trim();}
  else if(txt.startsWith('*SYS:*')){cls='sys';tag='SYS';txt=txt.slice(6).trim();}
  const tm=new Date().toLocaleTimeString();
  n.className='msg-line '+cls;
  n.innerHTML='<span class="msg-time">['+tm+']</span> '+(tag?'<span class="msg-tag">'+tag+':</span> ':'')+'<span class="msg-text">'+txt+'</span>';
  chat.appendChild(n);
  chat.scrollTop=chat.scrollHeight;
  while(chat.children.length>100)chat.removeChild(chat.lastChild);
  localStorage.setItem('chatLog',chat.innerHTML);
}
const savedChat=localStorage.getItem('chatLog');
if(savedChat){const ch=document.getElementById('chat');ch.innerHTML=savedChat;ch.scrollTop=ch.scrollHeight;}

const pingHistory=[];
function updatePingHistory(rssi,snr,dist,time){
  pingHistory.push({rssi,snr,dist,time});
  if(pingHistory.length>10)pingHistory.shift();
  const ph=document.getElementById('pingHistory');
  ph.textContent=pingHistory.map((p,i)=>(i+1)+'. RSSI:'+p.rssi+' dBm, SNR:'+p.snr+' dB, dist:'+p.dist+' km, time:'+p.time+' ms').join('\n');
}

const ws=new WebSocket('ws://'+location.hostname+':81/');
ws.onmessage=e=>{
  const t=e.data.trim();
  if(!t)return;
  t.split('\n').forEach(l=>{
    if(!l)return;
    appendChat(l);
    if(l.indexOf('Ping>OK')!==-1){
      try{
        const m=/RSSI:([\-\d\.]+) dBm\/SNR:([\-\d\.]+) dB distance:~([\d\.]+) km time:([\d\.]+) ms/.exec(l);
        if(m){
          const rssi=parseFloat(m[1]).toFixed(1);
          const snr=parseFloat(m[2]).toFixed(1);
          const dist=parseFloat(m[3]).toFixed(3);
          const time=parseFloat(m[4]).toFixed(2);
          updatePingHistory(rssi,snr,dist,time);
        }
      }catch(e){}
    }
  });
};

function applyProfile(name){
  if(name==='range'){
    setSelect('bwSelect','7.8');setSelect('sfSelect','12');setSelect('crSelect','8');setSelect('txpSelect','12');
    sendParam('setbw','7.8');sendParam('setsf','12');sendParam('setcr','8');sendParam('settxp','12');
  }else if(name==='speed'){
    setSelect('bwSelect','500');setSelect('sfSelect','7');setSelect('crSelect','5');setSelect('txpSelect','20');
    sendParam('setbw','500');sendParam('setsf','7');sendParam('setcr','5');sendParam('settxp','20');
  }else if(name==='balanced'){
    setSelect('bwSelect','125');setSelect('sfSelect','9');setSelect('crSelect','6');setSelect('txpSelect','14');
    sendParam('setbw','125');sendParam('setsf','9');sendParam('setcr','6');sendParam('settxp','14');
  }
}
function setSelect(id,val){const el=document.getElementById(id);if(el)el.value=val;}
function sendParam(path,val){fetch('/'+path+'?val='+encodeURIComponent(val));}

document.getElementById('sendBtn').addEventListener('click',()=>{
  const m=document.getElementById('msg').value;
  const st=document.getElementById('sendStatus');
  if(m){
    fetch('/send?msg='+encodeURIComponent(m)).then(r=>{
      if(r.ok){st.textContent='\u2714';st.style.color='var(--sys-color)';document.getElementById('msg').value='';}
      else{st.textContent='\u2716';st.style.color='red';}
    }).catch(()=>{st.textContent='\u2716';st.style.color='red';});
    setTimeout(()=>{st.textContent=''},2000);
  }
});

document.getElementById('cleanBtn').addEventListener('click',()=>{
  const c=document.getElementById('chat');
  c.innerHTML='';
  localStorage.removeItem('chatLog');
});

document.getElementById('bankSelect').addEventListener('change',e=>{localStorage.setItem('bank',e.target.value);sendParam('setbank',e.target.value);});
document.getElementById('presetSelect').addEventListener('change',e=>{localStorage.setItem('preset',e.target.value);sendParam('setpreset',e.target.value);});
document.getElementById('bwSelect').addEventListener('change',e=>{localStorage.setItem('bw',e.target.value);sendParam('setbw',e.target.value);});
document.getElementById('sfSelect').addEventListener('change',e=>{localStorage.setItem('sf',e.target.value);sendParam('setsf',e.target.value);});
document.getElementById('crSelect').addEventListener('change',e=>{localStorage.setItem('cr',e.target.value);sendParam('setcr',e.target.value);});
document.getElementById('txpSelect').addEventListener('change',e=>{localStorage.setItem('txp',e.target.value);sendParam('settxp',e.target.value);});
document.getElementById('ackChk').addEventListener('change',e=>{localStorage.setItem('ack',e.target.checked?'1':'0');fetch('/toggleack');});
document.getElementById('retryNInput').addEventListener('change',e=>{localStorage.setItem('retryN',e.target.value);sendParam('setretryn',e.target.value);});
document.getElementById('retryMSInput').addEventListener('change',e=>{localStorage.setItem('retryMS',e.target.value);sendParam('setretryms',e.target.value);});
document.getElementById('encChk').addEventListener('change',e=>{localStorage.setItem('enc',e.target.checked?'1':'0');fetch('/toggleenc');});
document.getElementById('kidInput').addEventListener('change',e=>{localStorage.setItem('kid',e.target.value);sendParam('setkid',e.target.value);});

document.getElementById('keyBtn').addEventListener('click',()=>{const k=document.getElementById('keyInput').value;fetch('/setkey?val='+encodeURIComponent(k));});
document.getElementById('saveBtn').addEventListener('click',()=>{fetch('/save');});
document.getElementById('loadBtn').addEventListener('click',()=>{fetch('/load');});
document.getElementById('resetBtn').addEventListener('click',()=>{fetch('/reset');});
document.getElementById('profileSelect').addEventListener('change',e=>{localStorage.setItem('profile',e.target.value);applyProfile(e.target.value);});

const themeToggle=document.getElementById('themeToggle');
const fontRange=document.getElementById('fontRange');
function applyPrefs(){const th=localStorage.getItem('theme')||'dark';const fs=localStorage.getItem('font')||'16';document.body.classList.toggle('light',th==='light');themeToggle.checked=th==='light';document.documentElement.style.setProperty('--base-font',fs+'px');fontRange.value=fs;}
applyPrefs();
themeToggle.addEventListener('change',()=>{const light=themeToggle.checked;document.body.classList.toggle('light',light);localStorage.setItem('theme',light?'light':'dark');});
fontRange.addEventListener('input',()=>{const v=fontRange.value;document.documentElement.style.setProperty('--base-font',v+'px');localStorage.setItem('font',v);});

function applySettings(){const m=[['profileSelect','profile'],['bankSelect','bank'],['presetSelect','preset'],['bwSelect','bw'],['sfSelect','sf'],['crSelect','cr'],['txpSelect','txp'],['retryNInput','retryN'],['retryMSInput','retryMS'],['kidInput','kid']];m.forEach(([id,key])=>{const el=document.getElementById(id);const val=localStorage.getItem(key);if(el&&val!==null)el.value=val;});document.getElementById('ackChk').checked=localStorage.getItem('ack')==='1';document.getElementById('encChk').checked=localStorage.getItem('enc')==='1';}
applySettings();

document.getElementById('pingBtn').addEventListener('click',()=>{fetch('/ping');});
document.getElementById('metricsBtn').addEventListener('click',()=>{fetch('/metrics').then(r=>r.text()).then(t=>{document.getElementById('metrics').textContent=t;});});
document.getElementById('selfTestBtn').addEventListener('click',()=>{fetch('/selftest');});

document.getElementById('simpleBtn').addEventListener('click',()=>{const st=document.getElementById('simpleStatus');fetch('/simple').then(r=>{st.textContent=r.ok?'\u2714':'\u2716';st.style.color=r.ok?'var(--sys-color)':'red';}).catch(()=>{st.textContent='\u2716';st.style.color='red';});setTimeout(()=>{st.textContent=''},2000);});
document.getElementById('largeBtn').addEventListener('click',()=>{const n=document.getElementById('largeSize').value;const st=document.getElementById('largeStatus');fetch('/large?size='+encodeURIComponent(n)).then(r=>{st.textContent=r.ok?'\u2714':'\u2716';st.style.color=r.ok?'var(--sys-color)':'red';}).catch(()=>{st.textContent='\u2716';st.style.color='red';});setTimeout(()=>{st.textContent=''},2000);});
document.getElementById('encTestBtn').addEventListener('click',()=>{const n=document.getElementById('encTestSize').value;let url='/enctest';if(n)url+='?size='+encodeURIComponent(n);fetch(url);});
document.getElementById('encTestBadBtn').addEventListener('click',()=>{fetch('/enctestbad');});
document.getElementById('msgIdBtn').addEventListener('click',()=>{const n=document.getElementById('msgIdVal').value;let url='/msgid';if(n)url+='?val='+encodeURIComponent(n);fetch(url);});

const keyTestBtn=document.getElementById('keyTestBtn');if(keyTestBtn){keyTestBtn.addEventListener('click',()=>{fetch('/keytest');});}
const keyReqBtn=document.getElementById('keyReqBtn');if(keyReqBtn){keyReqBtn.addEventListener('click',()=>{fetch('/keyreq');});}
const keySendBtn=document.getElementById('keySendBtn');if(keySendBtn){keySendBtn.addEventListener('click',()=>{fetch('/keysend');});}
const keyDhBtn=document.getElementById('keyDhBtn');if(keyDhBtn){keyDhBtn.addEventListener('click',()=>{fetch('/keydh');});}
// Кнопка отображения хеша ключа с обработкой ошибок запроса
const keyHashBtn=document.getElementById('keyHashBtn');if(keyHashBtn){keyHashBtn.addEventListener('click',()=>{fetch('/keystatus').then(r=>r.json()).then(d=>{alert('Hash: '+d.hash);}).catch(()=>{alert('Ошибка получения хеша');});});}

function updateKeyStatus(){fetch('/keystatus').then(r=>r.json()).then(d=>{const i=document.getElementById('keyIndicator');const t=document.getElementById('keyStatusText');if(!i||!t)return;const h='----';if(d.status==='local'){i.classList.remove('remote');i.classList.add('local');t.textContent='Local '+h;}else{i.classList.remove('local');i.classList.add('remote');t.textContent='Remote '+h;}i.classList.toggle('blink',Number(d.request)===1)}).catch(()=>{});setTimeout(updateKeyStatus,1000)}
updateKeyStatus();

document.getElementById('sendQBtn').addEventListener('click',()=>{const msg=document.getElementById('sendQMsg').value;const prio=document.getElementById('sendQPrio').value;if(msg){fetch('/sendq?prio='+encodeURIComponent(prio)+'&msg='+encodeURIComponent(msg));document.getElementById('sendQMsg').value='';}});
document.getElementById('largeQBtn').addEventListener('click',()=>{const sz=document.getElementById('largeQSize').value;const prio=document.getElementById('largeQPrio').value;fetch('/largeq?prio='+encodeURIComponent(prio)+'&size='+encodeURIComponent(sz));});
document.getElementById('qosModeBtn').addEventListener('click',()=>{const mode=document.getElementById('qosModeSelect').value;fetch('/qosmode?val='+encodeURIComponent(mode));});
document.getElementById('qosBtn').addEventListener('click',()=>{fetch('/qos').then(r=>r.text()).then(t=>{document.getElementById('qosStats').textContent=t;});});
)rawliteral";

