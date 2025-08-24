/* satprjct web/app.js — vanilla JS only */
/* State */
const UI = {
  tabs: ["chat", "channels", "settings", "security"],
  els: {},
  cfg: {
    endpoint: localStorage.getItem("endpoint") || "http://192.168.4.1",
    theme: localStorage.getItem("theme") || (matchMedia("(prefers-color-scheme: light)").matches ? "light" : "dark"),
  },
  key: {
    bytes: null, // Uint8Array(16) or null
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

  // Tabs
  for (const tab of UI.tabs) {
    const link = UI.els.nav.querySelector(`[data-tab="${tab}"]`);
    link?.addEventListener("click", (e) => {
      e.preventDefault();
      setTab(tab);
      history.replaceState(null, "", `#${tab}`);
    });
  }
  const initialTab = (location.hash?.replace("#","") && UI.tabs.includes(location.hash.slice(1))) ? location.hash.slice(1) : (localStorage.getItem("activeTab") || "chat");
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
  $("#btnPing")?.addEventListener("click", () => sendCommand("PI"));
  $("#btnSearch")?.addEventListener("click", () => sendCommand("SEAR"));
  $("#btnRefresh")?.addEventListener("click", refreshChannels);
  $("#btnExportCsv")?.addEventListener("click", exportChannelsCsv);

  // Settings
  $("#btnSaveSettings")?.addEventListener("click", saveSettingsLocal);
  $("#btnApplySettings")?.addEventListener("click", applySettingsToDevice);
  $("#btnExportSettings")?.addEventListener("click", exportSettings);
  $("#btnImportSettings")?.addEventListener("click", importSettings);
  $("#btnClearCache")?.addEventListener("click", clearCaches);
  // Initialize forms from localStorage
  loadSettings();

  // Security
  $("#btnKeyGen")?.addEventListener("click", generateKey);
  $("#btnKeySend")?.addEventListener("click", () => note("KEYTRANSFER SEND: заглушка"));
  $("#btnKeyRecv")?.addEventListener("click", () => note("KEYTRANSFER RECEIVE: заглушка"));
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
    link?.setAttribute("aria-current", is ? "page" : "false");
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
    if (cmd === "SEAR" || cmd === "PI") {
      // naive refresh
      refreshChannels();
    }
    return res.text;
  } else {
    status(`✗ ${cmd}`);
    note(`Ошибка: ${res.error}`);
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
  // simple placeholder data
  channels = [
    { idx: 0, ch: 1, f: 868.1, bw:125, sf:7, cr:"4/5", pw:14, rssi:-92, snr:8.5, st:"idle" },
    { idx: 1, ch: 2, f: 868.3, bw:125, sf:9, cr:"4/6", pw:14, rssi:-97, snr:7.1, st:"listen" },
    { idx: 2, ch: 3, f: 868.5, bw:250, sf:7, cr:"4/5", pw:20, rssi:-88, snr:10.2, st:"tx" },
  ];
}

function renderChannels() {
  const tbody = $("#channelsTable tbody");
  tbody.innerHTML = "";
  channels.forEach((c, i) => {
    const tr = document.createElement("tr");
    tr.innerHTML = `<td>${i+1}</td><td>${c.ch}</td><td>${c.f.toFixed(3)}</td><td>${c.bw}</td><td>${c.sf}</td><td>${c.cr}</td><td>${c.pw}</td><td>${c.rssi}</td><td>${c.snr}</td><td>${c.st}</td>`;
    tbody.appendChild(tr);
  });
}

async function refreshChannels() {
  // If device supports a channels dump endpoint, attempt to fetch it.
  // Fallback to mock display.
  try {
    const r = await deviceFetch("CHLIST");
    if (r && r.ok && r.text) {
      // Expect CSV-like lines, parse heuristically
      channels = parseChannels(r.text);
    } else if (!channels.length) {
      mockChannels();
    }
  } catch (e) {
    if (!channels.length) mockChannels();
  }
  renderChannels();
}

function parseChannels(text) {
  // Very tolerant parser: lines like "ch,freq,bw,sf,cr,pw,rssi,snr,status"
  const out = [];
  text.split(/\r?\n/).forEach(line => {
    const t = line.trim();
    if (!t || /ch\s*,/i.test(t)) return;
    const parts = t.split(/\s*[,;|\t]\s*/);
    if (parts.length < 5) return;
    const [ch, f, bw, sf, cr, pw, rssi, snr, st] = parts;
    out.push({
      ch: Number(ch), f: Number(f), bw: Number(bw), sf: Number(sf),
      cr: cr || "4/5", pw: Number(pw ?? 14), rssi: Number(rssi ?? 0), snr: Number(snr ?? 0), st: st || ""
    });
  });
  return out;
}

function exportChannelsCsv() {
  const lines = [["idx","ch","freq","bw","sf","cr","pw","rssi","snr","status"]];
  channels.forEach((c,i) => lines.push([i+1,c.ch,c.f,c.bw,c.sf,c.cr,c.pw,c.rssi,c.snr,c.st]));
  const csv = lines.map(a => a.join(",")).join("\n");
  const blob = new Blob([csv], { type: "text/csv" });
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url; a.download = "channels.csv";
  a.click();
  URL.revokeObjectURL(url);
}

/* Settings */
const SETTINGS_KEYS = ["ACK","BANK","BF","CH","CR","INFO","PW","SF","STS"];
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
  // Push each field as individual command, e.g. "PW 14" etc.
  for (const k of SETTINGS_KEYS) {
    const el = $("#"+k);
    if (!el) continue;
    const v = (el.type === "checkbox") ? (el.checked ? "1" : "0") : String(el.value || "").trim();
    if (!v) continue;
    await sendCommand(k, { v });
  }
  note("Применение завершено.");
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
  crypto.getRandomValues(k);
  UI.key.bytes = k;
  await saveKeyToStorage(k);
  await updateKeyUI();
  note("Сгенерирован новый ключ (16 байт).");
}
async function sha256Hex(bytes) {
  const digest = await crypto.subtle.digest("SHA-256", bytes);
  return [...new Uint8Array(digest)].map(x => x.toString(16).padStart(2, "0")).join("");
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
