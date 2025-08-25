/* satprjct web/app.js — vanilla JS only */
/* State */
const UI = {
  // список вкладок интерфейса
  tabs: ["chat", "channels", "settings", "security", "debug"],
  els: {},
  cfg: {
    endpoint: localStorage.getItem("endpoint") || "http://192.168.4.1",
    theme: localStorage.getItem("theme") || (matchMedia("(prefers-color-scheme: light)").matches ? "light" : "dark"),
  },
  key: {
    bytes: null, // Uint8Array(16) or null
  },
  state: {
    channel: null // текущий активный канал
  }
};

document.addEventListener("DOMContentLoaded", init);

function $(sel, root=document){ return root.querySelector(sel); }
function $all(sel, root=document){ return Array.from(root.querySelectorAll(sel)); }

async function init() {
  // Cache elements
  UI.els.menuToggle = $("#menuToggle");
  UI.els.nav = $("#siteNav");
  UI.els.endpoint = $("#endpoint");
  UI.els.themeToggle = $("#themeToggle");
  UI.els.status = $("#statusLine");
  UI.els.toast = $("#toast");
  UI.els.debugLog = $("#debugLog");

  // Tabs
  for (const tab of UI.tabs) {
    const link = UI.els.nav.querySelector(`[data-tab="${tab}"]`);
    // Проверяем существование ссылки, чтобы избежать ошибок в старых браузерах
    if (link) link.addEventListener("click", (e) => {
      e.preventDefault();
      setTab(tab);
      history.replaceState(null, "", `#${tab}`);
    });
  }
  // Определяем стартовую вкладку без использования optional chaining
  const hash = location.hash ? location.hash.slice(1) : "";
  const initialTab = UI.tabs.includes(hash) ? hash : (localStorage.getItem("activeTab") || "chat");
  setTab(initialTab);

  // Menu toggle
  UI.els.menuToggle.addEventListener("click", () => {
    const open = !UI.els.nav.classList.contains("open");
    UI.els.menuToggle.setAttribute("aria-expanded", String(open));
    UI.els.nav.classList.toggle("open", open);
  });
  UI.els.nav.addEventListener("click", () => UI.els.nav.classList.remove("open"));

  // Theme
  applyTheme(UI.cfg.theme);
  UI.els.themeToggle.addEventListener("click", () => toggleTheme());
  const themeSwitch = $("#themeSwitch");
  if (themeSwitch) {
    themeSwitch.checked = UI.cfg.theme === "dark" ? true : false;
    themeSwitch.addEventListener("change", () => toggleTheme());
  }

  // Endpoint
  UI.els.endpoint.value = UI.cfg.endpoint;
  UI.els.endpoint.addEventListener("change", () => {
    UI.cfg.endpoint = UI.els.endpoint.value.trim();
    localStorage.setItem("endpoint", UI.cfg.endpoint);
    note(`Endpoint: ${UI.cfg.endpoint}`);
  });

  // Chat
  UI.els.chatLog = $("#chatLog");
  UI.els.chatInput = $("#chatInput");
  UI.els.sendBtn = $("#sendBtn");
  UI.els.sendBtn.addEventListener("click", onSendChat);
  UI.els.chatInput.addEventListener("keydown", (e) => { if (e.key === "Enter") onSendChat(); });
  $all('[data-cmd]').forEach(btn => btn.addEventListener("click", () => sendCommand(btn.dataset.cmd)));

  loadChatHistory();

  // Channels page
  const btnPing = $("#btnPing"); if (btnPing) btnPing.addEventListener("click", () => scanChannels("PI"));
  const btnSearch = $("#btnSearch"); if (btnSearch) btnSearch.addEventListener("click", () => scanChannels("SEAR"));
  const btnRefresh = $("#btnRefresh"); if (btnRefresh) btnRefresh.addEventListener("click", refreshChannels);
  const btnExportCsv = $("#btnExportCsv"); if (btnExportCsv) btnExportCsv.addEventListener("click", exportChannelsCsv);

  // Settings
  const btnSaveSettings = $("#btnSaveSettings"); if (btnSaveSettings) btnSaveSettings.addEventListener("click", saveSettingsLocal);
  const btnApplySettings = $("#btnApplySettings"); if (btnApplySettings) btnApplySettings.addEventListener("click", applySettingsToDevice);
  const btnExportSettings = $("#btnExportSettings"); if (btnExportSettings) btnExportSettings.addEventListener("click", exportSettings);
  const btnImportSettings = $("#btnImportSettings"); if (btnImportSettings) btnImportSettings.addEventListener("click", importSettings);
  const btnClearCache = $("#btnClearCache"); if (btnClearCache) btnClearCache.addEventListener("click", clearCaches);
  // инициализация форм из localStorage
  loadSettings();
  // при смене банка каналов сразу обновляем таблицу
  const bankSel = $("#BANK"); if (bankSel) bankSel.addEventListener("change", refreshChannels);
  // начальная загрузка списка каналов
  refreshChannels().catch(()=>{});

  // Security
  const btnKeyGen = $("#btnKeyGen"); if (btnKeyGen) btnKeyGen.addEventListener("click", generateKey);
  const btnKeySend = $("#btnKeySend"); if (btnKeySend) btnKeySend.addEventListener("click", () => note("KEYTRANSFER SEND: заглушка"));
  const btnKeyRecv = $("#btnKeyRecv"); if (btnKeyRecv) btnKeyRecv.addEventListener("click", () => note("KEYTRANSFER RECEIVE: заглушка"));
  await loadKeyFromStorage();
  await updateKeyUI();

  // Try to probe device connectivity (non-blocking)
  probe().catch(()=>{});
}

/* Tabs */
function setTab(tab) {
  for (const t of UI.tabs) {
    const panel = $("#tab-"+t);
    const link = UI.els.nav.querySelector(`[data-tab="${t}"]`);
    const is = (t === tab);
    panel.hidden = !is;
    panel.classList.toggle("current", is);
    if (link) link.setAttribute("aria-current", is ? "page" : "false"); // избегаем optional chaining
  }
  localStorage.setItem("activeTab", tab);
}

/* Theme */
function applyTheme(mode) {
  UI.cfg.theme = (mode === "light") ? "light" : "dark";
  document.documentElement.classList.toggle("light", UI.cfg.theme === "light");
  localStorage.setItem("theme", UI.cfg.theme);
}
function toggleTheme(){ applyTheme(UI.cfg.theme === "dark" ? "light" : "dark"); }

/* Chat logic */
function loadChatHistory() {
  const entries = JSON.parse(localStorage.getItem("chatHistory") || "[]");
  UI.els.chatLog.innerHTML = "";
  entries.forEach(addChatMessage);
}
function persistChat(message, author) {
  const entries = JSON.parse(localStorage.getItem("chatHistory") || "[]");
  entries.push({ t: Date.now(), a: author, m: message });
  localStorage.setItem("chatHistory", JSON.stringify(entries.slice(-500))); // cap history
}
function addChatMessage({ t, a, m }) {
  const wrap = document.createElement("div");
  wrap.className = "msg " + (a === "you" ? "you" : "dev");
  const time = document.createElement("time");
  time.dateTime = new Date(t).toISOString();
  time.textContent = new Date(t).toLocaleTimeString();
  const bubble = document.createElement("div");
  bubble.className = "bubble";
  bubble.textContent = m;
  wrap.appendChild(time);
  wrap.appendChild(bubble);
  UI.els.chatLog.appendChild(wrap);
  UI.els.chatLog.scrollTop = UI.els.chatLog.scrollHeight;
}

async function onSendChat() {
  const text = UI.els.chatInput.value.trim();
  if (!text) return;
  UI.els.chatInput.value = "";
  const isCmd = text.startsWith("/") ? text.slice(1) : null;
  persistChat(text, "you");
  addChatMessage({ t: Date.now(), a: "you", m: text });
  if (isCmd) {
    await sendCommand(isCmd);
  } else {
    // TX: UTF-8 to CP1251 on device; here we send raw UTF-8 as placeholder
    await sendCommand("TX", { data: text });
  }
}

/* Device command transport */
/** Try multiple endpoints for compatibility:
 *  1) GET {endpoint}/cmd?c=PI
 *  2) GET {endpoint}/api/cmd?cmd=PI
 *  3) GET {endpoint}/?c=PI
 *  Optional params appended as query string.
 */
async function deviceFetch(cmd, params = {}, timeoutMs = 4000) {
  const u = new URL(UI.cfg.endpoint || "http://192.168.4.1");
  const candidates = [
    new URL(`/cmd`, u),
    new URL(`/api/cmd`, u),
    new URL(`/`, u),
  ];
  const qs = new URLSearchParams();
  if (cmd) { qs.set("c", cmd); qs.set("cmd", cmd); } // both for 1/2/3
  for (const [k,v] of Object.entries(params)) qs.set(k, v);
  // try each candidate
  let lastErr;
  for (const base of candidates) {
    let url = base.toString();
    // heuristic: /api/cmd expects ?cmd=; /cmd and / expect ?c=
    if (base.pathname.endsWith("/api/cmd")) {
      url += (url.includes("?") ? "&" : "?") + "cmd=" + encodeURIComponent(cmd);
    } else {
      url += (url.includes("?") ? "&" : "?") + "c=" + encodeURIComponent(cmd);
    }
    const extra = new URLSearchParams(params).toString();
    if (extra) url += "&" + extra;

    try {
      const ctrl = new AbortController();
      const id = setTimeout(() => ctrl.abort(), timeoutMs);
      const res = await fetch(url, { signal: ctrl.signal });
      clearTimeout(id);
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      const text = await res.text();
      // Если устройство не ответило и сервер вернул HTML-страницу,
      // помечаем такой ответ как ошибку, чтобы не выводить разметку в чат.
      if (/<!DOCTYPE|<html/i.test(text)) {
        throw new Error("HTML response");
      }
      return { ok: true, text, url };
    } catch (e) {
      lastErr = e;
    }
  }
  return { ok: false, error: String(lastErr || "Unknown error") };
}

async function sendCommand(cmd, params={}) {
  status(`→ ${cmd}`);
  const res = await deviceFetch(cmd, params);
  if (res.ok) {
    status(`✓ ${cmd}`);
    note(`${cmd}: ${res.text.slice(0, 200)}`);
    persistChat(`${cmd}: ${res.text}`, "dev");
    addChatMessage({ t: Date.now(), a: "dev", m: `${res.text}` });
    debugLog(`${cmd}: ${res.text}`);
    return res.text;
  } else {
    status(`✗ ${cmd}`);
    note(`Ошибка: ${res.error}`);
    debugLog(`ERR ${cmd}: ${res.error}`);
    return null;
  }
}

async function probe() {
  const r = await deviceFetch("PI", {}, 2500);
  if (r.ok) {
    status(`Подключено: ${UI.cfg.endpoint}`);
  } else {
    status(`Оффлайн · endpoint: ${UI.cfg.endpoint}`);
  }
}

/* Channels table (mock updater until device responds) */
let channels = [];
function mockChannels() {
  // простые данные-заглушки без столбцов BW/SF/CR/PW
  channels = [
    { idx: 0, ch: 1, tx: 868.1, rx: 868.1, rssi:-92, snr:8.5, st:"idle",   scan:"" },
    { idx: 1, ch: 2, tx: 868.3, rx: 868.3, rssi:-97, snr:7.1, st:"listen", scan:"" },
    { idx: 2, ch: 3, tx: 868.5, rx: 868.5, rssi:-88, snr:10.2, st:"tx",    scan:"" },
  ];
}

function renderChannels() {
  const tbody = $("#channelsTable tbody");
  if (!tbody) return;
  tbody.innerHTML = "";
  channels.forEach((c, i) => {
    const tr = document.createElement("tr");
    // класс строки в зависимости от статуса канала (учитываем регистр)
    const st = (c.st || "").toLowerCase();
    const stCls = { tx: "busy", listen: "busy", idle: "free" }[st] || "unknown";
    if (UI.state.channel === c.ch) tr.classList.add("active");
    tr.classList.add(stCls);
    // подсветка результатов сканирования по полю scan
    const scCls = c.scan ? (/crc/i.test(c.scan) ? "crc-error" : /timeout|noresp/i.test(c.scan) ? "no-response" : "signal") : "";
    if (scCls) tr.classList.add(scCls);
    // выводим строки без параметров BW/SF/CR/PW
    tr.innerHTML = `<td>${i+1}</td><td>${c.ch}</td><td>${c.tx.toFixed(3)}</td><td>${c.rx.toFixed(3)}</td><td>${c.rssi}</td><td>${c.snr}</td><td>${c.st}</td><td>${c.scan || ""}</td>`;
    tbody.appendChild(tr);
  });
}

// обновление выпадающего списка каналов в настройках
function updateChannelSelect() {
  const sel = $("#CH");
  if (!sel) return;
  const prev = UI.state.channel != null ? String(UI.state.channel) : sel.value;
  sel.innerHTML = "";
  channels.forEach(c => {
    const o = document.createElement("option");
    o.value = c.ch;
    o.textContent = c.ch;
    sel.appendChild(o);
  });
  if (channels.some(c => String(c.ch) === prev)) sel.value = prev;
}

async function refreshChannels() {
  // перед запросом списка каналов выставляем выбранный банк
  try {
    const bankSel = $("#BANK");
    const bank = bankSel ? bankSel.value : null; // поддержка старых браузеров
    if (bank) await deviceFetch("BANK", { v: bank });
    // запрашиваем список каналов с учётом выбранного банка
    const r = await deviceFetch("CHLIST", bank ? { bank } : {});
    if (r && r.ok && r.text) {
      // ожидаем строки CSV, разбираем их
      channels = parseChannels(r.text);
    } else if (!channels.length) {
      mockChannels();
    }
    // узнаём текущий канал через команды CH/CHANNEL
    const chRes = await deviceFetch("CH");
    let chTxt = chRes.ok ? chRes.text : null;
    if (!chTxt) {
      const chRes2 = await deviceFetch("CHANNEL");
      chTxt = chRes2.ok ? chRes2.text : null;
    }
    if (chTxt) {
      const n = parseInt(chTxt, 10);
      if (!isNaN(n)) UI.state.channel = n;
    }
  } catch (e) {
    if (!channels.length) mockChannels();
  }
  renderChannels();
  updateChannelSelect();
}

// последовательный пинг/поиск всех каналов
async function scanChannels(type) {
  const tbody = $("#channelsTable tbody");
  if (!tbody) return;
  for (let i = 0; i < channels.length; i++) {
    const c = channels[i];
    const tr = tbody.children[i];
    if (!tr) continue;
    tr.classList.add("scanning");
    tr.classList.remove("signal", "crc", "noresp");
    const cell = tr.querySelector("td:last-child");
    if (cell) cell.textContent = "";
    const res = await deviceFetch(type, { ch: c.ch });
    let txt = "";
    let cls = "";
    if (res && res.ok) {
      txt = res.text.trim();
      if (/crc/i.test(txt)) cls = "crc-error";
      else if (/timeout|noresp/i.test(txt)) cls = "no-response";
      else cls = "signal";
    } else {
      txt = res ? res.error : "";
      cls = "no-response";
    }
    c.scan = txt; // сохраняем текст результата
    tr.classList.remove("scanning");
    if (cls) tr.classList.add(cls); // подсветка результата
    if (cell) cell.textContent = txt;
  }
  renderChannels();
}

function parseChannels(text) {
  // Парсер CSV "ch,tx,rx,rssi,snr,status,scan"
  // Поддерживает старый формат с полями BW/SF/CR/PW.
  const out = [];
  text.split(/\r?\n/).forEach(line => {
    const t = line.trim();
    if (!t || /ch\s*,/i.test(t)) return;
    const p = t.split(/\s*[,;|\t]\s*/);
    if (p.length < 3) return;
    let ch, tx, rx, rssi, snr, st, scan;
    if (p.length >= 10) {
      // старый формат: ch,tx,rx,bw,sf,cr,pw,rssi,snr,st,scan
      [ch, tx, rx, , , , , rssi, snr, st, scan] = p;
    } else if (p.length >= 7) {
      // новый формат с отдельным RX и результатом сканирования
      [ch, tx, rx, rssi, snr, st, scan] = p;
    } else if (p.length >= 5) {
      // самый старый формат без RX/Scan
      [ch, tx, rssi, snr, st] = p;
      rx = tx;
      scan = "";
    } else {
      return;
    }
    out.push({
      ch: Number(ch),
      tx: Number(tx),
      rx: Number(rx),
      rssi: Number(rssi ?? 0),
      snr: Number(snr ?? 0),
      st: st || "",
      scan: scan || "",
    });
  });
  return out;
}
function exportChannelsCsv() {
  // Экспорт таблицы каналов без столбцов BW/SF/CR/PW
  const lines = [["idx","ch","tx","rx","rssi","snr","status","scan"]];
  channels.forEach((c,i) =>
    lines.push([i+1,c.ch,c.tx,c.rx,c.rssi,c.snr,c.st,c.scan])
  );
  const csv = lines.map(a => a.join(",")).join("\n");
  const blob = new Blob([csv], { type: "text/csv" });
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url; a.download = "channels.csv";
  a.click();
  URL.revokeObjectURL(url);
}

/* Settings */
// ключи настроек, сохраняемые локально
// список настроек, сохраняемых локально
const SETTINGS_KEYS = ["ACK","BANK","BF","CH","CR","PW","SF"]; // INFO вызывается отдельной кнопкой
function loadSettings() {
  for (const k of SETTINGS_KEYS) {
    const el = $("#"+k);
    if (!el) continue;
    const v = localStorage.getItem("set."+k);
    if (v !== null) {
      if (el.type === "checkbox") el.checked = v === "1";
      else el.value = v;
    }
  }
}
function saveSettingsLocal() {
  for (const k of SETTINGS_KEYS) {
    const el = $("#"+k);
    if (!el) continue;
    const v = (el.type === "checkbox") ? (el.checked ? "1" : "0") : el.value;
    localStorage.setItem("set."+k, v);
  }
  note("Сохранено локально.");
}
async function applySettingsToDevice() {
  // отправляем каждое поле отдельной командой, например "PW 14"
  for (const k of SETTINGS_KEYS) {
    const el = $("#"+k);
    if (!el) continue;
    const v = (el.type === "checkbox") ? (el.checked ? "1" : "0") : String(el.value || "").trim();
    if (!v) continue;
    await sendCommand(k, { v });
  }
  note("Применение завершено.");
  // обновляем список и активный канал после применения настроек
  await refreshChannels().catch(()=>{});
}
function exportSettings() {
  const obj = {};
  for (const k of SETTINGS_KEYS) {
    const el = $("#"+k);
    if (!el) continue;
    obj[k] = (el.type === "checkbox") ? (el.checked ? "1" : "0") : el.value;
  }
  const json = JSON.stringify(obj, null, 2);
  const blob = new Blob([json], { type:"application/json" });
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a"); a.href = url; a.download = "settings.json"; a.click();
  URL.revokeObjectURL(url);
}
function importSettings() {
  const inp = document.createElement("input");
  inp.type = "file"; inp.accept = "application/json";
  inp.onchange = async () => {
    const file = inp.files[0]; if (!file) return;
    const text = await file.text();
    const obj = JSON.parse(text);
    for (const k of SETTINGS_KEYS) {
      if (obj[k] == null) continue;
      const el = $("#"+k);
      if (!el) continue;
      if (el.type === "checkbox") el.checked = obj[k] === "1";
      else el.value = obj[k];
      localStorage.setItem("set."+k, String(obj[k]));
    }
    note("Импортировано.");
  };
  inp.click();
}
async function clearCaches() {
  // localStorage and all IndexedDB databases
  localStorage.clear();
  if ('databases' in indexedDB) {
    const dbs = await indexedDB.databases();
    await Promise.all(dbs.map(db => new Promise((resolve) => {
      const req = indexedDB.deleteDatabase(db.name);
      req.onsuccess = req.onerror = req.onblocked = () => resolve();
    })));
  }
  note("Кеш очищен.");
}

/* Security: key generation & status */
async function loadKeyFromStorage() {
  const b64 = localStorage.getItem("sec.key");
  if (b64) {
    UI.key.bytes = Uint8Array.from(atob(b64), c => c.charCodeAt(0)).slice(0,16);
  } else {
    UI.key.bytes = null;
  }
}
async function saveKeyToStorage(bytes) {
  localStorage.setItem("sec.key", btoa(String.fromCharCode(...bytes)));
}
async function generateKey() {
  const k = new Uint8Array(16);
  if (typeof crypto !== "undefined" && crypto.getRandomValues) {
    // предпочтительно используем криптографически стойкий генератор
    crypto.getRandomValues(k);
  } else {
    // запасной вариант на случай отсутствия WebCrypto
    for (let i = 0; i < 16; i++) k[i] = Math.floor(Math.random() * 256);
  }
  UI.key.bytes = k;
  await saveKeyToStorage(k);
  await updateKeyUI();
  note("Сгенерирован новый ключ (16 байт).");
}
// Вычисление SHA-256 с учётом отсутствия WebCrypto
async function sha256Hex(bytes) {
  if (typeof crypto !== "undefined" && crypto.subtle && crypto.subtle.digest) {
    // нормальный путь через WebCrypto
    const digest = await crypto.subtle.digest("SHA-256", bytes);
    return [...new Uint8Array(digest)].map(x => x.toString(16).padStart(2, "0")).join("");
  } else if (typeof sha256Bytes === "function") {
    // резервный путь через библиотеку
    return sha256Bytes(bytes);
  } else {
    // грубый фолбэк: сумма байтов
    let sum = 0;
    for (const b of bytes) sum = (sum + b) & 0xffffffff;
    return sum.toString(16).padStart(8, "0");
  }
}
async function updateKeyUI() {
  const stateEl = $("#keyState");
  const hashEl = $("#keyHash");
  const hexEl = $("#keyHex");
  if (!UI.key.bytes) {
    stateEl.textContent = "LOCAL";
    hashEl.textContent = "";
    hexEl.textContent = "";
  } else {
    const hex = [...UI.key.bytes].map(x => x.toString(16).padStart(2, "0")).join("");
    const h = await sha256Hex(UI.key.bytes);
    stateEl.textContent = h.slice(0,4).toUpperCase();
    hashEl.textContent = "SHA-256: " + h;
    hexEl.textContent = hex;
  }
}

/* Utilities */
function status(text) { UI.els.status.textContent = text; }
function note(text) {
  const t = UI.els.toast;
  t.textContent = text;
  t.hidden = false;
  t.classList.remove("show"); void t.offsetWidth; // restart animation
  t.classList.add("show");
  setTimeout(() => { t.hidden = true; }, 2200);
}

// вывод строки в окно отладочных логов
function debugLog(text) {
  const d = UI.els.debugLog;
  if (!d) return;
  const line = document.createElement("div");
  line.textContent = `[${new Date().toLocaleTimeString()}] ${text}`;
  d.appendChild(line);
  d.scrollTop = d.scrollHeight;
}
