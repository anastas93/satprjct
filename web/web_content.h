#pragma once
// Содержимое веб-интерфейса, встроенное в прошивку
// index.html
const char INDEX_HTML[] PROGMEM = R"~~~(
<!DOCTYPE html>
<html lang="ru">
<head>
  <meta charset="UTF-8" />
  <title>Sat Project Web</title>
  <link rel="stylesheet" href="style.css" />
</head>
<body>
  <!-- Ссылка для перехода к основному содержимому -->
  <a href="#main" class="sr-only focusable">К контенту</a>
  <!-- Панель навигации с тремя вкладками и отдельной ссылкой на безопасность -->
  <nav>
    <button class="tab-btn" data-tab="chat">Chat</button>
    <button class="tab-btn" data-tab="channels">Channels/Ping</button>
    <button class="tab-btn" data-tab="settings">Settings</button>
    <a href="#" id="security-link" data-tab="security">Security</a>
  </nav>
  <!-- Основное содержимое страницы -->
  <main id="main">
    <!-- Области содержимого для каждой вкладки -->
    <section id="chat" class="tab-content">
      <!-- Заголовок чата -->
      <h2>Chat</h2>
      <!-- Список сообщений -->
      <ul id="chat-messages"></ul>
      <!-- Поле ввода нового сообщения -->
      <input type="text" id="chat-input" placeholder="Введите сообщение" />
      <!-- Кнопка отправки текстового сообщения -->
      <button id="send-btn">Отправить</button>
      <!-- Блок кнопок команд -->
      <div id="cmd-buttons">
        <button class="cmd-btn" data-cmd="INFO">INFO</button>
        <button class="cmd-btn" data-cmd="STS">STS</button>
        <button class="cmd-btn" data-cmd="RSTS">RSTS</button>
      </div>
    </section>
    <section id="channels" class="tab-content">
      <!-- Вкладка отображения банка каналов и отправки пингов -->
      <h2>Channels / Ping</h2>
      <!-- Панель управления каналами -->
      <div id="channel-controls">
        <button id="ping-btn">Ping</button>
        <button id="search-btn">Search</button>
        <button id="export-btn">Выгрузить CSV</button>
      </div>
      <!-- Таблица активного банка каналов -->
      <table id="channel-table">
        <thead>
          <tr><th>Канал</th><th>Частота, МГц</th><th>Замеры приёма</th></tr>
        </thead>
        <tbody id="channel-table-body"></tbody>
      </table>
    </section>
    <section id="settings" class="tab-content">
      <!-- Здесь будут параметры настройки -->
      <h2>Settings</h2>
    </section>
    <section id="security" class="tab-content">
      <!-- Отдельная страница для параметров безопасности -->
      <h2>Security</h2>
      <!-- Отображение текущего состояния ключа -->
      <div id="key-info"></div>
      <!-- Кнопка генерации нового ключа -->
      <button id="keygen-btn">KEYGEN</button>
      <!-- Отправка текущего ключа на другое устройство -->
      <button id="keysend-btn">KEYTRANSFER SEND</button>
      <!-- Получение ключа от другого устройства -->
      <button id="keyrecv-btn">KEYTRANSFER RECEIVE</button>
    </section>
  </main>

  <!-- Подключение пользовательского скрипта -->
  <script src="app.js"></script>
</body>
</html>

)~~~";

// app.js
const char APP_JS[] PROGMEM = R"~~~(
// Простое переключение вкладок
document.addEventListener('DOMContentLoaded', () => {
  const buttons = document.querySelectorAll('.tab-btn'); // кнопки вкладок
  const sections = document.querySelectorAll('.tab-content'); // содержимое вкладок
  const securityLink = document.getElementById('security-link'); // отдельная ссылка безопасности

  // функция активации вкладки
  const activate = (id) => {
    sections.forEach(sec => sec.classList.remove('active')); // скрываем все разделы
    const target = document.getElementById(id);
    if (target) target.classList.add('active'); // показываем нужный
  };

  // обработка кликов по вкладкам
  buttons.forEach(btn => {
    btn.addEventListener('click', () => activate(btn.dataset.tab));
  });

  // обработка отдельной ссылки безопасности
  securityLink.addEventListener('click', (e) => {
    e.preventDefault();
    activate(securityLink.dataset.tab);
  });

  activate('chat'); // показываем чат по умолчанию

  // --- Логика чата ---
  const messageList = document.getElementById('chat-messages'); // список сообщений
  const input = document.getElementById('chat-input'); // поле ввода
  const sendBtn = document.getElementById('send-btn'); // кнопка отправки
  const cmdButtons = document.querySelectorAll('.cmd-btn'); // кнопки команд

  // загружаем сохранённые сообщения из localStorage
  let messages = JSON.parse(localStorage.getItem('chatMessages') || '[]');

  // функция сохранения сообщений
  const saveMessages = () => {
    localStorage.setItem('chatMessages', JSON.stringify(messages));
  };

  // функция отображения всех сообщений
  const renderMessages = () => {
    messageList.innerHTML = '';
    messages.forEach(msg => {
      const li = document.createElement('li');
      li.className = `message ${msg.type}`; // исходящее или входящее
      li.innerHTML = `<span class="time">${msg.time}</span> (<span>${msg.type === 'outgoing' ? 'Исходящее' : 'Входящее'}</span>) ${msg.text}`;
      messageList.appendChild(li);
    });
  };

  renderMessages(); // восстанавливаем историю при загрузке

  // добавление нового сообщения
  const addMessage = (text, type) => {
    const time = new Date().toLocaleTimeString();
    messages.push({ text, time, type });
    saveMessages();
    renderMessages();
  };

  // отправка текстового сообщения
  sendBtn.addEventListener('click', () => {
    const text = input.value.trim();
    if (text) {
      addMessage(text, 'outgoing');
      input.value = '';
    }
  });

  // обработка кнопок команд
    cmdButtons.forEach(btn => {
      btn.addEventListener('click', () => {
        const cmd = btn.dataset.cmd;
        addMessage(cmd, 'outgoing'); // отправка команды

      // имитация ответа от системы
      let response = '';
      switch (cmd) {
        case 'INFO':
          response = 'Информация о системе';
          break;
        case 'STS':
          response = 'Статус: OK';
          break;
        case 'RSTS':
          response = 'Расширенный статус: OK';
          break;
      }
      addMessage(response, 'incoming');
    });
  });

  // --- Логика таблицы каналов ---
  const tableBody = document.getElementById('channel-table-body'); // тело таблицы
  const pingBtn = document.getElementById('ping-btn'); // кнопка Ping
  const searchBtn = document.getElementById('search-btn'); // кнопка Search
  const exportBtn = document.getElementById('export-btn'); // кнопка выгрузки CSV

  // массив каналов активного банка с частотами
  const channels = [
    { channel: 1, freq: 433.05, status: '', measure: '' },
    { channel: 2, freq: 433.25, status: '', measure: '' },
    { channel: 3, freq: 433.45, status: '', measure: '' },
    { channel: 4, freq: 433.65, status: '', measure: '' },
    { channel: 5, freq: 433.85, status: '', measure: '' }
  ];

  let selectedIndex = 0; // номер выбранного канала

  // функция получения CSS‑класса по статусу
  const statusClass = (st) => {
    switch (st) {
      case 'ping': return 'status-pinging';
      case 'ok': return 'status-ok';
      case 'crc': return 'status-crc';
      case 'timeout': return 'status-timeout';
      default: return '';
    }
  };

  // перерисовка таблицы
  const renderTable = () => {
    tableBody.innerHTML = '';
    channels.forEach((ch, idx) => {
      const tr = document.createElement('tr');
      tr.innerHTML = `<td>${ch.channel}</td><td>${ch.freq.toFixed(2)}</td><td>${ch.measure}</td>`;
      tr.className = `${statusClass(ch.status)}${idx === selectedIndex ? ' selected' : ''}`;
      // выбор канала по клику
      tr.addEventListener('click', () => {
        selectedIndex = idx;
        renderTable();
      });
      tableBody.appendChild(tr);
    });
  };

  // обновление статуса канала
  const updateStatus = (idx, status, measure = '') => {
    channels[idx].status = status;
    channels[idx].measure = measure;
    renderTable();
  };

  // пинг одного канала с имитацией ответа
  const pingChannel = (idx) => {
    updateStatus(idx, 'ping');
    return new Promise((resolve) => {
      setTimeout(() => {
        const rnd = Math.random();
        if (rnd < 0.5) {
          const rssi = (-100 + Math.random() * 20).toFixed(1);
          const snr = (-5 + Math.random() * 10).toFixed(1);
          updateStatus(idx, 'ok', `RSSI=${rssi} SNR=${snr}`);
        } else if (rnd < 0.75) {
          updateStatus(idx, 'crc', 'CRC err');
        } else {
          updateStatus(idx, 'timeout', 'timeout');
        }
        resolve();
      }, 1000);
    });
  };

  // обработка одиночного пинга
  pingBtn.addEventListener('click', () => {
    pingChannel(selectedIndex);
  });

  // обработка последовательного поиска по всем каналам
  searchBtn.addEventListener('click', async () => {
    for (let i = 0; i < channels.length; i++) {
      selectedIndex = i;
      await pingChannel(i);
    }
  });

  // выгрузка таблицы в CSV
  exportBtn.addEventListener('click', () => {
    const dateStr = new Date().toISOString().split('T')[0];
    const count = channels.length;
    let csv = 'Канал,Частота,Замеры\n';
    channels.forEach(ch => {
      csv += `${ch.channel},${ch.freq},${ch.measure}\n`;
    });
    const blob = new Blob([csv], { type: 'text/csv;charset=utf-8;' });
    const link = document.createElement('a');
    link.href = URL.createObjectURL(blob);
    link.download = `channels_${dateStr}_${count}.csv`;
    link.click();
    URL.revokeObjectURL(link.href);
  });

  // --- Логика настроек ---
  const settingsSection = document.getElementById('settings'); // блок настроек

  // список названий параметров и сортировка по алфавиту
  const settingsNames = ['BF', 'SF', 'CR', 'BANK', 'CH', 'PW', 'INFO', 'STS', 'ACK'];
  settingsNames.sort(); // сортируем названия

  // создаём поле ввода для каждого параметра
  settingsNames.forEach(name => {
    const wrap = document.createElement('div');
    const label = document.createElement('label');
    label.htmlFor = `setting-${name}`;
    label.textContent = name;
    const inp = document.createElement('input');
    inp.id = `setting-${name}`;
    inp.value = localStorage.getItem(name) || '';
    // сохраняем значение в localStorage при изменении
    inp.addEventListener('change', () => {
      localStorage.setItem(name, inp.value);
    });
    wrap.appendChild(label);
    wrap.appendChild(inp);
    settingsSection.appendChild(wrap);
  });

  // кнопка переключения темы
  const themeBtn = document.createElement('button');
  themeBtn.id = 'theme-btn';
  settingsSection.appendChild(themeBtn);

  // функция применения темы
  const applyTheme = (theme) => {
    document.body.classList.toggle('dark', theme === 'dark');
    localStorage.setItem('theme', theme);
    themeBtn.textContent = theme === 'dark' ? 'Светлая тема' : 'Тёмная тема';
  };

  // восстанавливаем сохранённую тему
  applyTheme(localStorage.getItem('theme') || 'light');
  themeBtn.addEventListener('click', () => {
    const next = document.body.classList.contains('dark') ? 'light' : 'dark';
    applyTheme(next);
  });

  // кнопка очистки кеша (localStorage и IndexedDB)
  const clearBtn = document.createElement('button');
  clearBtn.id = 'clear-cache-btn';
  clearBtn.textContent = 'Очистить кеш';
  clearBtn.addEventListener('click', () => {
    localStorage.clear(); // очищаем localStorage
    // удаляем все базы IndexedDB, если доступно API
    if (indexedDB && indexedDB.databases) {
      indexedDB.databases().then(dbs => {
        dbs.forEach(db => indexedDB.deleteDatabase(db.name));
      });
    }
  });
  settingsSection.appendChild(clearBtn);

  // --- Логика безопасности ---
  const keyInfo = document.getElementById('key-info'); // блок отображения хеша ключа
  const keygenBtn = document.getElementById('keygen-btn'); // генерация нового ключа
  const keysendBtn = document.getElementById('keysend-btn'); // отправка текущего ключа
  const keyrecvBtn = document.getElementById('keyrecv-btn'); // приём ключа

  // загрузка ключа из localStorage
  const loadKey = () => localStorage.getItem('cryptoKey');

  // вычисление первых четырёх символов SHA-256
  const keyHash4 = async (keyHex) => {
    const bytes = new Uint8Array(keyHex.match(/.{2}/g).map(b => parseInt(b, 16)));
    const hash = await crypto.subtle.digest('SHA-256', bytes);
    const hex = Array.from(new Uint8Array(hash)).map(b => b.toString(16).padStart(2, '0')).join('');
    return hex.slice(0, 4);
  };

  // обновление информации о ключе
  const updateKeyInfo = async () => {
    const keyHex = loadKey();
    if (!keyHex) {
      keyInfo.textContent = 'LOCAL';
    } else {
      keyInfo.textContent = await keyHash4(keyHex);
    }
  };

  // генерация нового ключа и сохранение
  keygenBtn.addEventListener('click', async () => {
    const arr = new Uint8Array(16);
    crypto.getRandomValues(arr); // создание случайных байтов
    const keyHex = Array.from(arr).map(b => b.toString(16).padStart(2, '0')).join('');
    localStorage.setItem('cryptoKey', keyHex);
    await updateKeyInfo();
  });

  // заглушка передачи ключа
  keysendBtn.addEventListener('click', () => {
    // TODO: реализовать передачу ключа на другое устройство
  });

  // заглушка получения ключа
  keyrecvBtn.addEventListener('click', () => {
    // TODO: реализовать получение ключа от другого устройства
  });

  updateKeyInfo();

  renderTable(); // первоначальное заполнение таблицы
});

)~~~";

// style.css
const char STYLE_CSS[] PROGMEM = R"~~~(
/* Базовые цвета страницы */
body {
  background: #fff; /* светлый фон по умолчанию */
  color: #000; /* чёрный текст */
}

/* Тёмная тема */
body.dark {
  background: #121212; /* тёмный фон */
  color: #f0f0f0; /* светлый текст */
}

/* Простейшие стили для навигации и вкладок */
nav {
  display: flex;
  gap: 10px;
  margin-bottom: 20px;
}

body.dark nav {
  background: #1e1e1e; /* фон панели в тёмной теме */
}

.tab-content {
  display: none; /* скрываем все вкладки по умолчанию */
}

.tab-content.active {
  display: block; /* показываем выбранную вкладку */
}

#security-link {
  margin-left: auto; /* отделяем ссылку безопасности справа */
  align-self: center;
}

/* Стили для чата */
#chat-messages {
  list-style: none; /* убираем маркеры списка */
  padding: 0;
}

.message {
  margin-bottom: 5px; /* расстояние между сообщениями */
}

.message .time {
  font-size: 0.8em; /* уменьшаем размер времени */
  color: #666; /* серый цвет времени */
}

.outgoing {
  text-align: right; /* исходящие справа */
}

.incoming {
  text-align: left; /* входящие слева */
}

/* Стили для таблицы каналов */
#channel-controls {
  margin-bottom: 10px; /* отступ снизу от панели управления */
}

#channel-table {
  border-collapse: collapse; /* схлопываем границы */
  width: 100%; /* ширина на всю вкладку */
}

#channel-table th,
#channel-table td {
  border: 1px solid #ccc; /* простая рамка */
  padding: 5px; /* внутренний отступ */
  text-align: center; /* выравнивание по центру */
}

body.dark #channel-table th,
body.dark #channel-table td {
  border-color: #555; /* границы таблицы в тёмной теме */
}

#channel-table tr.selected {
  outline: 2px solid #000; /* выделение выбранной строки */
}

/* Цветовые статусы каналов */
.status-pinging {
  background: lightblue; /* голубой — пинг выполняется */
}

.status-ok {
  background: lightgreen; /* зелёный — получен ответ */
}

.status-crc {
  background: orange; /* оранжевый — ошибка CRC */
}

.status-timeout {
  background: lightgray; /* серый — нет ответа */
}

/* Блок настроек */
#settings div {
  margin-bottom: 5px; /* отступ между параметрами */
}

#settings label {
  margin-right: 5px; /* расстояние между подписью и полем */
}

body.dark input,
body.dark button,
body.dark table {
  background: #333; /* фон элементов управления в тёмной теме */
  color: #f0f0f0; /* текст в тёмной теме */
  border-color: #555; /* границы элементов */
}

)~~~";
