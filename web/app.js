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

  renderTable(); // первоначальное заполнение таблицы
});
