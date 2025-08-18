#pragma once
const char WEB_INTERFACE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
  <title>ESP32 LoRa Pipeline</title>
  <link rel="stylesheet" href="/style.css">
</head>
<body class="dark">
  <h2>ESP32 LoRa Pipeline</h2>
  <div id="statusBar">
    <span id="keyIndicator" class="indicator local"></span>
    <span id="keyStatusText">Local ----</span>
    <span id="txIndicator" class="indicator tx" title="Передача"></span>
    <span id="rxIndicator" class="indicator rx" title="Приём"></span>
  </div>
  <div id="chat"></div>
  <div id="inputRow">
    <input id="msg" type="text" placeholder="Enter message">
    <button id="sendBtn" class="btn-primary" title="Send typed message over LoRa">Send</button>
    <span id="sendStatus"></span>
  </div>
  <!-- Basic and Profile section -->
  <details open>
    <summary>Basic</summary>
    <div class="panel-content">
      <div class="row">
        <label for="profileSelect">Profile:</label>
        <select id="profileSelect" title="Choose a predefined configuration profile">
          <option value="custom">Custom</option>
          <option value="range">High Range</option>
          <option value="speed">Fast Data</option>
          <option value="balanced">Balanced</option>
        </select>
        <label for="bankSelect">Bank:</label>
        <select id="bankSelect" title="Select preset bank (0‑MAIN,1‑RESERVE,2‑TEST)">
          <option value="0">0</option>
          <option value="1">1</option>
          <option value="2">2</option>
        </select>
        <label for="presetSelect">Preset:</label>
        <select id="presetSelect" title="Select preset index within bank (0‑9)">
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
      </div>
    </div>
  </details>
  <!-- Radio parameters -->
  <details>
    <summary>Radio</summary>
    <div class="panel-content">
      <div class="row">
        <label for="bwSelect">BW (kHz):</label>
        <select id="bwSelect" title="LoRa bandwidth (kHz)">
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
        <select id="sfSelect" title="Spreading factor (7‑12)">
          <option value="7">7</option>
          <option value="8">8</option>
          <option value="9">9</option>
          <option value="10">10</option>
          <option value="11">11</option>
          <option value="12">12</option>
        </select>
        <label for="crSelect">CR:</label>
        <select id="crSelect" title="Coding rate (4/5 to 4/8)">
          <option value="5">4/5</option>
          <option value="6">4/6</option>
          <option value="7">4/7</option>
          <option value="8">4/8</option>
        </select>
        <label for="txpSelect">TXP (dBm):</label>
        <select id="txpSelect" title="Transmit power (dBm)">
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
    </div>
  </details>
  <!-- Reliability options -->
  <details>
    <summary>Reliability</summary>
    <div class="panel-content">
      <div class="row">
        <label><input type="checkbox" id="ackChk" %ACK% title="Toggle acknowledgment and retransmissions"> Ack</label>
        <label for="retryNInput">RetryN:</label>
        <input id="retryNInput" type="number" min="0" max="10" step="1" value="" title="Number of retries when Ack enabled">
        <label for="retryMSInput">RetryMS:</label>
        <input id="retryMSInput" type="number" min="100" max="10000" step="100" value="" title="Delay between retries (ms)">
      </div>
    </div>
  </details>
  <!-- Security -->
  <details>
    <summary>Security</summary>
    <div class="panel-content">
      <div class="row">
        <label><input type="checkbox" id="encChk" %ENC% title="Toggle AES‑CCM encryption"> Enc</label>
        <label for="kidInput">KID:</label>
        <input id="kidInput" type="number" min="0" max="255" value="" title="Encryption key identifier (0‑255)">
        <label for="keyInput">KEY:</label>
        <input id="keyInput" type="text" placeholder="32 hex chars" title="Enter 16‑byte key as 32 hex characters">
        <button id="keyBtn" class="btn-secondary" title="Load key for current KID">Load Key</button>
      </div>
      <!-- Key exchange test row -->
      <div class="row">
        <button id="keyTestBtn" class="btn-secondary" title="Perform ECDH key exchange on this device and set new key">KeyX</button>
      </div>
      <!-- Authenticated ECDH row -->
      <div class="row">
        <button id="keyDhBtn" class="btn-secondary" title="Initiate authenticated ECDH key exchange with remote device">Key DH</button>
      </div>
      <!-- Key request/response row -->
      <div class="row">
        <button id="keyReqBtn" class="btn-secondary" title="Request current encryption key from remote device">Key Req</button>
        <button id="keySendBtn" class="btn-danger" title="Send current encryption key to remote device">Key Send</button>
      </div>
      <!-- Кнопка отображения хеша ключа -->
      <div class="row">
        <button id="keyHashBtn" class="btn-secondary" title="Показать хеш текущего ключа">Key Hash</button>
      </div>
    </div>
  </details>
  <!-- Storage -->
  <details>
    <summary>Storage</summary>
    <div class="panel-content">
      <div class="row">
        <button id="saveBtn" class="btn-secondary" title="Save current settings to non‑volatile storage">Save</button>
        <button id="loadBtn" class="btn-secondary" title="Load settings from non‑volatile storage">Load</button>
        <button id="resetBtn" class="btn-danger" title="Clear settings in non‑volatile storage">Reset</button>
      </div>
    </div>
  </details>
  <!-- Diagnostics -->
  <details>
    <summary>Diagnostics</summary>
    <div class="panel-content">
      <div class="row">
        <button id="pingBtn" class="btn-secondary" title="Send ping and measure round‑trip time">Ping</button>
        <button id="metricsBtn" class="btn-secondary" title="Show current metrics">Metrics</button>
        <!-- Кнопка самотеста -->
        <button id="selfTestBtn" class="btn-danger" title="Run built-in self-test">SelfTest</button>
      </div>
      <div id="metrics" style="white-space: pre-wrap;"></div>
      <div id="pingHistory"></div>
    </div>
  </details>
  <!-- Commands -->
  <details>
    <summary>Commands</summary>
    <div class="panel-content">
      <div class="row">
        <button id="simpleBtn" class="btn-primary" title="Send a basic test message 'ping'">Simple</button>
        <span id="simpleStatus"></span>
        <label for="largeSize">Large size:</label>
        <input id="largeSize" type="number" min="1" max="2048" value="1200" style="width:80px" title="Size of large test message in bytes">
        <button id="largeBtn" class="btn-secondary" title="Enqueue a large test message of given size">Large</button>
        <span id="largeStatus"></span>
      </div>
      <div class="row">
        <label for="encTestSize">EncTest size:</label>
        <input id="encTestSize" type="number" min="1" max="2048" value="" style="width:80px" title="Optional size for encryption self‑test">
        <button id="encTestBtn" class="btn-secondary" title="Run encryption self‑test">EncTest</button>
        <button id="encTestBadBtn" class="btn-danger" title="Run encryption test with bad KID">EncTestBad</button>
      </div>
      <div class="row">
        <label for="msgIdVal">MSGID next:</label>
        <input id="msgIdVal" type="number" min="1" max="4294967295" value="" style="width:120px" title="Set next message ID">
        <button id="msgIdBtn" class="btn-secondary" title="Set next message ID">Set MsgID</button>
      </div>
    </div>
  </details>
  <!-- QoS -->
  <details>
    <summary>QoS</summary>
    <div class="panel-content">
      <div class="row">
        <input id="sendQMsg" type="text" placeholder="Message for QoS" title="Enter message to send with priority">
        <label for="sendQPrio">Priority:</label>
        <select id="sendQPrio" title="Select priority for SendQ (H:High,N:Normal,L:Low)">
          <option value="N">Normal</option>
          <option value="H">High</option>
          <option value="L">Low</option>
        </select>
        <button id="sendQBtn" class="btn-primary" title="Send message with selected priority">SendQ</button>
      </div>
      <div class="row">
        <label for="largeQSize">LargeQ size:</label>
        <input id="largeQSize" type="number" min="1" max="2048" value="1200" style="width:80px" title="Size of large QoS test message in bytes">
        <label for="largeQPrio">Priority:</label>
        <select id="largeQPrio" title="Select priority for LargeQ">
          <option value="N">Normal</option>
          <option value="H">High</option>
          <option value="L">Low</option>
        </select>
        <button id="largeQBtn" class="btn-secondary" title="Enqueue large test message with priority">LargeQ</button>
      </div>
      <div class="row">
        <label for="qosModeSelect">Mode:</label>
        <select id="qosModeSelect" title="Select QoS scheduling mode">
          <option value="STRICT">Strict</option>
          <option value="W421">Weighted 4‑2‑1</option>
        </select>
        <button id="qosModeBtn" class="btn-secondary" title="Set scheduling mode">Set Mode</button>
        <button id="qosBtn" class="btn-secondary" title="Show current QoS queue usage">Show QoS</button>
      </div>
      <div id="qosStats" style="white-space: pre-wrap;"></div>
    </div>
  </details>
  <!-- Appearance -->
  <details>
    <summary>Appearance</summary>
    <div class="panel-content">
      <div class="row">
        <label><input type="checkbox" id="themeToggle" title="Toggle light/dark theme"> Light theme</label>
        <label for="fontRange">Font size:</label>
        <input id="fontRange" type="range" min="12" max="24" value="16" title="Adjust UI base font size">
        <button id="cleanBtn" class="btn-secondary" title="Очистить окно чата">Clean</button>
      </div>
    </div>
  </details>
  <!-- Раздел справки -->
  <details>
    <summary>Help</summary>
    <div class="panel-content">
      <p>Этот интерфейс управляет параметрами радиомодуля LoRa. Профили позволяют быстро установить рекомендованные значения:</p>
      <ul>
        <li><strong>High Range</strong> — минимальная полоса, максимальный фактор расширения и кодирование, средняя мощность.</li>
        <li><strong>Fast Data</strong> — максимальная полоса, минимальный фактор расширения и кодирование, высокая мощность.</li>
        <li><strong>Balanced</strong> — умеренные параметры полосы, фактора расширения и кодирования.</li>
      </ul>
      <p><strong>Bank</strong> выбирает таблицу частот, <strong>Preset</strong> — конкретную запись из неё. Параметры <strong>BW</strong>, <strong>SF</strong>, <strong>CR</strong> и <strong>TXP</strong> задают конфигурацию радиоканала. Надёжность включает подтверждения и повторные передачи, безопасность — шифрование; KID выбирает ключ, а поле KEY задаёт 16‑байтовый AES‑ключ. Блок Storage позволяет сохранить, загрузить или сбросить настройки. В разделе Diagnostics доступны пинг и метрики.</p>
      <p>История чата и выбранные параметры сохраняются в браузере и восстанавливаются после перезагрузки. Кнопка <strong>Clean</strong> очищает окно чата.</p>
    </div>
  </details>

  <script src="/app.js"></script>
</body>
</html>
)rawliteral";
