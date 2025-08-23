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
});
