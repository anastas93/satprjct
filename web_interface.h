#pragma once
// Переписанный HTML веб-меню с сохранением функционала из README
// Строка хранится во флеше и отдаётся встроенным HTTP-сервером
const char WEB_INTERFACE_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
  <title>ESP32 LoRa Pipeline</title>
  <link rel="stylesheet" href="/style.css">
</head>
<body class="dark">
  <!-- Заголовок и панель статуса -->
  <h2>ESP32 LoRa Pipeline</h2>
  <div id="statusBar">
    <span id="keyIndicator" class="indicator round local"></span>
    <span id="keyStatusText">KID:0x00 • CRC:0x0000</span>
    <span id="txIndicator" class="indicator tx" title="Передача"></span>
    <span id="rxIndicator" class="indicator rx" title="Приём"></span>
    <span id="queueCounter" title="Пакетов в очереди">Q: 0</span>
  </div>

  <!-- Чат и отправка сообщения -->
  <div id="chatPanel">
    <div id="chat"></div>
    <div id="inputRow">
      <input id="msg" type="text" placeholder="Введите сообщение">
      <button id="sendBtn" type="button" class="btn-primary" title="Отправить текст по LoRa">Send</button><!-- исправлено: кнопка не отправляет форму -->
      <span id="sendStatus"></span>
    </div>
  </div>

  <!-- Базовые пресеты и профили -->
  <details open>
    <summary>Basic</summary>
    <div class="panel-content">
      <div class="row">
        <label for="profileSelect">Profile:</label>
        <select id="profileSelect" title="Выбор профиля конфигурации">
          <option value="custom">Custom</option>
          <option value="range">High Range</option>
          <option value="speed">Fast Data</option>
          <option value="balanced">Balanced</option>
        </select>
        <label for="bankSelect">Bank:</label>
        <select id="bankSelect" title="Выбор банка пресетов">
          <option value="0">0</option>
          <option value="1">1</option>
          <option value="2">2</option>
        </select>
      </div>
      <div class="row">
        <label for="txProfMode">Tx profile:</label>
        <select id="txProfMode" title="Режим профиля передачи">
          <option value="auto">Auto</option>
          <option value="manual">Manual</option>
        </select>
        <select id="txProfSelect" title="Выбор профиля P0–P3" disabled>
          <option value="0">P0</option>
          <option value="1">P1</option>
          <option value="2">P2</option>
          <option value="3">P3</option>
        </select>
      </div>
    </div>
  </details>

  <!-- Радиопараметры -->
  <details>
    <summary>Radio</summary>
    <div class="panel-content">
      <div class="row">
        <label for="presetSelect">Preset:</label>
        <select id="presetSelect" title="Выбор пресета внутри банка">
          <option value="0">0</option>
          <option value="1">1</option>
          <option value="2">2</option>
          <option value="3">3</option>
          <option value="4">4</option>
          <option value="5">5</option>
          <option value="6">6</option>
          <option value="7">7</option>
          <option value="8">8</option>
          <option value="9">9</option>
        </select>
        <label for="fecSelect">FEC:</label>
        <select id="fecSelect" title="Режим исправления ошибок">
          <option value="off">off</option>
          <option value="rs_vit">rs_vit</option>
          <option value="ldpc">ldpc</option>
        </select>
        <label for="interSelect">Interleave:</label>
        <select id="interSelect" title="Глубина интерливера">
          <option value="1">1</option>
          <option value="4">4</option>
          <option value="8">8</option>
          <option value="16">16</option>
        </select>
      </div>
      <div class="row">
        <label for="bwSelect">BW (kHz):</label>
        <select id="bwSelect" title="Полоса приёма">
          <option value="7.8">7.8</option>
          <option value="10.4">10.4</option>
          <option value="15.6">15.6</option>
          <option value="20.8">20.8</option>
          <option value="31.3">31.3</option>
          <option value="62.5">62.5</option>
          <option value="125">125</option>
          <option value="250">250</option>
          <option value="500">500</option>
        </select>
        <label for="sfSelect">SF:</label>
        <select id="sfSelect" title="Фактор расширения">
          <option value="7">7</option>
          <option value="8">8</option>
          <option value="9">9</option>
          <option value="10">10</option>
          <option value="11">11</option>
          <option value="12">12</option>
        </select>
        <label for="crSelect">CR:</label>
        <select id="crSelect" title="Коэффициент кодирования">
          <option value="5">4/5</option>
          <option value="6">4/6</option>
          <option value="7">4/7</option>
          <option value="8">4/8</option>
        </select>
        <label for="txpSelect">TXP (dBm):</label>
        <select id="txpSelect" title="Мощность передатчика">
          <option value="-9">-9</option>
          <option value="-6">-6</option>
          <option value="-3">-3</option>
          <option value="0">0</option>
          <option value="3">3</option>
          <option value="6">6</option>
          <option value="9">9</option>
          <option value="12">12</option>
          <option value="15">15</option>
          <option value="18">18</option>
          <option value="20">20</option>
          <option value="22">22</option>
        </select>
      </div>
      <div class="row">
        <label><input type="checkbox" id="rxBoostChk" %RXBOOST% title="Усиленный приём"> RX Boost</label>
      </div>
    </div>
  </details>

  <!-- Параметры надёжности -->
  <details>
    <summary>Reliability</summary>
    <div class="panel-content">
      <div class="row">
        <label><input type="checkbox" id="ackChk" %ACK% title="Подтверждения ACK"> Ack</label>
        <label for="retryNInput">RetryN:</label>
        <input id="retryNInput" type="number" min="0" max="10" step="1" title="Число повторов">
        <label for="retryMSInput">RetryMS:</label>
        <input id="retryMSInput" type="number" min="100" max="10000" step="100" title="Пауза между повторами (мс)">
      </div>
      <div class="row">
        <label for="payloadInput">Payload:</label>
        <input id="payloadInput" type="number" min="1" max="241" step="1" title="Размер полезной нагрузки">
        <label for="pilotInput">Pilot:</label>
        <input id="pilotInput" type="number" min="0" max="512" step="1" title="Интервал пилотов, 0=выкл">
        <label><input type="checkbox" id="dupChk" %DUP% title="Дублировать заголовок"> Dup</label>
        <label for="winInput">Win:</label>
        <input id="winInput" type="number" min="1" max="32" step="1" value="8" title="Размер окна SR-ARQ">
        <label for="ackAggInput">AckAgg:</label>
        <input id="ackAggInput" type="number" min="0" max="1000" step="10" value="50" title="Интервал агрегации ACK (мс)">
      </div>
      <div class="row">
        <label for="burstInput">Burst:</label>
        <input id="burstInput" type="number" min="1" max="32" step="1" title="Число фрагментов в серии перед ожиданием ACK">
        <label for="ackJitterInput">AckJitter:</label>
        <input id="ackJitterInput" type="number" min="0" max="100" step="1" title="Джиттер ожидания ACK (%)">
        <label for="backoffInput">Backoff:</label>
        <input id="backoffInput" type="text" placeholder="1,1.5,2" title="Коэффициенты задержки через запятую">
      </div>
    </div>
  </details>

  <!-- Планировщик -->
  <details>
    <summary>Scheduler</summary>
    <div class="panel-content">
      <div class="row">
        <label for="tddTxInput">TDD TX:</label>
        <input id="tddTxInput" type="number" min="0" max="10000" step="10" title="Длительность окна передачи (мс)">
        <label for="tddAckInput">ACK:</label>
        <input id="tddAckInput" type="number" min="0" max="10000" step="10" title="Окно ожидания ACK (мс)">
        <label for="tddGuardInput">Guard:</label>
        <input id="tddGuardInput" type="number" min="0" max="1000" step="10" title="Защитный интервал (мс)">
        <button id="tddApplyBtn" class="btn-secondary" title="Применить параметры TDD">Apply</button>
      </div>
    </div>
  </details>

  <!-- Безопасность -->
  <details>
    <summary>Security</summary>
    <div class="panel-content">
      <div class="row">
        <label><input type="checkbox" id="encChk" %ENC% title="Включить AES-CCM"> Enc</label>
        <label for="kidInput">KID:</label>
        <input id="kidInput" type="number" min="0" max="255" title="Идентификатор ключа">
        <label for="keyInput">KEY:</label>
        <input id="keyInput" type="text" placeholder="32 hex chars" title="16‑байтовый AES-ключ">
        <button id="keyBtn" class="btn-secondary" title="Загрузить ключ">Load Key</button>
      </div>
      <div class="row">
        <button id="keyTestBtn" class="btn-secondary" title="ECDH внутри устройства">KeyX</button>
      </div>
      <div class="row">
        <button id="keyDhBtn" class="btn-secondary" title="Аутентифицированный обмен ECDH">Key DH</button>
      </div>
      <div class="row">
        <button id="keyReqBtn" class="btn-secondary" title="Запросить ключ с удалённой стороны">Key Req</button>
        <button id="keySendBtn" class="btn-danger" title="Отправить текущий ключ">Key Send</button>
      </div>
    </div>
  </details>

  <!-- Безопасность для разработчика -->
  <details>
    <summary>Security (Dev)</summary>
    <div class="panel-content">
      <div class="row">
        <input id="devKeyFile" type="file" title="Файл с 16‑байтовым ключом в hex">
        <button id="uploadKeyBtn" class="btn-secondary" title="Загрузить ключ из файла">Upload Key</button>
        <label for="kidActInput">KID:</label>
        <input id="kidActInput" type="number" min="0" max="255" title="Активировать KID">
        <button id="kidActBtn" class="btn-secondary" title="Активировать выбранный KID">Activate</button>
      </div>
      <div class="row">
        <button id="idResetBtn" class="btn-danger" title="Сбросить счётчик сообщений">IDRESET</button>
        <button id="replayClrBtn" class="btn-danger" title="Очистить окно anti-replay">REPLAYCLR</button>
      </div>
    </div>
  </details>

  <!-- Работа с NVS -->
  <details>
    <summary>Storage</summary>
    <div class="panel-content">
      <div class="row">
        <button id="saveBtn" class="btn-secondary" title="Сохранить настройки">Save</button>
        <button id="loadBtn" class="btn-secondary" title="Загрузить настройки">Load</button>
        <button id="resetBtn" class="btn-danger" title="Сбросить настройки">Reset</button>
      </div>
    </div>
  </details>

  <!-- Диагностика -->
  <details>
    <summary>Diagnostics</summary>
    <div class="panel-content">
      <div class="row">
        <button id="pingBtn" class="btn-secondary" title="Отправить ping">Ping</button>
        <button id="chanPingBtn" class="btn-secondary" title="Проверка канала на текущем пресете">ChannelPing</button>
        <button id="presetPingBtn" class="btn-secondary" title="Пинг выбранного пресета">PresetPing</button>
        <button id="massPingBtn" class="btn-secondary" title="Пинг всех пресетов банка">MassPing</button>
      </div>
      <div class="row">
        <button id="metricsBtn" class="btn-secondary" title="Показать метрики">Metrics</button>
        <button id="selfTestBtn" class="btn-danger" title="Запустить самотест">SelfTest</button>
      </div>
      <div id="metrics" style="white-space: pre-wrap;"></div>
      <div id="pingHistory"></div>
      <div class="row">
        <label for="satCount">Cnt:</label>
        <input id="satCount" type="number" min="0" value="0" style="width:80px" title="число пакетов, 0=бесконечно">
        <label for="satInterval">Int(ms):</label>
        <input id="satInterval" type="number" min="10" value="1000" style="width:80px" title="интервал между пакетами">
        <label for="satSize">Size:</label>
        <input id="satSize" type="number" min="1" max="241" value="5" style="width:60px" title="размер полезной нагрузки">
        <label for="satFec">FEC:</label>
        <select id="satFec" title="режим FEC">
          <option value="off">off</option>
          <option value="rs_vit">rs_vit</option>
          <option value="ldpc">ldpc</option>
          <option value="rep2">repeat2</option>
        </select>
        <label for="satRetries">Ret:</label>
        <input id="satRetries" type="number" min="0" max="10" value="0" style="width:60px" title="повторы при потере">
        <button id="satRunBtn" class="btn-secondary" title="Запустить расширенный SatPing">SatPing+</button>
      </div>
      <div id="satPingResult" style="white-space: pre-wrap;"></div>
    </div>
  </details>

  <!-- Архив и очередь -->
  <details>
    <summary>Archive/Queue</summary>
    <div class="panel-content">
      <div class="row">
        <button id="archListBtn" class="btn-secondary" title="Показать архив">List</button>
        <button id="archRestoreBtn" class="btn-secondary" title="Вернуть сообщение из архива">Restore</button>
      </div>
      <div id="archiveList" style="white-space: pre-wrap;"></div>
    </div>
  </details>

  <!-- Последние кадры -->
  <details>
    <summary>Frames</summary>
    <div class="panel-content">
      <div class="row">
        <label for="frameDrop">drop_reason:</label>
        <input id="frameDrop" type="number" min="0" max="255" style="width:80px" title="Фильтр по drop_reason, пусто=все">
        <button id="frameRefreshBtn" class="btn-secondary" title="Обновить список кадров">Refresh</button>
      </div>
      <table id="frameTable" class="log-table"></table>
    </div>
  </details>

  <!-- Мониторинг канала -->
  <details>
    <summary>Link Diagnostics</summary>
    <div class="panel-content">
      <div class="row">
        <canvas id="perGraph" width="300" height="100" class="graph"></canvas>
        <canvas id="rttGraph" width="300" height="100" class="graph"></canvas>
        <canvas id="ebn0Graph" width="300" height="100" class="graph"></canvas>
      </div>
      <div class="row">
        <label for="diagFecSelect">FEC:</label>
        <select id="diagFecSelect" title="Режим исправления ошибок">
          <option value="off">off</option>
          <option value="rs_vit">rs_vit</option>
          <option value="ldpc">ldpc</option>
        </select>
        <label for="diagInterSelect">Interleave:</label>
        <select id="diagInterSelect" title="Глубина интерливера">
          <option value="1">1</option>
          <option value="4">4</option>
          <option value="8">8</option>
          <option value="16">16</option>
        </select>
        <label for="diagFragInput">Frag:</label>
        <input id="diagFragInput" type="number" min="1" max="241" step="1" title="Размер полезной нагрузки">
      </div>
      <div class="row">
        <div id="linkProfile"></div>
        <div id="ackBitmap"></div>
      </div>
      <div class="row">
        <label for="perHigh">PER High:</label>
        <input id="perHigh" type="number" min="0" max="1" step="0.01" title="Порог ухудшения PER">
        <label for="perLow">PER Low:</label>
        <input id="perLow" type="number" min="0" max="1" step="0.01" title="Порог улучшения PER">
        <label for="ebn0High">Eb/N0 High:</label>
        <input id="ebn0High" type="number" min="0" max="30" step="0.1" title="Порог улучшения Eb/N0">
        <label for="ebn0Low">Eb/N0 Low:</label>
        <input id="ebn0Low" type="number" min="0" max="30" step="0.1" title="Порог ухудшения Eb/N0">
      </div>
    </div>
  </details>

  <!-- Раздел команд -->
  <details>
    <summary>Commands</summary>
    <div class="panel-content">
      <div class="row">
        <button id="simpleBtn" class="btn-primary" title="Простой тест 'ping'">Simple</button>
        <span id="simpleStatus"></span>
        <label for="largeSize">Large size:</label>
        <input id="largeSize" type="number" min="1" max="2048" value="1200" style="width:80px" title="Размер большого сообщения">
        <button id="largeBtn" class="btn-secondary" title="Отправить большое сообщение">Large</button>
        <span id="largeStatus"></span>
      </div>
      <div class="row">
        <label for="encTestSize">EncTest size:</label>
        <input id="encTestSize" type="number" min="1" max="2048" style="width:80px" title="Размер для теста шифрования">
        <button id="encTestBtn" class="btn-secondary" title="Самотест шифрования">EncTest</button>
        <button id="encTestBadBtn" class="btn-danger" title="Тест с неверным KID">EncTestBad</button>
      </div>
      <div class="row">
        <label for="msgIdVal">MSGID next:</label>
        <input id="msgIdVal" type="number" min="1" max="4294967295" style="width:120px" title="Следующий идентификатор сообщения">
        <button id="msgIdBtn" class="btn-secondary" title="Установить следующий msg_id">Set MsgID</button>
      </div>
    </div>
  </details>

  <!-- QoS -->
  <details>
    <summary>QoS</summary>
    <div class="panel-content">
      <div class="row">
        <input id="sendQMsg" type="text" placeholder="Message for QoS" title="Сообщение с приоритетом">
        <label for="sendQPrio">Priority:</label>
        <select id="sendQPrio" title="Приоритет">
          <option value="N">Normal</option>
          <option value="H">High</option>
          <option value="L">Low</option>
        </select>
        <button id="sendQBtn" class="btn-primary" title="Отправить с приоритетом">SendQ</button>
      </div>
      <div class="row">
        <label for="largeQSize">LargeQ size:</label>
        <input id="largeQSize" type="number" min="1" max="2048" value="1200" style="width:80px" title="Размер большого сообщения">
        <label for="largeQPrio">Priority:</label>
        <select id="largeQPrio" title="Приоритет для LargeQ">
          <option value="N">Normal</option>
          <option value="H">High</option>
          <option value="L">Low</option>
        </select>
        <button id="largeQBtn" class="btn-secondary" title="Отправить большое сообщение с приоритетом">LargeQ</button>
      </div>
      <div class="row">
        <label for="qosModeSelect">Mode:</label>
        <select id="qosModeSelect" title="Режим планировщика QoS">
          <option value="STRICT">Strict</option>
          <option value="W421">Weighted 4‑2‑1</option>
        </select>
        <button id="qosModeBtn" class="btn-secondary" title="Установить режим">Set Mode</button>
        <button id="qosBtn" class="btn-secondary" title="Показать статистику">Show QoS</button>
      </div>
      <div id="qosStats" style="white-space: pre-wrap;"></div>
    </div>
  </details>

  <!-- Внешний вид -->
  <details>
    <summary>Appearance</summary>
    <div class="panel-content">
      <div class="row">
        <label><input type="checkbox" id="themeToggle" title="Светлая тема"> Light theme</label>
        <label for="fontRange">Font size:</label>
        <input id="fontRange" type="range" min="12" max="24" value="16" title="Размер шрифта">
        <button id="cleanBtn" class="btn-secondary" title="Очистить чат">Clean</button>
      </div>
    </div>
  </details>

  <!-- Справка -->
  <details>
    <summary>Help</summary>
    <div class="panel-content">
      <p>Интерфейс управляет радиомодулем LoRa. Профили задают рекомендуемые параметры:</p>
      <ul>
        <li><strong>High Range</strong> — минимальная полоса, максимальный SF и кодирование.</li>
        <li><strong>Fast Data</strong> — максимальная полоса, минимальный SF и высокая мощность.</li>
        <li><strong>Balanced</strong> — умеренные значения полосы и кодирования.</li>
      </ul>
      <p>Параметры и история чата сохраняются в браузере и восстанавливаются после перезагрузки. Кнопка <strong>Clean</strong> очищает чат.</p>
    </div>
  </details>

  <script src="/app.js"></script>
</body>
</html>
)rawliteral";
