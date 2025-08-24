// Простая IDE: переключение панелей и обмен с агентом Codex

// Идентификаторы панелей в порядке следования
const panes = ['pane-prompt','pane-editor','pane-result'];

// Активация панели и обновление степпера
function activate(id){
  panes.forEach(x=>document.getElementById(x).dataset.active = (x===id? '1':'0'));
  document.querySelectorAll('.stepper a').forEach(a=>a.setAttribute('aria-selected', a.dataset.pane===id));
  history.replaceState(null, '', '#'+id.replace('pane-',''));
}

// Инициализация состояния по хэшу URL
activate('pane-' + (location.hash.slice(1)||'prompt'));
document.querySelectorAll('.stepper a').forEach(a=>a.addEventListener('click', e=>{
  e.preventDefault(); activate(a.dataset.pane);
}));

// Получение текущего текста из редактора или textarea
function getCurrentCode(){
  const ta = document.getElementById('editorTA');
  if (ta) return ta.value;
  return window.monacoEditor?.getValue() || '';
}

// Элемент для вывода потока и буфер вставки
const streamEl = document.getElementById('stream');
let pending = '';
// Добавление текста с use requestAnimationFrame для плавности
function appendText(txt){
  pending += txt;
  requestAnimationFrame(()=>{
    streamEl.insertAdjacentText('beforeend', pending);
    pending = '';
  });
}

// Построчный LCS-дифф
function diffLines(a,b){
  const A=a.split('\n'), B=b.split('\n');
  const n=A.length, m=B.length;
  const dp=Array(n+1).fill(0).map(()=>Array(m+1).fill(0));
  for(let i=n-1;i>=0;--i) for(let j=m-1;j>=0;--j)
    dp[i][j]=A[i]===B[j]?dp[i+1][j+1]+1:Math.max(dp[i+1][j],dp[i][j+1]);
  const out=[];
  let i=0,j=0;
  while(i<n && j<m){
    if(A[i]===B[j]){ out.push(['eq',A[i++]]); j++; }
    else if(dp[i+1][j]>=dp[i][j+1]) out.push(['del',A[i++]]);
    else out.push(['ins',B[j++]]);
  }
  while(i<n) out.push(['del',A[i++]]);
  while(j<m) out.push(['ins',B[j++]]);
  return out;
}
function renderDiff(el, a, b){
  el.innerHTML='';
  for(const [t,line] of diffLines(a,b)){
    const div=document.createElement('div');
    div.className = t==='ins'?'ins': t==='del'?'del':'';
    div.textContent=line;
    el.appendChild(div);
  }
}

// Скелет API: отправка запроса Codex и приём токенов через SSE
async function sendCodex({prompt, code, mode, maxTokens, temp, signal, onDelta}){
  const res = await fetch('/api/codex/complete', {
    method:'POST',
    headers:{'Content-Type':'application/json'},
    body: JSON.stringify({prompt, code, mode, maxTokens, temperature: temp, stream:true}),
    signal
  });
  const reader = res.body.getReader();
  const dec = new TextDecoder();
  while(true){
    const {done, value} = await reader.read(); if(done) break;
    const chunk = dec.decode(value);
    onDelta?.(chunk);
  }
}

// Обработка генерации и остановки
let controller;
document.getElementById('send').addEventListener('click', async ()=>{
  const prompt = document.getElementById('prompt').value;
  const code = getCurrentCode();
  const maxTokens = parseInt(document.getElementById('maxTokens').value,10);
  const temp = parseFloat(document.getElementById('temp').value);
  streamEl.textContent='';
  let result = '';
  controller = new AbortController();
  await sendCodex({prompt, code, mode:'complete', maxTokens, temp, signal: controller.signal, onDelta:t=>{result+=t; appendText(t);}});
  renderDiff(document.getElementById('diff'), code, result);
});

// Остановка получения потока
document.getElementById('stopRun').addEventListener('click', ()=>controller?.abort());

// Копирование всего текста из редактора
document.getElementById('copyAll').onclick = async ()=>{
  const code = getCurrentCode();
  await navigator.clipboard.writeText(code);
};

// Отправка текста из поля prompt по радио через API
document.getElementById('sendRadio').addEventListener('click', async ()=>{
  const msg = document.getElementById('prompt').value;            // берём текст
  const res = await fetch('/api/tx', {                           // POST-запрос к устройству
    method: 'POST',
    headers: { 'Content-Type': 'text/plain' },
    body: msg
  });
  const txt = await res.text();                                  // ответ сервера
  appendText('\n[' + txt + ']\n');                              // выводим в поток
});
