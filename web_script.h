// Скрипт веб-интерфейса
#pragma once
// Сохранение настроек и истории чата, статус Simple/Large, прочерки вместо хеша ключей, кнопка просмотра хеша
const char WEB_SCRIPT_JS[] PROGMEM = R"rawliteral(
function appendChat(t){
  const chat=document.getElementById('chat');
  const n=document.createElement('div');
  let cls='',tag='',txt=t;
  const m=/^\*([A-Z]+):\*\s*(.*)$/.exec(txt);
  if(m){
    tag=m[1];
    txt=m[2];
    if(tag==='TX'){cls='tx';blinkIndicator('txIndicator');}
    else if(tag==='RX'){cls='rx';blinkIndicator('rxIndicator');}
    else cls='sys';
  }
  const tm=new Date().toLocaleTimeString();
  n.className='msg-line '+cls;
  n.innerHTML='<span class="msg-time">['+tm+']</span> '+(tag?'<span class="msg-tag">'+tag+':</span> ':'')+'<span class="msg-text">'+txt+'</span>';
  chat.appendChild(n);
  chat.scrollTop=chat.scrollHeight;
  while(chat.children.length>100)chat.removeChild(chat.lastChild);
  localStorage.setItem('chatLog',chat.innerHTML);
}

// Мигает выбранный индикатор активности TX/RX
function blinkIndicator(id){const e=document.getElementById(id);if(!e)return;e.classList.add('blink');setTimeout(()=>e.classList.remove('blink'),500);}
const savedChat=localStorage.getItem('chatLog');
if(savedChat){const ch=document.getElementById('chat');ch.innerHTML=savedChat;ch.scrollTop=ch.scrollHeight;}

const pingHistory=[];
function updatePingHistory(rssi,snr,dist,time){
  pingHistory.push({rssi,snr,dist,time});
  if(pingHistory.length>10)pingHistory.shift();
  const ph=document.getElementById('pingHistory');
  ph.textContent=pingHistory.map((p,i)=>(i+1)+'. RSSI:'+p.rssi+' dBm, SNR:'+p.snr+' dB, dist:'+p.dist+' km, time:'+p.time+' ms').join('\n');
}

// простые массивы для графиков диагностики канала
const perData=[],rtt50Data=[],rtt95Data=[],goodputData=[];
// последние значения счётчиков TX/RX для индикации активности
let lastTx=0,lastRx=0;
function drawGraph(id,data,color){
  const c=document.getElementById(id);if(!c)return;const ctx=c.getContext('2d');
  ctx.clearRect(0,0,c.width,c.height);if(data.length<2)return;
  const max=Math.max(...data),min=Math.min(...data),k=max-min||1;
  ctx.beginPath();data.forEach((v,i)=>{const x=i*(c.width/(data.length-1));const y=c.height-((v-min)/k*c.height);i?ctx.lineTo(x,y):ctx.moveTo(x,y);});
  ctx.strokeStyle=color;ctx.stroke();
}
function updateLinkDiag(){
  fetch('/linkdiag').then(r=>r.json()).then(d=>{
    perData.push(d.per);if(perData.length>50)perData.shift();
    rtt50Data.push(d.rtt_p50);if(rtt50Data.length>50)rtt50Data.shift();
    rtt95Data.push(d.rtt_p95);if(rtt95Data.length>50)rtt95Data.shift();
    goodputData.push(d.goodput);if(goodputData.length>50)goodputData.shift();
    drawGraph('perGraph',perData,'#66bb6a');
    drawGraph('rtt50Graph',rtt50Data,'#3f51b5');
    drawGraph('rtt95Graph',rtt95Data,'#e53935');
    drawGraph('goodputGraph',goodputData,'#ffb74d');
    const lp=document.getElementById('linkProfile');if(lp)lp.textContent='Профиль: '+d.profile;
    const ab=document.getElementById('ackBitmap');if(ab)ab.textContent='Bitmap долгов: '+d.bitmap;
    if(typeof d.tx_frames==='number' && d.tx_frames>lastTx){blinkIndicator('txIndicator');lastTx=d.tx_frames;}
    if(typeof d.rx_frames==='number' && d.rx_frames>lastRx){blinkIndicator('rxIndicator');lastRx=d.rx_frames;}
  }).catch(()=>{});
}
setInterval(updateLinkDiag,1000);

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
// Безопасно добавляет обработчики событий и пропускает отсутствующие элементы
function on(id,ev,fn){const el=document.getElementById(id);if(el)el.addEventListener(ev,fn);}

on('sendBtn','click',()=>{
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

on('cleanBtn','click',()=>{
  const c=document.getElementById('chat');
  c.innerHTML='';
  localStorage.removeItem('chatLog');
});

on('bankSelect','change',e=>{localStorage.setItem('bank',e.target.value);sendParam('setbank',e.target.value);});
on('presetSelect','change',e=>{localStorage.setItem('preset',e.target.value);sendParam('setpreset',e.target.value);});
on('bwSelect','change',e=>{localStorage.setItem('bw',e.target.value);sendParam('setbw',e.target.value);});
on('sfSelect','change',e=>{localStorage.setItem('sf',e.target.value);sendParam('setsf',e.target.value);});
on('crSelect','change',e=>{localStorage.setItem('cr',e.target.value);sendParam('setcr',e.target.value);});
on('txpSelect','change',e=>{localStorage.setItem('txp',e.target.value);sendParam('settxp',e.target.value);});
// Явная установка ACK вместо неопределённого переключения
on('ackChk','change',e=>{
  const v=e.target.checked?'1':'0';
  localStorage.setItem('ack',v);
  fetch('/setack?val='+v);
});
on('retryNInput','change',e=>{localStorage.setItem('retryN',e.target.value);sendParam('setretryn',e.target.value);});
on('retryMSInput','change',e=>{localStorage.setItem('retryMS',e.target.value);sendParam('setretryms',e.target.value);});
// Выбор режима FEC
on('fecSelect','change',e=>{localStorage.setItem('fec',e.target.value);sendParam('setfec',e.target.value);});
// Глубина интерливера
on('interSelect','change',e=>{localStorage.setItem('inter',e.target.value);sendParam('setinter',e.target.value);});
on('payloadInput','change',e=>{localStorage.setItem('payload',e.target.value);sendParam('setpayload',e.target.value);});
on('pilotInput','change',e=>{localStorage.setItem('pilot',e.target.value);sendParam('setpilot',e.target.value);});
on('dupChk','change',e=>{const v=e.target.checked?'1':'0';localStorage.setItem('dup',v);fetch('/setdup?val='+v);});
on('winInput','change',e=>{localStorage.setItem('win',e.target.value);sendParam('setwin',e.target.value);});
on('ackAggInput','change',e=>{localStorage.setItem('ackAgg',e.target.value);sendParam('setackagg',e.target.value);});
on('encChk','change',e=>{localStorage.setItem('enc',e.target.checked?'1':'0');fetch('/toggleenc');});
on('kidInput','change',e=>{localStorage.setItem('kid',e.target.value);sendParam('setkid',e.target.value);});

on('keyBtn','click',()=>{const k=document.getElementById('keyInput').value;fetch('/setkey?val='+encodeURIComponent(k));});
on('saveBtn','click',()=>{fetch('/save');});
on('loadBtn','click',()=>{fetch('/load');});
on('resetBtn','click',()=>{fetch('/reset');});
on('profileSelect','change',e=>{localStorage.setItem('profile',e.target.value);applyProfile(e.target.value);});

const themeToggle=document.getElementById('themeToggle');
const fontRange=document.getElementById('fontRange');
function applyPrefs(){const th=localStorage.getItem('theme')||'dark';const fs=localStorage.getItem('font')||'16';document.body.classList.toggle('light',th==='light');themeToggle.checked=th==='light';document.documentElement.style.setProperty('--base-font',fs+'px');fontRange.value=fs;}
applyPrefs();
themeToggle.addEventListener('change',()=>{const light=themeToggle.checked;document.body.classList.toggle('light',light);localStorage.setItem('theme',light?'light':'dark');});
fontRange.addEventListener('input',()=>{const v=fontRange.value;document.documentElement.style.setProperty('--base-font',v+'px');localStorage.setItem('font',v);});

function applySettings(){
  const m=[
    ['profileSelect','profile'],['bankSelect','bank'],['presetSelect','preset'],
      ['bwSelect','bw'],['sfSelect','sf'],['crSelect','cr'],['txpSelect','txp'],
      ['retryNInput','retryN'],['retryMSInput','retryMS'],['kidInput','kid'],
      ['fecSelect','fec'],['interSelect','inter'],['payloadInput','payload'],
      ['pilotInput','pilot'],['winInput','win'],['ackAggInput','ackAgg']
    ];
  m.forEach(([id,key])=>{
    const el=document.getElementById(id);
    const val=localStorage.getItem(key);
    if(el&&val!==null) el.value=val;
  });
    document.getElementById('ackChk').checked=localStorage.getItem('ack')==='1';
    document.getElementById('encChk').checked=localStorage.getItem('enc')==='1';
    document.getElementById('dupChk').checked=localStorage.getItem('dup')==='1';
  }
applySettings();

on('pingBtn','click',()=>{fetch('/ping');});
// Запуск расширенного SatPing с параметрами
on('satRunBtn','click',()=>{
  const p=new URLSearchParams();
  const c=document.getElementById('satCount').value;if(c)p.append('count',c);
  const i=document.getElementById('satInterval').value;if(i)p.append('interval',i);
  const s=document.getElementById('satSize').value;if(s)p.append('size',s);
  const f=document.getElementById('satFec').value;if(f)p.append('fec',f);
  const r=document.getElementById('satRetries').value;if(r)p.append('retries',r);
  fetch('/satping?'+p.toString()).then(r=>r.json()).then(d=>{
    document.getElementById('satPingResult').textContent='sent:'+d.sent+' recv:'+d.received+' timeout:'+d.timeout;
  });
});
on('metricsBtn','click',()=>{fetch('/metrics').then(r=>r.text()).then(t=>{document.getElementById('metrics').textContent=t;});});
on('selfTestBtn','click',()=>{fetch('/selftest');});

on('simpleBtn','click',()=>{const st=document.getElementById('simpleStatus');fetch('/simple').then(r=>{st.textContent=r.ok?'\u2714':'\u2716';st.style.color=r.ok?'var(--sys-color)':'red';}).catch(()=>{st.textContent='\u2716';st.style.color='red';});setTimeout(()=>{st.textContent=''},2000);});
on('largeBtn','click',()=>{const n=document.getElementById('largeSize').value;const st=document.getElementById('largeStatus');fetch('/large?size='+encodeURIComponent(n)).then(r=>{st.textContent=r.ok?'\u2714':'\u2716';st.style.color=r.ok?'var(--sys-color)':'red';}).catch(()=>{st.textContent='\u2716';st.style.color='red';});setTimeout(()=>{st.textContent=''},2000);});
on('encTestBtn','click',()=>{const n=document.getElementById('encTestSize').value;let url='/enctest';if(n)url+='?size='+encodeURIComponent(n);fetch(url);});
on('encTestBadBtn','click',()=>{fetch('/enctestbad');});
on('msgIdBtn','click',()=>{const n=document.getElementById('msgIdVal').value;let url='/msgid';if(n)url+='?val='+encodeURIComponent(n);fetch(url);});

const keyTestBtn=document.getElementById('keyTestBtn');if(keyTestBtn){on('keyTestBtn','click',()=>{fetch('/keytest');});}
const keyReqBtn=document.getElementById('keyReqBtn');if(keyReqBtn){on('keyReqBtn','click',()=>{fetch('/keyreq');});}
const keySendBtn=document.getElementById('keySendBtn');if(keySendBtn){on('keySendBtn','click',()=>{fetch('/keysend');});}
const keyDhBtn=document.getElementById('keyDhBtn');if(keyDhBtn){on('keyDhBtn','click',()=>{fetch('/keydh');});}
// Кнопка отображения хеша ключа с обработкой всех вариантов ответа
const keyHashBtn=document.getElementById('keyHashBtn');
if(keyHashBtn){
  on('keyHashBtn','click',()=>{
    fetch('/keyhash').then(r=>{
      if(r.status===200){
        return r.text().then(t=>{alert('Hash: '+t.trim());});
      }else if(r.status===404){
        return r.text().then(t=>{alert('Ключ не задан');});
      }else{
        alert('Нет связи с устройством');
      }
    }).catch(()=>{alert('Нет связи с устройством');});
  });
}

function updateKeyStatus(){
  fetch('/keystatus').then(r=>r.json()).then(d=>{
    const i=document.getElementById('keyIndicator');
    const t=document.getElementById('keyStatusText');
    if(!i||!t)return;
    const h=d.hash||'----';
    if(d.status==='local'){
      i.classList.remove('remote');i.classList.add('local');t.textContent='Local '+h;
    }else{
      i.classList.remove('local');i.classList.add('remote');t.textContent='Remote '+h;
    }
    i.classList.toggle('blink',Number(d.request)===1);
  }).catch(()=>{});
  setTimeout(updateKeyStatus,1000);
}
updateKeyStatus();

on('sendQBtn','click',()=>{const msg=document.getElementById('sendQMsg').value;const prio=document.getElementById('sendQPrio').value;if(msg){fetch('/sendq?prio='+encodeURIComponent(prio)+'&msg='+encodeURIComponent(msg));document.getElementById('sendQMsg').value='';}});
on('largeQBtn','click',()=>{const sz=document.getElementById('largeQSize').value;const prio=document.getElementById('largeQPrio').value;fetch('/largeq?prio='+encodeURIComponent(prio)+'&size='+encodeURIComponent(sz));});
on('qosModeBtn','click',()=>{const mode=document.getElementById('qosModeSelect').value;fetch('/qosmode?val='+encodeURIComponent(mode));});
on('qosBtn','click',()=>{fetch('/qos').then(r=>r.text()).then(t=>{document.getElementById('qosStats').textContent=t;});});
)rawliteral";

