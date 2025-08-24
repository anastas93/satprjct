// Основная логика веб-интерфейса
// Комментарии на русском языке

document.addEventListener('DOMContentLoaded', () => {
  // Получаем ключевые элементы
  const navLinks = document.querySelectorAll('.nav a');
  const tabs = document.querySelectorAll('.tab');
  const root = document.documentElement;
  const themeToggle = document.getElementById('themeToggle');
  const menuToggle = document.getElementById('menuToggle');
  const siteNav = document.getElementById('siteNav');
  const chatLog = document.getElementById('chatLog');
  const chatInput = document.getElementById('chatInput');
  const sendBtn = document.getElementById('sendBtn');
  const endpointInput = document.getElementById('endpoint');
  const toast = document.getElementById('toast');

  // ----- Вкладки -----
  function switchTab(name) {
    // Переключение видимости секций по клику в меню
    tabs.forEach(t => t.hidden = true);
    document.getElementById(`tab-${name}`).hidden = false;
  }
  navLinks.forEach(link => link.addEventListener('click', e => {
    e.preventDefault();
    switchTab(link.dataset.tab);
  }));

  // ----- Тема -----
  const savedTheme = localStorage.getItem('theme');
  if (savedTheme === 'light') root.classList.add('light');
  themeToggle.addEventListener('click', () => {
    // Переключаем класс на корневом элементе и сохраняем выбор
    root.classList.toggle('light');
    localStorage.setItem('theme', root.classList.contains('light') ? 'light' : 'dark');
  });

  // ----- Мобильное меню -----
  menuToggle.addEventListener('click', () => {
    siteNav.classList.toggle('open');
  });

  // ----- Чат -----
  function appendMessage(text, self = false) {
    // Добавляет сообщение в журнал чата
    const msg = document.createElement('div');
    msg.className = `msg${self ? ' you' : ' dev'}`;
    const avatar = document.createElement('div');
    avatar.className = 'avatar';
    avatar.textContent = self ? 'YOU' : 'DEV';
    const bubble = document.createElement('div');
    bubble.className = 'bubble';
    bubble.textContent = text;
    const time = document.createElement('time');
    time.textContent = new Date().toLocaleTimeString();
    msg.appendChild(avatar);
    msg.appendChild(bubble);
    msg.appendChild(time);
    chatLog.appendChild(msg);
    chatLog.scrollTop = chatLog.scrollHeight;
  }

  function sendText(text) {
    // Отправляет текст на выбранный endpoint и добавляет в журнал
    const endpoint = endpointInput.value.trim();
    if (endpoint) {
      fetch(`${endpoint}/api/tx`, {
        method: 'POST',
        body: text
      }).catch(() => showToast('Ошибка отправки'));
    }
  }

  sendBtn.addEventListener('click', () => {
    const text = chatInput.value.trim();
    if (!text) return;
    appendMessage(text, true);
    sendText(text);
    chatInput.value = '';
  });
  chatInput.addEventListener('keydown', e => {
    if (e.key === 'Enter') sendBtn.click();
  });

  // Кнопки команд
  document.querySelectorAll('[data-cmd]').forEach(btn => {
    btn.addEventListener('click', () => {
      const cmd = btn.dataset.cmd;
      appendMessage(cmd, true);
      sendText(cmd);
    });
  });

  // ----- Настройки -----
  const settingsIds = ['ACK','BANK','BF','CH','CR','PW','SF','STS'];
  function loadSettings() {
    // Загружает сохранённые настройки из localStorage
    const data = JSON.parse(localStorage.getItem('settings') || '{}');
    settingsIds.forEach(id => {
      const el = document.getElementById(id);
      if (!el) return;
      if (el.type === 'checkbox') el.checked = data[id] || false;
      else if (data[id] !== undefined) el.value = data[id];
    });
    return data;
  }
  function gatherSettings() {
    // Собирает значения формы настроек
    const out = {};
    settingsIds.forEach(id => {
      const el = document.getElementById(id);
      if (el.type === 'checkbox') out[id] = el.checked;
      else out[id] = el.value;
    });
    return out;
  }
  function saveSettings() {
    // Сохраняет настройки в localStorage
    localStorage.setItem('settings', JSON.stringify(gatherSettings()));
    showToast('Сохранено');
  }
  document.getElementById('btnSaveSettings').addEventListener('click', saveSettings);
  document.getElementById('btnApplySettings').addEventListener('click', () => {
    const cfg = gatherSettings();
    saveSettings();
    sendText('CONF ' + JSON.stringify(cfg));
  });
  document.getElementById('btnClearCache').addEventListener('click', () => {
    localStorage.clear();
    loadSettings();
    showToast('Кэш очищен');
  });
  document.getElementById('btnExportSettings').addEventListener('click', () => {
    const blob = new Blob([JSON.stringify(gatherSettings(), null, 2)], {type:'application/json'});
    const a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = 'settings.json';
    a.click();
    URL.revokeObjectURL(a.href);
  });
  document.getElementById('btnImportSettings').addEventListener('click', () => {
    const input = document.createElement('input');
    input.type = 'file';
    input.accept = 'application/json';
    input.onchange = e => {
      const file = e.target.files[0];
      if (!file) return;
      file.text().then(txt => {
        try {
          const cfg = JSON.parse(txt);
          settingsIds.forEach(id => {
            if (cfg[id] !== undefined) {
              const el = document.getElementById(id);
              if (el.type === 'checkbox') el.checked = cfg[id];
              else el.value = cfg[id];
            }
          });
          saveSettings();
          showToast('Импортировано');
        } catch {
          showToast('Ошибка импорта');
        }
      });
    };
    input.click();
  });
  loadSettings();

  // ----- Безопасность -----
  const keyState = document.getElementById('keyState');
  const keyHash = document.getElementById('keyHash');
  const keyHex = document.getElementById('keyHex');
  function updateKeyInfo() {
    // Обновляет отображение текущего ключа
    const hex = localStorage.getItem('secKey');
    if (hex) {
      keyState.textContent = 'LOCAL';
      keyHash.textContent = hex.substring(0,8);
      keyHex.textContent = hex;
    } else {
      keyState.textContent = 'DEFAULT';
      keyHash.textContent = '';
      keyHex.textContent = '';
    }
  }
  document.getElementById('btnKeyGen').addEventListener('click', () => {
    const arr = new Uint8Array(16);
    crypto.getRandomValues(arr);
    const hex = Array.from(arr).map(b => b.toString(16).padStart(2,'0')).join('');
    localStorage.setItem('secKey', hex);
    updateKeyInfo();
    showToast('Ключ создан');
  });
  document.getElementById('btnKeySend').addEventListener('click', () => {
    showToast('Передача не реализована');
  });
  document.getElementById('btnKeyRecv').addEventListener('click', () => {
    showToast('Приём не реализован');
  });
  updateKeyInfo();

  // ----- Служебные -----
  function showToast(text) {
    // Показ небольшого уведомления
    toast.textContent = text;
    toast.hidden = false;
    setTimeout(() => { toast.hidden = true; }, 2000);
  }
});

