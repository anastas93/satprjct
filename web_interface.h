#pragma once
const char WEB_INTERFACE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
  <title>ESP32 LoRa Pipeline</title>
  <style>
    :root {
      --bg-color: #222;
      --fg-color: #eee;
      --panel-bg: #333;
      --panel-bg-open: #2a2a2a;
      --primary: #3f51b5;
      --primary-hover: #5c6bc0;
      --base-font: 16px;
      /* Colours for message types in dark theme */
      --tx-color: #00ccff;
      --rx-color: #ffb74d;
      --sys-color: #66bb6a;
    }
    body.light {
      --bg-color: #f5f5f5;
      --fg-color: #222;
      --panel-bg: #eee;
      --panel-bg-open: #e0e0e0;
      --primary: #3f51b5;
      --primary-hover: #5c6bc0;
      /* Colours for message types in light theme */
      --tx-color: #01579b;
      --rx-color: #e65100;
      --sys-color: #2e7d32;
    }
    body {
      background-color: var(--bg-color);
      color: var(--fg-color);
      font-family: Arial, sans-serif;
      font-size: var(--base-font);
      margin: 0;
      padding: 10px;
    }
    h2 {
      margin-top: 0;
    }
    /* Status bar and key indicator styles */
    #statusBar { display: flex; align-items: center; margin-bottom: 8px; }
    .indicator { width: 12px; height: 12px; border-radius: 50%; display: inline-block; margin-right: 4px; }
    .indicator.local { background-color: var(--sys-color); }
    .indicator.remote { background-color: var(--rx-color); }
    .indicator.blink { animation: blink 1s linear infinite; }
    @keyframes blink {
      0%, 50% { opacity: 1; }
      50%, 100% { opacity: 0; }
    }
    #chat {
      height: 35vh;
      width: 100%;
      background-color: var(--panel-bg);
      border: 1px solid #555;
      padding: 10px;
      box-sizing: border-box;
      overflow-y: auto;
      display: flex;
      flex-direction: column-reverse;
      margin-bottom: 10px;
    }
    #inputRow {
      display: flex;
      margin-bottom: 10px;
    }
    #msg {
      flex: 1;
      padding: 8px;
      border: 1px solid #555;
      background-color: var(--panel-bg);
      color: var(--fg-color);
    }
    button {
      padding: 8px 12px;
      border: none;
      color: #fff;
      background-color: var(--primary);
      cursor: pointer;
      border-radius: 3px;
    }
    button:hover {
      background-color: var(--primary-hover);
    }
    select, input[type=number], input[type=text] {
      padding: 4px;
      background-color: var(--panel-bg);
      color: var(--fg-color);
      border: 1px solid #555;
      border-radius: 3px;
    }
    label {
      margin-right: 4px;
    }
    details {
      margin-bottom: 8px;
    }
    details summary {
      cursor: pointer;
      padding: 6px;
      background-color: var(--panel-bg);
      border: 1px solid #555;
      border-radius: 4px;
      list-style: none;
    }
    details[open] summary {
      background-color: var(--panel-bg-open);
    }
    .panel-content {
      padding: 6px;
      border: 1px solid #555;
      border-top: none;
      background-color: var(--panel-bg);
    }
    #pingHistory {
      font-family: monospace;
      font-size: 0.9em;
      white-space: pre;
    }
    .row {
      display: flex;
      flex-wrap: wrap;
      align-items: center;
      margin-bottom: 6px;
    }
    .row > * {
      margin-right: 6px;
      margin-bottom: 6px;
    }
    /* Apply colours to chat messages based on prefix */
    .tx { color: var(--tx-color); }
    .rx { color: var(--rx-color); }
    .sys { color: var(--sys-color); }
  </style>
</head>
<body class="dark">
  <h2>ESP32 LoRa Pipeline</h2>
  <div id="statusBar">
    <span id="keyIndicator" class="indicator local"></span>
    <span id="keyStatusText">Key: local</span>
  </div>
  <div id="chat"></div>
  <div id="inputRow">
    <input id="msg" type="text" placeholder="Enter message">
    <button id="sendBtn" title="Send typed message over LoRa">Send</button>
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
        <button id="keyBtn" title="Load key for current KID">Load Key</button>
      </div>
      <!-- Key exchange test row -->
      <div class="row">
        <button id="keyTestBtn" title="Perform ECDH key exchange on this device and set new key">KeyX</button>
      </div>
      <!-- Authenticated ECDH row -->
      <div class="row">
        <button id="keyDhBtn" title="Initiate authenticated ECDH key exchange with remote device">Key DH</button>
      </div>
      <!-- Key request/response row -->
      <div class="row">
        <button id="keyReqBtn" title="Request current encryption key from remote device">Key Req</button>
        <button id="keySendBtn" title="Send current encryption key to remote device">Key Send</button>
      </div>
    </div>
  </details>
  <!-- Storage -->
  <details>
    <summary>Storage</summary>
    <div class="panel-content">
      <div class="row">
        <button id="saveBtn" title="Save current settings to non‑volatile storage">Save</button>
        <button id="loadBtn" title="Load settings from non‑volatile storage">Load</button>
        <button id="resetBtn" title="Clear settings in non‑volatile storage">Reset</button>
      </div>
    </div>
  </details>
  <!-- Diagnostics -->
  <details>
    <summary>Diagnostics</summary>
    <div class="panel-content">
      <div class="row">
        <button id="pingBtn" title="Send ping and measure round‑trip time">Ping</button>
        <button id="metricsBtn" title="Show current metrics">Metrics</button>
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
        <button id="simpleBtn" title="Send a basic test message 'ping'">Simple</button>
        <label for="largeSize">Large size:</label>
        <input id="largeSize" type="number" min="1" max="2048" value="1200" style="width:80px" title="Size of large test message in bytes">
        <button id="largeBtn" title="Enqueue a large test message of given size">Large</button>
      </div>
      <div class="row">
        <label for="encTestSize">EncTest size:</label>
        <input id="encTestSize" type="number" min="1" max="2048" value="" style="width:80px" title="Optional size for encryption self‑test">
        <button id="encTestBtn" title="Run encryption self‑test">EncTest</button>
        <button id="encTestBadBtn" title="Run encryption test with bad KID">EncTestBad</button>
      </div>
      <div class="row">
        <label for="msgIdVal">MSGID next:</label>
        <input id="msgIdVal" type="number" min="1" max="4294967295" value="" style="width:120px" title="Set next message ID">
        <button id="msgIdBtn" title="Set next message ID">Set MsgID</button>
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
        <button id="sendQBtn" title="Send message with selected priority">SendQ</button>
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
        <button id="largeQBtn" title="Enqueue large test message with priority">LargeQ</button>
      </div>
      <div class="row">
        <label for="qosModeSelect">Mode:</label>
        <select id="qosModeSelect" title="Select QoS scheduling mode">
          <option value="STRICT">Strict</option>
          <option value="W421">Weighted 4‑2‑1</option>
        </select>
        <button id="qosModeBtn" title="Set scheduling mode">Set Mode</button>
        <button id="qosBtn" title="Show current QoS queue usage">Show QoS</button>
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
      </div>
    </div>
  </details>
  <!-- Help -->
  <details>
    <summary>Help</summary>
    <div class="panel-content">
      <p>This interface controls the LoRa radio parameters. Profiles quickly set recommended values:</p>
      <ul>
        <li><strong>High Range</strong>: minimal bandwidth, maximal spreading factor and coding rate, moderate power.</li>
        <li><strong>Fast Data</strong>: maximal bandwidth, minimal spreading factor, minimal coding rate, high power.</li>
        <li><strong>Balanced</strong>: moderate values for bandwidth, spreading factor and coding rate.</li>
      </ul>
      <p><strong>Bank</strong> selects a frequency table; <strong>Preset</strong> selects an entry in that table. Adjust <strong>BW</strong>, <strong>SF</strong> (spreading factor), <strong>CR</strong> (coding rate) and <strong>TXP</strong> for custom configurations. Reliability settings enable acknowledgments and configure retry behaviour. Security enables encryption; KID selects which key to use, and KEY sets the 16‑byte AES key. Storage operations save/load/reset settings. Diagnostics provide ping latency and other metrics.</p>
    </div>
  </details>

  <script>
    // Utility to append a line to chat and keep the list short
    function appendChat(txt) {
      const chat = document.getElementById('chat');
      const div = document.createElement('div');
      // Determine message type based on prefix and assign CSS class
      let cls = '';
      if (txt.startsWith('*TX:*')) {
        cls = 'tx';
      } else if (txt.startsWith('*RX:*')) {
        cls = 'rx';
      } else if (txt.startsWith('*SYS:*')) {
        cls = 'sys';
      }
      if (cls) div.className = cls;
      div.textContent = txt;
      chat.prepend(div);
      // Limit number of chat entries for performance
      while (chat.children.length > 100) {
        chat.removeChild(chat.lastChild);
      }
    }
    // Store last N ping results for simple trend display
    const pingHistory = [];
    function updatePingHistory(rssi, snr, dist, time) {
      pingHistory.push({rssi, snr, dist, time});
      if (pingHistory.length > 10) pingHistory.shift();
      const ph = document.getElementById('pingHistory');
      ph.textContent = pingHistory.map((p,i) =>
        (i+1)+'. RSSI:'+p.rssi+' dBm, SNR:'+p.snr+' dB, dist:'+p.dist+' km, time:'+p.time+' ms'
      ).join('\n');
    }
    // Poll serial endpoint periodically; parse ping results for metrics
    function pollSerial() {
      fetch('/serial')
        .then(r => r.text())
        .then(t => {
          if (t) {
            const lines = t.trim().split('\n');
            lines.forEach(l => {
              if (!l) return;
              appendChat(l);
              // Parse ping lines to update trend chart
              // Expect format like "Ping>OK RSSI: -XX.X dBm/SNR: YY.Y dB distance:~ZZZ km time:TTT ms"
              if (l.indexOf('Ping>OK') !== -1) {
                try {
                  const m = /RSSI:([\-\d\.]+) dBm\/SNR:([\-\d\.]+) dB distance:~([\d\.]+) km time:([\d\.]+) ms/.exec(l);
                  if (m) {
                    const rssi = parseFloat(m[1]).toFixed(1);
                    const snr = parseFloat(m[2]).toFixed(1);
                    const dist = parseFloat(m[3]).toFixed(3);
                    const time = parseFloat(m[4]).toFixed(2);
                    updatePingHistory(rssi, snr, dist, time);
                  }
                } catch (e) {}
              }
            });
          }
          setTimeout(pollSerial, 500);
        })
        .catch(() => setTimeout(pollSerial, 1000));
    }
    // Apply profile settings
    function applyProfile(name) {
      if (name === 'range') {
        // minimal BW, maximal SF/CR, moderate power
        setSelect('bwSelect','7.8'); setSelect('sfSelect','12'); setSelect('crSelect','8'); setSelect('txpSelect','12');
        sendParam('setbw','7.8'); sendParam('setsf','12'); sendParam('setcr','8'); sendParam('settxp','12');
      } else if (name === 'speed') {
        // maximal BW, minimal SF/CR, high power
        setSelect('bwSelect','500'); setSelect('sfSelect','7'); setSelect('crSelect','5'); setSelect('txpSelect','20');
        sendParam('setbw','500'); sendParam('setsf','7'); sendParam('setcr','5'); sendParam('settxp','20');
      } else if (name === 'balanced') {
        // balanced values
        setSelect('bwSelect','125'); setSelect('sfSelect','9'); setSelect('crSelect','6'); setSelect('txpSelect','14');
        sendParam('setbw','125'); sendParam('setsf','9'); sendParam('setcr','6'); sendParam('settxp','14');
      } else {
        // custom: do nothing
      }
    }
    // Helper to set select value by id
    function setSelect(id,val) {
      const el = document.getElementById(id);
      if (el) el.value = val;
    }
    // Send parameter to server
    function sendParam(path,val) {
      fetch('/'+path+'?val='+encodeURIComponent(val));
    }
    // Event handlers
    document.getElementById('sendBtn').addEventListener('click', () => {
      const m = document.getElementById('msg').value;
      if (m) {
        fetch('/send?msg=' + encodeURIComponent(m)).then(() => {
          document.getElementById('msg').value = '';
        });
      }
    });
    document.getElementById('bankSelect').addEventListener('change', (e) => {
      sendParam('setbank', e.target.value);
    });
    document.getElementById('presetSelect').addEventListener('change', (e) => {
      sendParam('setpreset', e.target.value);
    });
    document.getElementById('bwSelect').addEventListener('change', (e) => {
      sendParam('setbw', e.target.value);
    });
    document.getElementById('sfSelect').addEventListener('change', (e) => {
      sendParam('setsf', e.target.value);
    });
    document.getElementById('crSelect').addEventListener('change', (e) => {
      sendParam('setcr', e.target.value);
    });
    document.getElementById('txpSelect').addEventListener('change', (e) => {
      sendParam('settxp', e.target.value);
    });
    document.getElementById('ackChk').addEventListener('change', () => {
      fetch('/toggleack');
    });
    document.getElementById('retryNInput').addEventListener('change', (e) => {
      sendParam('setretryn', e.target.value);
    });
    document.getElementById('retryMSInput').addEventListener('change', (e) => {
      sendParam('setretryms', e.target.value);
    });
    document.getElementById('encChk').addEventListener('change', () => {
      fetch('/toggleenc');
    });
    document.getElementById('kidInput').addEventListener('change', (e) => {
      sendParam('setkid', e.target.value);
    });
    document.getElementById('keyBtn').addEventListener('click', () => {
      const key = document.getElementById('keyInput').value.trim();
      if (key.length === 32) {
        fetch('/setkey?val=' + encodeURIComponent(key));
      } else {
        alert('Key must be 32 hex characters.');
      }
    });
    document.getElementById('saveBtn').addEventListener('click', () => {
      fetch('/save');
    });
    document.getElementById('loadBtn').addEventListener('click', () => {
      fetch('/load');
    });
    document.getElementById('resetBtn').addEventListener('click', () => {
      fetch('/reset');
    });
    document.getElementById('pingBtn').addEventListener('click', () => {
      fetch('/ping');
    });
    document.getElementById('metricsBtn').addEventListener('click', () => {
      fetch('/metrics').then(r => r.text()).then(t => {
        document.getElementById('metrics').textContent = t;
      });
    });
    document.getElementById('profileSelect').addEventListener('change', (e) => {
      applyProfile(e.target.value);
    });
    // Theme toggle
    const themeToggle = document.getElementById('themeToggle');
    themeToggle.addEventListener('change', () => {
      const body = document.body;
      if (themeToggle.checked) {
        body.classList.add('light');
      } else {
        body.classList.remove('light');
      }
    });
    // Font size slider
    const fontRange = document.getElementById('fontRange');
    fontRange.addEventListener('input', () => {
      const val = fontRange.value;
      document.documentElement.style.setProperty('--base-font', val + 'px');
    });
    // Register command button handlers
    document.getElementById('simpleBtn').addEventListener('click', () => {
      fetch('/simple');
    });
    document.getElementById('largeBtn').addEventListener('click', () => {
      const n = document.getElementById('largeSize').value;
      fetch('/large?size=' + encodeURIComponent(n));
    });
    document.getElementById('encTestBtn').addEventListener('click', () => {
      const n = document.getElementById('encTestSize').value;
      let url = '/enctest';
      if (n) url += '?size=' + encodeURIComponent(n);
      fetch(url);
    });
    document.getElementById('encTestBadBtn').addEventListener('click', () => {
      fetch('/enctestbad');
    });
    document.getElementById('msgIdBtn').addEventListener('click', () => {
      const n = document.getElementById('msgIdVal').value;
      let url = '/msgid';
      if (n) url += '?val=' + encodeURIComponent(n);
      fetch(url);
    });
    // Key exchange test button
    const keyTestBtn = document.getElementById('keyTestBtn');
    if (keyTestBtn) {
      keyTestBtn.addEventListener('click', () => {
        fetch('/keytest');
      });
    }
    // Key request and key send buttons
    const keyReqBtn = document.getElementById('keyReqBtn');
    if (keyReqBtn) {
      keyReqBtn.addEventListener('click', () => {
        fetch('/keyreq');
      });
    }
    const keySendBtn = document.getElementById('keySendBtn');
    if (keySendBtn) {
      keySendBtn.addEventListener('click', () => {
        fetch('/keysend');
      });
    }
    // Authenticated ECDH button
    const keyDhBtn = document.getElementById('keyDhBtn');
    if (keyDhBtn) {
      keyDhBtn.addEventListener('click', () => {
        fetch('/keydh');
      });
    }
    // Poll key status from the server and update indicator and text.  The
    // server returns JSON {"status":"local|remote","request":0|1}.  When
    // a key request is active the indicator blinks; otherwise it is solid.
    function updateKeyStatus() {
      fetch('/keystatus').then(r => r.json()).then(data => {
        const ind = document.getElementById('keyIndicator');
        const txt = document.getElementById('keyStatusText');
        if (!ind || !txt) return;
        if (data.status === 'local') {
          ind.classList.remove('remote');
          ind.classList.add('local');
          // Show 4‑digit key hash next to local status
          const hash = data.hash ? data.hash : '----';
          txt.textContent = 'Key: local (' + hash + ')';
        } else {
          ind.classList.remove('local');
          ind.classList.add('remote');
          txt.textContent = 'Key: remote';
        }
        if (parseInt(data.request) === 1) {
          ind.classList.add('blink');
        } else {
          ind.classList.remove('blink');
        }
      }).catch(() => {});
      setTimeout(updateKeyStatus, 1000);
    }
    updateKeyStatus();
    // QoS commands
    document.getElementById('sendQBtn').addEventListener('click', () => {
      const msg = document.getElementById('sendQMsg').value;
      const prio = document.getElementById('sendQPrio').value;
      if (msg) {
        fetch('/sendq?prio=' + encodeURIComponent(prio) + '&msg=' + encodeURIComponent(msg));
        document.getElementById('sendQMsg').value = '';
      }
    });
    document.getElementById('largeQBtn').addEventListener('click', () => {
      const sz = document.getElementById('largeQSize').value;
      const prio = document.getElementById('largeQPrio').value;
      fetch('/largeq?prio=' + encodeURIComponent(prio) + '&size=' + encodeURIComponent(sz));
    });
    document.getElementById('qosModeBtn').addEventListener('click', () => {
      const mode = document.getElementById('qosModeSelect').value;
      fetch('/qosmode?val=' + encodeURIComponent(mode));
    });
    document.getElementById('qosBtn').addEventListener('click', () => {
      fetch('/qos').then(r => r.text()).then(t => {
        document.getElementById('qosStats').textContent = t;
      });
    });
    // Start polling serial after DOM ready
    pollSerial();
  </script>
</body>
</html>
)rawliteral";
