// Простое переключение вкладок
document.addEventListener('DOMContentLoaded', () => {
  const buttons = document.querySelectorAll('.tab-btn'); // кнопки вкладок
  const sections = document.querySelectorAll('.tab-content'); // содержимое вкладок
  const securityLink = document.getElementById('security-link'); // отдельная ссылка безопасности
  const allLinks = [...buttons, securityLink]; // все ссылки навигации
  const nav = document.getElementById('main-nav'); // контейнер навигации
  const menuToggle = document.getElementById('menu-toggle'); // кнопка меню для мобильных

  // функция переключения мобильного меню
  const toggleMenu = () => {
    const opened = nav.classList.toggle('open'); // меняем класс .open у навигации
    menuToggle.setAttribute('aria-expanded', String(opened)); // обновляем aria-expanded
  };

  // обработчик клика по кнопке меню
  menuToggle.addEventListener('click', toggleMenu);

  // функция активации вкладки
  const activate = (id) => {
    sections.forEach(sec => sec.classList.remove('active')); // скрываем все разделы
    const target = document.getElementById(id);
    if (target) target.classList.add('active'); // показываем нужный
    // отмечаем активную ссылку и снимаем отметку с остальных
    allLinks.forEach(link => link.removeAttribute('aria-current')); // убираем прошлые пометки
    const current = allLinks.find(link => link.dataset.tab === id); // ищем текущую ссылку
    if (current) current.setAttribute('aria-current', 'page'); // помечаем активную
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

  // вспомогательная функция создания выпадающего списка
  const createSelect = (name, options) => {
    const wrap = document.createElement('div');
    const label = document.createElement('label');
    label.htmlFor = `setting-${name}`;
    label.textContent = name; // подпись параметра
    const select = document.createElement('select');
    select.id = `setting-${name}`;
    options.forEach(opt => {
      const option = document.createElement('option');
      if (typeof opt === 'object') {
        option.value = opt.value;
        option.textContent = opt.label;
      } else {
        option.value = opt;
        option.textContent = opt;
      }
      select.appendChild(option);
    });
    const saved = localStorage.getItem(name);
    if (saved) select.value = saved;
    select.addEventListener('change', () => {
      localStorage.setItem(name, select.value); // сохранение выбранного значения
    });
    wrap.appendChild(label);
    wrap.appendChild(select);
    settingsSection.appendChild(wrap);
  };

  // вспомогательная функция создания чекбокса
  const createCheckbox = (name, text) => {
    const wrap = document.createElement('div');
    const label = document.createElement('label');
    const cb = document.createElement('input');
    cb.type = 'checkbox';
    cb.id = `setting-${name}`;
    cb.checked = localStorage.getItem(name) === '1';
    cb.addEventListener('change', () => {
      localStorage.setItem(name, cb.checked ? '1' : '0');
    });
    label.appendChild(cb);
    label.appendChild(document.createTextNode(text));
    wrap.appendChild(label);
    settingsSection.appendChild(wrap);
  };

  // параметры радиомодуля
  createSelect('BF', ['433.05', '433.25', '433.45', '433.65', '433.85']); // базовая частота
  createSelect('BANK', ['EAST', 'WEST', 'TEST', 'ALL']); // банк каналов
  createSelect('SF', [5, 6, 7, 8, 9, 10, 11, 12]); // фактор расширения
  createSelect('CR', [
    { value: '5', label: '4/5' },
    { value: '6', label: '4/6' },
    { value: '7', label: '4/7' },
    { value: '8', label: '4/8' }
  ]); // коэффициент кодирования
  createSelect('SR', ['125', '250', '500', '1000']); // скорость символов (кГц)
  createSelect('PW', [-5, -2, 1, 4, 7, 10, 13, 16, 19, 22]); // мощность в дБм
  createCheckbox('ACK', 'ACK'); // использовать подтверждения

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
