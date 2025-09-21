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
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no" />
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
    <div id="navOverlay" class="nav-overlay" hidden></div>
    <div class="header-actions">
      <button id="themeToggle" class="icon-btn" aria-label="Тема">🌓</button>
      <button id="themeRedToggle" class="icon-btn" aria-label="Красная тема">🔴</button>
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
        <button type="button" class="chip state state-chip" id="ackStateChip" data-state="unknown" title="Переключить ACK" aria-label="Переключить ACK">
          <span class="label">ACK</span>
          <span id="ackStateText">—</span>
        </button>
        <button type="button" class="chip state state-chip" id="encStateChip" data-state="unknown" title="Переключить шифрование" aria-label="Переключить шифрование">
          <span class="label">ENC</span>
          <span id="encStateText">—</span>
        </button>
      </div>
      <div class="group-title"><span>Команды</span><span class="line"></span></div>
      <div class="cmd-buttons actions">
        <button class="btn" data-cmd="INFO">INFO</button>
        <button class="btn" data-cmd="STS">STS</button>
        <button class="btn" data-cmd="RSTS">RSTS</button>
        <button class="btn" data-cmd="BCN">BCN</button>
        <button class="btn" data-cmd="TESTRXM">TESTRXM</button>
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
      <div class="channel-layout">
        <div class="table-wrap pretty">
          <table id="channelsTable">
            <thead>
              <tr><th>CH</th><th>TX,MHz</th><th>RX,MHz</th><th>RSSI</th><th>SNR</th><th>ST</th><th>SCAN</th></tr>
            </thead>
            <tbody></tbody>
          </table>
        </div>
        <aside id="channelInfoPanel" class="channel-info glass" hidden>
          <div class="channel-info-header">
            <div class="channel-info-title">Канал <span id="channelInfoTitle">—</span></div>
            <button id="channelInfoClose" type="button" class="icon-btn ghost small" aria-label="Скрыть информацию">✕</button>
          </div>
          <div id="channelInfoEmpty" class="channel-info-placeholder small muted">Выберите канал в таблице, чтобы увидеть подробности.</div>
          <div id="channelInfoStatus" class="channel-info-status small muted" hidden></div>
          <div id="channelInfoBody" class="channel-info-body" hidden>
            <div class="channel-info-block">
              <div class="channel-info-caption">Текущие данные</div>
              <dl class="channel-info-list">
                <div><dt>RX (MHz)</dt><dd id="channelInfoRxCurrent">—</dd></div>
                <div><dt>TX (MHz)</dt><dd id="channelInfoTxCurrent">—</dd></div>
                <div><dt>RSSI</dt><dd id="channelInfoRssi">—</dd></div>
                <div><dt>SNR</dt><dd id="channelInfoSnr">—</dd></div>
                <div><dt>Статус</dt><dd id="channelInfoStatusCurrent">—</dd></div>
                <div><dt>SCAN</dt><dd id="channelInfoScan">—</dd></div>
              </dl>
            </div>
            <div class="channel-info-block">
              <div class="channel-info-caption">Справочник</div>
              <dl class="channel-info-list">
                <div><dt>RX (MHz)</dt><dd id="channelInfoRxRef">—</dd></div>
                <div><dt>TX (MHz)</dt><dd id="channelInfoTxRef">—</dd></div>
                <div><dt>Система</dt><dd id="channelInfoSystem">—</dd></div>
                <div><dt>План</dt><dd id="channelInfoBand">—</dd></div>
                <div><dt>Назначение</dt><dd id="channelInfoPurpose">—</dd></div>
              </dl>
            </div>
            <div class="channel-info-actions">
              <button id="channelInfoSetCurrent" class="btn">Установить текущим каналом</button>
            </div>
          </div>
        </aside>
      </div>
    </section>
    <!-- Вкладка настроек -->
    <section id="tab-settings" class="tab" hidden>
      <h2>Settings</h2>
      <form id="settingsForm" class="settings-form">
        <label>Endpoint
          <input id="endpoint" type="text" placeholder="http://192.168.4.1" />
        </label>
        <div class="settings-toggle" id="autoNightControl">
          <label class="chip switch">
            <input type="checkbox" id="autoNightMode" />
            <span>Авто ночной режим</span>
          </label>
          <div id="autoNightHint" class="field-hint">Ночная тема включается автоматически с 21:00 до 07:00.</div>
        </div>
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
            <option value="7.81">7.81</option>
            <option value="10.42">10.42</option>
            <option value="15.63">15.63</option>
            <option value="20.83" selected>20.83</option>
            <option value="31.25">31.25</option>
          </select>
        </label>
        <label>Channel
          <select id="CH"></select>
          <div id="channelSelectHint" class="field-hint"></div>
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
            <option value="-5">-5 dBm</option><option value="-2">-2 dBm</option><option value="1">1 dBm</option><option value="4">4 dBm</option>
            <option value="7">7 dBm</option><option value="10">10 dBm</option><option value="13">13 dBm</option><option value="16">16 dBm</option>
            <option value="19">19 dBm</option><option value="22">22 dBm</option>
          </select>
        </label>
        <label>SF
          <select id="SF">
            <option>7</option><option>8</option><option>9</option><option>10</option><option>11</option><option>12</option>
          </select>
        </label>
        <div class="settings-toggle" id="ackSettingControl">
          <div class="field-label">ACK (подтверждения)</div>
          <div id="ackSettingHint" class="field-hint">Состояние не загружено.</div>
        </div>
        <label>ACK повторы
          <input id="ACKR" type="number" min="0" max="10" value="3" />
          <div id="ackRetryHint" class="field-hint">Доступно после включения ACK.</div>
        </label>
        <label>Пауза между пакетами (мс)
          <input id="PAUSE" type="number" min="0" max="60000" value="370" />
          <div id="pauseHint" class="field-hint">Пауза между пакетами не загружена.</div>
        </label>
        <label>ACK тайм-аут (мс)
          <input id="ACKT" type="number" min="0" max="60000" value="1500" />
          <div id="ackTimeoutHint" class="field-hint">Время ожидания ACK не загружено.</div>
        </label>
        <label>Сообщение для TESTRXM
          <input id="TESTRXMMSG" type="text" maxlength="256" placeholder="Lorem ipsum dolor sit amet" />
          <div id="testRxmMessageHint" class="field-hint">Будет использован встроенный шаблон Lorem ipsum.</div>
        </label>
        <div class="settings-actions actions">
          <button type="button" id="btnSaveSettings" class="btn">Сохранить</button>
          <button type="button" id="btnApplySettings" class="btn btn-primary">Применить</button>
          <button type="button" id="btnExportSettings" class="btn">Экспорт</button>
          <button type="button" id="btnImportSettings" class="btn">Импорт</button>
          <button type="button" id="btnClearCache" class="btn danger">Очистить</button>
        </div>
        <button type="button" id="INFO" class="btn" data-cmd="INFO">INFO</button>
      </form>
    </section>
    <!-- Вкладка безопасности -->
    <section id="tab-security" class="tab" hidden>
      <h2>Security</h2>
      <div class="key-info">
        <div>Состояние: <span id="keyState">—</span></div>
        <div class="small muted">ID: <span id="keyId"></span></div>
        <div id="keyPublic" class="mono small muted"></div>
        <div id="keyPeer" class="mono small muted" hidden></div>
        <div id="keyBackup" class="small muted"></div>
      </div>
      <div class="actions">
        <button id="btnKeyGen" class="btn">KEYGEN</button>
        <button id="btnKeyRestore" class="btn">KEY RESTORE</button>
        <button id="btnKeySend" class="btn">KEYTRANSFER SEND</button>
        <button id="btnKeyRecv" class="btn">KEYTRANSFER RECEIVE</button>
      </div>
      <div id="keyMessage" class="small muted"></div>
      <div id="encTest" class="enc-test" data-status="idle">
        <div class="enc-test-header">
          <div class="enc-test-title">ENCT · тест шифрования</div>
          <div id="encTestStatus" class="small muted">Нет данных</div>
          <button id="btnEncTestRun" class="btn ghost small" data-cmd="ENCT">ENCT</button>
        </div>
        <div class="enc-test-grid">
          <div class="enc-test-field">
            <div class="field-label">Исходные данные</div>
            <pre id="encTestPlain" class="codeblock small">—</pre>
          </div>
          <div class="enc-test-field">
            <div class="field-label">Расшифровка</div>
            <pre id="encTestDecoded" class="codeblock small">—</pre>
          </div>
          <div class="enc-test-field">
            <div class="field-label">Шифртекст (hex)</div>
            <pre id="encTestCipher" class="codeblock small mono">—</pre>
          </div>
          <div class="enc-test-field">
            <div class="field-label">Tag (hex)</div>
            <pre id="encTestTag" class="codeblock small mono">—</pre>
          </div>
          <div class="enc-test-field">
            <div class="field-label">Nonce</div>
            <pre id="encTestNonce" class="codeblock small mono">—</pre>
          </div>
        </div>
        <div class="small muted">Нажмите ENCT, чтобы увидеть актуальные данные теста.</div>
      </div>
    </section>
    <!-- Вкладка отладочных логов -->
    <section id="tab-debug" class="tab" hidden>
      <h2>Debug</h2>
      <div class="actions debug-actions">
        <button id="btnRstsFull" class="btn">RSTS FULL</button>
        <button id="btnRstsJson" class="btn ghost">RSTS JSON</button>
        <button id="btnRstsDownloadJson" class="btn ghost">Скачать RSTS FULL JSON</button>
      </div>
      <div id="debugLog" class="debug-log"></div>
    </section>
  </main>
  <footer class="site-footer">
    <div id="statusLine" class="small muted"></div>
    <div class="footer-meta" id="footerMeta">Powered by AS Systems · <span id="appVersion">—</span></div>
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
  --scan-bg: color-mix(in oklab, #f97316 25%, white 75%);
  --scan-fg: #1f2937;
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
  --scan-bg: color-mix(in oklab, #f97316 20%, white 80%);
  --scan-fg: #082f49;
  color-scheme: light;
}

:root.red {
  --bg: #1a0508;
  --panel: #21060c;
  --panel-2: #29070f;
  --text: #fee2e2;
  --muted: #fca5a5;
  --accent: #fb7185;
  --accent-2: #f43f5e;
  --danger: #f87171;
  --good: #facc15;
  --ring: rgba(248,113,113,.35);
  --ring-2: rgba(244,63,94,.25);
  --scan-bg: color-mix(in oklab, var(--accent-2) 25%, black 15%);
  --scan-fg: color-mix(in oklab, var(--text) 85%, white 15%);
  color-scheme: dark;
}

* { box-sizing: border-box; }
html { min-height: 100%; }
body {
  margin: 0;
  font-family: ui-sans-serif, system-ui, -apple-system, Segoe UI, Roboto, 'Noto Sans', Ubuntu, Cantarell, 'Helvetica Neue', Arial, 'Apple Color Emoji', 'Segoe UI Emoji';
  background: var(--bg);
  color: var(--text);
  line-height: 1.55;
  -webkit-font-smoothing: antialiased;
  -moz-osx-font-smoothing: grayscale;
  display: flex;
  flex-direction: column;
  min-height: 100vh;
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
  grid-template-areas: "brand nav actions";
  gap: .75rem 1rem;
  align-items: center;
  padding: .6rem clamp(.75rem, 2vw, 1rem);
}
.site-header .brand { grid-area: brand; }
.site-header .nav { grid-area: nav; }
.site-header .header-actions { grid-area: actions; justify-self: end; }
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
.header-actions { display:flex; align-items:center; gap:.5rem; justify-content:flex-end; }
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
.chip.switch { cursor: pointer; user-select: none; }
.chip.switch input { width: auto; accent-color: var(--accent); cursor: pointer; }
.chip[aria-busy="true"] { opacity:.65; pointer-events:none; }
.chip.input-chip input {
  width: 4.5rem;
  max-width: 6rem;
  text-align: center;
  font-family: 'JetBrains Mono','Fira Code','SFMono-Regular',monospace;
  font-weight: 600;
}
.icon-btn {
  background: var(--panel-2);
  color: var(--text);
  border: 1px solid color-mix(in oklab, var(--panel-2) 60%, white 40%);
  border-radius: .7rem;
  padding: .45rem .6rem;
  cursor: pointer;
}
.icon-btn[aria-pressed="true"] {
  background: linear-gradient(180deg, var(--accent), var(--accent-2));
  color: #0f172a;
  border-color: transparent;
  box-shadow: 0 0 0 1px color-mix(in oklab, var(--accent-2) 40%, black 10%);
}
.icon-btn.ghost {
  background: transparent;
  border-color: color-mix(in oklab, var(--panel-2) 55%, white 45%);
}
.icon-btn.small {
  padding: .25rem .45rem;
  font-size: .85rem;
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
main {
  padding: 1rem clamp(.75rem, 2vw, 1rem) calc(1rem + env(safe-area-inset-bottom));
  flex: 1 0 auto;
  display: flex;
  flex-direction: column;
  gap: 1.5rem;
}
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
.msg {
  display:grid;
  gap:.35rem;
  justify-items:flex-start;
  align-items:flex-start;
}
.msg.you { justify-items:flex-end; }
.msg .avatar {
  width: 30px; height: 30px; border-radius: 999px;
  display:grid; place-items:center;
  font-weight:800; font-size:.75rem;
  background: linear-gradient(180deg, var(--accent), var(--accent-2));
  color: #001018; border: none;
}
.msg.dev .avatar { background: linear-gradient(180deg, #64748b, #334155); color: #e2e8f0; }
.msg time { color: var(--muted); font-size:.72rem; margin: .1rem 0 0; }
.msg.you time { justify-self:end; text-align:right; }
.msg .bubble {
  background: color-mix(in oklab, var(--panel) 92%, black 8%);
  padding:.6rem .7rem; border-radius: .9rem; border:1px solid color-mix(in oklab, var(--panel-2) 70%, black 30%);
  max-width: 85%;
  word-wrap: break-word;
  transition: transform .12s ease;
  position: relative;
  margin-right:auto;
}
.msg.you .bubble {
  background: color-mix(in oklab, var(--accent) 16%, var(--panel) 84%);
  border-color: color-mix(in oklab, var(--accent) 35%, var(--panel) 65%);
  margin-left:auto;
  margin-right:0;
}
.msg.you .bubble-meta { justify-content:flex-end; }
.msg .bubble:active { transform: scale(.99); }
.msg.system .bubble {
  background: color-mix(in oklab, var(--panel-2) 88%, var(--accent-2) 12%);
  border-color: color-mix(in oklab, var(--accent-2) 30%, var(--panel-2) 70%);
  color: color-mix(in oklab, var(--text) 88%, white 12%);
}
.msg.system .bubble-text {
  font-style: italic;
  font-size: .92rem;
  opacity: .88;
}
.msg.system time { color: color-mix(in oklab, var(--accent-2) 35%, var(--muted) 65%); }
.bubble-text { line-height:1.55; }
.bubble-meta {
  display:flex;
  align-items:center;
  gap:.35rem;
  margin-top:.45rem;
  font-size:.78rem;
  color: var(--muted);
  justify-content:flex-start;
}
.bubble-tx.ok { color: color-mix(in oklab, var(--good) 55%, var(--muted) 45%); }
.bubble-tx.err { color: color-mix(in oklab, var(--danger) 65%, var(--muted) 35%); }
.bubble-tx-label {
  text-transform: uppercase;
  letter-spacing:.05em;
  font-weight:700;
  font-size:.72rem;
}
.bubble-tx-detail { font-weight:600; }

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
.state-chip {
  cursor: pointer;
  transition: transform .12s ease, box-shadow .2s ease;
}
.state-chip:focus-visible {
  outline: 2px solid var(--ring);
  outline-offset: 3px;
}
.state-chip:disabled {
  opacity: .65;
  cursor: wait;
}
.state-chip[aria-busy="true"] {
  box-shadow: inset 0 0 0 1px color-mix(in oklab, var(--panel-2) 50%, var(--ring) 50%);
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

/* Монитор готовых сообщений */
.received-panel {
  display:flex;
  flex-direction:column;
  gap:.6rem;
  padding:.75rem .85rem;
  border-radius:.9rem;
  border:1px solid color-mix(in oklab, var(--panel-2) 70%, black 30%);
  background: color-mix(in oklab, var(--panel) 88%, transparent);
  backdrop-filter: blur(8px);
}
.received-controls {
  display:flex;
  flex-wrap:wrap;
  align-items:center;
  gap:.5rem;
}
.received-controls .btn { min-width: unset; }
.received-controls .chip { padding:.35rem .7rem; }
.received-list {
  list-style:none;
  padding:0;
  margin:0;
  display:flex;
  flex-direction:column;
  gap:.35rem;
  max-height:240px;
  overflow-y:auto;
}
.received-list li {
  display:flex;
  align-items:center;
  justify-content:space-between;
  gap:.75rem;
  padding:.45rem .65rem;
  border-radius:.75rem;
  background: color-mix(in oklab, var(--panel-2) 88%, black 12%);
  border:1px solid color-mix(in oklab, var(--panel-2) 70%, black 30%);
  box-shadow: inset 0 1px 0 rgba(255,255,255,.02);
  transition: background .25s ease, transform .25s ease;
}
.received-list li:hover { background: color-mix(in oklab, var(--panel-2) 82%, var(--accent) 18%); }
.received-list li.fresh {
  background: color-mix(in oklab, var(--accent) 26%, var(--panel-2) 74%);
  transform: translateX(4px);
}
.received-name {
  font-family: 'JetBrains Mono','Fira Code','SFMono-Regular',monospace;
  letter-spacing:.04em;
  font-size:.9rem;
}
.received-actions { display:flex; gap:.35rem; }
.received-actions .icon-btn {
  width: 2.2rem;
  height: 2.2rem;
  border-radius:999px;
  display:grid;
  place-items:center;
  padding:0;
}
.received-actions .icon-btn.ghost {
  border-color: color-mix(in oklab, var(--accent) 35%, var(--panel-2) 65%);
}
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
tbody tr.scanning { background: var(--scan-bg); color: var(--scan-fg); font-weight:600; }
tbody tr.signal { background: color-mix(in oklab, var(--good) 15%, white); /* ~#dcfce7, зелёный фон */ }
tbody tr.crc-error { background: color-mix(in oklab, #f97316 20%, white); /* ~#fed7aa, оранжевый фон */ }
tbody tr.no-response { background: color-mix(in oklab, var(--muted) 15%, white); color:#374151; /* ~#e5e7eb, серый фон и тёмный текст */ }
.channel-layout {
  display: grid;
  gap: 1rem;
  margin-top: 1rem;
}
.channel-info {
  border-radius: .9rem;
  padding: 1rem;
  display: flex;
  flex-direction: column;
  gap: .75rem;
}
.channel-info-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: .5rem;
}
.channel-info-title {
  font-weight: 700;
  font-size: 1.1rem;
}
.channel-info-placeholder,
.channel-info-status {
  line-height: 1.4;
}
.channel-info-body {
  display: grid;
  gap: 1rem;
}
.channel-info-actions {
  display: flex;
  flex-direction: column;
  gap: .5rem;
}
.channel-info-actions .btn {
  width: 100%;
}
.channel-info-block {
  display: grid;
  gap: .6rem;
}
.channel-info-caption {
  font-weight: 700;
  font-size: .75rem;
  letter-spacing: .08em;
  text-transform: uppercase;
  color: var(--muted);
}
.channel-info-list {
  display: grid;
  gap: .4rem;
}
.channel-info-list > div {
  display: grid;
  grid-template-columns: auto 1fr;
  gap: .6rem;
  align-items: baseline;
}
.channel-info-list dt {
  margin: 0;
  font-weight: 600;
  color: var(--muted);
}
.channel-info-list dd {
  margin: 0;
  word-break: break-word;
}
tbody tr.selected-info {
  background: color-mix(in oklab, var(--accent-2) 22%, var(--panel-2) 78%);
  box-shadow: inset 0 0 0 1px color-mix(in oklab, var(--accent-2) 45%, var(--panel-2) 55%);
  color: color-mix(in oklab, var(--text) 88%, white 12%);
}
tbody tr.selected-info td { font-weight:600; }
.field-hint {
  margin-top:.25rem;
  font-size:.8rem;
  color: var(--muted);
}
.field-label { font-weight:600; color: var(--muted); }
@media (min-width: 900px) {
  .channel-layout {
    grid-template-columns: minmax(0, 2fr) minmax(0, 1fr);
    align-items: start;
  }
}
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

/* ENCT visualisation */
.enc-test {
  margin-top: 1.25rem;
  padding: 1rem;
  border-radius: .9rem;
  background: var(--panel-2);
  border: 1px solid color-mix(in oklab, var(--panel-2) 70%, black 30%);
  display: grid;
  gap: .9rem;
}
.enc-test[data-status="ok"] {
  border-color: color-mix(in oklab, var(--accent) 35%, var(--panel-2) 65%);
}
.enc-test[data-status="error"] {
  border-color: color-mix(in oklab, var(--danger) 45%, var(--panel-2) 55%);
  box-shadow: inset 0 0 0 1px color-mix(in oklab, var(--danger) 25%, black 10%);
}
.enc-test-header {
  display:flex;
  flex-wrap:wrap;
  align-items:baseline;
  justify-content:space-between;
  gap:.35rem 1rem;
}
.enc-test-title { font-weight:700; }
.enc-test-grid { display:grid; gap:.75rem; }
.enc-test-field { display:flex; flex-direction:column; gap:.35rem; }
.enc-test-field pre {
  margin:0;
  white-space:pre-wrap;
  word-break:break-word;
}
@media (min-width: 720px) {
  .enc-test-grid { grid-template-columns: repeat(2, minmax(0, 1fr)); }
}

/* Footer */
.site-footer {
  padding: .85rem clamp(.75rem, 2vw, 1rem) calc(.85rem + env(safe-area-inset-bottom));
  color: var(--muted);
  border-top: 1px solid color-mix(in oklab, var(--panel-2) 75%, black 25%);
  display:flex;
  flex-wrap:wrap;
  align-items:center;
  gap:.6rem;
  font-size:.85rem;
  margin-top: auto;
}
.footer-meta { margin-left:auto; font-size:.75rem; color: color-mix(in oklab, var(--muted) 80%, var(--text) 20%); }
.footer-meta strong { color: var(--accent); font-weight:600; }

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
.settings-form label input,
.settings-form label select {
  width: 100%;
  background: var(--panel-2);
  color: var(--text);
  border: 1px solid transparent;
  border-radius: .7rem;
  padding: .6rem .75rem;
  outline: none;
}
.settings-form label input:focus,
.settings-form label select:focus {
  border-color: var(--ring);
  box-shadow: 0 0 0 3px var(--ring);
}
.settings-form label input[type="number"] {
  max-width: 7rem;
}
.settings-form .settings-toggle { display:flex; flex-direction:column; gap:.25rem; }
.settings-form .settings-toggle .chip { gap:.6rem; justify-content:flex-start; }
.settings-form .settings-toggle .field-hint { margin:0; }
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
  .site-header { grid-template-columns: 1fr auto; grid-template-areas: "brand actions"; }
  .nav { position: fixed; top: 3.25rem; right: .75rem; left: auto; width: min(280px, calc(100% - 1.5rem)); z-index: 50;
         background: var(--panel); border-radius:.7rem; padding:.5rem;
         border:1px solid color-mix(in oklab, var(--panel-2) 70%, black 30%); }
  .nav { display:none; flex-direction:column; align-items:flex-end; text-align:right; }
  .nav.open { display:flex; }
  .chip input { width: 42vw; }
  .chip.input-chip input { width: min(6rem, 34vw); }
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
/* Безопасная обёртка для localStorage: веб-приложение должно работать даже без постоянного хранилища */
const storage = (() => {
  const memory = new Map();
  try {
    const ls = window.localStorage;
    const probeKey = "__sat_probe__";
    ls.setItem(probeKey, "1");
    ls.removeItem(probeKey);
    return {
      available: true,
      get(key) {
        try {
          const value = ls.getItem(key);
          return value === null && memory.has(key) ? memory.get(key) : value;
        } catch (err) {
          console.warn("[storage] ошибка чтения из localStorage:", err);
          return memory.has(key) ? memory.get(key) : null;
        }
      },
      set(key, value) {
        const normalized = value == null ? "" : String(value);
        memory.set(key, normalized);
        try {
          ls.setItem(key, normalized);
        } catch (err) {
          console.warn("[storage] не удалось сохранить значение, используется запасной буфер:", err);
        }
      },
      remove(key) {
        memory.delete(key);
        try {
          ls.removeItem(key);
        } catch (err) {
          console.warn("[storage] не удалось удалить значение:", err);
        }
      },
      clear() {
        memory.clear();
        try {
          ls.clear();
        } catch (err) {
          console.warn("[storage] не удалось очистить localStorage:", err);
        }
      },
    };
  } catch (err) {
    console.warn("[storage] localStorage недоступен, используется временное хранилище в памяти:", err);
    return {
      available: false,
      get(key) {
        return memory.has(key) ? memory.get(key) : null;
      },
      set(key, value) {
        memory.set(key, value == null ? "" : String(value));
      },
      remove(key) {
        memory.delete(key);
      },
      clear() {
        memory.clear();
      },
    };
  }
})();

/* Состояние интерфейса */
const UI = {
  tabs: ["chat", "channels", "settings", "security", "debug"],
  els: {},
  cfg: {
    endpoint: storage.get("endpoint") || "http://192.168.4.1",
    theme: storage.get("theme") || detectPreferredTheme(),
    accent: (storage.get("accent") === "red") ? "red" : "default",
  },
  key: {
    state: null,
    lastMessage: "",
  },
  state: {
    bank: null,
    channel: null,
    ack: null,
    ackRetry: null,
    ackBusy: false,
    encBusy: false,
    encryption: null,
    recvAuto: false,
    recvTimer: null,
    receivedKnown: new Set(),
    infoChannel: null,
    infoChannelTx: null,
    infoChannelRx: null,
    chatHistory: [],
    version: null,
    pauseMs: null,
    ackTimeout: null,
    encTest: null,
    testRxmTimers: [],
  }
};

// Максимальная длина текста пользовательского сообщения для TESTRXM
const TEST_RXM_MESSAGE_MAX = 256;

// Справочные данные по каналам из CSV
const channelReference = {
  map: new Map(),
  byTx: new Map(),
  ready: false,
  loading: false,
  error: null,
  promise: null,
};
const CHANNEL_REFERENCE_FALLBACK = `Channel,RX (MHz),TX (MHz),System,Band Plan,Purpose
0,243.625,316.725,UHF military,225-328.6 MHz,Military communications
1,243.625,300.400,UHF military,225-328.6 MHz,Military communications
2,243.800,298.200,UHF military,225-328.6 MHz,Military communications
3,244.135,296.075,UHF FO,Band Plan P,Tactical communications
4,244.275,300.250,UHF military,225-328.6 MHz,Military communications
5,245.200,312.850,UHF military,225-328.6 MHz,Military communications
6,245.800,298.650,UHF military,225-328.6 MHz,Military communications
7,245.850,314.230,UHF military,225-328.6 MHz,Military communications
8,245.950,299.400,UHF military,225-328.6 MHz,Military communications
9,247.450,298.800,UHF military,225-328.6 MHz,Military communications
10,248.750,306.900,Marisat,B,Tactical voice/data
11,248.825,294.375,30 kHz Transponder,30K Transponder,30 kHz voice/data transponder
12,249.375,316.975,UHF military,225-328.6 MHz,Military communications
13,249.400,300.975,UHF military,225-328.6 MHz,Military communications
14,249.450,299.000,UHF military,225-328.6 MHz,Military communications
15,249.450,312.750,UHF military,225-328.6 MHz,Military communications
16,249.490,313.950,UHF military,225-328.6 MHz,Military communications
17,249.530,318.280,UHF military,225-328.6 MHz,Military communications
18,249.850,316.250,UHF military,225-328.6 MHz,Military communications
19,249.850,298.830,UHF military,225-328.6 MHz,Military communications
20,249.890,300.500,UHF military,225-328.6 MHz,Military communications
21,249.930,308.750,UHF FO,Q,Tactical communications
22,250.090,312.600,UHF military,225-328.6 MHz,Military communications
23,250.900,308.300,UHF FO,Q,Tactical communications
24,251.275,296.500,30 kHz Transponder,30K Transponder,30 kHz voice/data transponder
25,251.575,308.450,UHF military,225-328.6 MHz,Military communications
26,251.600,298.225,UHF military,225-328.6 MHz,Military communications
27,251.850,292.850,Navy 25 kHz,Navy 25K,Tactical voice/data communications
28,251.900,292.900,FLTSATCOM/Leasat,A/X,Tactical communications
29,251.950,292.950,Navy 25 kHz,Navy 25K,Tactical voice/data communications
30,252.000,293.100,FLTSATCOM,B,Tactical communications
31,252.050,293.050,Navy 25 kHz,Navy 25K,Tactical voice/data communications
32,252.150,293.150,UHF military,225-328.6 MHz,Military communications
33,252.200,299.150,UHF military,225-328.6 MHz,Military communications
34,252.400,309.700,UHF FO,Q,Tactical communications
35,252.450,309.750,UHF military,225-328.6 MHz,Military communications
36,252.500,309.800,UHF military,225-328.6 MHz,Military communications
37,252.550,309.850,UHF military,225-328.6 MHz,Military communications
38,252.625,309.925,UHF military,225-328.6 MHz,Military communications
39,253.550,294.550,UHF military,225-328.6 MHz,Military communications
40,253.600,295.950,UHF military,225-328.6 MHz,Military communications
41,253.650,294.650,Navy 25 kHz,Navy 25K,Tactical voice/data communications
42,253.700,294.700,FLTSATCOM/Leasat/UHF FO,A/X/O,Tactical communications
43,253.750,294.750,UHF military,225-328.6 MHz,Military communications
44,253.800,296.000,UHF military,225-328.6 MHz,Military communications
45,253.850,294.850,Navy 25 kHz,Navy 25K,Tactical voice/data communications
46,253.850,294.850,Navy 25 kHz,Navy 25K,Tactical voice/data communications
47,253.900,307.500,UHF military,225-328.6 MHz,Military communications
48,254.000,298.630,UHF military,225-328.6 MHz,Military communications
49,254.730,312.550,UHF military,225-328.6 MHz,Military communications
50,254.775,310.800,UHF military,225-328.6 MHz,Military communications
51,254.830,296.200,UHF military,225-328.6 MHz,Military communications
52,255.250,302.425,UHF military,225-328.6 MHz,Military communications
53,255.350,296.350,Navy 25 kHz,Navy 25K,Tactical voice/data communications
54,255.400,296.400,Navy 25 kHz,Navy 25K,Tactical voice/data communications
55,255.450,296.450,UHF military,225-328.6 MHz,Military communications
56,255.550,296.550,Navy 25 kHz,Navy 25K,Tactical voice/data communications
57,255.550,296.550,Navy 25 kHz,Navy 25K,Tactical voice/data communications
58,255.775,309.300,UHF military,225-328.6 MHz,Military communications
59,256.450,313.850,UHF military,225-328.6 MHz,Military communications
60,256.600,305.950,UHF military,225-328.6 MHz,Military communications
61,256.850,297.850,Navy 25 kHz,Navy 25K,Tactical voice/data communications
62,256.900,296.100,UHF military,225-328.6 MHz,Military communications
63,256.950,297.950,UHF military,225-328.6 MHz,Military communications
64,257.000,297.675,Navy 25 kHz,Navy 25K,Tactical voice/data communications
65,257.050,298.050,Navy 25 kHz,Navy 25K,Tactical voice/data communications
66,257.100,295.650,UHF military,225-328.6 MHz,Military communications
67,257.150,298.150,UHF military,225-328.6 MHz,Military communications
68,257.200,308.800,UHF military,225-328.6 MHz,Military communications
69,257.250,309.475,UHF military,225-328.6 MHz,Military communications
70,257.300,309.725,UHF military,225-328.6 MHz,Military communications
71,257.350,307.200,UHF military,225-328.6 MHz,Military communications
72,257.500,311.350,UHF military,225-328.6 MHz,Military communications
73,257.700,316.150,UHF military,225-328.6 MHz,Military communications
74,257.775,311.375,UHF military,225-328.6 MHz,Military communications
75,257.825,297.075,UHF military,225-328.6 MHz,Military communications
76,257.900,298.000,UHF military,225-328.6 MHz,Military communications
77,258.150,293.200,UHF military,225-328.6 MHz,Military communications
78,258.350,299.350,Navy 25 kHz,Navy 25K,Tactical voice/data communications
79,258.450,299.450,Navy 25 kHz,Navy 25K,Tactical voice/data communications
80,258.500,299.500,Navy 25 kHz,Navy 25K,Tactical voice/data communications
81,258.550,299.550,Navy 25 kHz,Navy 25K,Tactical voice/data communications
82,258.650,299.650,Navy 25 kHz,Navy 25K,Tactical voice/data communications
83,259.000,317.925,UHF military,225-328.6 MHz,Military communications
84,259.050,317.975,UHF military,225-328.6 MHz,Military communications
85,259.975,310.050,UHF military,225-328.6 MHz,Military communications
86,260.025,310.225,UHF military,225-328.6 MHz,Military communications
87,260.075,310.275,UHF military,225-328.6 MHz,Military communications
88,260.125,310.125,UHF military,225-328.6 MHz,Military communications
89,260.175,310.325,UHF military,225-328.6 MHz,Military communications
90,260.375,292.975,UHF military,225-328.6 MHz,Military communications
91,260.425,297.575,UHF military,225-328.6 MHz,Military communications
92,260.425,294.025,DOD 25 kHz,DOD 25K,Tactical communications (DoD)
93,260.475,294.075,DOD 25 kHz,DOD 25K,Tactical communications (DoD)
94,260.525,294.125,UHF military,225-328.6 MHz,Military communications
95,260.550,296.775,UHF military,225-328.6 MHz,Military communications
96,260.575,294.175,DOD 25 kHz,DOD 25K,Tactical communications (DoD)
97,260.625,294.225,DOD 25 kHz,DOD 25K,Tactical communications (DoD)
98,260.675,294.475,DOD 25 kHz,DOD 25K,Tactical communications (DoD)
99,260.675,294.275,UHF military,225-328.6 MHz,Military communications
100,260.725,294.325,UHF military,225-328.6 MHz,Military communications
101,260.900,313.900,UHF military,225-328.6 MHz,Military communications
102,261.100,298.380,UHF military,225-328.6 MHz,Military communications
103,261.100,298.700,UHF military,225-328.6 MHz,Military communications
104,261.200,294.950,UHF military,225-328.6 MHz,Military communications
105,262.000,314.200,UHF military,225-328.6 MHz,Military communications
106,262.040,307.075,UHF military,225-328.6 MHz,Military communications
107,262.075,306.975,UHF military,225-328.6 MHz,Military communications
108,262.125,295.725,DOD 25 kHz,DOD 25K,Tactical communications (DoD)
109,262.175,297.025,UHF military,225-328.6 MHz,Military communications
110,262.175,295.775,DOD 25 kHz,DOD 25K,Tactical communications (DoD)
111,262.225,295.825,DOD 25 kHz,DOD 25K,Tactical communications (DoD)
112,262.275,295.875,UHF military,225-328.6 MHz,Military communications
113,262.275,300.275,UHF military,225-328.6 MHz,Military communications
114,262.325,295.925,DOD 25 kHz,DOD 25K,Tactical communications (DoD)
115,262.375,295.975,UHF military,225-328.6 MHz,Military communications
116,262.425,296.025,DOD 25 kHz,DOD 25K,Tactical communications (DoD)
117,263.450,311.400,UHF military,225-328.6 MHz,Military communications
118,263.500,309.875,UHF military,225-328.6 MHz,Military communications
119,263.575,297.175,DOD 25 kHz,DOD 25K,Tactical communications (DoD)
120,263.625,297.225,DOD 25 kHz,DOD 25K,Tactical communications (DoD)
121,263.675,297.275,DOD 25 kHz,DOD 25K,Tactical communications (DoD)
122,263.725,297.325,UHF military,225-328.6 MHz,Military communications
123,263.775,297.375,DOD 25 kHz,DOD 25K,Tactical communications (DoD)
124,263.825,297.425,DOD 25 kHz,DOD 25K,Tactical communications (DoD)
125,263.875,297.475,DOD 25 kHz,DOD 25K,Tactical communications (DoD)
126,263.925,297.525,DOD 25 kHz,DOD 25K,Tactical communications (DoD)
127,265.250,306.250,UHF military,225-328.6 MHz,Military communications
128,265.350,306.350,UHF military,225-328.6 MHz,Military communications
129,265.400,294.425,FLTSATCOM/Leasat,A/X,Tactical communications
130,265.450,306.450,UHF military,225-328.6 MHz,Military communications
131,265.500,302.525,UHF military,225-328.6 MHz,Military communications
132,265.550,306.550,Navy 25 kHz,Navy 25K,Tactical voice/data communications
133,265.675,306.675,UHF military,225-328.6 MHz,Military communications
134,265.850,306.850,UHF military,225-328.6 MHz,Military communications
135,266.750,316.575,UHF military,225-328.6 MHz,Military communications
136,266.850,307.850,Navy 25 kHz,Navy 25K,Tactical voice/data communications
137,266.900,297.625,UHF military,225-328.6 MHz,Military communications
138,266.950,307.950,Navy 25 kHz,Navy 25K,Tactical voice/data communications
139,267.050,308.050,Navy 25 kHz,Navy 25K,Tactical voice/data communications
140,267.100,308.100,Navy 25 kHz,Navy 25K,Tactical voice/data communications
141,267.150,308.150,Navy 25 kHz,Navy 25K,Tactical voice/data communications
142,267.200,308.200,Navy 25 kHz,Navy 25K,Tactical voice/data communications
143,267.250,308.250,Navy 25 kHz,Navy 25K,Tactical voice/data communications
144,267.400,294.900,UHF military,225-328.6 MHz,Military communications
145,267.875,310.375,UHF military,225-328.6 MHz,Military communications
146,267.950,310.450,UHF military,225-328.6 MHz,Military communications
147,268.000,310.475,UHF military,225-328.6 MHz,Military communications
148,268.025,309.025,Navy 25 kHz,Navy 25K,Tactical voice/data communications
149,268.050,310.550,UHF military,225-328.6 MHz,Military communications
150,268.100,310.600,UHF military,225-328.6 MHz,Military communications
151,268.150,309.150,Navy 25 kHz,Navy 25K,Tactical voice/data communications
152,268.200,296.050,UHF military,225-328.6 MHz,Military communications
153,268.250,309.250,Navy 25 kHz,Navy 25K,Tactical voice/data communications
154,268.300,309.300,Navy 25 kHz,Navy 25K,Tactical voice/data communications
155,268.350,309.350,UHF military,225-328.6 MHz,Military communications
156,268.400,295.900,FLTSATCOM/Leasat,C/Z,Tactical communications
157,268.450,309.450,UHF military,225-328.6 MHz,Military communications
158,269.700,309.925,UHF military,225-328.6 MHz,Military communications
159,269.750,310.750,UHF military,225-328.6 MHz,Military communications
160,269.800,310.025,UHF military,225-328.6 MHz,Military communications
161,269.850,310.850,Navy 25 kHz,Navy 25K,Tactical voice/data communications
162,269.950,310.950,DOD 25 kHz,DOD 25K,Tactical communications (DoD)
800,250.000,250.000,sattest,Random,Testing frequency mode
801,260.000,260.000,sattest,Random,Testing frequency mode
802,270.000,270.000,sattest,Random,Testing frequency mode
803,280.000,280.000,sattest,Random,Testing frequency mode
804,290.000,290.000,sattest,Random,Testing frequency mode
805,300.000,300.000,sattest,Random,Testing frequency mode
806,310.000,310.000,sattest,Random,Testing frequency mode
807,433.000,433.000,sattest,Random,Testing frequency mode
808,434.000,434.000,sattest,Random,Testing frequency mode
809,446.000,446.000,sattest,Random,Testing frequency mode`;
const POWER_PRESETS = [-5, -2, 1, 4, 7, 10, 13, 16, 19, 22];
const ACK_RETRY_MAX = 10;
const ACK_RETRY_DEFAULT = 3;
const PAUSE_MIN_MS = 0;
const PAUSE_MAX_MS = 60000;
const ACK_TIMEOUT_MIN_MS = 0;
const ACK_TIMEOUT_MAX_MS = 60000;

/* Определяем предпочитаемую тему только при наличии поддержки matchMedia */
function detectPreferredTheme() {
  try {
    if (typeof matchMedia === "function" && matchMedia("(prefers-color-scheme: light)").matches) {
      return "light";
    }
  } catch (err) {
    console.warn("[theme] не удалось получить предпочтение системы:", err);
  }
  return "dark";
}

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
  UI.els.themeRedToggle = $("#themeRedToggle");
  UI.els.status = $("#statusLine");
  UI.els.footerMeta = $("#footerMeta");
  UI.els.version = $("#appVersion");
  UI.els.toast = $("#toast");
  UI.els.debugLog = $("#debugLog");
  UI.els.chatLog = $("#chatLog");
  UI.els.chatInput = $("#chatInput");
  UI.els.sendBtn = $("#sendBtn");
  UI.els.ackChip = $("#ackStateChip");
  UI.els.ackText = $("#ackStateText");
  UI.els.encChip = $("#encStateChip");
  UI.els.encText = $("#encStateText");
  UI.els.ackSetting = $("#ACK");
  UI.els.ackSettingWrap = $("#ackSettingControl");
  UI.els.ackSettingHint = $("#ackSettingHint");
  UI.els.ackRetry = $("#ACKR");
  UI.els.ackRetryHint = $("#ackRetryHint");
  UI.els.pauseInput = $("#PAUSE");
  UI.els.pauseHint = $("#pauseHint");
  UI.els.ackTimeout = $("#ACKT");
  UI.els.ackTimeoutHint = $("#ackTimeoutHint");
  UI.els.testRxmMessage = $("#TESTRXMMSG");
  UI.els.testRxmMessageHint = $("#testRxmMessageHint");
  UI.els.channelSelect = $("#CH");
  UI.els.channelSelectHint = $("#channelSelectHint");
  UI.els.txlInput = $("#txlSize");
  UI.els.recvList = $("#recvList");
  UI.els.recvEmpty = $("#recvEmpty");
  UI.els.recvAuto = $("#recvAuto");
  UI.els.recvLimit = $("#recvLimit");
  UI.els.recvRefresh = $("#btnRecvRefresh");
  UI.els.channelInfoPanel = $("#channelInfoPanel");
  UI.els.channelInfoTitle = $("#channelInfoTitle");
  UI.els.channelInfoBody = $("#channelInfoBody");
  UI.els.channelInfoEmpty = $("#channelInfoEmpty");
  UI.els.channelInfoStatus = $("#channelInfoStatus");
  UI.els.channelInfoSetBtn = $("#channelInfoSetCurrent");
  UI.els.channelInfoFields = {
    rxCurrent: $("#channelInfoRxCurrent"),
    txCurrent: $("#channelInfoTxCurrent"),
    rssi: $("#channelInfoRssi"),
    snr: $("#channelInfoSnr"),
    status: $("#channelInfoStatusCurrent"),
    scan: $("#channelInfoScan"),
    rxRef: $("#channelInfoRxRef"),
    txRef: $("#channelInfoTxRef"),
    system: $("#channelInfoSystem"),
    band: $("#channelInfoBand"),
    purpose: $("#channelInfoPurpose"),
  };
  UI.els.encTest = {
    container: $("#encTest"),
    status: $("#encTestStatus"),
    plain: $("#encTestPlain"),
    cipher: $("#encTestCipher"),
    decoded: $("#encTestDecoded"),
    tag: $("#encTestTag"),
    nonce: $("#encTestNonce"),
  };
  if (UI.els.channelInfoSetBtn) {
    UI.els.channelInfoSetBtn.addEventListener("click", onChannelInfoSetCurrent);
  }
  const infoClose = $("#channelInfoClose");
  if (infoClose) infoClose.addEventListener("click", hideChannelInfo);
  const channelsTable = $("#channelsTable");
  const savedChannelRaw = storage.get("set.CH");
  if (savedChannelRaw != null && UI.state.channel == null) {
    const savedChannelNum = parseInt(savedChannelRaw, 10);
    if (!isNaN(savedChannelNum)) {
      UI.state.channel = savedChannelNum;
      if (UI.els.channelSelect) UI.els.channelSelect.value = String(savedChannelNum);
    }
  }
  updateFooterVersion();
  if (channelsTable) {
    // Обрабатываем клики по строкам таблицы каналов, чтобы открыть карточку информации
    channelsTable.addEventListener("click", (event) => {
      const row = event.target ? event.target.closest("tbody tr") : null;
      if (!row || !row.dataset) return;
      const value = row.dataset.ch;
      if (!value) return;
      const num = Number(value);
      if (!Number.isFinite(num)) return;
      if (UI.state.infoChannel === num) hideChannelInfo();
      else showChannelInfo(num);
    });
    channelsTable.addEventListener("keydown", (event) => {
      if (!event || (event.key !== "Enter" && event.key !== " " && event.key !== "Spacebar")) return;
      const row = event.target ? event.target.closest("tbody tr") : null;
      if (!row || !row.dataset) return;
      const value = row.dataset.ch;
      if (!value) return;
      const num = Number(value);
      if (!Number.isFinite(num)) return;
      event.preventDefault();
      if (UI.state.infoChannel === num) hideChannelInfo();
      else showChannelInfo(num);
    });
  }

  // Навигация по вкладкам
  const hash = location.hash ? location.hash.slice(1) : "";
  const initialTab = UI.tabs.includes(hash) ? hash : (storage.get("activeTab") || "chat");
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
  applyAccent(UI.cfg.accent);
  if (UI.els.themeToggle) {
    UI.els.themeToggle.setAttribute("aria-pressed", UI.cfg.theme === "dark" ? "true" : "false");
    UI.els.themeToggle.addEventListener("click", () => toggleTheme());
  }
  if (UI.els.themeRedToggle) {
    UI.els.themeRedToggle.setAttribute("aria-pressed", UI.cfg.accent === "red" ? "true" : "false");
    UI.els.themeRedToggle.addEventListener("click", () => toggleAccent());
  }

  // Настройка endpoint
  if (UI.els.endpoint) {
    UI.els.endpoint.value = UI.cfg.endpoint;
    UI.els.endpoint.addEventListener("change", () => {
      const value = UI.els.endpoint.value.trim() || "http://192.168.4.1";
      UI.cfg.endpoint = value;
      storage.set("endpoint", UI.cfg.endpoint);
      note("Endpoint: " + UI.cfg.endpoint);
      resyncAfterEndpointChange().catch((err) => console.warn("[endpoint] resync", err));
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
    if (!cmd) return;
    if (cmd === "TESTRXM") {
      btn.addEventListener("click", runTestRxm);
    } else {
      btn.addEventListener("click", () => sendCommand(cmd));
    }
  });

  // Список принятых сообщений
  const savedLimitRaw = storage.get("recvLimit");
  let savedLimit = parseInt(savedLimitRaw || "", 10);
  if (!Number.isFinite(savedLimit) || savedLimit <= 0) savedLimit = 20;
  savedLimit = Math.min(Math.max(savedLimit, 1), 200);
  if (UI.els.recvLimit) {
    UI.els.recvLimit.value = String(savedLimit);
    UI.els.recvLimit.addEventListener("change", onRecvLimitChange);
  }
  const savedAuto = storage.get("recvAuto") === "1";
  UI.state.recvAuto = savedAuto;
  if (UI.els.recvAuto) {
    UI.els.recvAuto.checked = savedAuto;
    UI.els.recvAuto.addEventListener("change", () => setRecvAuto(UI.els.recvAuto.checked));
  }
  if (UI.els.recvRefresh) {
    UI.els.recvRefresh.addEventListener("click", () => refreshReceivedList({ manual: true }));
  }
  loadChatHistory();
  setRecvAuto(savedAuto, { skipImmediate: true });
  refreshReceivedList({ silentError: true });

  // Управление ACK и тестами
  if (UI.els.ackChip) UI.els.ackChip.addEventListener("click", onAckChipToggle);
  if (UI.els.encChip) UI.els.encChip.addEventListener("click", onEncChipToggle);
  if (UI.els.ackSetting) {
    UI.els.ackSetting.addEventListener("change", () => {
      UI.els.ackSetting.indeterminate = false;
      void withAckLock(() => setAck(UI.els.ackSetting.checked));
    });
  }
  if (UI.els.ackRetry) UI.els.ackRetry.addEventListener("change", onAckRetryInput);
  if (UI.els.pauseInput) UI.els.pauseInput.addEventListener("change", onPauseInputChange);
  if (UI.els.ackTimeout) UI.els.ackTimeout.addEventListener("change", onAckTimeoutInputChange);
  if (UI.els.testRxmMessage) {
    UI.els.testRxmMessage.addEventListener("input", updateTestRxmMessageHint);
    UI.els.testRxmMessage.addEventListener("change", onTestRxmMessageChange);
  }
  const btnTxl = $("#btnTxlSend"); if (btnTxl) btnTxl.addEventListener("click", sendTxl);

  // Вкладка каналов
  const btnPing = $("#btnPing"); if (btnPing) btnPing.addEventListener("click", runPing);
  const btnSearch = $("#btnSearch"); if (btnSearch) { UI.els.searchBtn = btnSearch; btnSearch.addEventListener("click", runSearch); }
  const btnRefresh = $("#btnRefresh"); if (btnRefresh) btnRefresh.addEventListener("click", () => refreshChannels());
  const btnExportCsv = $("#btnExportCsv"); if (btnExportCsv) btnExportCsv.addEventListener("click", exportChannelsCsv);
  // Подгружаем справочник частот в фоне
  loadChannelReferenceData().then(() => {
    updateChannelInfoPanel();
  }).catch((err) => {
    console.warn("[freq-info] загрузка не удалась:", err);
    updateChannelInfoPanel();
  });
  renderEncTest(UI.state.encTest);

  // Настройки
  const btnSaveSettings = $("#btnSaveSettings"); if (btnSaveSettings) btnSaveSettings.addEventListener("click", saveSettingsLocal);
  const btnApplySettings = $("#btnApplySettings"); if (btnApplySettings) btnApplySettings.addEventListener("click", applySettingsToDevice);
  const btnExportSettings = $("#btnExportSettings"); if (btnExportSettings) btnExportSettings.addEventListener("click", exportSettings);
  const btnImportSettings = $("#btnImportSettings"); if (btnImportSettings) btnImportSettings.addEventListener("click", importSettings);
  const btnClearCache = $("#btnClearCache"); if (btnClearCache) btnClearCache.addEventListener("click", clearCaches);
  loadSettings();
  updateTestRxmMessageHint();
  updatePauseUi();
  updateAckTimeoutUi();
  const bankSel = $("#BANK"); if (bankSel) bankSel.addEventListener("change", () => refreshChannels({ forceBank: true }));
  if (UI.els.channelSelect) UI.els.channelSelect.addEventListener("change", onChannelSelectChange);
  updateChannelSelectHint();

  // Безопасность
  const btnKeyGen = $("#btnKeyGen"); if (btnKeyGen) btnKeyGen.addEventListener("click", () => requestKeyGen());
  const btnKeyRestore = $("#btnKeyRestore"); if (btnKeyRestore) btnKeyRestore.addEventListener("click", () => requestKeyRestore());
  const btnKeySend = $("#btnKeySend"); if (btnKeySend) btnKeySend.addEventListener("click", () => requestKeySend());
  const btnKeyRecv = $("#btnKeyRecv"); if (btnKeyRecv) btnKeyRecv.addEventListener("click", () => requestKeyReceive());
  await refreshKeyState({ silent: true });

  await syncSettingsFromDevice();
  await refreshAckState();
  await refreshAckRetry();
  await refreshEncryptionState();

  await loadVersion().catch(() => {});
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
  storage.set("activeTab", tab);
  if (tab !== "channels") hideChannelInfo();
}

/* Тема */
function applyTheme(mode) {
  UI.cfg.theme = mode === "light" ? "light" : "dark";
  document.documentElement.classList.toggle("light", UI.cfg.theme === "light");
  storage.set("theme", UI.cfg.theme);
  if (UI.els.themeToggle) {
    UI.els.themeToggle.setAttribute("aria-pressed", UI.cfg.theme === "dark" ? "true" : "false");
  }
}
function toggleTheme() {
  applyTheme(UI.cfg.theme === "dark" ? "light" : "dark");
}
function applyAccent(mode) {
  UI.cfg.accent = mode === "red" ? "red" : "default";
  document.documentElement.classList.toggle("red", UI.cfg.accent === "red");
  storage.set("accent", UI.cfg.accent);
  if (UI.els.themeRedToggle) {
    UI.els.themeRedToggle.setAttribute("aria-pressed", UI.cfg.accent === "red" ? "true" : "false");
  }
}
function toggleAccent() {
  applyAccent(UI.cfg.accent === "red" ? "default" : "red");
}

/* Работа чата */
function getChatHistory() {
  if (!Array.isArray(UI.state.chatHistory)) UI.state.chatHistory = [];
  return UI.state.chatHistory;
}
function saveChatHistory() {
  const entries = getChatHistory();
  try {
    storage.set("chatHistory", JSON.stringify(entries.slice(-500)));
  } catch (err) {
    console.warn("[chat] не удалось сохранить историю:", err);
  }
}
function normalizeChatEntries(rawEntries) {
  const out = [];
  if (!Array.isArray(rawEntries)) return out;
  const cleanString = (value) => {
    if (value == null) return "";
    return String(value).trim();
  };
  const dropRxPrefix = (value) => {
    const text = cleanString(value);
    if (!/^RX/i.test(text)) return text;
    let idx = 2;
    let sawWhitespace = false;
    while (idx < text.length && /\s/.test(text[idx])) {
      sawWhitespace = true;
      idx += 1;
    }
    let sawPunct = false;
    if (idx < text.length && /[·:>.\-]/.test(text[idx])) {
      sawPunct = true;
      idx += 1;
      while (idx < text.length && /\s/.test(text[idx])) {
        sawWhitespace = true;
        idx += 1;
      }
    }
    if (!sawWhitespace && !sawPunct) return text;
    return text.slice(idx).trim();
  };
  const parseKnownName = (value) => {
    const text = cleanString(value);
    if (!text) return "";
    const match = text.match(/(GO-[0-9A-Za-z_-]+|SP-[0-9A-Za-z_-]+|R-[0-9A-Za-z_-]+(?:\|[0-9A-Za-z_-]+)?)/);
    return match ? match[0] : text;
  };
  const toNumber = (value) => {
    if (value === null || value === undefined) return null;
    if (typeof value === "string" && value.trim() === "") return null;
    const num = Number(value);
    return Number.isFinite(num) ? num : null;
  };
  for (let i = 0; i < rawEntries.length; i++) {
    const entry = rawEntries[i];
    if (!entry) continue;
    if (typeof entry === "string") {
      out.push({ t: Date.now(), a: "dev", m: entry, role: "system" });
      continue;
    }
    const obj = { ...entry };
    if (!obj.role) obj.role = obj.a === "you" ? "user" : "system";
    if (!obj.a) obj.a = obj.role === "user" ? "you" : "dev";
    if (!obj.t) obj.t = Date.now();
    const prev = out[out.length - 1];
    if (obj.tag === "tx-status" && prev && prev.a === "you") {
      const value = obj.status != null ? String(obj.status) : (typeof obj.m === "string" ? obj.m.replace(/^TX\s*:\s*/i, "").trim() : "");
      prev.txStatus = {
        ok: true,
        text: value,
        raw: value,
        t: obj.t || Date.now(),
      };
      continue;
    }
    if (typeof obj.m === "string" && /^TX\s*ERR\s*:/i.test(obj.m.trim()) && prev && prev.a === "you") {
      const detail = obj.m.replace(/^TX\s*ERR\s*:\s*/i, "").trim();
      const payload = detail || obj.m.trim();
      prev.txStatus = {
        ok: false,
        text: detail,
        raw: payload,
        t: obj.t || Date.now(),
      };
      continue;
    }
    if (obj.txStatus && prev && prev.a === "you" && !prev.txStatus) {
      const status = obj.txStatus;
      prev.txStatus = {
        ok: status.ok === false ? false : true,
        text: status.text != null ? String(status.text) : "",
        raw: status.raw != null ? String(status.raw) : (status.text != null ? String(status.text) : ""),
        t: status.t || obj.t || Date.now(),
      };
      continue;
    }
    const isLegacyRx = obj.tag === "rx-name" || (obj.role === "rx" && obj.tag !== "rx-message");
    if (isLegacyRx) {
      const rawMeta = (obj.rx && typeof obj.rx === "object") ? obj.rx : {};
      let rxText = cleanString(rawMeta.text || obj.text || obj.detail);
      const rawMessage = cleanString(obj.m);
      if (!rxText) rxText = dropRxPrefix(rawMessage);
      let name = cleanString(rawMeta.name || obj.name);
      if (!name) name = parseKnownName(rawMessage);
      if (!name && rxText) name = parseKnownName(rxText);
      let type = cleanString(rawMeta.type || obj.type || obj.kind || obj.queue);
      if (!type && name) {
        if (/^GO-/i.test(name)) type = "ready";
        else if (/^SP-/i.test(name)) type = "split";
        else if (/^R-/i.test(name)) type = "raw";
      }
      const hex = cleanString(rawMeta.hex || obj.hex);
      const lenValue = toNumber(rawMeta.len != null ? rawMeta.len : (obj.len != null ? obj.len : obj.length));
      let bubbleCore = rxText || dropRxPrefix(rawMessage) || name;
      bubbleCore = cleanString(bubbleCore);
      if (!bubbleCore) bubbleCore = name ? name : "—";
      let finalMessage = bubbleCore;
      if (!/^RX\s*[·:>\-]/i.test(finalMessage)) {
        finalMessage = "RX · " + finalMessage;
      }
      obj.m = finalMessage;
      obj.tag = "rx-message";
      obj.role = "rx";
      const normalizedRx = {
        name: name || "",
        type: type || "",
        hex: hex || "",
        len: lenValue != null ? lenValue : 0,
      };
      if (rxText) normalizedRx.text = rxText;
      obj.rx = normalizedRx;
      out.push(obj);
      continue;
    }
    out.push(obj);
  }
  return out;
}
function loadChatHistory() {
  const raw = storage.get("chatHistory") || "[]";
  let entries;
  try {
    entries = JSON.parse(raw);
  } catch (e) {
    entries = [];
  }
  if (!Array.isArray(entries)) entries = [];
  const normalized = normalizeChatEntries(entries);
  UI.state.chatHistory = normalized;
  if (UI.els.chatLog) UI.els.chatLog.innerHTML = "";
  for (let i = 0; i < normalized.length; i++) {
    addChatMessage(normalized[i], i);
  }
  saveChatHistory();
}
function persistChat(message, author, meta) {
  const entries = getChatHistory();
  const record = { t: Date.now(), a: author, m: message };
  if (meta && typeof meta === "object") {
    if (meta.role) record.role = meta.role;
    if (meta.tag) record.tag = meta.tag;
    if (meta.status != null) record.status = meta.status;
    if (meta.txStatus) record.txStatus = meta.txStatus;
  }
  if (!record.role) record.role = author === "you" ? "user" : "system";
  entries.push(record);
  saveChatHistory();
  return { record, index: entries.length - 1 };
}
function addChatMessage(entry, index) {
  if (!UI.els.chatLog) return null;
  const data = typeof entry === "string" ? { t: Date.now(), a: "dev", m: entry, role: "system" } : entry;
  const wrap = document.createElement("div");
  const author = data.a === "you" ? "you" : "dev";
  wrap.className = "msg";
  wrap.classList.add(author);
  const role = data.role || (author === "you" ? "user" : "system");
  if (role !== "user") wrap.classList.add("system");
  if (typeof index === "number" && index >= 0) {
    wrap.dataset.index = String(index);
  }
  const time = document.createElement("time");
  const stamp = data.t ? new Date(data.t) : new Date();
  time.dateTime = stamp.toISOString();
  time.textContent = stamp.toLocaleTimeString();
  const bubble = document.createElement("div");
  bubble.className = "bubble";
  applyChatBubbleContent(bubble, data);
  wrap.appendChild(bubble);
  wrap.appendChild(time);
  UI.els.chatLog.appendChild(wrap);
  UI.els.chatLog.scrollTop = UI.els.chatLog.scrollHeight;
  return wrap;
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
function applyChatBubbleContent(node, entry) {
  const raw = entry && entry.m != null ? String(entry.m) : "";
  node.innerHTML = "";
  const textBox = document.createElement("div");
  textBox.className = "bubble-text";
  setBubbleText(textBox, raw);
  node.appendChild(textBox);
  const tx = entry && entry.txStatus;
  if (tx && typeof tx === "object") {
    const footer = document.createElement("div");
    footer.className = "bubble-meta bubble-tx";
    footer.classList.add(tx.ok === false ? "err" : "ok");
    const label = document.createElement("span");
    label.className = "bubble-tx-label";
    label.textContent = tx.ok === false ? "TX ERR" : "TX OK";
    footer.appendChild(label);
    const detailRaw = tx.text != null ? String(tx.text) : "";
    const detail = detailRaw.trim();
    if (detail) {
      const detailNode = document.createElement("span");
      detailNode.className = "bubble-tx-detail";
      detailNode.textContent = detail;
      footer.appendChild(detailNode);
    }
    node.appendChild(footer);
  }
}
function updateChatMessageContent(index) {
  if (!UI.els.chatLog) return;
  const entries = getChatHistory();
  if (!Array.isArray(entries) || index < 0 || index >= entries.length) return;
  const wrap = UI.els.chatLog.querySelector('.msg[data-index="' + index + '"]');
  if (!wrap) return;
  const bubble = wrap.querySelector('.bubble');
  if (!bubble) return;
  applyChatBubbleContent(bubble, entries[index]);
}
function attachTxStatus(index, info) {
  const entries = getChatHistory();
  if (!Array.isArray(entries) || typeof index !== "number") return false;
  if (index < 0 || index >= entries.length) return false;
  const entry = entries[index];
  if (!entry || entry.a !== "you") return false;
  const ok = info && info.ok === false ? false : true;
  const detailRaw = info && info.text != null ? String(info.text) : "";
  const raw = info && info.raw != null ? String(info.raw) : detailRaw;
  entry.txStatus = {
    ok,
    text: detailRaw,
    raw,
    t: Date.now(),
  };
  saveChatHistory();
  updateChatMessageContent(index);
  return true;
}
async function onSendChat() {
  if (!UI.els.chatInput) return;
  const text = UI.els.chatInput.value.trim();
  if (!text) return;
  UI.els.chatInput.value = "";
  const isCommand = text.startsWith("/");
  const saved = persistChat(text, "you");
  addChatMessage(saved.record, saved.index);
  if (isCommand) {
    const parsed = parseSlashCommand(text.slice(1));
    if (!parsed) {
      note("Команда не распознана");
      return;
    }
    if (parsed.cmd === "TX") {
      await sendTextMessage(parsed.message || "", { originIndex: saved.index });
    } else {
      await sendCommand(parsed.cmd, parsed.params || {});
    }
  } else {
    await sendTextMessage(text, { originIndex: saved.index });
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
    } else if (cmd === "PW") {
      const normalized = normalizePowerPreset(rest);
      params.v = normalized ? String(normalized.index) : rest;
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
      const text = cmd + ": " + res.text;
      note(text.slice(0, 200));
      const saved = persistChat(text, "dev", { role: "system" });
      addChatMessage(saved.record, saved.index);
    }
    debugLog(cmd + ": " + res.text);
    handleCommandSideEffects(cmd, res.text);
    return res.text;
  }
  if (!silent) {
    status("✗ " + cmd);
    const text = "ERR " + cmd + ": " + res.error;
    note("Ошибка: " + res.error);
    const saved = persistChat(text, "dev", { role: "system" });
    addChatMessage(saved.record, saved.index);
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
async function sendTextMessage(text, opts) {
  const options = opts || {};
  const originIndex = typeof options.originIndex === "number" ? options.originIndex : null;
  const payload = (text || "").trim();
  if (!payload) {
    note("Пустое сообщение");
    return null;
  }
  status("→ TX");
  const res = await postTx(payload, 6000);
  if (res.ok) {
    status("✓ TX");
    const value = res.text != null ? String(res.text) : "";
    const msg = "TX: " + value;
    note(msg);
    debugLog("TX: " + value);
    let attached = false;
    if (originIndex !== null) {
      attached = attachTxStatus(originIndex, { ok: true, text: value, raw: res.text });
    }
    if (!attached) {
      const saved = persistChat(msg, "dev", { role: "system" });
      addChatMessage(saved.record, saved.index);
    }
    return value;
  }
  status("✗ TX");
  const errorText = res.error != null ? String(res.error) : "";
  note("Ошибка TX: " + errorText);
  debugLog("ERR TX: " + errorText);
  let attached = false;
  if (originIndex !== null) {
    attached = attachTxStatus(originIndex, { ok: false, text: errorText, raw: res.error });
  }
  if (!attached) {
    const errMsg = "TX ERR: " + errorText;
    const saved = persistChat(errMsg, "dev", { role: "system" });
    addChatMessage(saved.record, saved.index);
  }
  return null;
}
function handleCommandSideEffects(cmd, text) {
  const upperRaw = cmd.toUpperCase();
  const parts = upperRaw.trim().split(/\s+/);
  const upper = parts[0] || "";
  const upperArg = parts[1] || "";
  if (upper === "ACK") {
    const state = parseAckResponse(text);
    if (state !== null) {
      UI.state.ack = state;
      updateAckUi();
    }
  } else if (upper === "ACKR") {
    const value = parseAckRetryResponse(text);
    if (value !== null) {
      UI.state.ackRetry = value;
      storage.set("set.ACKR", String(value));
      updateAckRetryUi();
    }
  } else if (upper === "PAUSE") {
    const value = parsePauseResponse(text);
    if (value !== null) {
      UI.state.pauseMs = value;
      storage.set("set.PAUSE", String(value));
      updatePauseUi();
    }
  } else if (upper === "ACKT") {
    const value = parseAckTimeoutResponse(text);
    if (value !== null) {
      UI.state.ackTimeout = value;
      storage.set("set.ACKT", String(value));
      updateAckTimeoutUi();
      updateAckRetryUi();
    }
  } else if (upper === "ENC") {
    const state = parseEncResponse(text);
    if (state !== null) {
      UI.state.encryption = state;
      updateEncryptionUi();
    }
  } else if (upper === "CH") {
    const value = parseInt(text, 10);
    if (!isNaN(value)) {
      UI.state.channel = value;
      storage.set("set.CH", String(value));
      updateChannelSelect();
      renderChannels();
    }
  } else if (upper === "PI") {
    applyPingResult(text);
  } else if (upper === "SEAR") {
    applySearchResult(text);
  } else if (upper === "BANK") {
    if (text && text.length >= 1) {
      const letter = text.trim().charAt(0);
      if (letter) {
        const normalized = letter.toLowerCase();
        UI.state.bank = normalized;
        storage.set("set.BANK", normalized);
        const bankSel = $("#BANK");
        if (bankSel) bankSel.value = normalized;
      }
    }
  } else if (upper === "ENCT") {
    const info = parseEncTestResponse(text);
    if (info) {
      UI.state.encTest = info;
      renderEncTest(info);
    }
  } else if (upper === "TESTRXM") {
    const info = parseTestRxmResponse(text);
    if (info) {
      if (info.status === "scheduled") {
        scheduleTestRxmRefresh(info);
        refreshReceivedList({ silentError: true });
      } else if (info.status === "busy") {
        note("TESTRXM уже выполняется");
      }
    }
  } else if (upper === "KEYSTATE" || upper === "KEYGEN" || upper === "KEYRESTORE" || upper === "KEYSEND" || upper === "KEYRECV" || upper === "KEYTRANSFER") {
    try {
      const data = JSON.parse(text);
      UI.key.state = data;
      if (upper === "KEYGEN") UI.key.lastMessage = "Сгенерирован новый локальный ключ";
      else if (upper === "KEYRESTORE") UI.key.lastMessage = "Восстановлена резервная версия ключа";
      else if (upper === "KEYSEND" || (upper === "KEYTRANSFER" && upperArg === "SEND")) UI.key.lastMessage = "Публичный ключ готов к передаче";
      else if (upper === "KEYRECV" || (upper === "KEYTRANSFER" && upperArg === "RECEIVE")) UI.key.lastMessage = "Получен внешний ключ";
      renderKeyState(data);
    } catch (err) {
      console.warn("[key] не удалось разобрать ответ", err);
    }
  }
}
async function probe() {
  const res = await deviceFetch("INFO", {}, 2500);
  if (res.ok) {
    status("Подключено: " + UI.cfg.endpoint);
  } else {
    status("Оффлайн · endpoint: " + UI.cfg.endpoint);
  }
}

/* Монитор принятых сообщений */
function getRecvLimit() {
  if (!UI.els.recvLimit) return 20;
  let limit = parseInt(UI.els.recvLimit.value || "20", 10);
  if (!Number.isFinite(limit) || limit <= 0) limit = 20;
  limit = Math.min(Math.max(limit, 1), 200);
  UI.els.recvLimit.value = String(limit);
  return limit;
}
function onRecvLimitChange() {
  const limit = getRecvLimit();
  storage.set("recvLimit", String(limit));
  if (UI.state.recvAuto) refreshReceivedList({ silentError: true });
}
function setRecvAuto(enabled, opts) {
  const options = opts || {};
  UI.state.recvAuto = enabled;
  if (UI.els.recvAuto) UI.els.recvAuto.checked = enabled;
  storage.set("recvAuto", enabled ? "1" : "0");
  if (UI.state.recvTimer) {
    clearInterval(UI.state.recvTimer);
    UI.state.recvTimer = null;
  }
  if (enabled) {
    if (!options.skipImmediate) {
      refreshReceivedList({ silentError: true });
    }
    const every = options.interval || 5000;
    UI.state.recvTimer = setInterval(() => refreshReceivedList({ silentError: true }), every);
  }
}
async function refreshReceivedList(opts) {
  if (!UI.els.recvList) return;
  const options = opts || {};
  const manual = options.manual === true;
  if (manual) status("→ RSTS");
  const limit = getRecvLimit();
  const text = await sendCommand("RSTS", { n: limit }, { silent: true, timeoutMs: 2500 });
  if (text === null) {
    if (!options.silentError) {
      if (manual) status("✗ RSTS");
      note("Не удалось получить список принятых сообщений");
    }
    return;
  }
  const names = text.split(/\r?\n/)
                    .map((line) => line.trim())
                    .filter((line) => line.length > 0);
  renderReceivedList(names);
  if (manual) status("✓ RSTS (" + names.length + ")");
}
function renderReceivedList(names) {
  if (!UI.els.recvList) return;
  UI.els.recvList.innerHTML = "";
  const prev = UI.state.receivedKnown instanceof Set ? UI.state.receivedKnown : new Set();
  const next = new Set();
  const frag = document.createDocumentFragment();
  names.forEach((name) => {
    next.add(name);
    const li = document.createElement("li");
    const label = document.createElement("span");
    label.className = "received-name";
    label.textContent = name;
    li.appendChild(label);
    const actions = document.createElement("div");
    actions.className = "received-actions";
    const copyBtn = document.createElement("button");
    copyBtn.type = "button";
    copyBtn.className = "icon-btn ghost";
    copyBtn.textContent = "⧉";
    copyBtn.title = "Скопировать имя";
    copyBtn.addEventListener("click", () => copyReceivedName(name));
    actions.appendChild(copyBtn);
    li.appendChild(actions);
    if (!prev.has(name)) {
      li.classList.add("fresh");
      setTimeout(() => li.classList.remove("fresh"), 1600);
      logReceivedNameInChat(name);
    }
    frag.appendChild(li);
  });
  UI.els.recvList.appendChild(frag);
  UI.state.receivedKnown = next;
  if (UI.els.recvEmpty) UI.els.recvEmpty.hidden = names.length > 0;
}

// Добавляем отметку о принятом сообщении в чат
function logReceivedNameInChat(name) {
  if (!name) return;
  const raw = String(name).trim();
  if (!/^GO-/i.test(raw)) return;
  const text = "RX: " + raw;
  const history = getChatHistory();
  const duplicate = Array.isArray(history) && history.some((entry) => entry && entry.tag === "rx-name" && entry.m === text);
  if (duplicate) return;
  const saved = persistChat(text, "dev", { role: "rx", tag: "rx-name" });
  addChatMessage(saved.record, saved.index);
}
async function copyReceivedName(name) {
  if (!name) return;
  let copied = false;
  try {
    if (navigator.clipboard && navigator.clipboard.writeText) {
      await navigator.clipboard.writeText(name);
      copied = true;
    }
  } catch (e) {
    copied = false;
  }
  if (!copied) {
    const area = document.createElement("textarea");
    area.value = name;
    area.setAttribute("readonly", "readonly");
    area.style.position = "absolute";
    area.style.left = "-9999px";
    document.body.appendChild(area);
    area.select();
    try {
      copied = document.execCommand("copy");
    } catch (err) {
      copied = false;
    }
    document.body.removeChild(area);
  }
  if (copied) {
    note("Имя " + name + " скопировано в буфер обмена");
  } else {
    note("Не удалось скопировать имя \"" + name + "\"");
  }
}

/* Таблица каналов */
let channels = [];
// Служебное состояние поиска по каналам
const searchState = { running: false, cancel: false };

// Скрываем панель информации о канале
function hideChannelInfo() {
  UI.state.infoChannel = null;
  UI.state.infoChannelTx = null;
  UI.state.infoChannelRx = null;
  updateChannelInfoPanel();
}

// Показываем сведения о выбранном канале и подгружаем справочник при необходимости
function showChannelInfo(ch) {
  const num = Number(ch);
  if (!Number.isFinite(num)) return;
  UI.state.infoChannel = num;
  const entry = channels.find((c) => c.ch === num);
  if (entry) {
    // Сохраняем частоты выбранного канала, чтобы карточка не пустела после обновления списка
    UI.state.infoChannelTx = entry.tx != null ? entry.tx : null;
    UI.state.infoChannelRx = entry.rx != null ? entry.rx : null;
  }
  updateChannelInfoPanel();
  if (!channelReference.ready && !channelReference.loading) {
    loadChannelReferenceData().then(() => {
      updateChannelInfoPanel();
    }).catch(() => {
      updateChannelInfoPanel();
    });
  }
}

// Обновляем визуальное выделение выбранной строки
function updateChannelInfoHighlight() {
  const rows = $all("#channelsTable tbody tr");
  rows.forEach((tr) => {
    const value = tr && tr.dataset ? Number(tr.dataset.ch) : NaN;
    const selected = UI.state.infoChannel != null && value === UI.state.infoChannel;
    tr.classList.toggle("selected-info", selected);
    if (tr) tr.setAttribute("aria-pressed", selected ? "true" : "false");
  });
}

// Обновляем содержимое панели информации
function updateChannelInfoPanel() {
  const panel = UI.els.channelInfoPanel || $("#channelInfoPanel");
  const body = UI.els.channelInfoBody || $("#channelInfoBody");
  const empty = UI.els.channelInfoEmpty || $("#channelInfoEmpty");
  const statusEl = UI.els.channelInfoStatus || $("#channelInfoStatus");
  if (!panel) return;
  if (UI.state.infoChannel == null) {
    panel.hidden = true;
    if (empty) empty.hidden = false;
    if (body) body.hidden = true;
    if (statusEl) { statusEl.textContent = ""; statusEl.hidden = true; }
    const setBtn = UI.els.channelInfoSetBtn || $("#channelInfoSetCurrent");
    if (setBtn) {
      setBtn.disabled = true;
      setBtn.textContent = "Установить текущим каналом";
    }
    updateChannelInfoHighlight();
    return;
  }

  panel.hidden = false;
  if (empty) empty.hidden = true;
  if (body) body.hidden = false;
  if (UI.els.channelInfoTitle) UI.els.channelInfoTitle.textContent = String(UI.state.infoChannel);

  const actualEntry = channels.find((c) => c.ch === UI.state.infoChannel);
  let entry = actualEntry;
  if (!entry && (UI.state.infoChannelTx != null || UI.state.infoChannelRx != null)) {
    entry = {
      ch: UI.state.infoChannel,
      tx: UI.state.infoChannelTx,
      rx: UI.state.infoChannelRx,
    };
  }
  const ref = entry ? (findChannelReferenceByTx(entry) || findChannelReferenceByTxValue(entry.tx)) : findChannelReferenceByTxValue(UI.state.infoChannelTx);
  const fields = UI.els.channelInfoFields || {};

  setChannelInfoText(fields.rxCurrent, entry ? formatChannelNumber(entry.rx, 3) : "—");
  setChannelInfoText(fields.txCurrent, entry ? formatChannelNumber(entry.tx, 3) : "—");
  setChannelInfoText(fields.rssi, entry && Number.isFinite(entry.rssi) ? String(entry.rssi) : "—");
  setChannelInfoText(fields.snr, entry && Number.isFinite(entry.snr) ? formatChannelNumber(entry.snr, 1) : "—");
  setChannelInfoText(fields.status, entry && entry.st ? entry.st : "—");
  setChannelInfoText(fields.scan, entry && entry.scan ? entry.scan : "—");

  setChannelInfoText(fields.rxRef, ref ? formatChannelNumber(ref.rx, 3) : "—");
  setChannelInfoText(fields.txRef, ref ? formatChannelNumber(ref.tx, 3) : "—");
  setChannelInfoText(fields.system, ref && ref.system ? ref.system : "—");
  setChannelInfoText(fields.band, ref && ref.band ? ref.band : "—");
  setChannelInfoText(fields.purpose, ref && ref.purpose ? ref.purpose : "—");

  const messages = [];
  if (channelReference.loading) messages.push("Загружаем справочник частот…");
  if (channelReference.error) {
    const errText = channelReference.error && channelReference.error.message ? channelReference.error.message : String(channelReference.error);
    messages.push("Не удалось загрузить справочник: " + errText);
  }
  if (!channelReference.loading && !channelReference.error && !ref) messages.push("В справочнике нет данных для этого канала.");
  if (!actualEntry) messages.push("Канал отсутствует в текущем списке.");
  if (statusEl) {
    const text = messages.join(" ");
    statusEl.textContent = text;
    statusEl.hidden = !text;
  }

  const setBtn = UI.els.channelInfoSetBtn || $("#channelInfoSetCurrent");
  if (setBtn) {
    const same = UI.state.channel != null && UI.state.channel === UI.state.infoChannel;
    setBtn.disabled = !!same;
    setBtn.textContent = same ? "Канал уже активен" : "Установить текущим каналом";
  }

  updateChannelInfoHighlight();
}

// Записываем значение в элемент, если он существует
function setChannelInfoText(el, text) {
  if (!el) return;
  el.textContent = text;
}

// Устанавливаем канал из карточки в качестве текущего
async function onChannelInfoSetCurrent() {
  const btn = UI.els.channelInfoSetBtn || $("#channelInfoSetCurrent");
  if (UI.state.infoChannel == null) {
    note("Выберите канал в таблице");
    return;
  }
  if (UI.state.channel != null && UI.state.channel === UI.state.infoChannel) {
    note("Канал " + UI.state.infoChannel + " уже активен");
    updateChannelInfoPanel();
    return;
  }
  if (btn) btn.disabled = true;
  let applied = false;
  try {
    const num = UI.state.infoChannel;
    const res = await sendCommand("CH", { v: String(num) });
    if (res === null) return;
    applied = true;
    storage.set("set.CH", String(num));
    UI.state.channel = num;
    updateChannelSelect();
    renderChannels();
    updateChannelSelectHint();
    updateChannelInfoPanel();
  } finally {
    if (!applied && btn) btn.disabled = false;
  }
}

// Форматируем числовое значение с заданным количеством знаков после запятой
function formatChannelNumber(value, digits) {
  const num = Number(value);
  if (!Number.isFinite(num)) return "—";
  return typeof digits === "number" ? num.toFixed(digits) : String(num);
}

// Загружаем CSV со справочной информацией
async function loadChannelReferenceData() {
  if (channelReference.ready) return channelReference.map;
  if (channelReference.promise) return channelReference.promise;
  channelReference.loading = true;
  channelReference.error = null;
  channelReference.promise = (async () => {
    const sources = ["libs/freq-info.csv", "./libs/freq-info.csv", "/libs/freq-info.csv"];
    let lastErr = null;
    try {
      for (let i = 0; i < sources.length; i++) {
        const url = sources[i];
        try {
          const res = await fetch(url, { cache: "no-store" });
          if (!res.ok) {
            lastErr = new Error("HTTP " + res.status + " (" + url + ")");
            continue;
          }
          const text = await res.text();
          applyChannelReferenceData(parseChannelReferenceCsv(text));
          channelReference.ready = true;
          channelReference.error = null;
          return channelReference.map;
        } catch (err) {
          lastErr = err;
        }
      }
      if (CHANNEL_REFERENCE_FALLBACK) {
        if (lastErr) console.warn("[freq-info] основной источник недоступен, используем встроенные данные:", lastErr);
        applyChannelReferenceData(parseChannelReferenceCsv(CHANNEL_REFERENCE_FALLBACK));
        channelReference.ready = true;
        channelReference.error = null;
        return channelReference.map;
      }
      const error = lastErr || new Error("Не удалось загрузить справочник");
      channelReference.error = error;
      throw error;
    } finally {
      channelReference.loading = false;
      channelReference.promise = null;
    }
  })();
  return channelReference.promise;
}

function applyChannelReferenceData(data) {
  const parsed = data || { byChannel: new Map(), byTx: new Map() };
  channelReference.map = parsed.byChannel instanceof Map ? parsed.byChannel : new Map();
  channelReference.byTx = parsed.byTx instanceof Map ? parsed.byTx : new Map();
}

function normalizeFrequencyKey(value) {
  const num = Number(value);
  if (!Number.isFinite(num)) return null;
  return Math.round(num * 1000);
}

function findChannelReferenceByTx(entry) {
  if (!entry || entry.tx == null) return null;
  const match = findChannelReferenceByTxValue(entry.tx, entry.ch);
  if (match) return match;
  if (entry.ch != null && channelReference.map.has(entry.ch)) {
    return channelReference.map.get(entry.ch);
  }
  return null;
}

// Ищем сведения по частоте передачи с учётом допустимого отклонения
function findChannelReferenceByTxValue(value, channelNumber) {
  if (value == null) return null;
  const key = normalizeFrequencyKey(value);
  if (key === null) return null;
  const list = channelReference.byTx.get(key);
  if (list && list.length) {
    if (channelNumber != null) {
      const exact = list.find((item) => item && item.ch === channelNumber);
      if (exact) return exact;
    }
    return list[0];
  }
  let closest = null;
  let closestDiff = Number.POSITIVE_INFINITY;
  const limit = 0.005; // 5 кГц допуска
  for (const items of channelReference.byTx.values()) {
    if (!items || !items.length) continue;
    for (let i = 0; i < items.length; i++) {
      const item = items[i];
      if (!item || item.tx == null) continue;
      const diff = Math.abs(Number(item.tx) - Number(value));
      if (diff < closestDiff && diff <= limit) {
        closest = item;
        closestDiff = diff;
        if (closestDiff === 0) return closest;
      }
    }
  }
  return closest;
}

// Преобразуем CSV в таблицу каналов
function parseChannelReferenceCsv(text) {
  const byChannel = new Map();
  const byTx = new Map();
  if (!text) return { byChannel, byTx };
  const lines = text.split(/\r?\n/);
  for (let i = 0; i < lines.length; i++) {
    let line = lines[i];
    if (!line) continue;
    line = line.replace(/\ufeff/g, "").trim();
    if (!line) continue;
    const cells = splitCsvRow(line);
    if (!cells.length) continue;
    const rawCh = cells[0] ? cells[0].trim() : "";
    if (!rawCh || /[^0-9]/.test(rawCh)) continue;
    const ch = parseInt(rawCh, 10);
    if (!Number.isFinite(ch)) continue;
    const rx = cells[1] ? Number(cells[1].trim()) : NaN;
    const tx = cells[2] ? Number(cells[2].trim()) : NaN;
    const entry = {
      ch,
      rx: Number.isFinite(rx) ? rx : null,
      tx: Number.isFinite(tx) ? tx : null,
      system: cells[3] ? cells[3].trim() : "",
      band: cells[4] ? cells[4].trim() : "",
      purpose: cells[5] ? cells[5].trim() : "",
    };
    byChannel.set(ch, entry);
    if (entry.tx != null) {
      const key = normalizeFrequencyKey(entry.tx);
      if (key !== null) {
        const list = byTx.get(key);
        if (list) list.push(entry);
        else byTx.set(key, [entry]);
      }
    }
  }
  return { byChannel, byTx };
}

// Разбиваем строку CSV с учётом кавычек
function splitCsvRow(row) {
  const out = [];
  let current = "";
  let quoted = false;
  for (let i = 0; i < row.length; i++) {
    const ch = row[i];
    if (ch === '"') {
      if (quoted && row[i + 1] === '"') {
        current += '"';
        i++;
      } else {
        quoted = !quoted;
      }
    } else if (ch === "," && !quoted) {
      out.push(current);
      current = "";
    } else {
      current += ch;
    }
  }
  out.push(current);
  return out;
}

function mockChannels() {
  channels = [
    { ch: 1, tx: 868.1, rx: 868.1, rssi: -92, snr: 8.5, st: "idle", scan: "", scanState: null },
    { ch: 2, tx: 868.3, rx: 868.3, rssi: -97, snr: 7.1, st: "listen", scan: "", scanState: null },
    { ch: 3, tx: 868.5, rx: 868.5, rssi: -88, snr: 10.2, st: "tx", scan: "", scanState: null },
  ];
}
function renderChannels() {
  const tbody = $("#channelsTable tbody");
  if (!tbody) return;
  tbody.innerHTML = "";
  channels.forEach((c) => {
    const tr = document.createElement("tr");
    const status = (c.st || "").toLowerCase();
    const stCls = status === "tx" || status === "listen" ? "busy" : status === "idle" ? "free" : "unknown";
    tr.classList.add(stCls);
    if (UI.state.channel === c.ch) tr.classList.add("active");
    const infoSelected = UI.state.infoChannel != null && UI.state.infoChannel === c.ch;
    const scanText = c.scan || "";
    const scanLower = scanText.toLowerCase();
    if (c.scanState) {
      tr.classList.add(c.scanState);
    } else if (scanLower.indexOf("crc") >= 0) {
      tr.classList.add("crc-error");
    } else if (scanLower.indexOf("timeout") >= 0 || scanLower.indexOf("тайм") >= 0 || scanLower.indexOf("noresp") >= 0) {
      tr.classList.add("no-response");
    } else if (scanLower) {
      tr.classList.add("signal");
    }
    tr.innerHTML = "<td>" + c.ch + "</td>" +
                   "<td>" + c.tx.toFixed(3) + "</td>" +
                   "<td>" + c.rx.toFixed(3) + "</td>" +
                   "<td>" + (isNaN(c.rssi) ? "" : c.rssi) + "</td>" +
                   "<td>" + (isNaN(c.snr) ? "" : c.snr) + "</td>" +
                   "<td>" + (c.st || "") + "</td>" +
                   "<td>" + scanText + "</td>";
    tr.dataset.ch = String(c.ch);
    tr.tabIndex = 0;
    tr.setAttribute("role", "button");
    tr.setAttribute("aria-pressed", infoSelected ? "true" : "false");
    if (infoSelected) tr.classList.add("selected-info");
    tbody.appendChild(tr);
  });
  if (UI.state.infoChannel != null) {
    const current = channels.find((c) => c.ch === UI.state.infoChannel);
    if (current) {
      UI.state.infoChannelTx = current.tx != null ? current.tx : UI.state.infoChannelTx;
      UI.state.infoChannelRx = current.rx != null ? current.rx : UI.state.infoChannelRx;
    }
  }
  updateChannelInfoPanel();
}
function updateChannelSelect() {
  const sel = UI.els.channelSelect || $("#CH");
  if (!sel) return;
  UI.els.channelSelect = sel;
  const prev = UI.state.channel != null ? String(UI.state.channel) : sel.value;
  sel.innerHTML = "";
  if (!channels.length) {
    const opt = document.createElement("option");
    opt.value = "";
    opt.textContent = "—";
    sel.appendChild(opt);
    sel.disabled = true;
    sel.value = "";
  } else {
    sel.disabled = false;
    channels.forEach((c) => {
      const opt = document.createElement("option");
      opt.value = c.ch;
      opt.textContent = c.ch;
      sel.appendChild(opt);
    });
    if (prev && channels.some((c) => String(c.ch) === prev)) sel.value = prev;
    else if (sel.options.length) sel.value = sel.options[0].value;
  }
  updateChannelSelectHint();
}
// Обработка выбора канала в выпадающем списке Settings с немедленным применением
async function onChannelSelectChange(event) {
  const sel = event && event.target ? event.target : (UI.els.channelSelect || $("#CH"));
  if (!sel) return;
  const raw = sel.value;
  const num = parseInt(raw, 10);
  if (isNaN(num)) {
    note("Некорректный номер канала");
    if (UI.state.channel != null) sel.value = String(UI.state.channel);
    updateChannelSelectHint();
    return;
  }
  if (UI.state.channel === num) return;
  const prev = UI.state.channel;
  sel.disabled = true;
  try {
    const res = await sendCommand("CH", { v: String(num) });
    if (res === null) {
      if (prev != null) {
        sel.value = String(prev);
      } else {
        await refreshChannels().catch(() => {});
      }
    } else {
      UI.state.channel = num;
      storage.set("set.CH", String(num));
      updateChannelSelect();
      renderChannels();
    }
  } finally {
    sel.disabled = false;
    updateChannelSelectHint();
  }
}
// Обновляем подпись с текущими частотами рядом с выпадающим списком каналов
function updateChannelSelectHint() {
  const hint = UI.els.channelSelectHint || $("#channelSelectHint");
  if (!hint) return;
  const sel = UI.els.channelSelect || $("#CH");
  if (!sel) return;
  const raw = sel.value;
  if (!raw) {
    hint.textContent = channels.length ? "Канал не выбран" : "Нет данных о каналах";
    hint.hidden = false;
    return;
  }
  const num = Number(raw);
  const entry = channels.find((c) => c.ch === num);
  if (!entry) {
    hint.textContent = "Нет данных о частотах";
    hint.hidden = false;
    return;
  }
  const rx = formatChannelNumber(entry.rx, 3);
  const tx = formatChannelNumber(entry.tx, 3);
  hint.textContent = "RX " + rx + " · TX " + tx;
  hint.hidden = false;
}
async function refreshChannels(options) {
  const opts = options || {};
  const bankSel = $("#BANK");
  const bank = bankSel ? bankSel.value : null;
  try {
    const shouldApplyBank = bank && (opts.forceBank === true || UI.state.bank !== bank);
    if (shouldApplyBank) {
      const res = await deviceFetch("BANK", { v: bank }, 2500);
      if (res.ok) {
        UI.state.bank = bank;
        storage.set("set.BANK", bank);
      }
    }
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
      if (!isNaN(num)) {
        UI.state.channel = num;
        storage.set("set.CH", String(num));
      }
    }
  } catch (e) {
    if (!channels.length) mockChannels();
    debugLog("ERR refreshChannels: " + e);
  }
  if (UI.state.channel == null) {
    const savedRaw = storage.get("set.CH");
    const savedNum = parseInt(savedRaw || "", 10);
    if (!isNaN(savedNum)) UI.state.channel = savedNum;
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
      scanState: null,
    });
  }
  return out;
}
function applyPingResult(text) {
  if (UI.state.channel == null) return;
  const entry = channels.find((c) => c.ch === UI.state.channel);
  if (!entry) return;
  applyPingToEntry(entry, text);
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
    const state = detectScanState(info);
    entry.scanState = state || entry.scanState;
    changed = true;
  }
  if (changed) renderChannels();
}
function exportChannelsCsv() {
  const lines = [["ch","tx","rx","rssi","snr","status","scan_state","scan"]];
  channels.forEach((c) => {
    lines.push([c.ch, c.tx, c.rx, c.rssi, c.snr, c.st, c.scanState || "", c.scan]);
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
  if (UI.state.channel != null) {
    const entry = setChannelScanState(UI.state.channel, "scanning", "Проверка...");
    if (entry) renderChannels();
  }
  await sendCommand("PI");
}
// Определяем состояние строки по тексту ответа
function detectScanState(text) {
  if (!text) return null;
  const low = text.toLowerCase();
  if (low.indexOf("crc") >= 0) return "crc-error";
  if (low.indexOf("timeout") >= 0 || low.indexOf("no resp") >= 0 || low.indexOf("noresp") >= 0 || low.indexOf("нет ответа") >= 0 || low.indexOf("тайм") >= 0 || (low.indexOf("error") >= 0 && low.indexOf("crc") < 0) || (low.indexOf("err") >= 0 && low.indexOf("crc") < 0) || low.indexOf("fail") >= 0) return "no-response";
  if (low.indexOf("rssi") >= 0 || low.indexOf("snr") >= 0 || low.indexOf("ok") >= 0 || low.indexOf("ответ") >= 0) return "signal";
  return null;
}
// Обновляем данные канала после пинга
function applyPingToEntry(entry, text) {
  if (!entry) return null;
  const raw = (text || "").trim();
  const rssiMatch = raw.match(/RSSI\s*(-?\d+(?:\.\d+)?)/i);
  const snrMatch = raw.match(/SNR\s*(-?\d+(?:\.\d+)?)/i);
  if (rssiMatch) entry.rssi = Number(rssiMatch[1]);
  if (snrMatch) entry.snr = Number(snrMatch[1]);
  entry.scan = raw;
  const state = detectScanState(raw);
  entry.scanState = state || "signal";
  return entry.scanState;
}
// Устанавливаем служебный статус строки
function setChannelScanState(ch, state, text) {
  const num = Number(ch);
  const entry = channels.find((c) => c.ch === (isNaN(num) ? ch : num));
  if (!entry) return null;
  entry.scanState = state || null;
  if (typeof text === "string") entry.scan = text;
  return entry;
}
async function runSearch() {
  if (!channels.length) {
    note("Список каналов пуст, обновите данные.");
    return;
  }
  if (searchState.running) {
    searchState.cancel = true;
    note("Останавливаю поиск...");
    return;
  }
  searchState.running = true;
  searchState.cancel = false;
  if (UI.els.searchBtn) {
    UI.els.searchBtn.textContent = "Stop";
    UI.els.searchBtn.classList.add("ghost");
  }
  await uiYield();
  const prevChannel = UI.state.channel;
  let cancelled = false;
  status("Search: запуск...");
  await uiYield();
  try {
    for (let i = 0; i < channels.length; i++) {
      if (searchState.cancel) {
        cancelled = true;
        break;
      }
      const entry = channels[i];
      setChannelScanState(entry.ch, "scanning", "Проверка...");
      renderChannels();
      await uiYield();
      status("Search: CH " + entry.ch + " (" + (i + 1) + "/" + channels.length + ")");
      await uiYield();
      const chRes = await deviceFetch("CH", { v: String(entry.ch) }, 2500);
      if (!chRes.ok) {
        entry.scan = "ERR CH: " + chRes.error;
        entry.scanState = "no-response";
        renderChannels();
        await uiYield();
        continue;
      }
      UI.state.channel = entry.ch;
      updateChannelSelect();
      await new Promise((resolve) => setTimeout(resolve, 120));
      const pingRes = await deviceFetch("PI", {}, 5000);
      if (searchState.cancel) {
        cancelled = true;
        break;
      }
      if (pingRes.ok) {
        applyPingToEntry(entry, pingRes.text);
      } else {
        entry.scan = "ERR PI: " + pingRes.error;
        entry.scanState = "no-response";
      }
      renderChannels();
      await uiYield();
    }
  } catch (e) {
    cancelled = cancelled || searchState.cancel;
    debugLog("ERR Search: " + e);
  } finally {
    const requestedCancel = searchState.cancel || cancelled;
    searchState.running = false;
    searchState.cancel = false;
    if (requestedCancel) {
      channels.forEach((c) => { if (c.scanState === "scanning") c.scanState = null; });
    }
    if (prevChannel != null) {
      const revert = await deviceFetch("CH", { v: String(prevChannel) }, 2500);
      if (revert.ok) {
        UI.state.channel = prevChannel;
        updateChannelSelect();
      }
    }
    renderChannels();
    if (UI.els.searchBtn) {
      UI.els.searchBtn.textContent = "Search";
      UI.els.searchBtn.classList.remove("ghost");
    }
    status(requestedCancel ? "Search: остановлено" : "Search: завершено");
    await uiYield();
    if (requestedCancel) note("Сканирование остановлено.");
    else note("Сканирование завершено.");
  }
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
// Унифицированная блокировка элементов управления ACK на время запроса
async function withAckLock(task) {
  if (UI.state.ackBusy) return null;
  UI.state.ackBusy = true;
  updateAckRetryUi();
  const chip = UI.els.ackChip;
  const ackSwitch = UI.els.ackSetting;
  const ackWrap = UI.els.ackSettingWrap;
  if (chip) {
    chip.disabled = true;
    chip.setAttribute("aria-busy", "true");
  }
  if (ackSwitch) {
    ackSwitch.disabled = true;
    ackSwitch.setAttribute("aria-busy", "true");
  }
  if (ackWrap) ackWrap.setAttribute("aria-busy", "true");
  try {
    return await task();
  } finally {
    UI.state.ackBusy = false;
    if (chip) {
      chip.removeAttribute("aria-busy");
      chip.disabled = false;
    }
    if (ackSwitch) {
      ackSwitch.removeAttribute("aria-busy");
      ackSwitch.disabled = false;
    }
    if (ackWrap) ackWrap.removeAttribute("aria-busy");
    updateAckUi();
  }
}
// Обработчик клика по чипу состояния ACK: переключаем режим с учётом блокировки
async function onAckChipToggle() {
  await withAckLock(async () => {
    const current = UI.state.ack;
    if (current === true) {
      await setAck(false);
    } else if (current === false) {
      await setAck(true);
    } else {
      const result = await setAck(true);
      if (result === null) await toggleAck();
    }
  });
}
function parseAckResponse(text) {
  if (!text) return null;
  const low = text.toLowerCase();
  if (low.indexOf("ack:1") >= 0 || /\b1\b/.test(low) || low.indexOf("on") >= 0 || low.indexOf("включ") >= 0) return true;
  if (low.indexOf("ack:0") >= 0 || /\b0\b/.test(low) || low.indexOf("off") >= 0 || low.indexOf("выключ") >= 0) return false;
  return null;
}
function clampAckRetry(value) {
  const num = Number(value);
  if (!Number.isFinite(num)) return ACK_RETRY_DEFAULT;
  if (num < 0) return 0;
  if (num > ACK_RETRY_MAX) return ACK_RETRY_MAX;
  return Math.round(num);
}
function clampPauseMs(value) {
  const num = Number(value);
  if (!Number.isFinite(num)) return PAUSE_MIN_MS;
  if (num < PAUSE_MIN_MS) return PAUSE_MIN_MS;
  if (num > PAUSE_MAX_MS) return PAUSE_MAX_MS;
  return Math.round(num);
}
function clampAckTimeoutMs(value) {
  const num = Number(value);
  if (!Number.isFinite(num)) return ACK_TIMEOUT_MIN_MS;
  if (num < ACK_TIMEOUT_MIN_MS) return ACK_TIMEOUT_MIN_MS;
  if (num > ACK_TIMEOUT_MAX_MS) return ACK_TIMEOUT_MAX_MS;
  return Math.round(num);
}
function clampTestRxmMessage(value) {
  const text = value == null ? "" : String(value);
  if (text.length > TEST_RXM_MESSAGE_MAX) {
    return text.slice(0, TEST_RXM_MESSAGE_MAX);
  }
  return text;
}
function parseAckRetryResponse(text) {
  if (!text) return null;
  const match = String(text).match(/-?\d+/);
  if (!match) return null;
  return clampAckRetry(Number(match[0]));
}
function parsePauseResponse(text) {
  if (!text) return null;
  const token = extractNumericToken(text);
  if (token == null) return null;
  return clampPauseMs(Number(token));
}
function parseAckTimeoutResponse(text) {
  if (!text) return null;
  const token = extractNumericToken(text);
  if (token == null) return null;
  return clampAckTimeoutMs(Number(token));
}
function updateAckRetryUi() {
  const input = UI.els.ackRetry;
  const state = UI.state.ackRetry;
  if (input) {
    if (state != null && document.activeElement !== input) {
      input.value = String(state);
    }
    const disabled = UI.state.ack !== true || UI.state.ackBusy;
    input.disabled = disabled;
  }
  const hint = UI.els.ackRetryHint;
  if (hint) {
    if (UI.state.ack === true) {
      const attempts = state != null ? state : "—";
      const timeout = UI.state.ackTimeout != null ? UI.state.ackTimeout + " мс" : "—";
      const pause = UI.state.pauseMs != null ? UI.state.pauseMs + " мс" : "—";
      hint.textContent = "Повторные отправки: " + attempts + " раз. Пауза: " + pause + ". Ожидание ACK: " + timeout + ".";
    } else {
      hint.textContent = "Доступно после включения ACK.";
    }
  }
}
function updatePauseUi() {
  const input = UI.els.pauseInput;
  const value = UI.state.pauseMs;
  if (input && document.activeElement !== input && value != null) {
    input.value = String(value);
  }
  const hint = UI.els.pauseHint;
  if (hint) {
    hint.textContent = value != null
      ? "Минимальная пауза между пакетами: " + value + " мс. Параметр глобальный и позволяет дождаться ACK для каждого пакета, но действует даже при выключенных подтверждениях."
      : "Пауза между пакетами не загружена.";
  }
}
function updateAckTimeoutUi() {
  const input = UI.els.ackTimeout;
  const value = UI.state.ackTimeout;
  if (input && document.activeElement !== input && value != null) {
    input.value = String(value);
  }
  const hint = UI.els.ackTimeoutHint;
  if (hint) {
    hint.textContent = value != null ? ("Время ожидания ACK: " + value + " мс.") : "Время ожидания ACK не загружено.";
  }
}
function updateAckUi() {
  const chip = UI.els.ackChip;
  const text = UI.els.ackText;
  const state = UI.state.ack;
  const mode = state === true ? "on" : state === false ? "off" : "unknown";
  if (chip) {
    chip.setAttribute("data-state", mode);
    chip.setAttribute("aria-pressed", state === true ? "true" : state === false ? "false" : "mixed");
  }
  if (text) text.textContent = state === true ? "ON" : state === false ? "OFF" : "—";
  const ackSwitch = UI.els.ackSetting;
  if (ackSwitch) {
    ackSwitch.indeterminate = !(state === true || state === false);
    ackSwitch.checked = state === true;
  }
  const hint = UI.els.ackSettingHint;
  if (hint) {
    if (state === true) {
      hint.textContent = "Автоподтверждения включены, устройство шлёт ACK автоматически.";
    } else if (state === false) {
      hint.textContent = "Автоподтверждения выключены, подтверждения нужно отправлять вручную.";
    } else {
      hint.textContent = "Состояние ACK неизвестно. Обновите данные или попробуйте снова.";
    }
  }
  updateAckRetryUi();
}
function onPauseInputChange() {
  if (!UI.els.pauseInput) return;
  const raw = UI.els.pauseInput.value;
  const num = clampPauseMs(parseInt(raw, 10));
  UI.els.pauseInput.value = String(num);
  UI.state.pauseMs = num;
  storage.set("set.PAUSE", String(num));
  updatePauseUi();
}
function onAckTimeoutInputChange() {
  if (!UI.els.ackTimeout) return;
  const raw = UI.els.ackTimeout.value;
  const num = clampAckTimeoutMs(parseInt(raw, 10));
  UI.els.ackTimeout.value = String(num);
  UI.state.ackTimeout = num;
  storage.set("set.ACKT", String(num));
  updateAckTimeoutUi();
  updateAckRetryUi();
}
async function setAck(value) {
  const response = await sendCommand("ACK", { v: value ? "1" : "0" });
  if (typeof response === "string") {
    const parsed = parseAckResponse(response);
    if (parsed !== null) {
      UI.state.ack = parsed;
      updateAckUi();
      return parsed;
    }
  }
  return await refreshAckState();
}
async function setAckRetry(count) {
  const clamped = clampAckRetry(count);
  const response = await withAckLock(async () => {
    const res = await sendCommand("ACKR", { v: String(clamped) });
    if (typeof res === "string") {
      const parsed = parseAckRetryResponse(res);
      if (parsed !== null) {
        UI.state.ackRetry = parsed;
        storage.set("set.ACKR", String(parsed));
        updateAckRetryUi();
        return parsed;
      }
    }
    return await refreshAckRetry();
  });
  return response;
}
async function toggleAck() {
  const response = await sendCommand("ACK", { toggle: "1" });
  if (typeof response === "string") {
    const parsed = parseAckResponse(response);
    if (parsed !== null) {
      UI.state.ack = parsed;
      updateAckUi();
      return parsed;
    }
  }
  return await refreshAckState();
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
  UI.state.ack = null;
  updateAckUi();
  return null;
}
async function refreshAckRetry() {
  const res = await deviceFetch("ACKR", {}, 2000);
  if (res.ok) {
    const parsed = parseAckRetryResponse(res.text);
    if (parsed !== null) {
      UI.state.ackRetry = parsed;
      storage.set("set.ACKR", String(parsed));
      updateAckRetryUi();
      return parsed;
    }
  }
  UI.state.ackRetry = null;
  updateAckRetryUi();
  return null;
}
async function onAckRetryInput() {
  if (!UI.els.ackRetry) return;
  const raw = UI.els.ackRetry.value;
  const clamped = clampAckRetry(parseInt(raw, 10));
  UI.els.ackRetry.value = String(clamped);
  storage.set("set.ACKR", String(clamped));
  if (UI.state.ack !== true) {
    updateAckRetryUi();
    return;
  }
  await setAckRetry(clamped);
}
function onTestRxmMessageChange() {
  if (!UI.els.testRxmMessage) return;
  const normalized = clampTestRxmMessage(UI.els.testRxmMessage.value || "");
  if (UI.els.testRxmMessage.value !== normalized) {
    UI.els.testRxmMessage.value = normalized;
  }
  storage.set("set.TESTRXMMSG", normalized);
  updateTestRxmMessageHint();
}
function updateTestRxmMessageHint() {
  const hint = UI.els.testRxmMessageHint || $("#testRxmMessageHint");
  const input = UI.els.testRxmMessage || $("#TESTRXMMSG");
  if (!hint) return;
  const raw = input ? String(input.value || "") : "";
  const trimmed = raw.trim();
  if (!trimmed) {
    hint.textContent = "Будет использован встроенный шаблон Lorem ipsum.";
  } else {
    hint.textContent = "Длина сообщения: " + raw.length + " символ(ов).";
  }
}
async function runTestRxm() {
  const params = {};
  if (UI.els.testRxmMessage) {
    let value = UI.els.testRxmMessage.value || "";
    value = clampTestRxmMessage(value);
    if (UI.els.testRxmMessage.value !== value) {
      UI.els.testRxmMessage.value = value;
    }
    storage.set("set.TESTRXMMSG", value);
    updateTestRxmMessageHint();
    const trimmed = value.trim();
    params.msg = trimmed ? value : " ";
  }
  await sendCommand("TESTRXM", params);
}

/* ENCT визуализация */
function parseEncTestResponse(text) {
  const raw = typeof text === "string" ? text.trim() : "";
  if (!raw) return null;
  if (raw.startsWith("{")) {
    try {
      const data = JSON.parse(raw);
      return {
        status: data && typeof data.status === "string" ? data.status : "unknown",
        plain: data && typeof data.plain === "string" ? data.plain : "",
        decoded: data && typeof data.decoded === "string" ? data.decoded : "",
        cipher: data && typeof data.cipher === "string" ? data.cipher : "",
        tag: data && typeof data.tag === "string" ? data.tag : "",
        nonce: data && typeof data.nonce === "string" ? data.nonce : "",
        error: data && typeof data.error === "string" ? data.error : null,
        legacy: false,
      };
    } catch (err) {
      console.warn("[enct] parse", err);
      return null;
    }
  }
  if (/ENCT:OK/i.test(raw)) {
    return { status: "ok", plain: "", decoded: "", cipher: "", tag: "", nonce: "", error: null, legacy: true };
  }
  if (/ENCT:ERR/i.test(raw)) {
    return { status: "error", plain: "", decoded: "", cipher: "", tag: "", nonce: "", error: "legacy", legacy: true };
  }
  return null;
}
function renderEncTest(data) {
  const els = UI.els.encTest || {};
  const container = els.container;
  const status = data && data.status ? String(data.status) : null;
  if (container) {
    if (!status) container.removeAttribute("data-status");
    else container.setAttribute("data-status", status === "ok" ? "ok" : (status === "error" ? "error" : "idle"));
  }
  if (els.status) {
    let text = "Нет данных";
    if (data) {
      if (status === "ok") text = data.legacy ? "Успех (ограниченный ответ)" : "Успех";
      else if (status === "error") text = data.error ? "Ошибка: " + data.error : "Ошибка";
      else text = status;
    }
    els.status.textContent = text;
  }
  const formatHex = (value) => {
    if (!value) return "—";
    return value.replace(/(.{2})/g, "$1 ").trim();
  };
  if (els.plain) {
    if (data && data.plain) els.plain.textContent = data.plain;
    else if (data && data.legacy) els.plain.textContent = "Недоступно";
    else els.plain.textContent = "—";
  }
  if (els.decoded) {
    if (data && data.decoded) els.decoded.textContent = data.decoded;
    else if (data && data.legacy) els.decoded.textContent = "Недоступно";
    else els.decoded.textContent = "—";
  }
  if (els.cipher) els.cipher.textContent = data ? formatHex(data.cipher) : "—";
  if (els.tag) els.tag.textContent = data ? formatHex(data.tag) : "—";
  if (els.nonce) els.nonce.textContent = data ? formatHex(data.nonce) : "—";
}
function parseTestRxmResponse(text) {
  const raw = typeof text === "string" ? text.trim() : "";
  if (!raw) return null;
  if (raw.startsWith("{")) {
    try {
      const data = JSON.parse(raw);
      return {
        status: data && typeof data.status === "string" ? data.status : "unknown",
        count: Number(data && data.count != null ? data.count : 0) || 0,
        intervalMs: Number(data && data.intervalMs != null ? data.intervalMs : 0) || 0,
      };
    } catch (err) {
      console.warn("[testrxm] parse", err);
      return null;
    }
  }
  if (/busy/i.test(raw)) return { status: "busy", count: 0, intervalMs: 0 };
  return null;
}
function clearTestRxmTimers() {
  if (!Array.isArray(UI.state.testRxmTimers)) UI.state.testRxmTimers = [];
  for (const id of UI.state.testRxmTimers) {
    clearTimeout(id);
  }
  UI.state.testRxmTimers = [];
}
function scheduleTestRxmRefresh(info) {
  clearTestRxmTimers();
  const count = Math.max(1, Number(info && info.count ? info.count : 5) || 5);
  const interval = Math.max(200, Number(info && info.intervalMs ? info.intervalMs : 500) || 500);
  for (let i = 1; i <= count + 1; ++i) {
    const timer = setTimeout(() => refreshReceivedList({ silentError: true }), interval * i + 80);
    UI.state.testRxmTimers.push(timer);
  }
}

/* Encryption */
async function withEncLock(task) {
  if (UI.state.encBusy) return null;
  UI.state.encBusy = true;
  const chip = UI.els.encChip;
  if (chip) {
    chip.disabled = true;
    chip.setAttribute("aria-busy", "true");
  }
  try {
    return await task();
  } finally {
    UI.state.encBusy = false;
    if (chip) {
      chip.disabled = false;
      chip.removeAttribute("aria-busy");
    }
    updateEncryptionUi();
  }
}
function parseEncResponse(text) {
  if (!text) return null;
  const low = text.toLowerCase();
  if (low.indexOf("enc:1") >= 0 || /\b1\b/.test(low) || low.indexOf("on") >= 0 || low.indexOf("включ") >= 0) return true;
  if (low.indexOf("enc:0") >= 0 || /\b0\b/.test(low) || low.indexOf("off") >= 0 || low.indexOf("выключ") >= 0) return false;
  return null;
}
function updateEncryptionUi() {
  const chip = UI.els.encChip;
  const text = UI.els.encText;
  const state = UI.state.encryption;
  const mode = state === true ? "on" : state === false ? "off" : "unknown";
  if (chip) {
    chip.setAttribute("data-state", mode);
    chip.setAttribute("aria-pressed", state === true ? "true" : state === false ? "false" : "mixed");
    if (UI.state.encBusy) chip.setAttribute("aria-busy", "true");
    else chip.removeAttribute("aria-busy");
    chip.disabled = UI.state.encBusy;
  }
  if (text) text.textContent = state === true ? "ON" : state === false ? "OFF" : "—";
}
async function setEncryption(enabled) {
  const desired = enabled ? true : false;
  const result = await withEncLock(async () => {
    const response = await sendCommand("ENC", { v: desired ? "1" : "0" });
    if (typeof response === "string") {
      const parsed = parseEncResponse(response);
      if (parsed !== null) {
        UI.state.encryption = parsed;
        updateEncryptionUi();
        return parsed;
      }
    }
    return await refreshEncryptionState();
  });
  return result;
}
async function toggleEncryption() {
  const current = UI.state.encryption;
  if (current === true) return await setEncryption(false);
  return await setEncryption(true);
}
async function onEncChipToggle() {
  await toggleEncryption();
}
async function refreshEncryptionState() {
  const res = await deviceFetch("ENC", {}, 2000);
  if (res.ok) {
    const state = parseEncResponse(res.text);
    if (state !== null) {
      UI.state.encryption = state;
      updateEncryptionUi();
      return state;
    }
  }
  UI.state.encryption = null;
  updateEncryptionUi();
  return null;
}

/* Настройки */
const SETTINGS_KEYS = ["BANK","BF","CH","CR","PW","SF","PAUSE","ACKT","ACKR","TESTRXMMSG"];
function normalizePowerPreset(raw) {
  if (raw == null) return null;
  const str = String(raw).trim();
  if (!str) return null;
  const num = Number(str);
  if (!Number.isFinite(num)) return null;
  const idxValue = POWER_PRESETS.indexOf(num);
  if (idxValue >= 0) {
    return { index: idxValue, value: POWER_PRESETS[idxValue] };
  }
  if (Number.isInteger(num) && num >= 0 && num < POWER_PRESETS.length) {
    return { index: num, value: POWER_PRESETS[num] };
  }
  return null;
}
function loadSettings() {
  for (let i = 0; i < SETTINGS_KEYS.length; i++) {
    const key = SETTINGS_KEYS[i];
    const el = $("#" + key);
    if (!el) continue;
    const v = storage.get("set." + key);
    if (v === null) continue;
    if (el.type === "checkbox") {
      el.checked = v === "1";
      if (typeof el.indeterminate === "boolean") el.indeterminate = false;
    } else if (key === "PW") {
      const resolved = normalizePowerPreset(v);
      if (resolved) {
        el.value = String(resolved.value);
      }
    } else if (key === "CH") {
      const num = parseInt(v, 10);
      if (!isNaN(num)) {
        UI.state.channel = num;
        el.value = String(num);
      } else {
        el.value = v;
      }
    } else if (key === "ACKR") {
      const num = clampAckRetry(parseInt(v, 10));
      if (UI.els.ackRetry) UI.els.ackRetry.value = String(num);
      UI.state.ackRetry = num;
      updateAckRetryUi();
    } else if (key === "PAUSE") {
      const num = clampPauseMs(parseInt(v, 10));
      if (UI.els.pauseInput) UI.els.pauseInput.value = String(num);
      UI.state.pauseMs = num;
      updatePauseUi();
    } else if (key === "ACKT") {
      const num = clampAckTimeoutMs(parseInt(v, 10));
      if (UI.els.ackTimeout) UI.els.ackTimeout.value = String(num);
      UI.state.ackTimeout = num;
      updateAckTimeoutUi();
    } else if (key === "TESTRXMMSG") {
      const text = clampTestRxmMessage(v);
      if (UI.els.testRxmMessage) UI.els.testRxmMessage.value = text;
      updateTestRxmMessageHint();
    } else {
      el.value = v;
      if (key === "BANK") {
        UI.state.bank = v;
      }
    }
  }
}
function saveSettingsLocal() {
  for (let i = 0; i < SETTINGS_KEYS.length; i++) {
    const key = SETTINGS_KEYS[i];
    const el = $("#" + key);
    if (!el) continue;
    if (el.type === "checkbox" && typeof el.indeterminate === "boolean" && el.indeterminate) {
      storage.remove("set." + key);
      continue;
    }
    let v = el.type === "checkbox" ? (el.checked ? "1" : "0") : el.value;
    if (key === "ACKR") {
      v = String(clampAckRetry(parseInt(v, 10)));
    } else if (key === "PAUSE") {
      v = String(clampPauseMs(parseInt(v, 10)));
    } else if (key === "ACKT") {
      v = String(clampAckTimeoutMs(parseInt(v, 10)));
    } else if (key === "TESTRXMMSG") {
      v = clampTestRxmMessage(v);
      if (UI.els.testRxmMessage) UI.els.testRxmMessage.value = v;
      updateTestRxmMessageHint();
    }
    storage.set("set." + key, v);
  }
  note("Сохранено локально.");
}
async function applySettingsToDevice() {
  for (let i = 0; i < SETTINGS_KEYS.length; i++) {
    const key = SETTINGS_KEYS[i];
    const el = $("#" + key);
    if (!el) continue;
    if (el.type === "checkbox" && typeof el.indeterminate === "boolean" && el.indeterminate) {
      continue;
    }
    let value = el.type === "checkbox" ? (el.checked ? "1" : "0") : String(el.value || "").trim();
    if (key === "TESTRXMMSG") {
      const normalized = clampTestRxmMessage(el.value || "");
      if (UI.els.testRxmMessage) UI.els.testRxmMessage.value = normalized;
      storage.set("set.TESTRXMMSG", normalized);
      updateTestRxmMessageHint();
      continue;
    }
    if (!value) continue;
    if (key === "PW") {
      const resolved = normalizePowerPreset(value);
      if (!resolved) {
        note("Некорректная мощность передачи");
        continue;
      }
      const resp = await sendCommand(key, { v: String(resolved.index) });
      if (resp !== null) {
        storage.set("set.PW", String(resolved.value));
        el.value = String(resolved.value);
      }
    } else if (key === "ACKR") {
      await setAckRetry(parseInt(value, 10));
    } else if (key === "CH") {
      const resp = await sendCommand(key, { v: value });
      if (resp !== null) {
        const num = parseInt(value, 10);
        if (!isNaN(num)) {
          UI.state.channel = num;
          storage.set("set.CH", String(num));
          updateChannelSelect();
          renderChannels();
        }
      }
    } else if (key === "PAUSE") {
      const parsed = clampPauseMs(parseInt(value, 10));
      const resp = await sendCommand("PAUSE", { v: String(parsed) });
      if (resp !== null) {
        const applied = parsePauseResponse(resp);
        const effective = applied != null ? applied : parsed;
        UI.state.pauseMs = effective;
        if (UI.els.pauseInput) UI.els.pauseInput.value = String(effective);
        updatePauseUi();
        storage.set("set.PAUSE", String(effective));
      }
    } else if (key === "ACKT") {
      const parsed = clampAckTimeoutMs(parseInt(value, 10));
      const resp = await sendCommand("ACKT", { v: String(parsed) });
      if (resp !== null) {
        const applied = parseAckTimeoutResponse(resp);
        const effective = applied != null ? applied : parsed;
        UI.state.ackTimeout = effective;
        if (UI.els.ackTimeout) UI.els.ackTimeout.value = String(effective);
        updateAckTimeoutUi();
        updateAckRetryUi();
        storage.set("set.ACKT", String(effective));
      }
    } else {
      const resp = await sendCommand(key, { v: value });
      if (resp !== null) storage.set("set." + key, value);
    }
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
    if (el.type === "checkbox" && typeof el.indeterminate === "boolean" && el.indeterminate) {
      continue;
    }
    if (key === "ACKR") {
      obj[key] = String(clampAckRetry(parseInt(el.value, 10)));
    } else if (key === "PAUSE") {
      obj[key] = String(clampPauseMs(parseInt(el.value, 10)));
    } else if (key === "ACKT") {
      obj[key] = String(clampAckTimeoutMs(parseInt(el.value, 10)));
    } else if (key === "TESTRXMMSG") {
      obj[key] = clampTestRxmMessage(el.value || "");
    } else {
      obj[key] = el.type === "checkbox" ? (el.checked ? "1" : "0") : el.value;
    }
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
      if (key === "ACKR") {
        const num = clampAckRetry(parseInt(obj[key], 10));
        if (UI.els.ackRetry) UI.els.ackRetry.value = String(num);
        UI.state.ackRetry = num;
        updateAckRetryUi();
        storage.set("set.ACKR", String(num));
        continue;
      }
      if (key === "PAUSE") {
        const num = clampPauseMs(parseInt(obj[key], 10));
        if (UI.els.pauseInput) UI.els.pauseInput.value = String(num);
        UI.state.pauseMs = num;
        updatePauseUi();
        storage.set("set.PAUSE", String(num));
        continue;
      }
      if (key === "ACKT") {
        const num = clampAckTimeoutMs(parseInt(obj[key], 10));
        if (UI.els.ackTimeout) UI.els.ackTimeout.value = String(num);
        UI.state.ackTimeout = num;
        updateAckTimeoutUi();
        updateAckRetryUi();
        storage.set("set.ACKT", String(num));
        continue;
      }
      if (key === "TESTRXMMSG") {
        const text = clampTestRxmMessage(String(obj[key] || ""));
        if (UI.els.testRxmMessage) UI.els.testRxmMessage.value = text;
        storage.set("set.TESTRXMMSG", text);
        updateTestRxmMessageHint();
        continue;
      }
      if (el.type === "checkbox") {
        el.checked = obj[key] === "1";
        if (typeof el.indeterminate === "boolean") el.indeterminate = false;
      } else el.value = obj[key];
      storage.set("set." + key, String(obj[key]));
    }
    note("Импортировано.");
  };
  input.click();
}
async function clearCaches() {
  storage.clear();
  if (typeof indexedDB !== "undefined" && indexedDB.databases) {
    const dbs = await indexedDB.databases();
    await Promise.all(dbs.map((db) => new Promise((resolve) => {
      const req = indexedDB.deleteDatabase(db.name);
      req.onsuccess = req.onerror = req.onblocked = () => resolve();
    })));
  }
  note("Кеш очищен.");
}

function extractNumericToken(text) {
  if (!text) return null;
  const match = String(text).match(/-?\d+(?:\.\d+)?/);
  return match ? match[0] : null;
}
async function syncSettingsFromDevice() {
  const bankEl = $("#BANK");
  try {
    const bankRes = await deviceFetch("BANK", {}, 2000);
    if (bankRes.ok && bankRes.text) {
      const letter = bankRes.text.trim().charAt(0);
      if (letter) {
        const value = letter.toLowerCase();
        if (bankEl) bankEl.value = value;
        UI.state.bank = value;
        storage.set("set.BANK", value);
      }
    }
  } catch (err) {
    console.warn("[settings] BANK", err);
  }

  await refreshChannels().catch((err) => console.warn("[settings] refreshChannels", err));
  updateChannelSelect();
  updateChannelSelectHint();

  try {
    const chRes = await deviceFetch("CH", {}, 2000);
    if (chRes.ok && chRes.text) {
      const num = parseInt(chRes.text, 10);
      if (!isNaN(num)) {
        UI.state.channel = num;
        if (UI.els.channelSelect) UI.els.channelSelect.value = String(num);
        storage.set("set.CH", String(num));
        updateChannelSelect();
        updateChannelSelectHint();
      }
    }
  } catch (err) {
    console.warn("[settings] CH", err);
  }

  const applyNumeric = (id, raw) => {
    const el = $("#" + id);
    if (!el) return;
    const token = extractNumericToken(raw);
    if (!token) return;
    if (id === "PW") {
      const resolved = normalizePowerPreset(token);
      if (resolved) {
        el.value = String(resolved.value);
        storage.set("set.PW", String(resolved.value));
      }
    } else {
      el.value = token;
      storage.set("set." + id, token);
    }
  };

  try {
    const bfRes = await deviceFetch("BF", {}, 2000);
    if (bfRes.ok) applyNumeric("BF", bfRes.text);
  } catch (err) {
    console.warn("[settings] BF", err);
  }
  try {
    const sfRes = await deviceFetch("SF", {}, 2000);
    if (sfRes.ok) applyNumeric("SF", sfRes.text);
  } catch (err) {
    console.warn("[settings] SF", err);
  }
  try {
    const crRes = await deviceFetch("CR", {}, 2000);
    if (crRes.ok) applyNumeric("CR", crRes.text);
  } catch (err) {
    console.warn("[settings] CR", err);
  }
  try {
    const pwRes = await deviceFetch("PW", {}, 2000);
    if (pwRes.ok) applyNumeric("PW", pwRes.text);
  } catch (err) {
    console.warn("[settings] PW", err);
  }
  try {
    const pauseRes = await deviceFetch("PAUSE", {}, 2000);
    if (pauseRes.ok) {
      const parsed = parsePauseResponse(pauseRes.text);
      if (parsed !== null) {
        UI.state.pauseMs = parsed;
        if (UI.els.pauseInput) UI.els.pauseInput.value = String(parsed);
        storage.set("set.PAUSE", String(parsed));
        updatePauseUi();
      }
    }
  } catch (err) {
    console.warn("[settings] PAUSE", err);
  }
  try {
    const ackTRes = await deviceFetch("ACKT", {}, 2000);
    if (ackTRes.ok) {
      const parsed = parseAckTimeoutResponse(ackTRes.text);
      if (parsed !== null) {
        UI.state.ackTimeout = parsed;
        if (UI.els.ackTimeout) UI.els.ackTimeout.value = String(parsed);
        storage.set("set.ACKT", String(parsed));
        updateAckTimeoutUi();
        updateAckRetryUi();
      }
    }
  } catch (err) {
    console.warn("[settings] ACKT", err);
  }

  updateChannelSelect();
  updateChannelSelectHint();
}

/* Безопасность */
function renderKeyState(state) {
  const data = state || UI.key.state;
  const stateEl = $("#keyState");
  const idEl = $("#keyId");
  const pubEl = $("#keyPublic");
  const peerEl = $("#keyPeer");
  const backupEl = $("#keyBackup");
  const messageEl = $("#keyMessage");
  if (!data || typeof data !== "object") {
    if (stateEl) stateEl.textContent = "—";
    if (idEl) idEl.textContent = "";
    if (pubEl) pubEl.textContent = "";
    if (peerEl) peerEl.textContent = "";
    if (backupEl) backupEl.textContent = "";
  } else {
    const type = data.type === "external" ? "EXTERNAL" : "LOCAL";
    if (stateEl) stateEl.textContent = type;
    if (idEl) idEl.textContent = data.id || "";
    if (pubEl) pubEl.textContent = data.public ? ("PUB " + data.public) : "";
    if (peerEl) {
      if (data.peer) {
        peerEl.textContent = "PEER " + data.peer;
        peerEl.hidden = false;
      } else {
        peerEl.textContent = "";
        peerEl.hidden = true;
      }
    }
    if (backupEl) backupEl.textContent = data.hasBackup ? "Есть резерв" : "";
  }
  if (messageEl) messageEl.textContent = UI.key.lastMessage || "";
}

async function refreshKeyState(options) {
  const opts = options || {};
  if (!opts.silent) status("→ KEYSTATE");
  debugLog("KEYSTATE → запрос состояния");
  const res = await deviceFetch("KEYSTATE", {}, 4000);
  if (res.ok) {
    debugLog("KEYSTATE ← " + res.text);
    try {
      const data = JSON.parse(res.text);
      if (data && data.error) {
        if (!opts.silent) note("KEYSTATE: " + data.error);
        return;
      }
      UI.key.state = data;
      UI.key.lastMessage = "";
      renderKeyState(data);
      if (!opts.silent) status("✓ KEYSTATE");
    } catch (err) {
      console.warn("[key] parse error", err);
      note("Не удалось разобрать состояние ключа");
    }
  } else if (!opts.silent) {
    status("✗ KEYSTATE");
    note("Ошибка KEYSTATE: " + res.error);
  }
  if (!res.ok) debugLog("KEYSTATE ✗ " + res.error);
}

async function requestKeyGen() {
  status("→ KEYGEN");
  debugLog("KEYGEN → запрос генерации");
  const res = await deviceFetch("KEYGEN", {}, 6000);
  if (!res.ok) {
    debugLog("KEYGEN ✗ " + res.error);
    status("✗ KEYGEN");
    note("Ошибка KEYGEN: " + res.error);
    return;
  }
  try {
    debugLog("KEYGEN ← " + res.text);
    const data = JSON.parse(res.text);
    if (data && data.error) {
      note("KEYGEN: " + data.error);
      status("✗ KEYGEN");
      return;
    }
    UI.key.state = data;
    UI.key.lastMessage = "Сгенерирован новый локальный ключ";
    renderKeyState(data);
    debugLog("KEYGEN ✓ ключ обновлён на устройстве");
    status("✓ KEYGEN");
  } catch (err) {
    status("✗ KEYGEN");
    note("Некорректный ответ KEYGEN");
  }
}

async function requestKeyRestore() {
  status("→ KEYRESTORE");
  debugLog("KEYRESTORE → запрос восстановления");
  const res = await deviceFetch("KEYRESTORE", {}, 6000);
  if (!res.ok) {
    debugLog("KEYRESTORE ✗ " + res.error);
    status("✗ KEYRESTORE");
    note("Ошибка KEYRESTORE: " + res.error);
    return;
  }
  try {
    debugLog("KEYRESTORE ← " + res.text);
    const data = JSON.parse(res.text);
    if (data && data.error) {
      note("KEYRESTORE: " + data.error);
      status("✗ KEYRESTORE");
      return;
    }
    UI.key.state = data;
    UI.key.lastMessage = "Восстановлена резервная версия ключа";
    renderKeyState(data);
    debugLog("KEYRESTORE ✓ восстановление завершено");
    status("✓ KEYRESTORE");
  } catch (err) {
    status("✗ KEYRESTORE");
    note("Некорректный ответ KEYRESTORE");
  }
}

async function requestKeySend() {
  status("→ KEYTRANSFER SEND");
  debugLog("KEYTRANSFER SEND → запрос отправки ключа");
  const res = await deviceFetch("KEYTRANSFER SEND", {}, 5000);
  if (!res.ok) {
    debugLog("KEYTRANSFER SEND ✗ " + res.error);
    status("✗ KEYTRANSFER SEND");
    note("Ошибка KEYTRANSFER SEND: " + res.error);
    return;
  }
  try {
    debugLog("KEYTRANSFER SEND ← " + res.text);
    const data = JSON.parse(res.text);
    if (data && data.error) {
      note("KEYTRANSFER SEND: " + data.error);
      status("✗ KEYTRANSFER SEND");
      return;
    }
    UI.key.state = data;
    UI.key.lastMessage = "Публичный ключ готов к передаче";
    renderKeyState(data);
    if (data && data.public && navigator.clipboard && navigator.clipboard.writeText) {
      try {
        await navigator.clipboard.writeText(data.public);
        UI.key.lastMessage = "Публичный ключ скопирован";
        renderKeyState(data);
        debugLog("KEYTRANSFER SEND ✓ ключ скопирован в буфер обмена");
      } catch (err) {
        console.warn("[key] clipboard", err);
        debugLog("KEYTRANSFER SEND ⚠ не удалось скопировать ключ: " + String(err));
      }
    }
    debugLog("KEYTRANSFER SEND ✓ данные отправлены");
    status("✓ KEYTRANSFER SEND");
  } catch (err) {
    status("✗ KEYTRANSFER SEND");
    note("Некорректный ответ KEYTRANSFER SEND");
  }
}

async function requestKeyReceive() {
  status("→ KEYTRANSFER RECEIVE");
  UI.key.lastMessage = "Ожидание ключа по LoRa";
  renderKeyState(UI.key.state);
  debugLog("KEYTRANSFER RECEIVE → ожидание ключа");
  const res = await deviceFetch("KEYTRANSFER RECEIVE", {}, 8000);
  if (!res.ok) {
    debugLog("KEYTRANSFER RECEIVE ✗ " + res.error);
    status("✗ KEYTRANSFER RECEIVE");
    note("Ошибка KEYTRANSFER RECEIVE: " + res.error);
    return;
  }
  try {
    debugLog("KEYTRANSFER RECEIVE ← " + res.text);
    const data = JSON.parse(res.text);
    if (data && data.error) {
      if (data.error === "timeout") note("KEYTRANSFER: тайм-аут ожидания ключа");
      else if (data.error === "apply") note("KEYTRANSFER: ошибка применения ключа");
      else note("KEYTRANSFER RECEIVE: " + data.error);
      status("✗ KEYTRANSFER RECEIVE");
      return;
    }
    UI.key.state = data;
    UI.key.lastMessage = "Получен внешний ключ";
    renderKeyState(data);
    debugLog("KEYTRANSFER RECEIVE ✓ ключ принят");
    status("✓ KEYTRANSFER RECEIVE");
  } catch (err) {
    status("✗ KEYTRANSFER RECEIVE");
    note("Некорректный ответ KEYTRANSFER RECEIVE");
  }
}

/* Утилиты */
// Возвращает управление циклу отрисовки, чтобы элементы UI успевали обновляться
function uiYield() {
  return new Promise((resolve) => {
    if (typeof requestAnimationFrame === "function") {
      requestAnimationFrame(() => resolve());
    } else {
      setTimeout(() => resolve(), 0);
    }
  });
}
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

async function loadVersion() {
  let base;
  try {
    base = new URL(UI.cfg.endpoint || "http://192.168.4.1");
  } catch (err) {
    base = new URL("http://192.168.4.1");
  }
  const url = new URL("/ver", base);
  try {
    const res = await fetch(url.toString(), { cache: "no-store" });
    if (!res.ok) throw new Error("HTTP " + res.status);
    const raw = await res.text();
    const text = normalizeVersionText(raw);
    UI.state.version = text || null;
    updateFooterVersion();
    return UI.state.version;
  } catch (err) {
    UI.state.version = null;
    updateFooterVersion();
    throw err;
  }
}
function updateFooterVersion() {
  const el = UI.els.version || $("#appVersion");
  if (!el) return;
  const text = UI.state.version != null ? String(UI.state.version) : "";
  el.textContent = text ? ("v" + text) : "—";
}
// Приводим текст версии к человеку понятному виду
function normalizeVersionText(value) {
  if (!value) return "";
  let text = String(value).trim();
  text = text.replace(/^v+/i, "").trim();
  if (!text) return "";
  if (/^(unknown|undefined|none)$/i.test(text)) return "";
  return text;
}
async function resyncAfterEndpointChange() {
  try {
    await syncSettingsFromDevice();
    await refreshAckState();
    await refreshAckRetry();
    await refreshEncryptionState();
    await refreshKeyState({ silent: true });
    await loadVersion().catch(() => {});
    refreshReceivedList({ silentError: true });
    probe().catch(() => {});
  } catch (err) {
    console.warn("[endpoint] resync error", err);
  }
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
