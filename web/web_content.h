#pragma once
#include <pgmspace.h>
// Содержимое веб-интерфейса, встроенное в прошивку
// index.html, style.css, script.js и libs/sha256.js подключаются без конвертации

// index.html
const char INDEX_HTML[] PROGMEM = R"~~~(
<!DOCTYPE html>
<html lang="ru">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Sat Project Web</title>
  <link rel="stylesheet" href="style.css" />
</head>
<body>
  <div class="bg"><div class="grid"></div><div class="aurora"></div><div class="stars"></div></div>
  <!-- Шапка сайта с навигацией -->
  <header class="site-header glass">
    <div class="brand">sat<span class="tag">web</span></div>
    <nav class="nav" id="siteNav">
      <a href="#" data-tab="chat">Chat</a>
      <a href="#" data-tab="channels">Channels/Ping</a>
      <a href="#" data-tab="settings">Settings</a>
      <a href="#" data-tab="security">Security</a>
      <a href="#" data-tab="debug">Debug</a>
    </nav>
    <div class="header-actions">
      <div class="chip"><input id="endpoint" placeholder="endpoint" /></div>
      <button id="themeToggle" class="icon-btn" aria-label="Тема">🌓</button>
      <button id="menuToggle" class="icon-btn only-mobile" aria-label="Меню">☰</button>
    </div>
  </header>
  <main>
    <!-- Вкладка чата -->
    <section id="tab-chat" class="tab">
      <h2>Chat</h2>
      <div id="chatLog" class="chat-log"></div>
      <div class="chat-input">
        <input id="chatInput" type="text" placeholder="Сообщение" />
        <button id="sendBtn" class="btn">Отправить</button>
      </div>
      <div class="group-title"><span>Состояние ACK</span><span class="line"></span></div>
      <div class="status-row">
        <div class="chip state" id="ackStateChip" data-state="unknown">
          <span class="label">ACK</span>
          <span id="ackStateText">—</span>
        </div>
        <div class="ack-actions">
          <button class="btn ghost" id="btnAckOn">Включить</button>
          <button class="btn ghost" id="btnAckOff">Выключить</button>
          <button class="btn ghost" id="btnAckToggle">Переключить</button>
          <button class="btn ghost" id="btnAckRefresh">Обновить</button>
        </div>
      </div>
      <div class="group-title"><span>Команды</span><span class="line"></span></div>
      <div class="cmd-buttons actions">
        <button class="btn" data-cmd="INFO">INFO</button>
        <button class="btn" data-cmd="STS">STS</button>
        <button class="btn" data-cmd="RSTS">RSTS</button>
        <button class="btn" data-cmd="BCN">BCN</button>
        <button class="btn" data-cmd="ENCT">ENCT</button>
      </div>
      <div class="cmd-inline">
        <label for="txlSize">TXL (байт)</label>
        <input id="txlSize" type="number" min="1" max="8192" value="1024" />
        <button id="btnTxlSend" class="btn">Отправить тест</button>
      </div>
      <div class="group-title"><span>Принятые сообщения</span><span class="line"></span></div>
      <div class="received-panel">
        <div class="received-controls">
          <button id="btnRecvRefresh" class="btn ghost">Обновить список</button>
          <label class="chip switch">
            <input type="checkbox" id="recvAuto" />
            <span>Автообновление</span>
          </label>
          <label class="chip input-chip">
            <span>Предел</span>
            <input id="recvLimit" type="number" min="1" max="200" value="20" />
          </label>
        </div>
        <ul id="recvList" class="received-list"></ul>
        <div id="recvEmpty" class="muted small" hidden>Нет готовых сообщений</div>
      </div>
    </section>
    <!-- Вкладка каналов и пинга -->
    <section id="tab-channels" class="tab" hidden>
      <h2>Channels / Ping</h2>
      <div class="actions">
        <button id="btnPing" class="btn">Ping</button>
        <button id="btnSearch" class="btn">Search</button>
        <button id="btnRefresh" class="btn">Обновить</button>
        <button id="btnExportCsv" class="btn">CSV</button>
      </div>
      <div class="table-wrap pretty">
        <table id="channelsTable">
          <thead>
            <tr><th>#</th><th>CH</th><th>TX,MHz</th><th>RX,MHz</th><th>RSSI</th><th>SNR</th><th>ST</th><th>SCAN</th></tr>
          </thead>
          <tbody></tbody>
        </table>
      </div>
    </section>
    <!-- Вкладка настроек -->
    <section id="tab-settings" class="tab" hidden>
      <h2>Settings</h2>
      <form id="settingsForm" class="settings-form">
        <label><input type="checkbox" id="ACK" /> ACK</label>
        <label>Bank
          <select id="BANK">
            <option value="e">EAST</option>
            <option value="w">WEST</option>
            <option value="t">TEST</option>
            <option value="a">ALL</option>
          </select>
        </label>
        <label>BW (kHz)
          <select id="BF">
            <option value="7.8">7.8</option>
            <option value="10.4">10.4</option>
            <option value="15.6">15.6</option>
            <option value="20.8">20.8</option>
            <option value="31.25">31.25</option>
            <option value="41.7">41.7</option>
            <option value="62.5">62.5</option>
            <option value="125">125</option>
            <option value="250">250</option>
            <option value="500">500</option>
          </select>
        </label>
        <label>Channel
          <select id="CH"></select>
        </label>
        <label>CR
          <select id="CR">
            <option value="5">4/5</option>
            <option value="6">4/6</option>
            <option value="7">4/7</option>
            <option value="8">4/8</option>
          </select>
        </label>
        <label>Power
          <select id="PW">
            <option>2</option><option>3</option><option>4</option><option>5</option>
            <option>6</option><option>7</option><option>8</option><option>9</option>
            <option>10</option><option>11</option><option>12</option><option>13</option>
            <option>14</option><option>15</option><option>16</option><option>17</option>
            <option>18</option><option>19</option><option>20</option>
          </select>
        </label>
        <label>SF
          <select id="SF">
            <option>7</option><option>8</option><option>9</option><option>10</option><option>11</option><option>12</option>
          </select>
        </label>
        <div class="settings-actions actions">
          <button type="button" id="btnSaveSettings" class="btn">Сохранить</button>
          <button type="button" id="btnApplySettings" class="btn btn-primary">Применить</button>
          <button type="button" id="btnExportSettings" class="btn">Экспорт</button>
          <button type="button" id="btnImportSettings" class="btn">Импорт</button>
          <button type="button" id="btnClearCache" class="btn">Очистить</button>
        </div>
        <button type="button" id="INFO" class="btn" data-cmd="INFO">INFO</button>
      </form>
    </section>
    <!-- Вкладка безопасности -->
    <section id="tab-security" class="tab" hidden>
      <h2>Security</h2>
      <div>Состояние: <span id="keyState"></span></div>
      <div id="keyHash" class="small muted"></div>
      <div id="keyHex" class="mono small"></div>
      <div class="actions">
        <button id="btnKeyGen" class="btn">KEYGEN</button>
        <button id="btnKeySend" class="btn">KEYTRANSFER SEND</button>
        <button id="btnKeyRecv" class="btn">KEYTRANSFER RECEIVE</button>
      </div>
    </section>
    <!-- Вкладка отладочных логов -->
    <section id="tab-debug" class="tab" hidden>
      <h2>Debug</h2>
      <div id="debugLog" class="debug-log"></div>
    </section>
  </main>
  <footer>
    <div id="statusLine" class="small muted"></div>
  </footer>
  <div id="toast" class="toast" hidden></div>
  <script src="libs/sha256.js"></script>
  <script src="script.js"></script>
</body>
</html>

)~~~";

// style.css
const char STYLE_CSS[] PROGMEM = R"~~~(
/* satprjct redesign (rev2): Aurora + glass + depth — responsive with mobile dock */
/* Themes */
:root {
  --bg: #0b1224;
  --panel: #0d162e;
  --panel-2: #0b1430;
  --text: #e7ebff;
  --muted: #99a6c3;
  --accent: #22d3ee;
  --accent-2: #38bdf8;
  --danger: #ef4444;
  --good: #22c55e;
  --ring: rgba(34,211,238,.35);
  --ring-2: rgba(56,189,248,.25);
  color-scheme: dark;
}

:root.light {
  --bg: #f6f9ff;
  --panel: #ffffff;
  --panel-2: #f2f6ff;
  --text: #0e1325;
  --muted: #5a6a8a;
  --accent: #0ea5e9;
  --accent-2: #06b6d4;
  --danger: #dc2626;
  --good: #16a34a;
  --ring: rgba(14,165,233,.25);
  --ring-2: rgba(6,182,212,.15);
  color-scheme: light;
}

* { box-sizing: border-box; }
html, body { height: 100%; }
body {
  margin: 0;
  font-family: ui-sans-serif, system-ui, -apple-system, Segoe UI, Roboto, 'Noto Sans', Ubuntu, Cantarell, 'Helvetica Neue', Arial, 'Apple Color Emoji', 'Segoe UI Emoji';
  background: var(--bg);
  color: var(--text);
  line-height: 1.55;
  -webkit-font-smoothing: antialiased;
  -moz-osx-font-smoothing: grayscale;
}

/* Background decorations */
.bg { position: fixed; inset: 0; z-index: -2; overflow: clip; }
.bg .grid {
  position: absolute; inset: 0;
  background-image: linear-gradient(to right, rgba(255,255,255,.05) 1px, transparent 1px),
                    linear-gradient(to bottom, rgba(255,255,255,.04) 1px, transparent 1px);
  background-size: 40px 40px;
  mask-image: radial-gradient(900px 600px at 15% -10%, #000 40%, transparent 70%),
              radial-gradient(1200px 800px at 110% 10%, #000 40%, transparent 70%);
  opacity: .22;
}
.bg .aurora {
  position: absolute; inset: -20% -10% auto -10%; height: 60vh;
  background:
    radial-gradient(40% 60% at 15% 10%, rgba(34,211,238,.25), transparent 60%),
    radial-gradient(40% 50% at 85% 25%, rgba(56,189,248,.22), transparent 60%),
    radial-gradient(25% 40% at 55% -10%, rgba(99,102,241,.15), transparent 60%);
  filter: blur(40px);
  animation: float 18s ease-in-out infinite alternate;
}
.bg .stars::before, .bg .stars::after {
  content: ""; position: absolute; inset: 0;
  background-image: radial-gradient(2px 2px at 20% 30%, rgba(255,255,255,.5), transparent 50%),
                    radial-gradient(2px 2px at 70% 40%, rgba(255,255,255,.5), transparent 50%),
                    radial-gradient(1px 1px at 40% 80%, rgba(255,255,255,.4), transparent 50%),
                    radial-gradient(2px 2px at 85% 60%, rgba(255,255,255,.5), transparent 50%);
  opacity: .25;
  animation: drift 30s linear infinite;
}
@keyframes float { to { transform: translateY(-10px); } }
@keyframes drift { to { transform: translateY(-25px); } }

/* Utilities */
.muted { color: var(--muted); }
.small { font-size:.85rem; }
.mono { font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, 'Liberation Mono', 'Courier New', monospace; }
.codeblock { padding:.75rem; border-radius:.7rem; background: var(--panel-2); border:1px solid color-mix(in oklab, var(--panel-2) 75%, black 25%); }

a { color: inherit; }

/* Glass cards & chrome */
.glass {
  background: color-mix(in oklab, var(--panel) 88%, black 12%);
  backdrop-filter: blur(8px);
  border: 1px solid color-mix(in oklab, var(--panel-2) 70%, black 30%);
}
.card {
  position: relative;
  border-radius: 1rem;
  padding: 1rem;
  border: 1px solid color-mix(in oklab, var(--panel-2) 70%, black 30%);
  background:
    linear-gradient(180deg, color-mix(in oklab, var(--panel) 92%, black 8%), var(--panel));
  box-shadow: 0 10px 30px rgba(0,0,0,.18);
}
.card.layered::before {
  content: ""; position: absolute; inset: -1px;
  padding: 1px; border-radius: 1rem;
  background: conic-gradient(from 180deg at 50% 50%, var(--ring), var(--ring-2), transparent 30%);
  -webkit-mask: linear-gradient(#000 0 0) content-box, linear-gradient(#000 0 0);
  -webkit-mask-composite: xor; mask-composite: exclude;
  pointer-events: none;
  opacity: .6;
}

/* Header */
.site-header {
  position: sticky; top: 0; z-index: 40;
  display: grid;
  grid-template-columns: auto 1fr auto;
  gap: .75rem 1rem;
  align-items: center;
  padding: .6rem clamp(.75rem, 2vw, 1rem);
}
.brand { display:flex; gap:.6rem; align-items:center; font-weight:800; letter-spacing:.3px; }
.brand .tag { font-size:.75rem; padding:.15rem .45rem; border-radius:.5rem; background: var(--panel-2); border:1px solid color-mix(in oklab, var(--panel-2) 70%, black 30%); color: var(--muted); }

.nav { display:flex; gap:.5rem; flex-wrap:wrap; }
.nav a {
  display:inline-flex; gap:.5rem; align-items:center;
  padding: .45rem .7rem;
  border-radius: .7rem;
  text-decoration: none;
  color: var(--muted);
  font-weight: 700;
  position: relative;
}
.nav a[aria-current="page"] {
  color: var(--text);
  background: linear-gradient(180deg, color-mix(in oklab, var(--panel-2) 70%, white 30%), var(--panel-2));
  box-shadow: inset 0 0 0 1px color-mix(in oklab, var(--panel-2) 75%, black 25%);
}

/* Header controls */
.header-actions { display:flex; align-items:center; gap:.5rem; }
.chip {
  display:flex; align-items:center; gap:.35rem;
  background: var(--panel-2);
  border: 1px solid color-mix(in oklab, var(--panel-2) 70%, black 30%);
  border-radius: .6rem;
  padding: .35rem .5rem;
}
.chip input {
  width: 15rem; max-width: 40vw;
  background: transparent; border: none; outline: none; color: var(--text);
}
.icon-btn {
  background: var(--panel-2);
  color: var(--text);
  border: 1px solid color-mix(in oklab, var(--panel-2) 60%, white 40%);
  border-radius: .7rem;
  padding: .45rem .6rem;
  cursor: pointer;
}
.only-mobile { display:none; }

/* Hero */
.hero { margin: 1rem 0 1.25rem; }
.hero h1 { margin: 0 0 .25rem; font-size: clamp(1.25rem, 3.5vw, 1.75rem); }
.hero p { margin: 0; color: var(--muted); }

/* Section titles */
.group-title { display:flex; align-items:center; gap:.75rem; margin: .75rem 0 .6rem; }
.group-title .line { height: 1px; background: linear-gradient(90deg, var(--ring), transparent); flex:1; }

/* Layout */
main { padding: 1rem clamp(.75rem, 2vw, 1rem) calc(1rem + env(safe-area-inset-bottom)); }
.tab[hidden] { display:none; }

/* Chat */
.chat-log {
  height: min(52vh, 560px);
  overflow: auto;
  display:flex; flex-direction:column; gap:.55rem;
  padding:.5rem;
  border-radius: .8rem;
  background: var(--panel-2);
  border: 1px solid color-mix(in oklab, var(--panel-2) 70%, black 30%);
}
.msg { display:grid; grid-template-columns: auto 1fr; gap:.5rem; align-items:flex-start; }
.msg .avatar {
  width: 30px; height: 30px; border-radius: 999px;
  display:grid; place-items:center;
  font-weight:800; font-size:.75rem;
  background: linear-gradient(180deg, var(--accent), var(--accent-2));
  color: #001018; border: none;
}
.msg.dev .avatar { background: linear-gradient(180deg, #64748b, #334155); color: #e2e8f0; }
.msg time { color: var(--muted); font-size:.72rem; grid-column: 2/3; margin: .1rem 0 -.2rem; }
.msg .bubble {
  background: color-mix(in oklab, var(--panel) 92%, black 8%);
  padding:.6rem .7rem; border-radius: .9rem; border:1px solid color-mix(in oklab, var(--panel-2) 70%, black 30%);
  max-width: 85%;
  word-wrap: break-word;
  grid-column: 2/3;
  transition: transform .12s ease;
}
.msg.you .bubble { background: color-mix(in oklab, var(--accent) 16%, var(--panel) 84%); border-color: color-mix(in oklab, var(--accent) 35%, var(--panel) 65%); }
.msg .bubble:active { transform: scale(.99); }

.chat-input { display:flex; gap:.6rem; margin-top:.8rem; }
.chat-input button { min-width: 7.5rem; }
.chat-input-row { display:grid; gap:.6rem; grid-template-columns: 1fr; margin-top:.8rem; }
.cmd-buttons { display:flex; gap:.4rem; flex-wrap:wrap; }
.send-row { position: relative; display:flex; gap:.5rem; }
#chatInput {
  flex:1;
  background: var(--panel-2);
  color: var(--text);
  border: 1px solid transparent;
  border-radius: .9rem;
  padding: .7rem .9rem;
  outline: none;
}
#chatInput:focus { border-color: var(--ring); box-shadow: 0 0 0 3px var(--ring); }
.fab { border-radius: 999px; width: 46px; height: 46px; display:grid; place-items:center; padding: 0; }

/* Buttons */
.btn {
  background: color-mix(in oklab, var(--panel-2) 80%, white 20%);
  border: 1px solid color-mix(in oklab, var(--panel-2) 70%, white 30%);
  color: var(--text);
  padding: .55rem .85rem;
  border-radius: .8rem;
  cursor: pointer;
  font-weight: 700;
  transition: transform .08s ease, box-shadow .2s ease;
  box-shadow: 0 2px 0 rgba(0,0,0,.25);
}
.btn:active { transform: translateY(1px); box-shadow: 0 1px 0 rgba(0,0,0,.25); }
.btn.primary { background: linear-gradient(180deg, var(--accent), var(--accent-2)); color: #001018; border-color: transparent; }
.btn.soft { background: color-mix(in oklab, var(--panel-2) 90%, var(--accent) 10%); }
.btn.ghost { background: var(--panel-2); }
.btn.danger { background: linear-gradient(180deg, var(--danger), #b91c1c); color:#fff; border-color: transparent; }

/* Дополнительные блоки в чате */
.status-row { display:flex; flex-wrap:wrap; align-items:center; gap:.6rem; margin: .75rem 0; }
.chip.state {
  min-width: 6rem;
  justify-content: space-between;
  text-transform: uppercase;
  letter-spacing: .05em;
  font-weight: 700;
}
.chip.state .label { font-size:.72rem; color: var(--muted); }
.chip.state[data-state="on"] {
  background: color-mix(in oklab, var(--good) 28%, var(--panel-2) 72%);
  border-color: color-mix(in oklab, var(--good) 45%, var(--panel-2) 55%);
  color: #0f172a;
}
.chip.state[data-state="off"] {
  background: color-mix(in oklab, var(--danger) 24%, var(--panel-2) 76%);
  border-color: color-mix(in oklab, var(--danger) 45%, var(--panel-2) 55%);
}
.chip.state[data-state="unknown"] {
  background: var(--panel-2);
  border-color: color-mix(in oklab, var(--panel-2) 70%, black 30%);
}
.ack-actions { display:flex; flex-wrap:wrap; gap:.4rem; }
.ack-actions .btn { padding:.45rem .75rem; }
.cmd-inline {
  display:flex;
  flex-wrap:wrap;
  gap:.5rem;
  align-items:center;
  background: color-mix(in oklab, var(--panel-2) 92%, black 8%);
  border: 1px solid color-mix(in oklab, var(--panel-2) 70%, black 30%);
  border-radius: .8rem;
  padding: .6rem .75rem;
}
.cmd-inline label { font-weight:600; color: var(--muted); }
.cmd-inline input {
  width: 6.5rem;
  background: var(--bg);
  border: 1px solid color-mix(in oklab, var(--panel-2) 70%, black 30%);
  border-radius: .6rem;
  padding: .45rem .6rem;
  color: var(--text);
}
.cmd-inline input:focus { border-color: var(--ring); box-shadow: 0 0 0 3px var(--ring); outline: none; }

/* Tables */
.table-wrap { overflow:auto; border-radius:.8rem; border:1px solid color-mix(in oklab, var(--panel-2) 70%, black 30%); background: var(--panel-2); }
.table-wrap.pretty table thead th { position: sticky; top: 0; background: linear-gradient(180deg, var(--panel-2), color-mix(in oklab, var(--panel-2) 80%, white 20%)); }
table { border-collapse: collapse; width: 100%; min-width: 760px; }
th, td { text-align: left; padding: .6rem .7rem; border-bottom: 1px solid color-mix(in oklab, var(--panel-2) 70%, black 30%); }
tbody tr:nth-child(odd) { background: color-mix(in oklab, var(--panel-2) 90%, white 10%); }
/* подсветка статусов каналов */
tbody tr.active { background: color-mix(in oklab, var(--accent-2) 35%, transparent); }
tbody tr.busy { background: color-mix(in oklab, var(--danger) 25%, transparent); }
tbody tr.free { background: color-mix(in oklab, var(--good) 20%, transparent); }
tbody tr.unknown { opacity:.6; }
/* подсветка процесса и итогов сканирования */
/* используем color-mix для согласования с темой */
tbody tr.scanning { background: color-mix(in oklab, var(--accent-2) 15%, white); /* ~#e0f2fe, голубой фон */ }
tbody tr.signal { background: color-mix(in oklab, var(--good) 15%, white); /* ~#dcfce7, зелёный фон */ }
tbody tr.crc-error { background: color-mix(in oklab, #f97316 20%, white); /* ~#fed7aa, оранжевый фон */ }
tbody tr.no-response { background: color-mix(in oklab, var(--muted) 15%, white); color:#374151; /* ~#e5e7eb, серый фон и тёмный текст */ }
.status-chip {
  display:inline-block; padding:.15rem .45rem; border-radius:.6rem; font-size:.78rem; font-weight:700;
  border:1px solid transparent;
}
.status-chip.idle { background: #0e7490; color:#e0f2fe; border-color:#164e63; }
.status-chip.listen { background: #1e3a8a; color:#dbeafe; border-color:#1e40af; }
.status-chip.tx { background: #166534; color:#dcfce7; border-color:#14532d; }

/* Forms */
.grid-form { display:grid; gap: .9rem; }
.grid-form fieldset { border:1px solid color-mix(in oklab, var(--panel-2) 75%, black 25%); border-radius:.9rem; padding:.8rem; }
.grid-form legend { padding: 0 .35rem; color: var(--muted); }
.grid-form .field { display:flex; flex-direction:column; gap:.35rem; }
.grid-form .actions { display:flex; flex-wrap:wrap; gap:.5rem; }
.grid-form .full { grid-column: 1 / -1; }
.grid-form input[type="text"], .grid-form input[type="number"], .grid-form select {
  background: var(--panel-2);
  color: var(--text);
  border: 1px solid transparent;
  border-radius: .7rem;
  padding: .6rem .7rem;
  outline: none;
}
.grid-form input:focus, .grid-form select:focus { border-color: var(--ring); box-shadow: 0 0 0 3px var(--ring); }
.switch { display:flex; align-items:center; gap:.6rem; }

/* Security */
.key-grid { display:grid; grid-template-columns: 1fr auto; gap:.75rem; align-items:center; }
.key-title { color: var(--muted); font-weight:700; }
.key-pill {
  display:inline-block; padding:.35rem .6rem; border-radius:.7rem;
  background: linear-gradient(180deg, color-mix(in oklab, var(--panel-2) 80%, white 20%), var(--panel-2));
  box-shadow: inset 0 0 0 1px color-mix(in oklab, var(--panel-2) 75%, black 25%);
  font-weight:800; letter-spacing:.5px;
}

/* Footer */
.site-footer {
  padding: .85rem clamp(.75rem, 2vw, 1rem) calc(.85rem + env(safe-area-inset-bottom));
  color: var(--muted);
  border-top: 1px solid color-mix(in oklab, var(--panel-2) 75%, black 25%);
}

/* Toast */
.toast {
  position: fixed;
  left: 50%; transform: translateX(-50%);
  bottom: calc(1.1rem + env(safe-area-inset-bottom));
  padding: .7rem .9rem;
  border-radius: .8rem;
  background: var(--panel);
  border:1px solid color-mix(in oklab, var(--panel-2) 70%, black 30%);
  box-shadow: 0 10px 30px rgba(0,0,0,.35);
}
.toast.show { animation: fade 2.2s ease; }
@keyframes fade {
  0% { opacity: 0; transform: translate(-50%, 10px); }
  10%,80% { opacity: 1; transform: translate(-50%, 0); }
  100% { opacity: 0; transform: translate(-50%, 0); }
}

/* Общие блоки действий */
.actions { display:flex; flex-wrap:wrap; gap:.5rem; margin:.5rem 0; }

/* Форма настроек радиомодуля */
.settings-form { display:grid; gap:.75rem; max-width:420px; }
.settings-form label { display:flex; flex-direction:column; gap:.25rem; font-weight:600; }
.settings-actions { margin-top:.5rem; }

/* Mobile dock */
.dock {
  position: fixed; left: 0; right: 0; bottom: 0;
  display:flex; justify-content:space-around; gap:.5rem;
  padding: .5rem max(.75rem, env(safe-area-inset-left)) calc(.5rem + env(safe-area-inset-bottom)) max(.75rem, env(safe-area-inset-right));
  background: color-mix(in oklab, var(--panel) 85%, black 15%);
  border-top: 1px solid color-mix(in oklab, var(--panel-2) 75%, black 25%);
  backdrop-filter: blur(8px);
}
.dock button {
  flex:1; max-width: 80px; aspect-ratio: 1 / 1;
  border-radius: .9rem;
  background: var(--panel-2);
  border:1px solid color-mix(in oklab, var(--panel-2) 70%, black 30%);
  font-size: 1.15rem;
}
.dock button.active { background: linear-gradient(180deg, var(--accent), var(--accent-2)); color: #001018; border-color: transparent; }

/* Accessibility & misc */
.skip-link { position:absolute; left:-1000px; }
.skip-link:focus { left: 8px; top: 8px; background: var(--accent); color:#001018; padding:.4rem .5rem; border-radius:.4rem; }

/* Responsive */
@media (max-width: 860px) {
  .only-mobile { display:block; }
  .nav { position: fixed; inset: 3.25rem .75rem auto .75rem; z-index: 50;
         background: var(--panel); border-radius:.7rem; padding:.5rem;
         border:1px solid color-mix(in oklab, var(--panel-2) 70%, black 30%); }
  .nav { display:none; flex-direction:column; }
  .nav.open { display:flex; }
  .chip input { width: 42vw; }
  table { min-width: 640px; }
  .key-grid { grid-template-columns: 1fr; }
}

/* Отладочный вывод */
.debug-log {
  height: min(52vh, 560px);
  overflow: auto;
  display: flex;
  flex-direction: column;
  gap: .25rem;
  padding: .5rem;
  border-radius: .8rem;
  background: var(--panel-2);
  border: 1px solid color-mix(in oklab, var(--panel-2) 70%, black 30%);
  font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, 'Liberation Mono', 'Courier New', monospace;
  font-size: .85rem;
}
)~~~";

// libs/sha256.js — библиотека SHA-256 на чистом JavaScript
const char SHA256_JS[] PROGMEM = R"~~~(
/* Простая реализация SHA-256 на чистом JS.
 * Используется, когда WebCrypto недоступен (например, на HTTP).
 */
(function(global){
  function rotr(x,n){ return (x>>>n) | (x<<(32-n)); }
  function sha256Bytes(bytes){
    const K = new Uint32Array([
      0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
      0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
      0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
      0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
      0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
      0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
      0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
      0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    ]);
    const H = new Uint32Array([
      0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
      0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19
    ]);
    const l = bytes.length;
    const bitLen = l * 8;
    const paddedLen = (((l + 9) >> 6) + 1) << 6; // длина с дополнением кратная 64
    const buffer = new Uint8Array(paddedLen);
    buffer.set(bytes);
    buffer[l] = 0x80; // добавляем бит "1"
    const dv = new DataView(buffer.buffer);
    dv.setUint32(paddedLen - 8, Math.floor(bitLen / 4294967296));
    dv.setUint32(paddedLen - 4, bitLen >>> 0);
    const w = new Uint32Array(64);
    for (let i = 0; i < paddedLen; i += 64) {
      for (let j = 0; j < 16; j++) w[j] = dv.getUint32(i + j*4);
      for (let j = 16; j < 64; j++) {
        const s0 = rotr(w[j-15],7) ^ rotr(w[j-15],18) ^ (w[j-15]>>>3);
        const s1 = rotr(w[j-2],17) ^ rotr(w[j-2],19) ^ (w[j-2]>>>10);
        w[j] = (w[j-16] + s0 + w[j-7] + s1) >>> 0;
      }
      let a=H[0],b=H[1],c=H[2],d=H[3],e=H[4],f=H[5],g=H[6],h=H[7];
      for (let j=0; j<64; j++) {
        const S1 = rotr(e,6) ^ rotr(e,11) ^ rotr(e,25);
        const ch = (e & f) ^ (~e & g);
        const t1 = (h + S1 + ch + K[j] + w[j]) >>> 0;
        const S0 = rotr(a,2) ^ rotr(a,13) ^ rotr(a,22);
        const maj = (a & b) ^ (a & c) ^ (b & c);
        const t2 = (S0 + maj) >>> 0;
        h=g; g=f; f=e; e=(d + t1)>>>0; d=c; c=b; b=a; a=(t1 + t2)>>>0;
      }
      H[0]=(H[0]+a)>>>0; H[1]=(H[1]+b)>>>0; H[2]=(H[2]+c)>>>0; H[3]=(H[3]+d)>>>0;
      H[4]=(H[4]+e)>>>0; H[5]=(H[5]+f)>>>0; H[6]=(H[6]+g)>>>0; H[7]=(H[7]+h)>>>0;
    }
    return Array.from(H).map(x=>x.toString(16).padStart(8,"0")).join("");
  }
  // экспорт в глобальный объект
  global.sha256Bytes = sha256Bytes;
})(this);
)~~~";

// script.js
const char SCRIPT_JS[] PROGMEM = R"~~~(
/* satprjct web/app.js — vanilla JS only */
/* Состояние интерфейса */
const UI = {
  tabs: ["chat", "channels", "settings", "security", "debug"],
  els: {},
  cfg: {
    endpoint: localStorage.getItem("endpoint") || "http://192.168.4.1",
    theme: localStorage.getItem("theme") || (matchMedia("(prefers-color-scheme: light)").matches ? "light" : "dark"),
  },
  key: {
    bytes: null,
  },
  state: {
    channel: null,
    ack: null,
  }
};

document.addEventListener("DOMContentLoaded", init);

function $(sel, root) {
  return (root || document).querySelector(sel);
}
function $all(sel, root) {
  return Array.from((root || document).querySelectorAll(sel));
}

async function init() {
  // Базовые элементы интерфейса
  UI.els.menuToggle = $("#menuToggle");
  UI.els.nav = $("#siteNav");
  UI.els.endpoint = $("#endpoint");
  UI.els.themeToggle = $("#themeToggle");
  UI.els.status = $("#statusLine");
  UI.els.toast = $("#toast");
  UI.els.debugLog = $("#debugLog");
  UI.els.chatLog = $("#chatLog");
  UI.els.chatInput = $("#chatInput");
  UI.els.sendBtn = $("#sendBtn");
  UI.els.ackChip = $("#ackStateChip");
  UI.els.ackText = $("#ackStateText");
  UI.els.txlInput = $("#txlSize");

  // Навигация по вкладкам
  const hash = location.hash ? location.hash.slice(1) : "";
  const initialTab = UI.tabs.includes(hash) ? hash : (localStorage.getItem("activeTab") || "chat");
  for (const tab of UI.tabs) {
    const link = UI.els.nav ? UI.els.nav.querySelector('[data-tab="' + tab + '"]') : null;
    if (link) {
      link.addEventListener("click", (event) => {
        event.preventDefault();
        setTab(tab);
        history.replaceState(null, "", "#" + tab);
      });
    }
  }
  setTab(initialTab);

  // Меню на мобильных устройствах
  if (UI.els.menuToggle && UI.els.nav) {
    UI.els.menuToggle.addEventListener("click", () => {
      const open = !UI.els.nav.classList.contains("open");
      UI.els.menuToggle.setAttribute("aria-expanded", String(open));
      UI.els.nav.classList.toggle("open", open);
    });
    UI.els.nav.addEventListener("click", () => UI.els.nav.classList.remove("open"));
  }

  // Тема оформления
  applyTheme(UI.cfg.theme);
  if (UI.els.themeToggle) {
    UI.els.themeToggle.addEventListener("click", () => toggleTheme());
  }
  const themeSwitch = $("#themeSwitch");
  if (themeSwitch) {
    themeSwitch.checked = UI.cfg.theme === "dark" ? true : false;
    themeSwitch.addEventListener("change", () => toggleTheme());
  }

  // Настройка endpoint
  if (UI.els.endpoint) {
    UI.els.endpoint.value = UI.cfg.endpoint;
    UI.els.endpoint.addEventListener("change", () => {
      UI.cfg.endpoint = UI.els.endpoint.value.trim();
      localStorage.setItem("endpoint", UI.cfg.endpoint);
      note("Endpoint: " + UI.cfg.endpoint);
    });
  }

  // Чат и быстрые команды
  if (UI.els.sendBtn) UI.els.sendBtn.addEventListener("click", onSendChat);
  if (UI.els.chatInput) {
    UI.els.chatInput.addEventListener("keydown", (event) => {
      if (event.key === "Enter") onSendChat();
    });
  }
  $all('[data-cmd]').forEach((btn) => {
    const cmd = btn.dataset ? btn.dataset.cmd : null;
    if (cmd) btn.addEventListener("click", () => sendCommand(cmd));
  });

  // Управление ACK и тестами
  const btnAckOn = $("#btnAckOn"); if (btnAckOn) btnAckOn.addEventListener("click", () => setAck(true));
  const btnAckOff = $("#btnAckOff"); if (btnAckOff) btnAckOff.addEventListener("click", () => setAck(false));
  const btnAckToggle = $("#btnAckToggle"); if (btnAckToggle) btnAckToggle.addEventListener("click", toggleAck);
  const btnAckRefresh = $("#btnAckRefresh"); if (btnAckRefresh) btnAckRefresh.addEventListener("click", () => refreshAckState());
  const btnTxl = $("#btnTxlSend"); if (btnTxl) btnTxl.addEventListener("click", sendTxl);

  // Вкладка каналов
  const btnPing = $("#btnPing"); if (btnPing) btnPing.addEventListener("click", runPing);
  const btnSearch = $("#btnSearch"); if (btnSearch) btnSearch.addEventListener("click", runSearch);
  const btnRefresh = $("#btnRefresh"); if (btnRefresh) btnRefresh.addEventListener("click", () => refreshChannels());
  const btnExportCsv = $("#btnExportCsv"); if (btnExportCsv) btnExportCsv.addEventListener("click", exportChannelsCsv);

  loadChatHistory();

  // Настройки
  const btnSaveSettings = $("#btnSaveSettings"); if (btnSaveSettings) btnSaveSettings.addEventListener("click", saveSettingsLocal);
  const btnApplySettings = $("#btnApplySettings"); if (btnApplySettings) btnApplySettings.addEventListener("click", applySettingsToDevice);
  const btnExportSettings = $("#btnExportSettings"); if (btnExportSettings) btnExportSettings.addEventListener("click", exportSettings);
  const btnImportSettings = $("#btnImportSettings"); if (btnImportSettings) btnImportSettings.addEventListener("click", importSettings);
  const btnClearCache = $("#btnClearCache"); if (btnClearCache) btnClearCache.addEventListener("click", clearCaches);
  loadSettings();
  const bankSel = $("#BANK"); if (bankSel) bankSel.addEventListener("change", () => refreshChannels());

  // Безопасность
  const btnKeyGen = $("#btnKeyGen"); if (btnKeyGen) btnKeyGen.addEventListener("click", generateKey);
  const btnKeySend = $("#btnKeySend"); if (btnKeySend) btnKeySend.addEventListener("click", () => note("KEYTRANSFER SEND: заглушка"));
  const btnKeyRecv = $("#btnKeyRecv"); if (btnKeyRecv) btnKeyRecv.addEventListener("click", () => note("KEYTRANSFER RECEIVE: заглушка"));
  await loadKeyFromStorage();
  await updateKeyUI();

  await refreshChannels().catch(() => {});
  await refreshAckState();

  // Пробное подключение
  probe().catch(() => {});
}

/* Вкладки */
function setTab(tab) {
  for (const t of UI.tabs) {
    const panel = $("#tab-" + t);
    const link = UI.els.nav ? UI.els.nav.querySelector('[data-tab="' + t + '"]') : null;
    const active = (t === tab);
    if (panel) {
      panel.hidden = !active;
      panel.classList.toggle("current", active);
    }
    if (link) link.setAttribute("aria-current", active ? "page" : "false");
  }
  localStorage.setItem("activeTab", tab);
}

/* Тема */
function applyTheme(mode) {
  UI.cfg.theme = mode === "light" ? "light" : "dark";
  document.documentElement.classList.toggle("light", UI.cfg.theme === "light");
  localStorage.setItem("theme", UI.cfg.theme);
}
function toggleTheme() {
  applyTheme(UI.cfg.theme === "dark" ? "light" : "dark");
}

/* Работа чата */
function loadChatHistory() {
  const raw = localStorage.getItem("chatHistory") || "[]";
  let entries;
  try {
    entries = JSON.parse(raw);
    if (!Array.isArray(entries)) entries = [];
  } catch (e) {
    entries = [];
  }
  if (UI.els.chatLog) UI.els.chatLog.innerHTML = "";
  entries.forEach((entry) => addChatMessage(entry));
}
function persistChat(message, author) {
  const raw = localStorage.getItem("chatHistory") || "[]";
  let entries;
  try {
    entries = JSON.parse(raw);
    if (!Array.isArray(entries)) entries = [];
  } catch (e) {
    entries = [];
  }
  entries.push({ t: Date.now(), a: author, m: message });
  localStorage.setItem("chatHistory", JSON.stringify(entries.slice(-500)));
}
function addChatMessage(entry) {
  if (!UI.els.chatLog) return;
  const data = typeof entry === "string" ? { t: Date.now(), a: "dev", m: entry } : entry;
  const wrap = document.createElement("div");
  wrap.className = "msg " + (data.a === "you" ? "you" : "dev");
  const time = document.createElement("time");
  const stamp = data.t ? new Date(data.t) : new Date();
  time.dateTime = stamp.toISOString();
  time.textContent = stamp.toLocaleTimeString();
  const bubble = document.createElement("div");
  bubble.className = "bubble";
  setBubbleText(bubble, data.m);
  wrap.appendChild(time);
  wrap.appendChild(bubble);
  UI.els.chatLog.appendChild(wrap);
  UI.els.chatLog.scrollTop = UI.els.chatLog.scrollHeight;
}
function setBubbleText(node, text) {
  const value = text == null ? "" : String(text);
  const parts = value.split(/\r?\n/);
  node.innerHTML = "";
  for (let i = 0; i < parts.length; i++) {
    if (i > 0) node.appendChild(document.createElement("br"));
    node.appendChild(document.createTextNode(parts[i]));
  }
}
async function onSendChat() {
  if (!UI.els.chatInput) return;
  const text = UI.els.chatInput.value.trim();
  if (!text) return;
  UI.els.chatInput.value = "";
  const isCommand = text.startsWith("/");
  persistChat(text, "you");
  addChatMessage({ t: Date.now(), a: "you", m: text });
  if (isCommand) {
    const parsed = parseSlashCommand(text.slice(1));
    if (!parsed) {
      note("Команда не распознана");
      return;
    }
    if (parsed.cmd === "TX") {
      await sendTextMessage(parsed.message || "");
    } else {
      await sendCommand(parsed.cmd, parsed.params || {});
    }
  } else {
    await sendTextMessage(text);
  }
}
function parseSlashCommand(raw) {
  const trimmed = raw.trim();
  if (!trimmed) return null;
  const tokens = trimmed.split(/\s+/);
  const base = tokens.shift();
  if (!base) return null;
  const cmd = base.toUpperCase();
  const params = {};
  const freeParts = [];
  for (const tok of tokens) {
    const eq = tok.indexOf("=");
    if (eq > 0) {
      const key = tok.slice(0, eq);
      const value = tok.slice(eq + 1);
      if (key) params[key] = value;
    } else {
      freeParts.push(tok);
    }
  }
  if (freeParts.length) {
    const rest = freeParts.join(" ");
    if (cmd === "STS" || cmd === "RSTS") {
      params.n = rest;
    } else if (cmd === "ACK") {
      const low = rest.toLowerCase();
      if (low === "toggle" || low === "t") params.toggle = "1";
      else params.v = rest;
    } else if (cmd === "TXL") {
      params.size = rest;
    } else if (cmd === "TX") {
      return { cmd, params, message: rest };
    } else {
      params.v = rest;
    }
  }
  return { cmd, params, message: "" };
}

/* Команды устройства */
async function deviceFetch(cmd, params, timeoutMs) {
  const timeout = timeoutMs || 4000;
  let base;
  try {
    base = new URL(UI.cfg.endpoint || "http://192.168.4.1");
  } catch (e) {
    base = new URL("http://192.168.4.1");
  }
  const candidates = [
    new URL("/cmd", base),
    new URL("/api/cmd", base),
    new URL("/", base),
  ];
  const extras = new URLSearchParams();
  if (params) {
    for (const key in params) {
      if (Object.prototype.hasOwnProperty.call(params, key)) {
        const value = params[key];
        if (value != null && value !== "") extras.set(key, value);
      }
    }
  }
  let lastErr = null;
  for (const url of candidates) {
    let requestUrl = url.toString();
    if (cmd) {
      if (url.pathname.endsWith("/api/cmd")) {
        requestUrl += (requestUrl.indexOf("?") >= 0 ? "&" : "?") + "cmd=" + encodeURIComponent(cmd);
      } else {
        requestUrl += (requestUrl.indexOf("?") >= 0 ? "&" : "?") + "c=" + encodeURIComponent(cmd);
      }
    }
    const extra = extras.toString();
    if (extra) requestUrl += (requestUrl.indexOf("?") >= 0 ? "&" : "?") + extra;
    const ctrl = new AbortController();
    const timer = setTimeout(() => ctrl.abort(), timeout);
    try {
      const res = await fetch(requestUrl, { signal: ctrl.signal });
      clearTimeout(timer);
      if (!res.ok) {
        lastErr = "HTTP " + res.status;
        continue;
      }
      const text = await res.text();
      if (/<!DOCTYPE|<html/i.test(text)) {
        lastErr = "HTML response";
        continue;
      }
      return { ok: true, text: text, url: requestUrl };
    } catch (e) {
      clearTimeout(timer);
      lastErr = String(e);
    }
  }
  return { ok: false, error: lastErr || "Unknown error" };
}
async function sendCommand(cmd, params, opts) {
  const options = opts || {};
  const silent = options.silent === true;
  const timeout = options.timeoutMs || 4000;
  const payload = params || {};
  if (!silent) status("→ " + cmd);
  const res = await deviceFetch(cmd, payload, timeout);
  if (res.ok) {
    if (!silent) {
      status("✓ " + cmd);
      note(cmd + ": " + res.text.slice(0, 200));
      persistChat(cmd + ": " + res.text, "dev");
      addChatMessage({ t: Date.now(), a: "dev", m: cmd + ": " + res.text });
    }
    debugLog(cmd + ": " + res.text);
    handleCommandSideEffects(cmd, res.text);
    return res.text;
  }
  if (!silent) {
    status("✗ " + cmd);
    note("Ошибка: " + res.error);
    persistChat("ERR " + cmd + ": " + res.error, "dev");
    addChatMessage({ t: Date.now(), a: "dev", m: "ERR " + cmd + ": " + res.error });
  }
  debugLog("ERR " + cmd + ": " + res.error);
  return null;
}
async function postTx(text, timeoutMs) {
  const timeout = timeoutMs || 5000;
  let base;
  try {
    base = new URL(UI.cfg.endpoint || "http://192.168.4.1");
  } catch (e) {
    base = new URL("http://192.168.4.1");
  }
  const url = new URL("/api/tx", base);
  const ctrl = new AbortController();
  const timer = setTimeout(() => ctrl.abort(), timeout);
  try {
    const res = await fetch(url.toString(), {
      method: "POST",
      headers: { "Content-Type": "text/plain;charset=utf-8" },
      body: text,
      signal: ctrl.signal,
    });
    clearTimeout(timer);
    const body = await res.text();
    if (!res.ok) {
      return { ok: false, error: body || ("HTTP " + res.status) };
    }
    if (/<!DOCTYPE|<html/i.test(body)) {
      return { ok: false, error: "HTML response" };
    }
    return { ok: true, text: body };
  } catch (e) {
    clearTimeout(timer);
    return { ok: false, error: String(e) };
  }
}
async function sendTextMessage(text) {
  const payload = (text || "").trim();
  if (!payload) {
    note("Пустое сообщение");
    return null;
  }
  status("→ TX");
  const res = await postTx(payload, 6000);
  if (res.ok) {
    status("✓ TX");
    note("TX: " + res.text);
    persistChat("TX: " + res.text, "dev");
    addChatMessage({ t: Date.now(), a: "dev", m: "TX: " + res.text });
    debugLog("TX: " + res.text);
    return res.text;
  }
  status("✗ TX");
  note("Ошибка TX: " + res.error);
  persistChat("TX ERR: " + res.error, "dev");
  addChatMessage({ t: Date.now(), a: "dev", m: "TX ERR: " + res.error });
  debugLog("ERR TX: " + res.error);
  return null;
}
function handleCommandSideEffects(cmd, text) {
  const upper = cmd.toUpperCase();
  if (upper === "ACK") {
    const state = parseAckResponse(text);
    if (state !== null) {
      UI.state.ack = state;
      updateAckUi();
    }
  } else if (upper === "CH") {
    const value = parseInt(text, 10);
    if (!isNaN(value)) {
      UI.state.channel = value;
      updateChannelSelect();
      renderChannels();
    }
  } else if (upper === "PI") {
    applyPingResult(text);
  } else if (upper === "SEAR") {
    applySearchResult(text);
  } else if (upper === "BANK") {
    if (text && text.length === 1) {
      const bankSel = $("#BANK");
      if (bankSel) bankSel.value = text;
    }
  }
}
async function probe() {
  const res = await deviceFetch("PI", {}, 2500);
  if (res.ok) {
    status("Подключено: " + UI.cfg.endpoint);
  } else {
    status("Оффлайн · endpoint: " + UI.cfg.endpoint);
  }
}

/* Таблица каналов */
let channels = [];
function mockChannels() {
  channels = [
    { ch: 1, tx: 868.1, rx: 868.1, rssi: -92, snr: 8.5, st: "idle", scan: "" },
    { ch: 2, tx: 868.3, rx: 868.3, rssi: -97, snr: 7.1, st: "listen", scan: "" },
    { ch: 3, tx: 868.5, rx: 868.5, rssi: -88, snr: 10.2, st: "tx", scan: "" },
  ];
}
function renderChannels() {
  const tbody = $("#channelsTable tbody");
  if (!tbody) return;
  tbody.innerHTML = "";
  channels.forEach((c, idx) => {
    const tr = document.createElement("tr");
    const status = (c.st || "").toLowerCase();
    const stCls = status === "tx" || status === "listen" ? "busy" : status === "idle" ? "free" : "unknown";
    tr.classList.add(stCls);
    if (UI.state.channel === c.ch) tr.classList.add("active");
    const scanText = c.scan || "";
    const scanLower = scanText.toLowerCase();
    if (scanLower.indexOf("crc") >= 0) tr.classList.add("crc-error");
    else if (scanLower.indexOf("timeout") >= 0 || scanLower.indexOf("тайм") >= 0 || scanLower.indexOf("noresp") >= 0) tr.classList.add("no-response");
    else if (scanLower) tr.classList.add("signal");
    tr.innerHTML = "<td>" + (idx + 1) + "</td>" +
                   "<td>" + c.ch + "</td>" +
                   "<td>" + c.tx.toFixed(3) + "</td>" +
                   "<td>" + c.rx.toFixed(3) + "</td>" +
                   "<td>" + (isNaN(c.rssi) ? "" : c.rssi) + "</td>" +
                   "<td>" + (isNaN(c.snr) ? "" : c.snr) + "</td>" +
                   "<td>" + (c.st || "") + "</td>" +
                   "<td>" + scanText + "</td>";
    tbody.appendChild(tr);
  });
}
function updateChannelSelect() {
  const sel = $("#CH");
  if (!sel) return;
  const prev = UI.state.channel != null ? String(UI.state.channel) : sel.value;
  sel.innerHTML = "";
  channels.forEach((c) => {
    const opt = document.createElement("option");
    opt.value = c.ch;
    opt.textContent = c.ch;
    sel.appendChild(opt);
  });
  if (prev && channels.some((c) => String(c.ch) === prev)) sel.value = prev;
}
async function refreshChannels() {
  const bankSel = $("#BANK");
  const bank = bankSel ? bankSel.value : null;
  try {
    if (bank) await deviceFetch("BANK", { v: bank }, 2500);
    const list = await deviceFetch("CHLIST", bank ? { bank: bank } : {}, 4000);
    if (list.ok && list.text) {
      const parsed = parseChannels(list.text);
      if (parsed.length) {
        channels = parsed;
      } else if (!channels.length) {
        mockChannels();
      }
    } else if (!channels.length) {
      mockChannels();
    }
    const current = await deviceFetch("CH", {}, 2000);
    if (current.ok && current.text) {
      const num = parseInt(current.text, 10);
      if (!isNaN(num)) UI.state.channel = num;
    }
  } catch (e) {
    if (!channels.length) mockChannels();
    debugLog("ERR refreshChannels: " + e);
  }
  renderChannels();
  updateChannelSelect();
}
function parseChannels(text) {
  const out = [];
  const lines = text.split(/\r?\n/);
  for (let i = 0; i < lines.length; i++) {
    const line = lines[i].trim();
    if (!line || /ch\s*,/i.test(line)) continue;
    const parts = line.split(/\s*[,;|\t]\s*/);
    if (parts.length < 3) continue;
    let ch, tx, rx, rssi, snr, st, scan;
    if (parts.length >= 10) {
      ch = parts[0]; tx = parts[1]; rx = parts[2]; rssi = parts[7]; snr = parts[8]; st = parts[9]; scan = parts[10];
    } else if (parts.length >= 7) {
      [ch, tx, rx, rssi, snr, st, scan] = parts;
    } else {
      ch = parts[0]; tx = parts[1]; rx = parts[1]; rssi = parts[2]; snr = parts[3]; st = parts[4]; scan = "";
    }
    out.push({
      ch: Number(ch),
      tx: Number(tx),
      rx: Number(rx),
      rssi: Number(rssi || 0),
      snr: Number(snr || 0),
      st: st || "",
      scan: scan || "",
    });
  }
  return out;
}
function applyPingResult(text) {
  if (UI.state.channel == null) return;
  const entry = channels.find((c) => c.ch === UI.state.channel);
  if (!entry) return;
  const rssiMatch = text.match(/RSSI\s*(-?\d+(?:\.\d+)?)/i);
  const snrMatch = text.match(/SNR\s*(-?\d+(?:\.\d+)?)/i);
  if (rssiMatch) entry.rssi = Number(rssiMatch[1]);
  if (snrMatch) entry.snr = Number(snrMatch[1]);
  entry.scan = text.trim();
  renderChannels();
}
function applySearchResult(text) {
  const lines = text.split(/\r?\n/);
  let changed = false;
  for (let i = 0; i < lines.length; i++) {
    const line = lines[i].trim();
    if (!line) continue;
    const match = line.match(/CH\s*(\d+)\s*:\s*(.+)/i);
    if (!match) continue;
    const ch = Number(match[1]);
    const info = match[2];
    const entry = channels.find((c) => c.ch === ch);
    if (!entry) continue;
    entry.scan = info;
    const rssiMatch = info.match(/RSSI\s*(-?\d+(?:\.\d+)?)/i);
    const snrMatch = info.match(/SNR\s*(-?\d+(?:\.\d+)?)/i);
    if (rssiMatch) entry.rssi = Number(rssiMatch[1]);
    if (snrMatch) entry.snr = Number(snrMatch[1]);
    changed = true;
  }
  if (changed) renderChannels();
}
function exportChannelsCsv() {
  const lines = [["idx","ch","tx","rx","rssi","snr","status","scan"]];
  channels.forEach((c, idx) => {
    lines.push([idx + 1, c.ch, c.tx, c.rx, c.rssi, c.snr, c.st, c.scan]);
  });
  const csv = lines.map((row) => row.join(",")).join("\n");
  const blob = new Blob([csv], { type: "text/csv" });
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url;
  a.download = "channels.csv";
  a.click();
  URL.revokeObjectURL(url);
}
async function runPing() {
  await sendCommand("PI");
}
async function runSearch() {
  await sendCommand("SEAR");
}
async function sendTxl() {
  if (!UI.els.txlInput) return;
  const value = UI.els.txlInput.value.trim();
  if (!value) {
    note("Укажите размер пакета");
    return;
  }
  const size = parseInt(value, 10);
  if (isNaN(size) || size <= 0) {
    note("Некорректный размер");
    return;
  }
  await sendCommand("TXL", { size: String(size) });
}

/* ACK */
function parseAckResponse(text) {
  if (!text) return null;
  const low = text.toLowerCase();
  if (low.indexOf("ack:1") >= 0 || /\b1\b/.test(low) || low.indexOf("on") >= 0 || low.indexOf("включ") >= 0) return true;
  if (low.indexOf("ack:0") >= 0 || /\b0\b/.test(low) || low.indexOf("off") >= 0 || low.indexOf("выключ") >= 0) return false;
  return null;
}
function updateAckUi() {
  const chip = UI.els.ackChip;
  const text = UI.els.ackText;
  const state = UI.state.ack;
  const mode = state === true ? "on" : state === false ? "off" : "unknown";
  if (chip) chip.setAttribute("data-state", mode);
  if (text) text.textContent = state === true ? "ON" : state === false ? "OFF" : "—";
  const ackInput = $("#ACK");
  if (ackInput && typeof state === "boolean") ackInput.checked = state;
}
async function setAck(value) {
  await sendCommand("ACK", { v: value ? "1" : "0" });
  await refreshAckState();
}
async function toggleAck() {
  await sendCommand("ACK", { toggle: "1" });
  await refreshAckState();
}
async function refreshAckState() {
  const res = await deviceFetch("ACK", {}, 2000);
  if (res.ok) {
    const state = parseAckResponse(res.text);
    if (state !== null) {
      UI.state.ack = state;
      updateAckUi();
      return state;
    }
  }
  return null;
}

/* Настройки */
const SETTINGS_KEYS = ["ACK","BANK","BF","CH","CR","PW","SF"];
function loadSettings() {
  for (let i = 0; i < SETTINGS_KEYS.length; i++) {
    const key = SETTINGS_KEYS[i];
    const el = $("#" + key);
    if (!el) continue;
    const v = localStorage.getItem("set." + key);
    if (v === null) continue;
    if (el.type === "checkbox") el.checked = v === "1";
    else el.value = v;
  }
}
function saveSettingsLocal() {
  for (let i = 0; i < SETTINGS_KEYS.length; i++) {
    const key = SETTINGS_KEYS[i];
    const el = $("#" + key);
    if (!el) continue;
    const v = el.type === "checkbox" ? (el.checked ? "1" : "0") : el.value;
    localStorage.setItem("set." + key, v);
  }
  note("Сохранено локально.");
}
async function applySettingsToDevice() {
  for (let i = 0; i < SETTINGS_KEYS.length; i++) {
    const key = SETTINGS_KEYS[i];
    const el = $("#" + key);
    if (!el) continue;
    const value = el.type === "checkbox" ? (el.checked ? "1" : "0") : String(el.value || "").trim();
    if (!value) continue;
    await sendCommand(key, { v: value });
  }
  note("Применение завершено.");
  await refreshChannels().catch(() => {});
  await refreshAckState();
}
function exportSettings() {
  const obj = {};
  for (let i = 0; i < SETTINGS_KEYS.length; i++) {
    const key = SETTINGS_KEYS[i];
    const el = $("#" + key);
    if (!el) continue;
    obj[key] = el.type === "checkbox" ? (el.checked ? "1" : "0") : el.value;
  }
  const json = JSON.stringify(obj, null, 2);
  const blob = new Blob([json], { type: "application/json" });
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url;
  a.download = "settings.json";
  a.click();
  URL.revokeObjectURL(url);
}
function importSettings() {
  const input = document.createElement("input");
  input.type = "file";
  input.accept = "application/json";
  input.onchange = async () => {
    const file = input.files && input.files[0];
    if (!file) return;
    const text = await file.text();
    let obj;
    try {
      obj = JSON.parse(text);
    } catch (e) {
      note("Некорректный файл настроек");
      return;
    }
    for (let i = 0; i < SETTINGS_KEYS.length; i++) {
      const key = SETTINGS_KEYS[i];
      if (!Object.prototype.hasOwnProperty.call(obj, key)) continue;
      const el = $("#" + key);
      if (!el) continue;
      if (el.type === "checkbox") el.checked = obj[key] === "1";
      else el.value = obj[key];
      localStorage.setItem("set." + key, String(obj[key]));
    }
    note("Импортировано.");
  };
  input.click();
}
async function clearCaches() {
  localStorage.clear();
  if (typeof indexedDB !== "undefined" && indexedDB.databases) {
    const dbs = await indexedDB.databases();
    await Promise.all(dbs.map((db) => new Promise((resolve) => {
      const req = indexedDB.deleteDatabase(db.name);
      req.onsuccess = req.onerror = req.onblocked = () => resolve();
    })));
  }
  note("Кеш очищен.");
}

/* Безопасность */
async function loadKeyFromStorage() {
  const b64 = localStorage.getItem("sec.key");
  if (b64) {
    UI.key.bytes = Uint8Array.from(atob(b64), (c) => c.charCodeAt(0)).slice(0, 16);
  } else {
    UI.key.bytes = null;
  }
}
async function saveKeyToStorage(bytes) {
  localStorage.setItem("sec.key", btoa(String.fromCharCode.apply(null, bytes)));
}
async function generateKey() {
  const buf = new Uint8Array(16);
  if (typeof crypto !== "undefined" && crypto.getRandomValues) {
    crypto.getRandomValues(buf);
  } else {
    for (let i = 0; i < buf.length; i++) buf[i] = Math.floor(Math.random() * 256);
  }
  UI.key.bytes = buf;
  await saveKeyToStorage(buf);
  await updateKeyUI();
  note("Сгенерирован новый ключ (16 байт).");
}
async function sha256Hex(bytes) {
  if (typeof crypto !== "undefined" && crypto.subtle && crypto.subtle.digest) {
    const digest = await crypto.subtle.digest("SHA-256", bytes);
    return Array.from(new Uint8Array(digest)).map((x) => x.toString(16).padStart(2, "0")).join("");
  }
  if (typeof sha256Bytes === "function") {
    return sha256Bytes(bytes);
  }
  let sum = 0;
  for (let i = 0; i < bytes.length; i++) sum = (sum + bytes[i]) & 0xffffffff;
  return sum.toString(16).padStart(8, "0");
}
async function updateKeyUI() {
  const stateEl = $("#keyState");
  const hashEl = $("#keyHash");
  const hexEl = $("#keyHex");
  if (!UI.key.bytes) {
    if (stateEl) stateEl.textContent = "LOCAL";
    if (hashEl) hashEl.textContent = "";
    if (hexEl) hexEl.textContent = "";
  } else {
    const hex = Array.from(UI.key.bytes).map((x) => x.toString(16).padStart(2, "0")).join("");
    const h = await sha256Hex(UI.key.bytes);
    if (stateEl) stateEl.textContent = h.slice(0, 4).toUpperCase();
    if (hashEl) hashEl.textContent = "SHA-256: " + h;
    if (hexEl) hexEl.textContent = hex;
  }
}

/* Утилиты */
function status(text) {
  if (UI.els.status) UI.els.status.textContent = text;
}
function note(text) {
  const toast = UI.els.toast;
  if (!toast) return;
  toast.textContent = text;
  toast.hidden = false;
  toast.classList.remove("show");
  void toast.offsetWidth;
  toast.classList.add("show");
  setTimeout(() => { toast.hidden = true; }, 2200);
}
function debugLog(text) {
  const log = UI.els.debugLog;
  if (!log) return;
  const line = document.createElement("div");
  line.textContent = "[" + new Date().toLocaleTimeString() + "] " + text;
  log.appendChild(line);
  log.scrollTop = log.scrollHeight;
}
)~~~";

// libs/sha256.js
const char SHA_JS[] PROGMEM = R"~~~(
/* Простая реализация SHA-256 на чистом JS.
 * Используется, когда WebCrypto недоступен (например, на HTTP).
 */
(function(global){
  function rotr(x,n){ return (x>>>n) | (x<<(32-n)); }
  function sha256Bytes(bytes){
    const K = new Uint32Array([
      0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
      0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
      0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
      0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
      0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
      0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
      0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
      0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    ]);
    const H = new Uint32Array([
      0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
      0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19
    ]);
    const l = bytes.length;
    const bitLen = l * 8;
    const paddedLen = (((l + 9) >> 6) + 1) << 6; // длина с дополнением кратная 64
    const buffer = new Uint8Array(paddedLen);
    buffer.set(bytes);
    buffer[l] = 0x80; // добавляем бит "1"
    const dv = new DataView(buffer.buffer);
    dv.setUint32(paddedLen - 8, Math.floor(bitLen / 4294967296));
    dv.setUint32(paddedLen - 4, bitLen >>> 0);
    const w = new Uint32Array(64);
    for (let i = 0; i < paddedLen; i += 64) {
      for (let j = 0; j < 16; j++) w[j] = dv.getUint32(i + j*4);
      for (let j = 16; j < 64; j++) {
        const s0 = rotr(w[j-15],7) ^ rotr(w[j-15],18) ^ (w[j-15]>>>3);
        const s1 = rotr(w[j-2],17) ^ rotr(w[j-2],19) ^ (w[j-2]>>>10);
        w[j] = (w[j-16] + s0 + w[j-7] + s1) >>> 0;
      }
      let a=H[0],b=H[1],c=H[2],d=H[3],e=H[4],f=H[5],g=H[6],h=H[7];
      for (let j=0; j<64; j++) {
        const S1 = rotr(e,6) ^ rotr(e,11) ^ rotr(e,25);
        const ch = (e & f) ^ (~e & g);
        const t1 = (h + S1 + ch + K[j] + w[j]) >>> 0;
        const S0 = rotr(a,2) ^ rotr(a,13) ^ rotr(a,22);
        const maj = (a & b) ^ (a & c) ^ (b & c);
        const t2 = (S0 + maj) >>> 0;
        h=g; g=f; f=e; e=(d + t1)>>>0; d=c; c=b; b=a; a=(t1 + t2)>>>0;
      }
      H[0]=(H[0]+a)>>>0; H[1]=(H[1]+b)>>>0; H[2]=(H[2]+c)>>>0; H[3]=(H[3]+d)>>>0;
      H[4]=(H[4]+e)>>>0; H[5]=(H[5]+f)>>>0; H[6]=(H[6]+g)>>>0; H[7]=(H[7]+h)>>>0;
    }
    return Array.from(H).map(x=>x.toString(16).padStart(8,"0")).join("");
  }
  // экспорт в глобальный объект
  global.sha256Bytes = sha256Bytes;
})(this);

)~~~";
