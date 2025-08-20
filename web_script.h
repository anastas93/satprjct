// Скрипт веб-интерфейса
#pragma once
// Сохранение настроек и истории чата, статус Simple/Large, автоматическое отображение хеша ключа
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
    else if(tag==='SYS' && /^F(REQ|RX|TX)/.test(txt)){cls='freq';} // сообщения об изменении частоты
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
// Мигает выбранный индикатор активности TX/RX (до 1 секунды после события)
const blinkTimers={};
function blinkIndicator(id){const e=document.getElementById(id);if(!e)return;e.classList.add('active');clearTimeout(blinkTimers[id]);blinkTimers[id]=setTimeout(()=>e.classList.remove('active'),1000);}
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
// ранее отсутствовал знак '=' при объявлении ebn0Data, что ломало скрипт
const perData = [], rttData = [], ebn0Data = [];
// последние значения счётчиков TX/RX для индикации активности
let lastTx=0,lastRx=0;
function drawGraph(id,data,color){
  const c=document.getElementById(id);if(!c)return;const ctx=c.getContext('2d');
  ctx.clearRect(0,0,c.width,c.height);if(data.length<2)return;
  const max=Math.max(...data),min=Math.min(...data),k=max-min||1;
  ctx.beginPath();data.forEach((v,i)=>{const x=i*(c.width/(data.length-1));const y=c.height-((v-min)/k*c.height);i?ctx.lineTo(x,y):ctx.moveTo(x,y);});
  ctx.strokeStyle=color;ctx.stroke();
}
function updateLinkDiag(){ // QOS: Низкий Получение диагностических данных
  fetch('/linkdiag').then(r=>r.json()).then(d=>{
    perData.push(d.per);if(perData.length>50)perData.shift();
    rttData.push(d.rtt);if(rttData.length>50)rttData.shift();
    ebn0Data.push(d.ebn0);if(ebn0Data.length>50)ebn0Data.shift();
    drawGraph('perGraph',perData,'#66bb6a');
    drawGraph('rttGraph',rttData,'#3f51b5');
    drawGraph('ebn0Graph',ebn0Data,'#ffb74d');
    const lp=document.getElementById('linkProfile');if(lp)lp.textContent='Профиль: '+d.profile;
    const ab=document.getElementById('ackBitmap');if(ab)ab.textContent='Bitmap долгов: '+d.bitmap;
    const qc=document.getElementById('queueCounter');if(qc)qc.textContent='Q: '+d.queue;
    const txf=Number(d.tx_frames); // счётчик передач
    if(!isNaN(txf) && txf>lastTx){blinkIndicator('txIndicator');lastTx=txf;}
    const rxf=Number(d.rx_frames); // счётчик приёмов
    if(!isNaN(rxf) && rxf>lastRx){blinkIndicator('rxIndicator');lastRx=rxf;}
  }).catch(()=>{});
}
setInterval(updateLinkDiag,1000);
// Отдельный WebSocket только для чата, запросы веб‑меню идут по HTTP и не блокируют его
let chatWs;
let wsDelay=1000; // начальная задержка между попытками (мс)
function connectChat(){
  chatWs=new WebSocket('ws://'+location.hostname+':81/');
  chatWs.onmessage=e=>{
    const t=e.data.trim();
    if(!t)return;
    if(t[0]==='{'){
      // Обработка JSON‑событий от сервера
      try{
        const m=JSON.parse(t);
        if(m.type==='key_changed'){
          const kid=Number(m.kid||0).toString(16).padStart(2,'0').toUpperCase();
          const crc=Number(m.key_crc16||0).toString(16).padStart(4,'0').toUpperCase();
          const el=document.getElementById('keyStatusText');
          if(el)el.textContent='KID:0x'+kid+' • CRC:0x'+crc;
        }else if(m.type==='rx_start'){
          const e=document.getElementById('rxIndicator');if(e)e.classList.add('active');
        }else if(m.type==='rx_done'){
          const e=document.getElementById('rxIndicator');if(e)setTimeout(()=>e.classList.remove('active'),180);
        }else if(m.type==='tx_start'){
          const e=document.getElementById('txIndicator');if(e)e.classList.add('active');
        }else if(m.type==='tx_done'){
          const e=document.getElementById('txIndicator');if(e)setTimeout(()=>e.classList.remove('active'),180);
        }else if(m.type==='metrics'){
          // обновляем блок метрик без опроса сервера
          const el=document.getElementById('metrics');
          if(el)el.textContent=`PER:${(m.per||0).toFixed(3)} RTT:${(m.rtt_ms||0).toFixed(1)}ms EbN0:${(m.ebn0||0).toFixed(2)}dB`;
        }
      }catch(e){}
      return;
    }
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
  chatWs.onclose=()=>{ // при разрыве соединения переподключаемся с ростом задержки
    setTimeout(connectChat,wsDelay);
    wsDelay=Math.min(wsDelay*2,10000);
  };
  chatWs.onopen=()=>{wsDelay=1000;};
}
connectChat();
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
function sendParam(path,val){fetch('/'+path+'?val='+encodeURIComponent(val));} // QOS: Нормальный Настройка параметров радио
function sendPerTh(){const hi=document.getElementById('perHigh').value;const lo=document.getElementById('perLow').value;fetch('/setperth?hi='+encodeURIComponent(hi)+'&lo='+encodeURIComponent(lo));} // QOS: Нормальный Установка порогов PER
function sendEbn0Th(){const hi=document.getElementById('ebn0High').value;const lo=document.getElementById('ebn0Low').value;fetch('/setebn0th?hi='+encodeURIComponent(hi)+'&lo='+encodeURIComponent(lo));} // QOS: Нормальный Установка порогов Eb/N0
// Безопасно добавляет обработчики событий и пропускает отсутствующие элементы
function on(id,ev,fn){const el=document.getElementById(id);if(el)el.addEventListener(ev,fn);}
// Обработчик отправки сообщения: WebSocket для чата + резервный HTTP
on('sendBtn','click',e=>{e.preventDefault(); // предотвращаем перезагрузку страницы
  const m=document.getElementById('msg').value;
  const st=document.getElementById('sendStatus');
  const btn=document.getElementById('sendBtn');
  if(m){
    // мгновенно показываем в чате
    appendChat('*TX:* '+m);
    btn.disabled=true;
    st.textContent='...';
    let sendPromise;
    if(chatWs && chatWs.readyState===WebSocket.OPEN){
      try{chatWs.send(m);sendPromise=Promise.resolve({ok:true});}
      catch(e){sendPromise=Promise.reject(e);}
    }else{
      sendPromise=fetch('/send?msg='+encodeURIComponent(m));
    }
    sendPromise.then(r=>{
      if(r.ok){st.textContent='\u2714';st.style.color='var(--sys-color)';}
      else{st.textContent='\u2716';st.style.color='red';}
    }).catch(()=>{st.textContent='\u2716';st.style.color='red';})
      .finally(()=>{btn.disabled=false;});
    document.getElementById('msg').value='';
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
on('rxBoostChk','change',e=>{const v=e.target.checked?'1':'0';localStorage.setItem('rxboost',v);fetch('/setrxboost?val='+v);}); // QOS: Нормальный Переключение усиления приёмника
// Явная установка ACK вместо неопределённого переключения
on('ackChk','change',e=>{ // QOS: Нормальный Включение/отключение ACK
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
// Переключатели на вкладке Link Diagnostics
on('diagFecSelect','change',e=>{localStorage.setItem('fec',e.target.value);sendParam('setfec',e.target.value);setSelect('fecSelect',e.target.value);});
on('diagInterSelect','change',e=>{localStorage.setItem('inter',e.target.value);sendParam('setinter',e.target.value);setSelect('interSelect',e.target.value);});
on('diagFragInput','change',e=>{localStorage.setItem('payload',e.target.value);sendParam('setpayload',e.target.value);const p=document.getElementById('payloadInput');if(p)p.value=e.target.value;});
on('pilotInput','change',e=>{localStorage.setItem('pilot',e.target.value);sendParam('setpilot',e.target.value);});
on('dupChk','change',e=>{const v=e.target.checked?'1':'0';localStorage.setItem('dup',v);fetch('/setdup?val='+v);}); // QOS: Нормальный Переключение режима дублирования
on('winInput','change',e=>{localStorage.setItem('win',e.target.value);sendParam('setwin',e.target.value);});
on('ackAggInput','change',e=>{localStorage.setItem('ackAgg',e.target.value);sendParam('setackagg',e.target.value);});
on('burstInput','change',e=>{localStorage.setItem('burst',e.target.value);sendParam('setburst',e.target.value);});
on('ackJitterInput','change',e=>{localStorage.setItem('ackJitter',e.target.value);sendParam('setackjitter',e.target.value);});
on('backoffInput','change',e=>{localStorage.setItem('backoff',e.target.value);sendParam('setbackoff',e.target.value);});
on('perHigh','change',e=>{localStorage.setItem('perHigh',e.target.value);sendPerTh();});
on('perLow','change',e=>{localStorage.setItem('perLow',e.target.value);sendPerTh();});
on('ebn0High','change',e=>{localStorage.setItem('ebn0High',e.target.value);sendEbn0Th();});
on('ebn0Low','change',e=>{localStorage.setItem('ebn0Low',e.target.value);sendEbn0Th();});
on('txProfMode','change',e=>{const manual=e.target.value==='manual';localStorage.setItem('txProfMode',e.target.value);const sel=document.getElementById('txProfSelect');if(sel)sel.disabled=!manual;fetch('/setautorate?val='+(manual?'0':'1'));});
on('txProfSelect','change',e=>{localStorage.setItem('txProfile',e.target.value);fetch('/settxprofile?val='+e.target.value);});
on('encChk','change',e=>{localStorage.setItem('enc',e.target.checked?'1':'0');fetch('/toggleenc');}); // QOS: Нормальный Переключение шифрования
on('kidInput','change',e=>{localStorage.setItem('kid',e.target.value);sendParam('setkid',e.target.value);}); // QOS: Высокий Установка идентификатора ключа
on('keyBtn','click',()=>{const k=document.getElementById('keyInput').value;fetch('/setkey?val='+encodeURIComponent(k));}); // QOS: Высокий Установка ключа шифрования
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
      ['pilotInput','pilot'],['winInput','win'],['ackAggInput','ackAgg'],
      ['burstInput','burst'],['ackJitterInput','ackJitter'],['backoffInput','backoff'],
      ['perHigh','perHigh'],['perLow','perLow'],['ebn0High','ebn0High'],['ebn0Low','ebn0Low'],
      ['tddTxInput','tddTx'],['tddAckInput','tddAck'],['tddGuardInput','tddGuard'],
      ['diagFecSelect','fec'],['diagInterSelect','inter'],['diagFragInput','payload'],
      ['txProfMode','txProfMode'],['txProfSelect','txProfile']
    ];
  m.forEach(([id,key])=>{
    const el=document.getElementById(id);
    const val=localStorage.getItem(key);
    if(el&&val!==null) el.value=val;
  });
    document.getElementById('ackChk').checked=localStorage.getItem('ack')==='1';
    document.getElementById('encChk').checked=localStorage.getItem('enc')==='1';
    document.getElementById('dupChk').checked=localStorage.getItem('dup')==='1';
    document.getElementById('rxBoostChk').checked=localStorage.getItem('rxboost')==='1';
    const mode=localStorage.getItem('txProfMode')||'auto';
    const sel=document.getElementById('txProfSelect');
    if(sel)sel.disabled=mode!=='manual';
  }
applySettings();
const initMode=localStorage.getItem('txProfMode')||'auto';
fetch('/setautorate?val='+(initMode==='manual'?'0':'1'));
if(initMode==='manual'){const p=localStorage.getItem('txProfile')||'0';fetch('/settxprofile?val='+p);}
on('pingBtn','click',()=>{fetch('/ping');}); // QOS: Низкий Диагностический ping
// Дополнительные режимы пинга
on('chanPingBtn','click',()=>{fetch('/channelping');}); // QOS: Низкий Диагностический ping канала
on('presetPingBtn','click',()=>{ // QOS: Низкий Диагностический ping пресета
  const bank=document.getElementById('bankSelect').value;
  const preset=document.getElementById('presetSelect').value;
  fetch('/presetping?bank='+encodeURIComponent(bank)+'&preset='+encodeURIComponent(preset));
});
on('massPingBtn','click',()=>{ // QOS: Низкий Массовый ping
  const bank=document.getElementById('bankSelect').value;
  fetch('/massping?bank='+encodeURIComponent(bank));
});
// Запуск расширенного SatPing с параметрами
on('satRunBtn','click',()=>{ // QOS: Низкий Расширенный SatPing
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
on('metricsBtn','click',()=>{fetch('/metrics').then(r=>r.text()).then(t=>{document.getElementById('metrics').textContent=t;});}); // QOS: Низкий Получение метрик
on('selfTestBtn','click',()=>{fetch('/selftest');}); // QOS: Низкий Тестовый запрос selftest
on('simpleBtn','click',()=>{const st=document.getElementById('simpleStatus');fetch('/simple').then(r=>{st.textContent=r.ok?'\u2714':'\u2716';st.style.color=r.ok?'var(--sys-color)':'red';}).catch(()=>{st.textContent='\u2716';st.style.color='red';});setTimeout(()=>{st.textContent=''},2000);}); // QOS: Низкий Тестовый запрос simple
on('largeBtn','click',()=>{const n=document.getElementById('largeSize').value;const st=document.getElementById('largeStatus');fetch('/large?size='+encodeURIComponent(n)).then(r=>{st.textContent=r.ok?'\u2714':'\u2716';st.style.color=r.ok?'var(--sys-color)':'red';}).catch(()=>{st.textContent='\u2716';st.style.color='red';});setTimeout(()=>{st.textContent=''},2000);}); // QOS: Нормальный Отправка большого сообщения
on('encTestBtn','click',()=>{const n=document.getElementById('encTestSize').value;let url='/enctest';if(n)url+='?size='+encodeURIComponent(n);fetch(url);}); // QOS: Низкий Тест шифрования
on('encTestBadBtn','click',()=>{fetch('/enctestbad');}); // QOS: Низкий Негативный тест шифрования
on('msgIdBtn','click',()=>{const n=document.getElementById('msgIdVal').value;let url='/msgid';if(n)url+='?val='+encodeURIComponent(n);fetch(url);}); // QOS: Низкий Управление счётчиком сообщений
const keyTestBtn=document.getElementById('keyTestBtn');if(keyTestBtn){on('keyTestBtn','click',()=>{fetch('/keytest');});} // QOS: Высокий Тест ключей
const keyReqBtn=document.getElementById('keyReqBtn');if(keyReqBtn){on('keyReqBtn','click',()=>{fetch('/keyreq');});} // QOS: Высокий Запрос ключа
const keySendBtn=document.getElementById('keySendBtn');if(keySendBtn){on('keySendBtn','click',()=>{fetch('/keysend');});} // QOS: Высокий Отправка ключа
const keyDhBtn=document.getElementById('keyDhBtn');if(keyDhBtn){on('keyDhBtn','click',()=>{fetch('/keydh');});} // QOS: Высокий Диффи-Хеллман
// Обновление информации о ключе: статус локальный/удалённый и отображение KID+CRC
function updateKeyStatus(){ // QOS: Высокий Получение статуса ключа
  fetch('/keystatus').then(r=>r.json()).then(d=>{
    const i=document.getElementById('keyIndicator');
    const t=document.getElementById('keyStatusText');
    if(!i||!t)return;
    const kid=Number(d.kid||0).toString(16).padStart(2,'0').toUpperCase();
    const crc=Number(d.key_crc16||0).toString(16).padStart(4,'0').toUpperCase();
    t.textContent='KID:0x'+kid+' • CRC:0x'+crc;
    if(d.status==='local'){
      i.classList.remove('remote');i.classList.add('local');
    }else{
      i.classList.remove('local');i.classList.add('remote');
    }
    i.classList.toggle('active',Number(d.request)===1);
  }).catch(()=>{});
  setTimeout(updateKeyStatus,1000);
}
updateKeyStatus();
on('sendQBtn','click',()=>{const msg=document.getElementById('sendQMsg').value;const prio=document.getElementById('sendQPrio').value;if(msg){fetch('/sendq?prio='+encodeURIComponent(prio)+'&msg='+encodeURIComponent(msg));document.getElementById('sendQMsg').value='';}}); // QOS: Зависит Сообщение с явным приоритетом
on('largeQBtn','click',()=>{const sz=document.getElementById('largeQSize').value;const prio=document.getElementById('largeQPrio').value;fetch('/largeq?prio='+encodeURIComponent(prio)+'&size='+encodeURIComponent(sz));}); // QOS: Зависит Отправка большого сообщения с явным QoS
on('qosModeBtn','click',()=>{const mode=document.getElementById('qosModeSelect').value;fetch('/qosmode?val='+encodeURIComponent(mode));}); // QOS: Нормальный Управление режимом QoS
on('qosBtn','click',()=>{fetch('/qos').then(r=>r.text()).then(t=>{document.getElementById('qosStats').textContent=t;});}); // QOS: Нормальный Статистика QoS
// Настройка TDD-планировщика
on('tddApplyBtn','click',()=>{ // QOS: Нормальный Настройка TDD-планировщика
  const tx=document.getElementById('tddTxInput').value;
  const ack=document.getElementById('tddAckInput').value;
  const g=document.getElementById('tddGuardInput').value;
  localStorage.setItem('tddTx',tx);
  localStorage.setItem('tddAck',ack);
  localStorage.setItem('tddGuard',g);
  fetch('/tdd?tx='+encodeURIComponent(tx)+'&ack='+encodeURIComponent(ack)+'&guard='+encodeURIComponent(g));
});
// Разработческие функции безопасности
on('uploadKeyBtn','click',()=>{
  const f=document.getElementById('devKeyFile').files[0];
  if(!f)return;const r=new FileReader();
  r.onload=e=>{fetch('/setkey?val='+encodeURIComponent(e.target.result.trim()));};
  r.readAsText(f);
});
on('kidActBtn','click',()=>{const k=document.getElementById('kidActInput').value;fetch('/setkid?val='+encodeURIComponent(k));}); // QOS: Высокий Активация ключевого идентификатора
on('idResetBtn','click',()=>{fetch('/idreset');}); // QOS: Высокий Сброс счётчиков идентификаторов
on('replayClrBtn','click',()=>{fetch('/replayclr');}); // QOS: Высокий Очистка счётчиков реплеев
// Работа с архивом сообщений
on('archListBtn','click',()=>{fetch('/archivelist').then(r=>r.text()).then(t=>{document.getElementById('archiveList').textContent=t;});}); // QOS: Нормальный Список архива сообщений
on('archRestoreBtn','click',()=>{fetch('/archiverestore');}); // QOS: Нормальный Восстановление архива сообщений
// Отображение последних кадров
function loadFrames(){ // QOS: Низкий Получение последних кадров
  const dr=document.getElementById('frameDrop').value;
  let url='/frames';
  if(dr)url+='?drop='+encodeURIComponent(dr);
  fetch(url).then(r=>r.json()).then(data=>{
    const tbl=document.getElementById('frameTable');
    tbl.innerHTML='<tr><th>Dir</th><th>Seq</th><th>Len</th><th>FEC</th><th>Inter</th><th>SNR</th><th>RS</th><th>Vit</th><th>Drop</th><th>RTT</th></tr>'+
      data.map(e=>`<tr><td>${e.dir}</td><td>${e.seq}</td><td>${e.len}</td><td>${e.fec}</td><td>${e.inter}</td><td>${e.snr.toFixed(1)}</td><td>${e.rs}</td><td>${e.vit}</td><td>${e.drop}</td><td>${e.rtt}</td></tr>`).join('');
  });
}
on('frameRefreshBtn','click',loadFrames);
on('frameDrop','change',loadFrames);
// периодическое обновление таблицы кадров без перезагрузки страницы
setInterval(loadFrames,2000);
)rawliteral";
