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

// Константы для вкладки наведения антенны
const EARTH_RADIUS_KM = 6378.137; // экваториальный радиус Земли
const GEO_ALTITUDE_KM = 35786.0;   // высота геостационарной орбиты над поверхностью
const MU_EARTH = 398600.4418;      // гравитационный параметр Земли (км^3/с^2)
const DEG2RAD = Math.PI / 180;
const RAD2DEG = 180 / Math.PI;
const TWO_PI = Math.PI * 2;
const POINTING_DEFAULT_MIN_ELEVATION = 10; // минимальный угол возвышения по умолчанию
const POINTING_COMPASS_OFFSET_DEG = 180; // отображаем юг в верхней части компаса

/* Состояние интерфейса */
const UI = {
  tabs: ["chat", "channels", "pointing", "settings", "security", "debug"],
  els: {},
  cfg: {
    endpoint: storage.get("endpoint") || "http://192.168.4.1",
    theme: storage.get("theme") || detectPreferredTheme(),
    accent: (storage.get("accent") === "red") ? "red" : "default",
    autoNight: storage.get("autoNight") !== "0",
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
    chatHydrating: false,
    chatScrollPinned: true,
    chatSoundCtx: null,
    chatSoundLast: 0,
    version: normalizeVersionText(storage.get("appVersion") || "") || null,
    pauseMs: null,
    ackTimeout: null,
    encTest: null,
    testRxmTimers: [],
    autoNightTimer: null,
    autoNightActive: false,
    pointing: {
      satellites: [],
      visible: [],
      observer: null,
      observerMgrs: null,
      minElevation: POINTING_DEFAULT_MIN_ELEVATION,
      selectedSatId: null,
      tleReady: false,
      tleError: null,
      mgrsReady: false,
      mgrsError: null,
    },
  }
};

// Максимальная длина текста пользовательского сообщения для TESTRXM
const TEST_RXM_MESSAGE_MAX = 2048;

// Справочные данные по каналам из CSV
const channelReference = {
  map: new Map(),
  byTx: new Map(),
  ready: false,
  loading: false,
  error: null,
  promise: null,
  source: null,
  lastError: null,
};
const CHANNEL_REFERENCE_FALLBACK = `Channel,RX (MHz),TX (MHz),System,Band Plan,Purpose,Frequency (MHz),Bandwidth,Satellite Name,Satellite Position,Comments,Modulation,Usage
0,243.625,316.725,UHF military,225-328.6 MHz,Military communications,243.625,36KHz,ComsatBW-2,13.2 East,,Wideband FM/AM,Широкополосные военные/правительственные каналы
1,243.625,300.4,UHF military,225-328.6 MHz,Military communications,243.625,36KHz,ComsatBW-2,13.2 East,,Wideband FM/AM,Широкополосные военные/правительственные каналы
2,243.8,298.2,UHF military,225-328.6 MHz,Military communications,,,,,,,
3,244.135,296.075,UHF FO,Band Plan P,Tactical communications,244.135,5KHz,UFO-7,23.3 West,,Narrowband FM/AM,Тактические голос/данные каналы
4,244.275,300.25,UHF military,225-328.6 MHz,Military communications,,,,,,,
5,245.2,312.85,UHF military,225-328.6 MHz,Military communications,,,,,,,
6,245.8,298.65,UHF military,225-328.6 MHz,Military communications,245.8,35KHz,Skynet 5C,17.8 West,,PSK/FM Mixed,Военные голос/данные (UK/NATO)
7,245.85,314.23,UHF military,225-328.6 MHz,Military communications,245.85,35KHz,Skynet 5A,5.9 East,,PSK/FM Mixed,Военные голос/данные (UK/NATO)
8,245.95,299.4,UHF military,225-328.6 MHz,Military communications,,,,,,,
9,247.45,298.8,UHF military,225-328.6 MHz,Military communications,,,,,,,
10,248.75,306.9,Marisat,B,Tactical voice/data,248.75,36KHz,ComsatBW-2,13.2 East,,Wideband FM/AM,Широкополосные военные/правительственные каналы
11,248.825,294.375,30 kHz Transponder,30K Transponder,30 kHz voice/data transponder,248.825,30KHz,? IOR,,? March 2010,,
12,249.375,316.975,UHF military,225-328.6 MHz,Military communications,249.375,30KHz,?,,IOR March 2010,,
13,249.4,300.975,UHF military,225-328.6 MHz,Military communications,,,,,,,
14,249.45,299.0,UHF military,225-328.6 MHz,Military communications,,,,,,,
15,249.45,312.75,UHF military,225-328.6 MHz,Military communications,,,,,,,
16,249.49,313.95,UHF military,225-328.6 MHz,Military communications,,,,,,,
17,249.53,318.28,UHF military,225-328.6 MHz,Military communications,249.5295,8KHz,Skynet 5A,5.9 East,,PSK/FM Mixed,Военные голос/данные (UK/NATO)
18,249.85,316.25,UHF military,225-328.6 MHz,Military communications,,,,,,,
19,249.85,298.83,UHF military,225-328.6 MHz,Military communications,,,,,,,
20,249.89,300.5,UHF military,225-328.6 MHz,Military communications,,,,,,,
21,249.93,308.75,UHF FO,Q,Tactical communications,,,,,,,
22,250.09,312.6,UHF military,225-328.6 MHz,Military communications,,,,,,,
23,250.9,308.3,UHF FO,Q,Tactical communications,250.9,36KHz,ComsatBW-2,13.2 East,,Wideband FM/AM,Широкополосные военные/правительственные каналы
24,251.275,296.5,30 kHz Transponder,30K Transponder,30 kHz voice/data transponder,,,,,,,
25,251.575,308.45,UHF military,225-328.6 MHz,Military communications,,,,,,,
26,251.6,298.225,UHF military,225-328.6 MHz,Military communications,,,,,,,
27,251.85,292.85,Navy 25 kHz,Navy 25K,Tactical voice/data communications,251.85,25KHz,UFO-10/11,70.0 East,,Narrowband FM/AM,Тактические голос/данные каналы
28,251.9,292.9,FLTSATCOM/Leasat,A/X,Tactical communications,251.9,25KHz,UFO-10/11,70.0 East,,Narrowband FM/AM,Тактические голос/данные каналы
29,251.95,292.95,Navy 25 kHz,Navy 25K,Tactical voice/data communications,251.95,25KHz,UFO-2,28.3 East,,Narrowband FM/AM,Тактические голос/данные каналы
30,252.0,293.1,FLTSATCOM,B,Tactical communications,252.0,30KHz,UFO-10/11,70.0 East,March 2010,Narrowband FM/AM,Тактические голос/данные каналы
31,252.05,293.05,Navy 25 kHz,Navy 25K,Tactical voice/data communications,252.05,25KHz,UFO-7,23.3 West,DAMA kg_net= 7 9k6,Narrowband FM/AM,DAMA (динамическое распределение каналов)
32,252.15,293.15,UHF military,225-328.6 MHz,Military communications,252.15,25KHz,Fltsatcom 8,15.5 West,DAMA kg_net= 5 9k6,Narrowband FM/AM,DAMA (динамическое распределение каналов)
33,252.2,299.15,UHF military,225-328.6 MHz,Military communications,,,,,,,
34,252.4,309.7,UHF FO,Q,Tactical communications,252.4,30KHz,Sicral 1B,11.8 East,,PSK (Phase Shift Keying),Итальянская военная связь (голос/данные)
35,252.45,309.75,UHF military,225-328.6 MHz,Military communications,252.45,30KHz,Sicral 1B,11.8 East,,PSK (Phase Shift Keying),Итальянская военная связь (голос/данные)
36,252.5,309.8,UHF military,225-328.6 MHz,Military communications,252.5,30KHz,Sicral 1B,11.8 East,,PSK (Phase Shift Keying),Итальянская военная связь (голос/данные)
37,252.55,309.85,UHF military,225-328.6 MHz,Military communications,252.55,30KHz,Sicral 1B,11.8 East,,PSK (Phase Shift Keying),Итальянская военная связь (голос/данные)
38,252.625,309.925,UHF military,225-328.6 MHz,Military communications,,,,,,,
39,253.55,294.55,UHF military,225-328.6 MHz,Military communications,253.55,25KHz,UFO-10/11,70.0 East,,Narrowband FM/AM,Тактические голос/данные каналы
40,253.6,295.95,UHF military,225-328.6 MHz,Military communications,253.6,25KHz,UFO-10/11,70.0 East,,Narrowband FM/AM,Тактические голос/данные каналы
41,253.65,294.65,Navy 25 kHz,Navy 25K,Tactical voice/data communications,253.65,25KHz,UFO-2,28.3 East,,Narrowband FM/AM,Тактические голос/данные каналы
42,253.7,294.7,FLTSATCOM/Leasat/UHF FO,A/X/O,Tactical communications,253.7,25KHz,UFO-10/11,70.0 East,,Narrowband FM/AM,Тактические голос/данные каналы
43,253.75,294.75,UHF military,225-328.6 MHz,Military communications,253.75,25KHz,UFO-7,23.3 West,,Narrowband FM/AM,Тактические голос/данные каналы
44,253.8,296.0,UHF military,225-328.6 MHz,Military communications,,,,,,,
45,253.85,294.85,Navy 25 kHz,Navy 25K,Tactical voice/data communications,253.85,25KHz,UFO-7,23.3 West,DAMA kg_net= 9 9k6+32k,Narrowband FM/AM,DAMA (динамическое распределение каналов)
46,253.85,294.85,Navy 25 kHz,Navy 25K,Tactical voice/data communications,253.85,25KHz,UFO-7,23.3 West,DAMA kg_net= 9 9k6+32k,Narrowband FM/AM,DAMA (динамическое распределение каналов)
47,253.9,307.5,UHF military,225-328.6 MHz,Military communications,,,,,,,
48,254.0,298.63,UHF military,225-328.6 MHz,Military communications,,,,,,,
49,254.73,312.55,UHF military,225-328.6 MHz,Military communications,,,,,,,
50,254.775,310.8,UHF military,225-328.6 MHz,Military communications,,,,,,,
51,254.83,296.2,UHF military,225-328.6 MHz,Military communications,,,,,,,
52,255.25,302.425,UHF military,225-328.6 MHz,Military communications,255.25,25KHz,UFO-10/11,70.0 East,,Narrowband FM/AM,Тактические голос/данные каналы
53,255.35,296.35,Navy 25 kHz,Navy 25K,Tactical voice/data communications,255.35,25KHz,UFO-2,28.3 East,,Narrowband FM/AM,Тактические голос/данные каналы
54,255.4,296.4,Navy 25 kHz,Navy 25K,Tactical voice/data communications,255.4,25KHz,UFO-10/11,70.0 East,,Narrowband FM/AM,Тактические голос/данные каналы
55,255.45,296.45,UHF military,225-328.6 MHz,Military communications,255.45,25KHz,UFO-7,23.3 West,DAMA kg_net= 3 9k6+19k2+32k,Narrowband FM/AM,DAMA (динамическое распределение каналов)
56,255.55,296.55,Navy 25 kHz,Navy 25K,Tactical voice/data communications,255.55,25KHz,Fltsatcom 8,15.5 West,,Narrowband FM/AM,Тактические голос/данные каналы
57,255.55,296.55,Navy 25 kHz,Navy 25K,Tactical voice/data communications,255.55,25KHz,Fltsatcom 8,15.5 West,,Narrowband FM/AM,Тактические голос/данные каналы
58,255.775,309.3,UHF military,225-328.6 MHz,Military communications,,,,,,,
59,256.45,313.85,UHF military,225-328.6 MHz,Military communications,,,,,,,
60,256.6,305.95,UHF military,225-328.6 MHz,Military communications,,,,,,,
61,256.85,297.85,Navy 25 kHz,Navy 25K,Tactical voice/data communications,256.85,25KHz,UFO-10/11,70.0 East,,Narrowband FM/AM,Тактические голос/данные каналы
62,256.9,296.1,UHF military,225-328.6 MHz,Military communications,256.9,25KHz,UFO-10/11,70.0 East,,Narrowband FM/AM,Тактические голос/данные каналы
63,256.95,297.95,UHF military,225-328.6 MHz,Military communications,256.95,25KHz,UFO-2,28.3 East,,Narrowband FM/AM,Тактические голос/данные каналы
64,257.0,297.675,Navy 25 kHz,Navy 25K,Tactical voice/data communications,257.0,25KHz,UFO-10/11,70.0 East,,Narrowband FM/AM,Тактические голос/данные каналы
65,257.05,298.05,Navy 25 kHz,Navy 25K,Tactical voice/data communications,257.05,25KHz,UFO-7,23.3 West,DAMA kg_net= 4 9k6+32k,Narrowband FM/AM,DAMA (динамическое распределение каналов)
66,257.1,295.65,UHF military,225-328.6 MHz,Military communications,257.1,25KHz,UFO-10/11,70.0 East,,Narrowband FM/AM,Тактические голос/данные каналы
67,257.15,298.15,UHF military,225-328.6 MHz,Military communications,257.15,25KHz,Fltsatcom 8,15.5 West,DAMA kg_net= 6 9k6,Narrowband FM/AM,DAMA (динамическое распределение каналов)
68,257.2,308.8,UHF military,225-328.6 MHz,Military communications,,,,,,,
69,257.25,309.475,UHF military,225-328.6 MHz,Military communications,,,,,,,
70,257.3,309.725,UHF military,225-328.6 MHz,Military communications,,,,,,,
71,257.35,307.2,UHF military,225-328.6 MHz,Military communications,,,,,,,
72,257.5,311.35,UHF military,225-328.6 MHz,Military communications,,,,,,,
73,257.7,316.15,UHF military,225-328.6 MHz,Military communications,,,,,,,
74,257.775,311.375,UHF military,225-328.6 MHz,Military communications,,,,,,,
75,257.825,297.075,UHF military,225-328.6 MHz,Military communications,,,,,,,
76,257.9,298.0,UHF military,225-328.6 MHz,Military communications,,,,,,,
77,258.15,293.2,UHF military,225-328.6 MHz,Military communications,,,,,,,
78,258.35,299.35,Navy 25 kHz,Navy 25K,Tactical voice/data communications,258.35,25KHz,UFO-10/11,70.0 East,,Narrowband FM/AM,Тактические голос/данные каналы
79,258.45,299.45,Navy 25 kHz,Navy 25K,Tactical voice/data communications,258.45,25KHz,UFO-2,28.3 East,PSK modem,PSK (Phase Shift Keying),Модемные каналы данных
80,258.5,299.5,Navy 25 kHz,Navy 25K,Tactical voice/data communications,258.5,25KHz,UFO-10/11,70.0 East,,Narrowband FM/AM,Тактические голос/данные каналы
81,258.55,299.55,Navy 25 kHz,Navy 25K,Tactical voice/data communications,258.55,25KHz,UFO-7,23.3 West,,Narrowband FM/AM,Тактические голос/данные каналы
82,258.65,299.65,Navy 25 kHz,Navy 25K,Tactical voice/data communications,258.65,25KHz,Fltsatcom 8,15.5 West,Strong GMSK,GMSK (Gaussian Minimum Shift Keying),Цифровая передача (тактическая/мобильная)
83,259.0,317.925,UHF military,225-328.6 MHz,Military communications,,,,,,,
84,259.05,317.975,UHF military,225-328.6 MHz,Military communications,,,,,,,
85,259.975,310.05,UHF military,225-328.6 MHz,Military communications,259.975,30KHz,Sicral 1B,11.8 East,,PSK (Phase Shift Keying),Итальянская военная связь (голос/данные)
86,260.025,310.225,UHF military,225-328.6 MHz,Military communications,260.025,30KHz,Sicral 1B,11.8 East,,PSK (Phase Shift Keying),Итальянская военная связь (голос/данные)
87,260.075,310.275,UHF military,225-328.6 MHz,Military communications,260.075,30KHz,Sicral 1B,11.8 East,,PSK (Phase Shift Keying),Итальянская военная связь (голос/данные)
88,260.125,310.125,UHF military,225-328.6 MHz,Military communications,260.125,30KHz,Sicral 1B,11.8 East,,PSK (Phase Shift Keying),Итальянская военная связь (голос/данные)
89,260.175,310.325,UHF military,225-328.6 MHz,Military communications,,,,,,,
90,260.375,292.975,UHF military,225-328.6 MHz,Military communications,260.375,25KHz,UFO-10/11,70.0 East,,Narrowband FM/AM,Тактические голос/данные каналы
91,260.425,297.575,UHF military,225-328.6 MHz,Military communications,260.425,25KHz,UFO-7,23.3 West,DAMA kg_net= 8 9k6+19k2,Narrowband FM/AM,DAMA (динамическое распределение каналов)
92,260.425,294.025,DOD 25 kHz,DOD 25K,Tactical communications (DoD),260.425,25KHz,UFO-7,23.3 West,DAMA kg_net= 8 9k6+19k2,Narrowband FM/AM,DAMA (динамическое распределение каналов)
93,260.475,294.075,DOD 25 kHz,DOD 25K,Tactical communications (DoD),260.475,25KHz,UFO-10/11,70.0 East,,Narrowband FM/AM,Тактические голос/данные каналы
94,260.525,294.125,UHF military,225-328.6 MHz,Military communications,260.525,25KHz,UFO-7,23.3 West,BPSK modem,BPSK (Binary Phase Shift Keying),Модемные каналы данных
95,260.55,296.775,UHF military,225-328.6 MHz,Military communications,,,,,,,
96,260.575,294.175,DOD 25 kHz,DOD 25K,Tactical communications (DoD),260.575,25KHz,UFO-2,28.3 East,,Narrowband FM/AM,Тактические голос/данные каналы
97,260.625,294.225,DOD 25 kHz,DOD 25K,Tactical communications (DoD),260.625,25KHz,UFO-10/11,70.0 East,,Narrowband FM/AM,Тактические голос/данные каналы
98,260.675,294.475,DOD 25 kHz,DOD 25K,Tactical communications (DoD),260.675,25KHz,UFO-2,28.3 East,,Narrowband FM/AM,Тактические голос/данные каналы
99,260.675,294.275,UHF military,225-328.6 MHz,Military communications,260.675,25KHz,UFO-2,28.3 East,,Narrowband FM/AM,Тактические голос/данные каналы
100,260.725,294.325,UHF military,225-328.6 MHz,Military communications,260.725,25KHz,UFO-10/11,70.0 East,,Narrowband FM/AM,Тактические голос/данные каналы
101,260.9,313.9,UHF military,225-328.6 MHz,Military communications,,,,,,,
102,261.1,298.38,UHF military,225-328.6 MHz,Military communications,,,,,,,
103,261.1,298.7,UHF military,225-328.6 MHz,Military communications,,,,,,,
104,261.2,294.95,UHF military,225-328.6 MHz,Military communications,,,,,,,
105,262.0,314.2,UHF military,225-328.6 MHz,Military communications,262.0,25KHz,UFO-10/11,70.0 East,,Narrowband FM/AM,Тактические голос/данные каналы
106,262.04,307.075,UHF military,225-328.6 MHz,Military communications,,,,,,,
107,262.075,306.975,UHF military,225-328.6 MHz,Military communications,262.075,25KHz,UFO-2,28.3 East,,Narrowband FM/AM,Тактические голос/данные каналы
108,262.125,295.725,DOD 25 kHz,DOD 25K,Tactical communications (DoD),262.125,25KHz,UFO-10/11,70.0 East,,Narrowband FM/AM,Тактические голос/данные каналы
109,262.175,297.025,UHF military,225-328.6 MHz,Military communications,262.175,25KHz,UFO-2,28.3 East,,Narrowband FM/AM,Тактические голос/данные каналы
110,262.175,295.775,DOD 25 kHz,DOD 25K,Tactical communications (DoD),262.175,25KHz,UFO-2,28.3 East,,Narrowband FM/AM,Тактические голос/данные каналы
111,262.225,295.825,DOD 25 kHz,DOD 25K,Tactical communications (DoD),262.225,25KHz,UFO-10/11,70.0 East,,Narrowband FM/AM,Тактические голос/данные каналы
112,262.275,295.875,UHF military,225-328.6 MHz,Military communications,262.275,25KHz,UFO-2,28.3 East,,Narrowband FM/AM,Тактические голос/данные каналы
113,262.275,300.275,UHF military,225-328.6 MHz,Military communications,262.275,25KHz,UFO-2,28.3 East,,Narrowband FM/AM,Тактические голос/данные каналы
114,262.325,295.925,DOD 25 kHz,DOD 25K,Tactical communications (DoD),262.325,25KHz,UFO-10/11,70.0 East,,Narrowband FM/AM,Тактические голос/данные каналы
115,262.375,295.975,UHF military,225-328.6 MHz,Military communications,262.375,25KHz,UFO-2,28.3 East,,Narrowband FM/AM,Тактические голос/данные каналы
116,262.425,296.025,DOD 25 kHz,DOD 25K,Tactical communications (DoD),262.425,25KHz,UFO-10/11,70.0 East,,Narrowband FM/AM,Тактические голос/данные каналы
117,263.45,311.4,UHF military,225-328.6 MHz,Military communications,,,,,,,
118,263.5,309.875,UHF military,225-328.6 MHz,Military communications,,,,,,,
119,263.575,297.175,DOD 25 kHz,DOD 25K,Tactical communications (DoD),263.575,25Khz,UFO-10/11,70.0 East,Data March 2010,Digital Narrowband Data,Цифровые данные (низкая/средняя скорость)
120,263.625,297.225,DOD 25 kHz,DOD 25K,Tactical communications (DoD),263.625,25KHz,UFO-7,23.3 West,BPSK modem,BPSK (Binary Phase Shift Keying),Модемные каналы данных
121,263.675,297.275,DOD 25 kHz,DOD 25K,Tactical communications (DoD),263.675,25KHz,UFO-10/11,70.0 East,,Narrowband FM/AM,Тактические голос/данные каналы
122,263.725,297.325,UHF military,225-328.6 MHz,Military communications,263.725,25KHz,UFO-7,23.3 West,,Narrowband FM/AM,Тактические голос/данные каналы
123,263.775,297.375,DOD 25 kHz,DOD 25K,Tactical communications (DoD),263.775,25KHz,UFO-2,28.3 East,,Narrowband FM/AM,Тактические голос/данные каналы
124,263.825,297.425,DOD 25 kHz,DOD 25K,Tactical communications (DoD),263.825,25KHz,UFO-10/11,70.0 East,,Narrowband FM/AM,Тактические голос/данные каналы
125,263.875,297.475,DOD 25 kHz,DOD 25K,Tactical communications (DoD),263.875,25KHz,UFO-2,28.3 East,,Narrowband FM/AM,Тактические голос/данные каналы
126,263.925,297.525,DOD 25 kHz,DOD 25K,Tactical communications (DoD),263.925,25KHz,UFO-10/11,70.0 East,,Narrowband FM/AM,Тактические голос/данные каналы
127,265.25,306.25,UHF military,225-328.6 MHz,Military communications,265.25,25KHz,UFO-10/11,70.0 East,,Narrowband FM/AM,Тактические голос/данные каналы
128,265.35,306.35,UHF military,225-328.6 MHz,Military communications,265.35,25KHz,UFO-2,28.3 East,,Narrowband FM/AM,Тактические голос/данные каналы
129,265.4,294.425,FLTSATCOM/Leasat,A/X,Tactical communications,265.4,25KHz,UFO-10/11,70.0 East,,Narrowband FM/AM,Тактические голос/данные каналы
130,265.45,306.45,UHF military,225-328.6 MHz,Military communications,265.45,25KHz,UFO-7,23.3 West,Daily RATT transmissions,Narrowband FM/AM,Тактические голос/данные каналы
131,265.5,302.525,UHF military,225-328.6 MHz,Military communications,265.5,25KHz,UFO-10/11,70.0 East,,Narrowband FM/AM,Тактические голос/данные каналы
132,265.55,306.55,Navy 25 kHz,Navy 25K,Tactical voice/data communications,265.55,25KHz,Fltsatcom 8,15.5 West,Many interfering carriers in tpx,Narrowband FM/AM,Тактические голос/данные каналы
133,265.675,306.675,UHF military,225-328.6 MHz,Military communications,,,,,,,
134,265.85,306.85,UHF military,225-328.6 MHz,Military communications,,,,,,,
135,266.75,316.575,UHF military,225-328.6 MHz,Military communications,266.75,25KHz,UFO-10/11,70.0 East,,Narrowband FM/AM,Тактические голос/данные каналы
136,266.85,307.85,Navy 25 kHz,Navy 25K,Tactical voice/data communications,266.85,25KHz,UFO-2,28.3 East,,Narrowband FM/AM,Тактические голос/данные каналы
137,266.9,297.625,UHF military,225-328.6 MHz,Military communications,266.9,25KHz,UFO-10/11,70.0 East,,Narrowband FM/AM,Тактические голос/данные каналы
138,266.95,307.95,Navy 25 kHz,Navy 25K,Tactical voice/data communications,266.95,25KHz,UFO-7,23.3 West,Russian phone relays,Narrowband FM/AM,Тактические голос/данные каналы
139,267.05,308.05,Navy 25 kHz,Navy 25K,Tactical voice/data communications,267.05,25KHz,UFO-7,23.3 West,,Narrowband FM/AM,Тактические голос/данные каналы
140,267.1,308.1,Navy 25 kHz,Navy 25K,Tactical voice/data communications,,,,,,,
141,267.15,308.15,Navy 25 kHz,Navy 25K,Tactical voice/data communications,,,,,,,
142,267.2,308.2,Navy 25 kHz,Navy 25K,Tactical voice/data communications,,,,,,,
143,267.25,308.25,Navy 25 kHz,Navy 25K,Tactical voice/data communications,,,,,,,
144,267.4,294.9,UHF military,225-328.6 MHz,Military communications,,,,,,,
145,267.875,310.375,UHF military,225-328.6 MHz,Military communications,267.875,30KHz,Sicral 1B,11.8 East,,PSK (Phase Shift Keying),Итальянская военная связь (голос/данные)
146,267.95,310.45,UHF military,225-328.6 MHz,Military communications,267.95,30KHz,Sicral 1B,11.8 East,,PSK (Phase Shift Keying),Итальянская военная связь (голос/данные)
147,268.0,310.475,UHF military,225-328.6 MHz,Military communications,268.0,30KHz,Sicral 1B,11.8 East,,PSK (Phase Shift Keying),Итальянская военная связь (голос/данные)
148,268.025,309.025,Navy 25 kHz,Navy 25K,Tactical voice/data communications,,,,,,,
149,268.05,310.55,UHF military,225-328.6 MHz,Military communications,268.05,30KHz,Sicral 1B,11.8 East,,PSK (Phase Shift Keying),Итальянская военная связь (голос/данные)
150,268.1,310.6,UHF military,225-328.6 MHz,Military communications,268.1,30KHz,Sicral 1B,11.8 East,,PSK (Phase Shift Keying),Итальянская военная связь (голос/данные)
151,268.15,309.15,Navy 25 kHz,Navy 25K,Tactical voice/data communications,268.15,25KHz,UFO-10/11,70.0 East,,Narrowband FM/AM,Тактические голос/данные каналы
152,268.2,296.05,UHF military,225-328.6 MHz,Military communications,268.2,25KHz,UFO-10/11,70.0 East,,Narrowband FM/AM,Тактические голос/данные каналы
153,268.25,309.25,Navy 25 kHz,Navy 25K,Tactical voice/data communications,268.25,25KHz,UFO-2,28.3 East,,Narrowband FM/AM,Тактические голос/данные каналы
154,268.3,309.3,Navy 25 kHz,Navy 25K,Tactical voice/data communications,268.3,25KHz,UFO-10/11,70.0 East,,Narrowband FM/AM,Тактические голос/данные каналы
155,268.35,309.35,UHF military,225-328.6 MHz,Military communications,268.35,25KHz,UFO-7,23.3 West,DAMA kg_net= 1 9k6+19k2+32k,Narrowband FM/AM,DAMA (динамическое распределение каналов)
156,268.4,295.9,FLTSATCOM/Leasat,C/Z,Tactical communications,268.4,25KHz,UFO-10/11,70.0 East,,Narrowband FM/AM,Тактические голос/данные каналы
157,268.45,309.45,UHF military,225-328.6 MHz,Military communications,268.45,25KHz,UFO-7,23.3 West,Many interfering carriers in tpx,Narrowband FM/AM,Тактические голос/данные каналы
158,269.7,309.925,UHF military,225-328.6 MHz,Military communications,269.7,25KHz,UFO-10/11,70.0 East,,Narrowband FM/AM,Тактические голос/данные каналы
159,269.75,310.75,UHF military,225-328.6 MHz,Military communications,269.75,25KHz,UFO-2,28.3 East,Wheel,Narrowband FM/AM,Тактические голос/данные каналы
160,269.8,310.025,UHF military,225-328.6 MHz,Military communications,269.8,25KHz,UFO-10/11,70.0 East,,Narrowband FM/AM,Тактические голос/данные каналы
161,269.85,310.85,Navy 25 kHz,Navy 25K,Tactical voice/data communications,269.85,30KHz,UFO-7,23.3 West,DAMA kg_net= 2 9k6+19k2+32k,Narrowband FM/AM,DAMA (динамическое распределение каналов)
162,269.95,310.95,DOD 25 kHz,DOD 25K,Tactical communications (DoD),269.95,25KHz,UFO-7,23.3 West,PSK mode,PSK (Phase Shift Keying),Цифровые данные`;
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
  UI.els.navOverlay = $("#navOverlay");
  UI.els.endpoint = $("#endpoint");
  UI.els.themeToggle = $("#themeToggle");
  UI.els.themeRedToggle = $("#themeRedToggle");
  UI.els.status = $("#statusLine");
  UI.els.footerMeta = $("#footerMeta");
  UI.els.version = $("#appVersion");
  updateFooterVersion(); // сразу показываем сохранённую версию, если она есть
  UI.els.toast = $("#toast");
  UI.els.debugLog = $("#debugLog");
  UI.els.rstsFullBtn = $("#btnRstsFull");
  UI.els.rstsJsonBtn = $("#btnRstsJson");
  UI.els.rstsDownloadBtn = $("#btnRstsDownloadJson");
  UI.els.chatLog = $("#chatLog");
  UI.els.chatInput = $("#chatInput");
  UI.els.chatScrollBtn = $("#chatScrollBottom");
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
  UI.els.recvClear = $("#btnRecvClear");
  UI.els.autoNightSwitch = $("#autoNightMode");
  UI.els.autoNightHint = $("#autoNightHint");
  UI.els.channelInfoPanel = $("#channelInfoPanel");
  UI.els.channelInfoTitle = $("#channelInfoTitle");
  UI.els.channelInfoBody = $("#channelInfoBody");
  UI.els.channelInfoEmpty = $("#channelInfoEmpty");
  UI.els.channelInfoStatus = $("#channelInfoStatus");
  UI.els.channelRefStatus = $("#channelRefStatus");
  UI.els.channelInfoSetBtn = $("#channelInfoSetCurrent");
  UI.els.channelInfoRow = null;
  UI.els.channelInfoCell = null;
  UI.els.channelInfoFields = {
    rxCurrent: $("#channelInfoRxCurrent"),
    txCurrent: $("#channelInfoTxCurrent"),
    rssi: $("#channelInfoRssi"),
    snr: $("#channelInfoSnr"),
    status: $("#channelInfoStatusCurrent"),
    scan: $("#channelInfoScan"),
    rxRef: $("#channelInfoRxRef"),
    txRef: $("#channelInfoTxRef"),
    frequency: $("#channelInfoFrequency"),
    system: $("#channelInfoSystem"),
    band: $("#channelInfoBand"),
    bandwidth: $("#channelInfoBandwidth"),
    purpose: $("#channelInfoPurpose"),
    satellite: $("#channelInfoSatellite"),
    position: $("#channelInfoPosition"),
    modulation: $("#channelInfoModulation"),
    usage: $("#channelInfoUsage"),
    comments: $("#channelInfoComments"),
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
  UI.els.pointing = {
    tab: $("#tab-pointing"),
    status: $("#pointingStatus"),
    summary: $("#pointingSummary"),
    tleBadge: $("#pointingTleBadge"),
    tleBadgeText: $("#pointingTleText"),
    locationBadge: $("#pointingLocationBadge"),
    locationBadgeText: $("#pointingLocationText"),
    satBadge: $("#pointingSatBadge"),
    satBadgeText: $("#pointingSatText"),
    observerDetails: $("#pointingObserverDetails"),
    observerLabel: $("#pointingObserverLabel"),
    mgrsInput: $("#pointingMgrsInput"),
    lat: $("#pointingLat"),
    lon: $("#pointingLon"),
    alt: $("#pointingAlt"),
    manualAlt: $("#pointingManualAlt"),
    mgrsApply: $("#pointingMgrsApply"),
    satSummary: $("#pointingSatSummary"),
    satSelect: $("#pointingSatSelect"),
    satDetails: $("#pointingSatDetails"),
    satList: $("#pointingSatList"),
    horizon: $("#pointingHorizon"),
    horizonTrack: $("#pointingHorizonTrack"),
    horizonEmpty: $("#pointingHorizonEmpty"),
    targetAz: $("#pointingTargetAz"),
    targetEl: $("#pointingTargetEl"),
    compass: $("#pointingCompass"),
    compassRadar: $("#pointingCompassRadar"),
    compassLegend: $("#pointingCompassLegend"),
    minElSlider: $("#pointingMinElevation"),
    minElValue: $("#pointingMinElValue"),
    subLon: $("#pointingSubLon"),
    subLat: $("#pointingSubLat"),
    satAltitude: $("#pointingSatAltitude"),
    range: $("#pointingRange"),
  };
  UI.els.keyRecvBtn = $("#btnKeyRecv");
  UI.els.encTestBtn = $("#btnEncTestRun");
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
  initPointingTab();

  // Меню на мобильных устройствах
  if (UI.els.menuToggle && UI.els.nav) {
    UI.els.menuToggle.addEventListener("click", () => {
      const open = !UI.els.nav.classList.contains("open");
      setMenuOpen(open);
    });
  }
  if (UI.els.nav) {
    UI.els.nav.addEventListener("click", () => setMenuOpen(false));
  }
  if (UI.els.navOverlay) {
    UI.els.navOverlay.addEventListener("click", () => setMenuOpen(false));
  }

  // Тема оформления
  initThemeSystem();
  applyAccent(UI.cfg.accent);
  if (UI.els.themeToggle) {
    UI.els.themeToggle.addEventListener("click", () => toggleTheme());
  }
  if (UI.els.themeRedToggle) {
    UI.els.themeRedToggle.setAttribute("aria-pressed", UI.cfg.accent === "red" ? "true" : "false");
    UI.els.themeRedToggle.addEventListener("click", () => toggleAccent());
  }
  if (UI.els.autoNightSwitch) {
    UI.els.autoNightSwitch.checked = UI.cfg.autoNight;
    UI.els.autoNightSwitch.addEventListener("change", () => setAutoNight(UI.els.autoNightSwitch.checked));
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
  if (UI.els.chatLog) {
    UI.els.chatLog.addEventListener("scroll", onChatScroll);
    updateChatScrollButton();
  }
  if (UI.els.chatScrollBtn) {
    UI.els.chatScrollBtn.addEventListener("click", (event) => {
      event.preventDefault();
      scrollChatToBottom(true);
    });
  }
  if (typeof document !== "undefined") {
    const unlockAudio = () => {
      ensureChatAudioContext();
    };
    document.addEventListener("pointerdown", unlockAudio, { once: true, capture: true });
    document.addEventListener("keydown", unlockAudio, { once: true, capture: true });
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

  // Управление командами RSTS на вкладке Debug
  if (UI.els.rstsFullBtn) UI.els.rstsFullBtn.addEventListener("click", requestRstsFullDebug);
  if (UI.els.rstsJsonBtn) UI.els.rstsJsonBtn.addEventListener("click", requestRstsJsonDebug);
  if (UI.els.rstsDownloadBtn) UI.els.rstsDownloadBtn.addEventListener("click", downloadRstsFullJson);

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
  if (UI.els.recvClear) {
    UI.els.recvClear.addEventListener("click", () => clearReceivedList());
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

function setMenuOpen(open) {
  const nav = UI.els.nav || $("#siteNav");
  const toggle = UI.els.menuToggle || $("#menuToggle");
  const overlay = UI.els.navOverlay || $("#navOverlay");
  if (nav) nav.classList.toggle("open", open);
  if (toggle) toggle.setAttribute("aria-expanded", String(open));
  if (overlay) {
    overlay.hidden = !open;
    overlay.classList.toggle("visible", open);
  }
  document.body.classList.toggle("nav-open", open);
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
  if (tab === "chat") updateChatScrollButton();
}

/* Тема */
function initThemeSystem() {
  if (UI.cfg.autoNight) {
    startAutoNightTimer();
    applyAutoNightTheme(true);
  } else {
    applyTheme(UI.cfg.theme);
    updateAutoNightUi();
  }
}
function applyTheme(mode) {
  UI.cfg.theme = mode === "light" ? "light" : "dark";
  document.documentElement.classList.toggle("light", UI.cfg.theme === "light");
  storage.set("theme", UI.cfg.theme);
  if (UI.els.themeToggle) {
    UI.els.themeToggle.setAttribute("aria-pressed", UI.cfg.theme === "dark" ? "true" : "false");
  }
  updateAutoNightUi();
}
function toggleTheme() {
  if (UI.cfg.autoNight) setAutoNight(false);
  applyTheme(UI.cfg.theme === "dark" ? "light" : "dark");
}
function setAutoNight(enabled) {
  if (enabled) {
    UI.cfg.autoNight = true;
    storage.set("autoNight", "1");
    startAutoNightTimer();
    applyAutoNightTheme(true);
  } else {
    if (UI.cfg.autoNight) storage.set("autoNight", "0");
    UI.cfg.autoNight = false;
    stopAutoNightTimer();
    UI.state.autoNightActive = false;
    updateAutoNightUi();
  }
}
function startAutoNightTimer() {
  if (UI.state.autoNightTimer) clearInterval(UI.state.autoNightTimer);
  UI.state.autoNightTimer = setInterval(() => applyAutoNightTheme(false), 5 * 60 * 1000);
}
function stopAutoNightTimer() {
  if (UI.state.autoNightTimer) {
    clearInterval(UI.state.autoNightTimer);
    UI.state.autoNightTimer = null;
  }
}
function applyAutoNightTheme(force) {
  const now = new Date();
  const hour = now.getHours();
  const shouldDark = hour >= 21 || hour < 7;
  if (force || shouldDark !== UI.state.autoNightActive) {
    UI.state.autoNightActive = shouldDark;
    applyTheme(shouldDark ? "dark" : "light");
  } else {
    updateAutoNightUi();
  }
}
function updateAutoNightUi() {
  const input = UI.els.autoNightSwitch || $("#autoNightMode");
  if (input) input.checked = UI.cfg.autoNight;
  const hint = UI.els.autoNightHint || $("#autoNightHint");
  if (hint) {
    if (UI.cfg.autoNight) {
      hint.textContent = UI.state.autoNightActive
        ? "Сейчас активна ночная тема (21:00–07:00)."
        : "Автовключение с 21:00 до 07:00 работает.";
    } else {
      hint.textContent = "Автоматическое переключение отключено.";
    }
  }
  if (UI.els.themeToggle) {
    UI.els.themeToggle.setAttribute("aria-pressed", UI.cfg.theme === "dark" ? "true" : "false");
  }
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
const CHAT_SCROLL_EPSILON = 32;

function chatIsNearBottom(log) {
  if (!log) return true;
  const diff = log.scrollHeight - log.scrollTop - log.clientHeight;
  return diff <= CHAT_SCROLL_EPSILON;
}

function updateChatScrollButton() {
  const log = UI.els.chatLog;
  const btn = UI.els.chatScrollBtn;
  if (!btn || !log) return;
  const pinned = chatIsNearBottom(log);
  btn.hidden = pinned;
}

function scrollChatToBottom(force) {
  const log = UI.els.chatLog;
  if (!log) return;
  if (force || UI.state.chatScrollPinned || chatIsNearBottom(log)) {
    log.scrollTop = log.scrollHeight;
    UI.state.chatScrollPinned = true;
  }
  updateChatScrollButton();
}

function onChatScroll() {
  const log = UI.els.chatLog;
  if (!log) return;
  UI.state.chatScrollPinned = chatIsNearBottom(log);
  updateChatScrollButton();
}

function shouldPlayIncomingSound(entry) {
  if (!entry) return false;
  const role = entry.role || (entry.a === "you" ? "user" : "system");
  return role === "rx";
}

function ensureChatAudioContext() {
  if (typeof window === "undefined") return null;
  const Ctx = window.AudioContext || window.webkitAudioContext;
  if (!Ctx) return null;
  if (!UI.state.chatSoundCtx) {
    try {
      UI.state.chatSoundCtx = new Ctx();
    } catch (err) {
      UI.state.chatSoundCtx = null;
      return null;
    }
  }
  const ctx = UI.state.chatSoundCtx;
  if (ctx && typeof ctx.resume === "function" && ctx.state === "suspended") {
    ctx.resume().catch(() => {});
  }
  return ctx;
}

function playIncomingChatSound() {
  const now = Date.now();
  if (now - UI.state.chatSoundLast < 250) return;
  const ctx = ensureChatAudioContext();
  if (!ctx) return;
  try {
    const osc = ctx.createOscillator();
    const gain = ctx.createGain();
    const start = ctx.currentTime;
    osc.type = "sine";
    osc.frequency.setValueAtTime(660, start);
    gain.gain.setValueAtTime(0, start);
    gain.gain.linearRampToValueAtTime(0.09, start + 0.02);
    gain.gain.exponentialRampToValueAtTime(0.0001, start + 0.38);
    osc.connect(gain);
    gain.connect(ctx.destination);
    osc.start(start);
    osc.stop(start + 0.4);
    UI.state.chatSoundLast = now;
  } catch (err) {
    console.warn("[chat] аудио уведомление недоступно:", err);
  }
}

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
      if (!rxText && rawMeta && rawMeta.hex) {
        const decodedMetaText = resolveReceivedText(rawMeta);
        if (decodedMetaText) rxText = decodedMetaText;
      }
      let name = cleanString(rawMeta.name || obj.name);
      if (!name) name = parseKnownName(rawMessage);
      if (!name && rxText) name = parseKnownName(rxText);
      const type = normalizeReceivedType(name, rawMeta.type || obj.type || obj.kind || obj.queue);
      const hex = cleanString(rawMeta.hex || obj.hex);
      let lenValue = toNumber(rawMeta.len != null ? rawMeta.len : (obj.len != null ? obj.len : obj.length));
      if ((!lenValue || lenValue <= 0) && rawMeta && rawMeta.hex) {
        const metaLen = getReceivedLength(rawMeta);
        if (metaLen) lenValue = metaLen;
      }
      const bubbleSource = rxText || dropRxPrefix(rawMessage) || "";
      const bubbleCore = cleanString(bubbleSource);
      const finalMessage = bubbleCore || "—";
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
      else if (bubbleCore) normalizedRx.text = bubbleCore;
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
  UI.state.chatHydrating = true;
  for (let i = 0; i < normalized.length; i++) {
    addChatMessage(normalized[i], i, { skipSound: true, skipScroll: true });
  }
  UI.state.chatHydrating = false;
  scrollChatToBottom(true);
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
    if (meta.rx) record.rx = meta.rx;
    if (meta.detail != null) record.detail = meta.detail;
  }
  if (!record.role) record.role = author === "you" ? "user" : "system";
  entries.push(record);
  saveChatHistory();
  return { record, index: entries.length - 1 };
}
function addChatMessage(entry, index, options) {
  const opts = options || {};
  if (!UI.els.chatLog) return null;
  const data = typeof entry === "string" ? { t: Date.now(), a: "dev", m: entry, role: "system" } : entry;
  const wrap = document.createElement("div");
  const author = data.a === "you" ? "you" : "dev";
  wrap.className = "msg";
  wrap.classList.add(author);
  const role = data.role || (author === "you" ? "user" : "system");
  if (role === "rx") wrap.classList.add("rx");
  else if (role !== "user") wrap.classList.add("system");
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
  if (!opts.skipSound && !UI.state.chatHydrating && shouldPlayIncomingSound(data)) {
    playIncomingChatSound();
  }
  if (opts.skipScroll) {
    updateChatScrollButton();
  } else {
    scrollChatToBottom(opts.forceScroll === true);
  }
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
  const isRx = entry && entry.role === "rx";
  node.innerHTML = "";
  const textBox = document.createElement("div");
  textBox.className = "bubble-text";
  if (isRx) {
    const rxInfo = entry && entry.rx && typeof entry.rx === "object" ? entry.rx : {};
    const rxTextRaw = rxInfo.text != null ? String(rxInfo.text) : "";
    const rxText = rxTextRaw.trim();
    const fallback = raw.replace(/^RX\s*[·:>.\-]?\s*/i, "").trim();
    const displayText = rxText || fallback || raw || "—";
    setBubbleText(textBox, displayText || "—");
  } else {
    setBubbleText(textBox, raw);
  }
  node.appendChild(textBox);
  const detailRaw = entry && entry.detail != null ? String(entry.detail) : "";
  const detail = detailRaw.trim();
  if (detail) {
    // Показываем полный текст ответа устройства отдельно от заголовка
    const visibleText = (textBox.textContent || "").trim();
    if (!visibleText || detail !== visibleText) {
      const detailBox = document.createElement("pre");
      detailBox.className = "bubble-detail";
      detailBox.textContent = detail;
      node.appendChild(detailBox);
    }
  }
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
  if (isRx) {
    const rxInfo = entry && entry.rx && typeof entry.rx === "object" ? entry.rx : {};
    const footer = document.createElement("div");
    footer.className = "bubble-meta bubble-rx";
    const nameNode = document.createElement("span");
    nameNode.className = "bubble-rx-name";
    const metaNameRaw = rxInfo.name != null ? String(rxInfo.name) : "";
    const metaName = metaNameRaw.trim();
    nameNode.textContent = "name: " + (metaName || "—");
    if (metaName) nameNode.title = metaName;
    footer.appendChild(nameNode);
    const lenNode = document.createElement("span");
    lenNode.className = "bubble-rx-len";
    let lenValue = null;
    if (rxInfo.len != null) {
      const lenNum = Number(rxInfo.len);
      if (Number.isFinite(lenNum) && lenNum >= 0) lenValue = lenNum;
    }
    if (lenValue == null) {
      const metaText = rxInfo.text != null ? String(rxInfo.text) : "";
      if (metaText) lenValue = metaText.length;
      else if (typeof raw === "string" && raw) lenValue = raw.length;
    }
    lenNode.textContent = "Len: " + (lenValue != null ? lenValue + " байт" : "—");
    footer.appendChild(lenNode);
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
function summarizeResponse(text, fallback) {
  const raw = text != null ? String(text) : "";
  const trimmed = raw.trim();
  if (!trimmed) return fallback || "—";
  const firstLine = trimmed.split(/\r?\n/)[0].trim();
  if (firstLine.length > 140) return firstLine.slice(0, 137) + "…";
  return firstLine;
}
function logSystemCommand(cmd, payload, state) {
  const clean = (cmd || "").toUpperCase();
  const ok = state === "ok";
  const statusText = ok ? "выполнено" : "ошибка";
  const detailRaw = payload != null ? String(payload) : "";
  const messageText = ok
    ? `СИСТЕМА · ${clean} → ${statusText}`
    : `СИСТЕМА · ${clean} ✗ ${statusText}`;
  const meta = { role: "system", tag: "cmd", cmd: clean, status: ok ? "ok" : "error" };
  if (detailRaw) meta.detail = detailRaw;
  const saved = persistChat(messageText, "dev", meta);
  addChatMessage(saved.record, saved.index);
}
async function sendCommand(cmd, params, opts) {
  const options = opts || {};
  const silent = options.silent === true;
  const timeout = options.timeoutMs || 4000;
  const debugLabel = options.debugLabel != null ? String(options.debugLabel) : cmd;
  const payload = params || {};
  if (!silent) status("→ " + cmd);
  const res = await deviceFetch(cmd, payload, timeout);
  if (res.ok) {
    if (!silent) {
      status("✓ " + cmd);
      note("Команда " + cmd + ": " + summarizeResponse(res.text, "успешно"));
      logSystemCommand(cmd, res.text, "ok");
    }
    debugLog((debugLabel || cmd) + ": " + res.text);
    handleCommandSideEffects(cmd, res.text);
    return res.text;
  }
  if (!silent) {
    status("✗ " + cmd);
    const errText = res.error != null ? String(res.error) : "ошибка";
    note("Команда " + cmd + ": " + summarizeResponse(errText, "ошибка"));
    logSystemCommand(cmd, errText, "error");
  }
  debugLog("ERR " + (debugLabel || cmd) + ": " + res.error);
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
    note("Команда TX: " + summarizeResponse(value, "успешно"));
    debugLog("TX: " + value);
    let attached = false;
    if (originIndex !== null) {
      attached = attachTxStatus(originIndex, { ok: true, text: value, raw: res.text });
    }
    if (!attached) {
      logSystemCommand("TX", value, "ok");
    }
    return value;
  }
  status("✗ TX");
  const errorText = res.error != null ? String(res.error) : "";
  note("Команда TX: " + summarizeResponse(errorText, "ошибка"));
  debugLog("ERR TX: " + errorText);
  let attached = false;
  if (originIndex !== null) {
    attached = attachTxStatus(originIndex, { ok: false, text: errorText, raw: res.error });
  }
  if (!attached) {
    logSystemCommand("TX", errorText, "error");
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
  const options = opts || {};
  const manual = options.manual === true;
  if (manual) status("→ RSTS");
  const limit = getRecvLimit();
  debugLog("→ RSTS FULL (" + (manual ? "ручной" : "авто") + ", n=" + limit + ")");
  const text = await sendCommand("RSTS", { n: limit, full: "1" }, { silent: true, timeoutMs: 2500, debugLabel: "RSTS FULL" });
  if (text === null) {
    if (!options.silentError) {
      if (manual) status("✗ RSTS");
      note("Не удалось получить список принятых сообщений");
    }
    return;
  }
  const entries = parseReceivedResponse(text);
  renderReceivedList(entries);
  if (manual) status("✓ RSTS (" + entries.length + ")");
}


// Кэш декодеров для преобразования входящих сообщений
const receivedTextDecoderCache = {};

// Допустимые знаки пунктуации для эвристики читаемости текста (оставляем комментарии по-русски)
const textAllowedPunctuation = new Set([
  ".", ",", ";", ":", "!", "?", "\"", "'", "(", ")", "[", "]", "{", "}",
  "<", ">", "-", "—", "–", "_", "+", "=", "*", "&", "%", "#", "@", "$",
  "^", "~", "`", "/", "\\", "|", "№", "…", "«", "»", "„", "“", "”", "‹", "›"
]);

// Подозрительные пары символов, указывающие на «кракозябры» после неверной перекодировки
const textSuspiciousPairRules = [
  { lead: 0x00C2, ranges: [[0x00A0, 0x00BF]] },
  { lead: 0x00C3, ranges: [[0x0080, 0x00BF]] },
  { lead: 0x00D0, ranges: [[0x0080, 0x00BF]] },
  { lead: 0x00D1, ranges: [[0x0080, 0x00BF]] },
  { lead: 0x0420, ranges: [[0x00A0, 0x00BF]] },
  { lead: 0x0421, ranges: [[0x0080, 0x009F], [0x0450, 0x045F]] }
];

// Оценка качества текста: возвращаем метрики и итоговый балл (чем меньше, тем лучше)
function evaluateTextCandidate(text) {
  if (!text) {
    return {
      text: "",
      score: Number.POSITIVE_INFINITY,
      replacements: Number.POSITIVE_INFINITY,
      cyrLower: 0,
      cyrUpper: 0,
      asciiLetters: 0,
      digits: 0,
      whitespace: 0,
      punctuation: 0,
      latinExtended: 0,
      mojibakePairs: 0,
    };
  }

  let replacements = 0;
  let control = 0;
  let asciiLetters = 0;
  let digits = 0;
  let whitespace = 0;
  let punctuation = 0;
  let cyrLower = 0;
  let cyrUpper = 0;
  let latinExtended = 0;
  let otherSymbols = 0;

  for (let i = 0; i < text.length; i += 1) {
    const code = text.charCodeAt(i);
    const ch = text[i];
    if (code === 0xFFFD) {
      replacements += 1;
      continue;
    }
    if (code === 0x0009 || code === 0x000A || code === 0x000D) {
      whitespace += 1;
      continue;
    }
    if (code < 0x20 || (code >= 0x7F && code <= 0x9F)) {
      control += 1;
      continue;
    }
    if (code === 0x20 || code === 0x00A0) {
      whitespace += 1;
      continue;
    }
    if (code >= 0x30 && code <= 0x39) {
      digits += 1;
      continue;
    }
    if ((code >= 0x41 && code <= 0x5A) || (code >= 0x61 && code <= 0x7A)) {
      asciiLetters += 1;
      continue;
    }
    if (textAllowedPunctuation.has(ch) || (code >= 0x2010 && code <= 0x2043) || (code >= 0x2100 && code <= 0x214F)) {
      punctuation += 1;
      continue;
    }
    if ((code >= 0x0430 && code <= 0x044F) || code === 0x0451) {
      cyrLower += 1;
      continue;
    }
    if ((code >= 0x0410 && code <= 0x042F) || code === 0x0401) {
      cyrUpper += 1;
      continue;
    }
    if ((code >= 0x00A0 && code <= 0x036F) || (code >= 0x2000 && code <= 0x206F)) {
      latinExtended += 1;
      continue;
    }
    otherSymbols += 1;
  }

  let mojibakePairs = 0;
  for (let i = 0; i + 1 < text.length; i += 1) {
    const lead = text.charCodeAt(i);
    const next = text.charCodeAt(i + 1);
    let matched = false;
    for (let r = 0; r < textSuspiciousPairRules.length && !matched; r += 1) {
      const rule = textSuspiciousPairRules[r];
      if (lead !== rule.lead) continue;
      const segments = rule.ranges;
      for (let s = 0; s < segments.length; s += 1) {
        const range = segments[s];
        if (next >= range[0] && next <= range[1]) {
          mojibakePairs += 1;
          matched = true;
          break;
        }
      }
    }
  }

  const recognized = asciiLetters + digits + whitespace + punctuation + cyrLower + cyrUpper;
  const unknown = Math.max(0, text.length - recognized - replacements);

  let score = (replacements * 400)
    + (control * 40)
    + (latinExtended * 12)
    + (otherSymbols * 25)
    + (unknown * 18)
    + (mojibakePairs * 65);

  const lowerBonus = Math.min(cyrLower, 12) * 6;
  const asciiBonus = Math.min(asciiLetters + digits, 12) * 2;
  score = Math.max(0, score - lowerBonus - asciiBonus);

  return {
    text,
    score,
    replacements,
    control,
    latinExtended,
    otherSymbols,
    unknown,
    mojibakePairs,
    cyrLower,
    cyrUpper,
    asciiLetters,
    digits,
    whitespace,
    punctuation,
  };
}

// Выбор наилучшего текстового кандидата на основе эвристик
function selectReadableTextCandidate(items) {
  if (!Array.isArray(items) || !items.length) return null;
  let best = null;
  for (let i = 0; i < items.length; i += 1) {
    const rawItem = items[i];
    if (!rawItem || typeof rawItem.text !== "string") continue;
    const sanitized = rawItem.text.replace(/\u0000+$/g, "");
    const trimmed = sanitized.trim();
    const prepared = trimmed || sanitized;
    if (!prepared) continue;
    const metrics = evaluateTextCandidate(prepared);
    if (!best) {
      best = { text: prepared, source: rawItem.source || "", metrics };
      continue;
    }
    const currentScore = metrics.score;
    const bestScore = best.metrics.score;
    if (currentScore < bestScore - 0.001) {
      best = { text: prepared, source: rawItem.source || "", metrics };
      continue;
    }
    if (Math.abs(currentScore - bestScore) <= 0.001) {
      if (metrics.cyrLower > best.metrics.cyrLower) {
        best = { text: prepared, source: rawItem.source || "", metrics };
        continue;
      }
      if (metrics.cyrLower === best.metrics.cyrLower && prepared.length > best.text.length) {
        best = { text: prepared, source: rawItem.source || "", metrics };
      }
    }
  }
  return best;
}

// Получение (или создание) TextDecoder для нужной кодировки
function getCachedTextDecoder(label) {
  if (typeof TextDecoder !== "function") return null;
  const key = label || "utf-8";
  if (Object.prototype.hasOwnProperty.call(receivedTextDecoderCache, key)) {
    return receivedTextDecoderCache[key];
  }
  try {
    const decoder = new TextDecoder(key, { fatal: false });
    receivedTextDecoderCache[key] = decoder;
    return decoder;
  } catch (err) {
    receivedTextDecoderCache[key] = null;
    console.warn("[recv] неподдерживаемая кодировка", label, err);
    return null;
  }
}

// Преобразование hex-строки в массив байтов
function hexToBytes(hex) {
  if (hex == null) return null;
  const clean = String(hex).trim().replace(/[^0-9A-Fa-f]/g, "");
  if (!clean) return null;
  const size = Math.floor(clean.length / 2);
  if (!size) return null;
  const out = new Uint8Array(size);
  let pos = 0;
  for (let i = 0; i + 1 < clean.length && pos < size; i += 2) {
    const byte = parseInt(clean.slice(i, i + 2), 16);
    if (Number.isNaN(byte)) return null;
    out[pos] = byte;
    pos += 1;
  }
  return pos === size ? out : out.slice(0, pos);
}

// Декодирование массива байтов в строку (cp1251 → utf-8 → ASCII)
function decodeBytesToText(bytes) {
  if (!(bytes instanceof Uint8Array) || bytes.length === 0) return "";
  const candidates = [];
  const decoderUtf8 = getCachedTextDecoder("utf-8");
  if (decoderUtf8) {
    try {
      candidates.push({ text: decoderUtf8.decode(bytes), source: "utf-8" });
    } catch (err) {
      console.warn("[recv] ошибка декодирования utf-8", err);
    }
  }
  const decoder1251 = getCachedTextDecoder("windows-1251");
  if (decoder1251) {
    try {
      candidates.push({ text: decoder1251.decode(bytes), source: "cp1251" });
    } catch (err) {
      console.warn("[recv] ошибка декодирования cp1251", err);
    }
  }
  const best = selectReadableTextCandidate(candidates);
  if (best && best.text) return best.text;
  let ascii = "";
  for (let i = 0; i < bytes.length; i += 1) {
    ascii += String.fromCharCode(bytes[i]);
  }
  return ascii.replace(/\u0000+$/g, "").trim();
}

// Подбор читаемого текста для элемента списка приёма
function resolveReceivedText(entry) {
  if (!entry || typeof entry !== "object") return "";
  if (typeof entry.resolvedText === "string") return entry.resolvedText;
  const raw = entry.text != null ? String(entry.text) : "";
  const candidates = [];
  if (raw) {
    candidates.push({ text: raw, source: "raw" });
  }
  let bytes = null;
  if (entry._hexBytes instanceof Uint8Array) bytes = entry._hexBytes;
  if (!(bytes instanceof Uint8Array) || bytes.length === 0) {
    bytes = hexToBytes(entry.hex);
    if (bytes && bytes.length) entry._hexBytes = bytes;
  }
  if (bytes && bytes.length) {
    const decoded = decodeBytesToText(bytes);
    if (decoded) {
      candidates.push({ text: decoded, source: "decoded" });
    }
  }
  const best = selectReadableTextCandidate(candidates);
  const cleaned = best && best.text ? best.text : "";
  entry.resolvedText = cleaned;
  if (best && best.source === "decoded" && cleaned) {
    const rawMetrics = raw ? evaluateTextCandidate(raw.trim() || raw) : null;
    if (!rawMetrics || rawMetrics.score > best.metrics.score) {
      entry.text = cleaned;
    }
  } else if (!entry.text && cleaned) {
    entry.text = cleaned;
  }
  return cleaned;
}

// Определение длины полезной нагрузки (len → hex → текст)
function getReceivedLength(entry) {
  if (!entry || typeof entry !== "object") return 0;
  if (Number.isFinite(entry.len) && entry.len > 0) return Number(entry.len);
  let bytes = null;
  if (entry._hexBytes instanceof Uint8Array) bytes = entry._hexBytes;
  if (!(bytes instanceof Uint8Array) || bytes.length === 0) {
    bytes = hexToBytes(entry.hex);
    if (bytes && bytes.length) entry._hexBytes = bytes;
  }
  if (bytes && bytes.length) {
    entry.resolvedLen = bytes.length;
    return bytes.length;
  }
  const textValue = resolveReceivedText(entry);
  const len = textValue ? textValue.length : 0;
  entry.resolvedLen = len;
  return len;
}

// Нормализуем статус принятого сообщения по имени и исходному типу
function normalizeReceivedType(name, rawType) {
  const typeText = rawType != null ? String(rawType).trim().toLowerCase() : "";
  if (typeText === "ready" || typeText === "split" || typeText === "raw") {
    return typeText;
  }
  const cleanName = name != null ? String(name).trim().toUpperCase() : "";
  if (cleanName.startsWith("GO-")) return "ready";
  if (cleanName.startsWith("SP-")) return "split";
  if (cleanName.startsWith("R-")) return "raw";
  return typeText || "raw";
}

function parseReceivedResponse(raw) {
  if (raw == null) return [];
  const trimmed = String(raw).trim();
  if (!trimmed) return [];
  if (trimmed.startsWith("{") || trimmed.startsWith("[")) {
    try {
      const parsed = JSON.parse(trimmed);
      const list = Array.isArray(parsed) ? parsed : (parsed && Array.isArray(parsed.items) ? parsed.items : []);
      if (Array.isArray(list)) {
        return list.map((entry) => {
          if (!entry) return null;
          const name = entry.name != null ? String(entry.name).trim() : "";
          const type = normalizeReceivedType(name, entry.type);
          const text = entry.text != null ? String(entry.text) : "";
          const hex = entry.hex != null ? String(entry.hex) : "";
          const bytes = hexToBytes(hex);
          const len = Number.isFinite(entry.len) ? Number(entry.len)
            : (bytes && bytes.length ? bytes.length : text.length);
          const normalized = { name, type, text, hex, len };
          if (bytes && bytes.length) normalized._hexBytes = bytes;
          const resolved = resolveReceivedText(normalized);
          if (resolved && !normalized.text) normalized.text = resolved;
          normalized.resolvedText = resolved;
          normalized.resolvedLen = getReceivedLength(normalized);
          return normalized;
        }).filter(Boolean);
      }
    } catch (err) {
      console.warn("[recv] не удалось разобрать ответ RSTS:", err);
    }
  }
  return trimmed.split(/\r?\n/)
                .map((line) => line.trim())
                .filter((line) => line.length > 0)
                .map((name) => {
                  const type = normalizeReceivedType(name, null);
                  return { name, type, text: "", hex: "", len: 0 };
                });
}
function renderReceivedList(items) {
  const hasList = !!UI.els.recvList;
  if (hasList) UI.els.recvList.innerHTML = "";
  const prev = UI.state.receivedKnown instanceof Set ? UI.state.receivedKnown : new Set();
  const next = new Set();
  const frag = hasList ? document.createDocumentFragment() : null;
  const list = Array.isArray(items) ? items : [];
  list.forEach((entry) => {
    const name = entry && entry.name ? String(entry.name).trim() : "";
    if (name) next.add(name);
    const isNew = name && !prev.has(name);
    if (hasList && frag) {
      const li = document.createElement("li");
      const entryType = normalizeReceivedType(name, entry && entry.type);
      li.classList.add("received-type-" + entryType);
      const body = document.createElement("div");
      body.className = "received-body";
      const textNode = document.createElement("div");
      const textValue = resolveReceivedText(entry);
      textNode.className = "received-text" + (textValue ? "" : " empty");
      textNode.textContent = textValue || "Без текста";
      body.appendChild(textNode);
      const meta = document.createElement("div");
      meta.className = "received-meta";
      const nameNode = document.createElement("span");
      nameNode.className = "received-name";
      nameNode.textContent = name || "—";
      meta.appendChild(nameNode);
      const lenValue = getReceivedLength(entry);
      if (lenValue && lenValue > 0) {
        const lenNode = document.createElement("span");
        lenNode.className = "received-length";
        lenNode.textContent = lenValue + " байт";
        meta.appendChild(lenNode);
      }
      body.appendChild(meta);
      li.appendChild(body);
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
      if (isNew) {
        li.classList.add("fresh");
        setTimeout(() => li.classList.remove("fresh"), 1600);
      }
      frag.appendChild(li);
    }
    logReceivedMessage(entry, { isNew });
  });
  if (hasList && frag) {
    UI.els.recvList.appendChild(frag);
  }
  UI.state.receivedKnown = next;
  if (UI.els.recvEmpty) UI.els.recvEmpty.hidden = list.length > 0;
}

// Полностью очищаем список входящих сообщений без запроса к устройству
function clearReceivedList() {
  UI.state.receivedKnown = new Set();
  if (UI.els.recvList) UI.els.recvList.innerHTML = "";
  if (UI.els.recvEmpty) UI.els.recvEmpty.hidden = false;
}

// Оцениваем количество элементов в JSON-ответе RSTS для подсказок пользователю
function countRstsJsonItems(value) {
  if (!value) return 0;
  let data = value;
  if (typeof value === "string") {
    try {
      data = JSON.parse(value);
    } catch (err) {
      console.warn("[debug] не удалось разобрать JSON RSTS:", err);
      return 0;
    }
  }
  if (Array.isArray(data)) return data.length;
  if (data && Array.isArray(data.items)) return data.items.length;
  return 0;
}

// Ручной запрос полного списка RSTS для вкладки Debug
async function requestRstsFullDebug() {
  debugLog("→ RSTS FULL (ручной запрос во вкладке Debug)");
  const text = await sendCommand("RSTS", { full: "1" }, { silent: true, timeoutMs: 4000, debugLabel: "RSTS FULL" });
  if (text === null) {
    note("RSTS FULL: ошибка запроса");
    return;
  }
  const entries = parseReceivedResponse(text);
  note("RSTS FULL: получено " + entries.length + " элементов");
}

// Ручной запрос JSON-версии списка RSTS для вкладки Debug
async function requestRstsJsonDebug() {
  const text = await sendCommand("RSTS", { json: "1" }, { silent: true, timeoutMs: 4000, debugLabel: "RSTS JSON" });
  if (text === null) {
    note("RSTS JSON: ошибка запроса");
    return;
  }
  const count = countRstsJsonItems(text);
  if (count > 0) note("RSTS JSON: получено " + count + " элементов");
  else note("RSTS JSON: ответ получен");
}

// Загрузка полного списка RSTS в формате JSON из вкладки Debug
async function downloadRstsFullJson() {
  debugLog("→ RSTS FULL (подготовка JSON для скачивания)");
  const text = await sendCommand(
    "RSTS",
    { full: "1", json: "1" },
    { silent: true, timeoutMs: 5000, debugLabel: "RSTS FULL JSON" }
  );
  if (text === null) {
    note("RSTS FULL JSON: ошибка запроса");
    return;
  }
  let parsed = null;
  try {
    parsed = JSON.parse(text);
  } catch (err) {
    console.warn("[debug] некорректный JSON RSTS FULL, выполняется разбор текстового ответа:", err);
  }
  let items = null;
  if (Array.isArray(parsed)) {
    items = parsed;
  } else if (parsed && Array.isArray(parsed.items)) {
    items = parsed.items;
  }
  if (!Array.isArray(items)) {
    const fallbackList = parseReceivedResponse(text);
    items = Array.isArray(fallbackList) ? fallbackList : [];
  }
  const normalized = items.map((item) => {
    const source = item && typeof item === "object" ? item : {};
    const name = source.name != null ? String(source.name) : "";
    const type = normalizeReceivedType(name, source.type);
    const textRaw = source.text != null ? String(source.text) : "";
    const resolvedRaw = source.resolvedText != null ? String(source.resolvedText) : "";
    const textValue = textRaw || resolvedRaw;
    const hex = source.hex != null ? String(source.hex) : "";
    let lenValue = null;
    if (source.len != null && source.len !== "") {
      const parsedLen = Number(source.len);
      if (Number.isFinite(parsedLen) && parsedLen >= 0) lenValue = parsedLen;
    }
    if (lenValue == null) {
      const measured = getReceivedLength(source);
      if (Number.isFinite(measured) && measured >= 0) lenValue = measured;
    }
    if (lenValue == null) {
      lenValue = textValue.length;
    }
    return { name, type, text: textValue, hex, len: lenValue };
  });
  const payload = {
    source: "RSTS FULL",
    generatedAt: new Date().toISOString(),
    items: normalized,
  };
  const pretty = JSON.stringify(payload, null, 2);
  const blob = new Blob([pretty], { type: "application/json" });
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a");
  const stamp = new Date().toISOString().replace(/[:.]/g, "-");
  a.href = url;
  a.download = "rsts-full-" + stamp + ".json";
  a.click();
  URL.revokeObjectURL(url);
  debugLog("RSTS FULL JSON сохранён (" + normalized.length + " элементов)");
  note("RSTS FULL JSON: файл сохранён");
}

// Добавляем отметку о принятом сообщении в чат
function logReceivedMessage(entry, opts) {
  if (!entry) return;
  const options = opts || {};
  const name = entry.name != null ? String(entry.name).trim() : "";
  const entryType = normalizeReceivedType(name, entry.type);
  const isGoPacket = name.toUpperCase().startsWith("GO-");
  if (!isGoPacket || entryType !== "ready") {
    // В чат попадают только финальные GO-пакеты.
    return;
  }
  const textValue = resolveReceivedText(entry);
  const fallbackTextRaw = entry.text != null ? String(entry.text) : "";
  const fallbackText = fallbackTextRaw.trim();
  const messageBody = textValue || fallbackText;
  const message = messageBody || "—";
  let length = getReceivedLength(entry);
  if (!Number.isFinite(length) || length < 0) length = null;
  if (length == null && messageBody) length = messageBody.length;
  const rxMeta = {
    name,
    type: entryType,
    hex: entry.hex || "",
    text: messageBody || "",
  };
  if (length != null) rxMeta.len = length;
  const history = getChatHistory();
  let existingIndex = -1;
  if (name && Array.isArray(history)) {
    existingIndex = history.findIndex((item) => item && item.tag === "rx-message" && item.rx && item.rx.name === name);
    if (existingIndex < 0) {
      const legacyMessage = "RX: " + name;
      existingIndex = history.findIndex((item) => item && item.tag === "rx-name" && typeof item.m === "string" && item.m.trim() === legacyMessage);
    }
  }
  if (existingIndex >= 0) {
    const existing = history[existingIndex];
    let changed = false;
    const desiredBody = message;
    const desiredMessage = desiredBody || "—";
    if (existing.m !== desiredMessage) {
      existing.m = desiredMessage;
      changed = true;
    }
    if (existing.tag !== "rx-message") {
      existing.tag = "rx-message";
      changed = true;
    }
    if (existing.role !== "rx") {
      existing.role = "rx";
      changed = true;
    }
    if (!existing.rx || typeof existing.rx !== "object") {
      existing.rx = { ...rxMeta };
      changed = true;
    } else {
      if (existing.rx.name !== name) {
        existing.rx.name = name;
        changed = true;
      }
      if (rxMeta.type && existing.rx.type !== rxMeta.type) {
        existing.rx.type = rxMeta.type;
        changed = true;
      }
      if (rxMeta.hex && existing.rx.hex !== rxMeta.hex) {
        existing.rx.hex = rxMeta.hex;
        changed = true;
      }
      if (rxMeta.len != null && existing.rx.len !== rxMeta.len) {
        existing.rx.len = rxMeta.len;
        changed = true;
      }
      if (rxMeta.len == null && existing.rx.len != null) {
        existing.rx.len = rxMeta.len;
        changed = true;
      }
      const nextText = rxMeta.text || "";
      if (existing.rx.text !== nextText) {
        existing.rx.text = nextText;
        changed = true;
      }
    }
    if (!existing.t) {
      existing.t = Date.now();
      changed = true;
    }
    if (changed) {
      saveChatHistory();
      updateChatMessageContent(existingIndex);
    }
    return;
  }
  if (options.isNew === false && !messageBody) {
    // Для старых записей без текста не добавляем дубль.
    return;
  }
  const meta = { role: "rx", tag: "rx-message", rx: rxMeta };
  const saved = persistChat(message, "dev", meta);
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

function getChannelTableColumnCount() {
  const table = $("#channelsTable");
  if (!table) return 0;
  const head = table.tHead && table.tHead.rows && table.tHead.rows[0];
  if (head) return head.cells.length;
  const firstBodyRow = table.tBodies && table.tBodies[0] && table.tBodies[0].rows && table.tBodies[0].rows[0];
  return firstBodyRow ? firstBodyRow.cells.length : 0;
}

function ensureChannelInfoRow() {
  let row = UI.els.channelInfoRow;
  if (!row) {
    row = document.createElement("tr");
    row.className = "channel-info-row";
    const cell = document.createElement("td");
    cell.className = "channel-info-cell";
    row.appendChild(cell);
    UI.els.channelInfoRow = row;
    UI.els.channelInfoCell = cell;
  }
  const cell = UI.els.channelInfoCell;
  if (cell) {
    const cols = getChannelTableColumnCount() || 7;
    cell.colSpan = cols;
  }
  return row;
}

function removeChannelInfoRow() {
  if (UI.els.channelInfoRow && UI.els.channelInfoRow.parentNode) {
    UI.els.channelInfoRow.parentNode.removeChild(UI.els.channelInfoRow);
  }
}

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
    removeChannelInfoRow();
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
  const freqValue = ref && ref.frequency != null ? ref.frequency : ref && ref.rx != null ? ref.rx : null;
  setChannelInfoText(fields.frequency, freqValue != null ? formatChannelNumber(freqValue, 3) : "—");
  setChannelInfoText(fields.system, ref && ref.system ? ref.system : "—");
  setChannelInfoText(fields.band, ref && ref.band ? ref.band : "—");
  setChannelInfoText(fields.bandwidth, ref && ref.bandwidth ? ref.bandwidth : "—");
  setChannelInfoText(fields.purpose, ref && ref.purpose ? ref.purpose : "—");
  setChannelInfoText(fields.satellite, ref && ref.satellite ? ref.satellite : "—");
  setChannelInfoText(fields.position, ref && ref.position ? ref.position : "—");
  setChannelInfoText(fields.modulation, ref && ref.modulation ? ref.modulation : "—");
  setChannelInfoText(fields.usage, ref && ref.usage ? ref.usage : "—");
  setChannelInfoText(fields.comments, ref && ref.comments ? ref.comments : "—");

  const messages = [];
  if (channelReference.loading) messages.push("Загружаем справочник частот…");
  if (channelReference.error) {
    const errText = describeChannelReferenceError(channelReference.error) || "неизвестная ошибка";
    messages.push("Не удалось загрузить справочник: " + errText);
  }
  if (!channelReference.loading && !channelReference.error && channelReference.source && channelReference.source.kind === "fallback") {
    const rawReason = channelReference.source.reason || describeChannelReferenceError(channelReference.lastError);
    const reason = rawReason ? rawReason.replace(/\s+/g, " ").trim() : "";
    messages.push("Используется встроенный справочник каналов." + (reason ? " Причина: " + reason : ""));
  }
  if (!channelReference.loading && !channelReference.error && !ref) messages.push("В справочнике нет данных для этого канала.");
  if (!actualEntry) messages.push("Канал отсутствует в текущем списке.");
  if (statusEl) {
    if (messages.length) {
      statusEl.textContent = messages.join(" ");
      statusEl.hidden = false;
    } else {
      statusEl.textContent = "";
      statusEl.hidden = true;
    }
  }

  const setBtn = UI.els.channelInfoSetBtn || $("#channelInfoSetCurrent");
  if (setBtn) {
    const same = UI.state.channel != null && UI.state.channel === UI.state.infoChannel;
    setBtn.disabled = !!same;
    setBtn.textContent = same ? "Канал уже активен" : "Установить текущим каналом";
  }

  const table = $("#channelsTable");
  const tbody = table && table.tBodies ? table.tBodies[0] : null;
  let targetRow = null;
  if (tbody) {
    const rows = Array.from(tbody.querySelectorAll("tr[data-ch]"));
    targetRow = rows.find((tr) => Number(tr.dataset.ch) === UI.state.infoChannel) || null;
  }
  if (targetRow && tbody) {
    const infoRow = ensureChannelInfoRow();
    const cell = UI.els.channelInfoCell;
    if (cell) {
      while (cell.firstChild) cell.removeChild(cell.firstChild);
      cell.appendChild(panel);
    }
    infoRow.hidden = false;
    tbody.insertBefore(infoRow, targetRow.nextSibling);
  } else {
    removeChannelInfoRow();
  }

  updateChannelInfoHighlight();
}

// Записываем значение в элемент, если он существует
function setChannelInfoText(el, text) {
  if (!el) return;
  const value = text == null ? "" : String(text);
  el.textContent = value;
  el.title = value && value !== "—" ? value : "";
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

// Преобразуем ошибку загрузки справочника в читаемую строку
function describeChannelReferenceError(error) {
  if (!error) return "";
  if (typeof error === "string") return error;
  if (typeof error.message === "string" && error.message.trim()) return error.message.trim();
  if (typeof error.status === "number") {
    const statusText = typeof error.statusText === "string" && error.statusText ? " " + error.statusText : "";
    return "HTTP " + error.status + statusText;
  }
  try {
    const serialized = JSON.stringify(error);
    return serialized && serialized !== "{}" ? serialized : String(error);
  } catch (err) {
    return String(error);
  }
}

// Обновляем индикатор состояния справочника каналов в UI
function updateChannelReferenceStatusIndicator() {
  const el = UI.els.channelRefStatus || $("#channelRefStatus");
  if (!el) return;
  if (channelReference.loading) {
    el.textContent = "⏳ Загружаем справочник частот…";
    el.setAttribute("data-state", "loading");
    el.hidden = false;
    return;
  }
  if (channelReference.error) {
    const errText = describeChannelReferenceError(channelReference.error) || "неизвестная ошибка";
    el.textContent = "⚠️ Справочник частот недоступен: " + errText;
    el.setAttribute("data-state", "error");
    el.hidden = false;
    return;
  }
  if (channelReference.source && channelReference.source.kind === "fallback") {
    const rawReason = channelReference.source.reason || describeChannelReferenceError(channelReference.lastError);
    const reason = rawReason ? rawReason.replace(/\s+/g, " ").trim() : "";
    const details = reason ? " Причина: " + reason : "";
    el.textContent = "⚠️ Используются встроенные данные справочника (fallback)." + details;
    el.setAttribute("data-state", "fallback");
    el.hidden = false;
    return;
  }
  el.hidden = true;
  el.textContent = "";
  el.removeAttribute("data-state");
}

// Загружаем CSV со справочной информацией
async function loadChannelReferenceData() {
  if (channelReference.ready) return channelReference.map;
  if (channelReference.promise) return channelReference.promise;
  channelReference.loading = true;
  channelReference.error = null;
  channelReference.source = null;
  channelReference.lastError = null;
  updateChannelReferenceStatusIndicator();
  channelReference.promise = (async () => {
    const sources = [];
    const seen = new Set();
    const addSource = (value) => {
      if (!value) return;
      const url = String(value);
      if (!url || seen.has(url)) return;
      seen.add(url);
      sources.push(url);
    };
    try {
      if (typeof document !== "undefined" && document.baseURI) {
        addSource(new URL("libs/freq-info.csv", document.baseURI).toString());
      }
    } catch (err) {
      console.warn("[freq-info] не удалось сформировать путь от baseURI:", err);
    }
    try {
      if (typeof document !== "undefined") {
        const currentScript = document.currentScript && document.currentScript.src ? document.currentScript.src : null;
        if (currentScript) addSource(new URL("./libs/freq-info.csv", currentScript).toString());
      }
    } catch (err) {
      console.warn("[freq-info] не удалось сформировать путь от script.src:", err);
    }
    addSource("libs/freq-info.csv");
    addSource("./libs/freq-info.csv");
    addSource("/libs/freq-info.csv");
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
          channelReference.source = { kind: "remote", url };
          channelReference.lastError = null;
          console.info("[freq-info] справочник загружен по адресу:", url);
          return channelReference.map;
        } catch (err) {
          lastErr = err;
          channelReference.lastError = err;
          const errText = describeChannelReferenceError(err);
          if (errText) console.warn("[freq-info] не удалось загрузить " + url + ": " + errText, err);
          else console.warn("[freq-info] не удалось загрузить " + url + ":", err);
        }
      }
      if (CHANNEL_REFERENCE_FALLBACK) {
        const reason = lastErr ? describeChannelReferenceError(lastErr) : "";
        if (lastErr) console.warn("[freq-info] основной источник недоступен, используем встроенные данные:", lastErr);
        applyChannelReferenceData(parseChannelReferenceCsv(CHANNEL_REFERENCE_FALLBACK));
        channelReference.ready = true;
        channelReference.error = null;
        channelReference.source = { kind: "fallback", reason };
        channelReference.lastError = lastErr;
        const msg = reason ? " Причина: " + reason : "";
        console.warn("[freq-info] используется резервный справочник частот (fallback)." + msg);
        return channelReference.map;
      }
      const error = lastErr || new Error("Не удалось загрузить справочник");
      channelReference.error = error;
      channelReference.source = null;
      channelReference.lastError = error;
      throw error;
    } finally {
      channelReference.loading = false;
      channelReference.promise = null;
      updateChannelReferenceStatusIndicator();
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
    const freqCell = cells[6] ? cells[6].trim() : "";
    const freq = freqCell ? Number(freqCell) : NaN;
    const bandwidth = cells[7] ? cells[7].trim() : "";
    const satellite = cells[8] ? cells[8].trim() : "";
    const position = cells[9] ? cells[9].trim() : "";
    const comments = cells[10] ? cells[10].trim() : "";
    const modulation = cells[11] ? cells[11].trim() : "";
    const usage = cells[12] ? cells[12].trim() : "";
    const entry = {
      ch,
      rx: Number.isFinite(rx) ? rx : null,
      tx: Number.isFinite(tx) ? tx : null,
      system: cells[3] ? cells[3].trim() : "",
      band: cells[4] ? cells[4].trim() : "",
      purpose: cells[5] ? cells[5].trim() : "",
      frequency: Number.isFinite(freq) ? freq : null,
      bandwidth,
      satellite,
      position,
      comments,
      modulation,
      usage,
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
    const cells = [
      { label: "Канал", value: c.ch != null ? c.ch : "—" },
      { label: "TX (МГц)", value: Number.isFinite(c.tx) ? c.tx.toFixed(3) : "—" },
      { label: "RX (МГц)", value: Number.isFinite(c.rx) ? c.rx.toFixed(3) : "—" },
      { label: "RSSI", value: Number.isFinite(c.rssi) ? String(c.rssi) : "" },
      { label: "SNR", value: Number.isFinite(c.snr) ? c.snr.toFixed(1) : "" },
      { label: "Статус", value: c.st || "" },
      { label: "SCAN", value: scanText },
    ];
    const ref = findChannelReferenceByTx(c);
    if (ref) {
      const tooltipLines = [];
      if (ref.frequency != null) tooltipLines.push("Частота: " + formatChannelNumber(ref.frequency, 3) + " МГц");
      if (ref.system) tooltipLines.push("Система: " + ref.system);
      if (ref.band) tooltipLines.push("План: " + ref.band);
      if (ref.bandwidth) tooltipLines.push("Полоса: " + ref.bandwidth);
      if (ref.purpose) tooltipLines.push("Назначение: " + ref.purpose);
      if (ref.satellite) tooltipLines.push("Спутник: " + ref.satellite);
      if (ref.position) tooltipLines.push("Позиция: " + ref.position);
      if (ref.modulation) tooltipLines.push("Модуляция: " + ref.modulation);
      if (ref.usage) tooltipLines.push("Использование: " + ref.usage);
      if (ref.comments) tooltipLines.push("Комментарий: " + ref.comments);
      const tooltip = tooltipLines.join("\n");
      if (tooltip) tr.title = tooltip;
      else tr.removeAttribute("title");
    } else {
      tr.removeAttribute("title");
    }
    cells.forEach((info) => {
      const td = document.createElement("td");
      td.dataset.label = info.label;
      td.textContent = info.value != null ? String(info.value) : "";
      td.title = info.value != null && info.value !== "" ? String(info.value) : "";
      tr.appendChild(td);
    });
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
  const now = new Date();
  const pad = (value) => String(value).padStart(2, "0");
  const datePart = [now.getFullYear(), pad(now.getMonth() + 1), pad(now.getDate())].join("-");
  const timePart = [pad(now.getHours()), pad(now.getMinutes()), pad(now.getSeconds())].join("-");
  const foundChannelsCount = channels.reduce((total, channel) => {
    if (channel.scanState === "signal") return total + 1;
    const scanText = typeof channel.scan === "string" ? channel.scan.trim() : "";
    if (!channel.scanState && scanText && !/^err/i.test(scanText)) return total + 1;
    return total;
  }, 0);
  // Имя файла: дата_время_количество найденных каналов
  const fileName = `${datePart}_${timePart}_${foundChannelsCount}.csv`;
  a.download = fileName;
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

function setKeyReceiveWaiting(active) {
  const btn = UI.els.keyRecvBtn || $("#btnKeyRecv");
  if (!btn) return;
  btn.classList.toggle("waiting", active);
  btn.disabled = active;
  if (active) btn.setAttribute("aria-busy", "true");
  else btn.removeAttribute("aria-busy");
}

async function requestKeyReceive() {
  status("→ KEYTRANSFER RECEIVE");
  setKeyReceiveWaiting(true);
  UI.key.lastMessage = "Ожидание ключа по LoRa";
  renderKeyState(UI.key.state);
  debugLog("KEYTRANSFER RECEIVE → ожидание ключа");
  try {
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
  } finally {
    setKeyReceiveWaiting(false);
  }
}

/* Наведение антенны (вкладка Pointing) */
function initPointingTab() {
  const els = UI.els.pointing;
  const state = UI.state.pointing;
  if (!els || !els.tab) return;
  if (els.mgrsInput && !els.mgrsInput.dataset.defaultPlaceholder) {
    els.mgrsInput.dataset.defaultPlaceholder = els.mgrsInput.getAttribute("placeholder") || "";
  }

  state.minElevation = POINTING_DEFAULT_MIN_ELEVATION;
  updatePointingMinElevationUi();

  const rawTle = Array.isArray(window.SAT_TLE_DATA) ? window.SAT_TLE_DATA : [];
  if (!rawTle.length) {
    console.warn("[pointing] нет данных TLE — список спутников будет пустым");
  }
  state.satellites = parsePointingSatellites(rawTle);
  state.tleReady = state.satellites.length > 0;
  state.tleError = state.tleReady ? null : (rawTle.length ? "Некорректные TLE" : "Нет данных TLE");

  const storedMgrsRaw = storage.get("pointingMgrs");
  const storedAltRaw = Number(storage.get("pointingAlt"));
  const hasStoredAlt = Number.isFinite(storedAltRaw);
  const mgrsApi = pointingMgrsApi();
  if (!mgrsApi) {
    state.mgrsReady = false;
    state.mgrsError = "Не удалось загрузить /libs/mgrs.js";
    state.observerMgrs = storedMgrsRaw ? pointingFormatMgrs(storedMgrsRaw) : null;
    state.observer = hasStoredAlt ? {
      lat: null,
      lon: null,
      heightM: storedAltRaw,
      mgrs: state.observerMgrs,
      mgrsRaw: storedMgrsRaw || null,
      source: "mgrs",
    } : null;
    if (els.mgrsInput) {
      els.mgrsInput.disabled = true;
      const fallbackPlaceholder = state.mgrsError || "Конвертер MGRS не загружен";
      els.mgrsInput.value = state.observerMgrs || "";
      els.mgrsInput.placeholder = fallbackPlaceholder;
    }
    if (els.mgrsApply) {
      els.mgrsApply.disabled = true;
      els.mgrsApply.title = state.mgrsError;
    }
    console.error("[pointing] " + state.mgrsError + " — проверьте отдачу файла");
    note("MGRS недоступен: проверьте /libs/mgrs.js");
    status("Позиция • ошибка загрузки MGRS");
  } else {
    state.mgrsReady = true;
    state.mgrsError = null;
    if (els.mgrsInput) {
      els.mgrsInput.disabled = false;
      const basePlaceholder = els.mgrsInput.dataset.defaultPlaceholder || "43U CR";
      els.mgrsInput.placeholder = basePlaceholder;
    }
    if (els.mgrsApply) {
      els.mgrsApply.disabled = false;
      els.mgrsApply.removeAttribute("title");
    }
    const storedMgrs = pointingCanonicalMgrs(storedMgrsRaw) || "43UCR";
    const initialAlt = hasStoredAlt ? storedAltRaw : 0;
    if (!setPointingObserverFromMgrs(storedMgrs, initialAlt, { silent: true, skipStore: true })) {
      setPointingObserverFromMgrs("43UCR", 0, { silent: true, skipStore: true });
    }
  }
  updatePointingObserverUi();
  renderPointingSatellites();
  updatePointingSatDetails(getPointingSelectedSatellite());
  updatePointingBadges();

  if (els.mgrsApply) {
    els.mgrsApply.addEventListener("click", (event) => {
      event.preventDefault();
      applyPointingMgrs();
    });
  }
  if (els.mgrsInput) {
    els.mgrsInput.addEventListener("keydown", (event) => {
      if (event && event.key === "Enter") {
        event.preventDefault();
        applyPointingMgrs();
      }
    });
    els.mgrsInput.addEventListener("blur", () => {
      updatePointingObserverUi();
    });
  }
  if (els.manualAlt) {
    const commitAlt = () => applyPointingAltitude();
    els.manualAlt.addEventListener("change", commitAlt);
    els.manualAlt.addEventListener("blur", commitAlt);
    els.manualAlt.addEventListener("keydown", (event) => {
      if (event && event.key === "Enter") {
        event.preventDefault();
        commitAlt();
      }
    });
  }
  if (els.satSelect) {
    els.satSelect.addEventListener("change", (event) => {
      const value = event && event.target ? event.target.value : "";
      setPointingActiveSatellite(value || null);
    });
  }
  if (els.satList) {
    els.satList.addEventListener("click", (event) => {
      const btn = event && event.target ? event.target.closest("button[data-sat-id]") : null;
      if (!btn || !btn.dataset) return;
      setPointingActiveSatellite(btn.dataset.satId || null);
    });
  }
  if (els.horizon) {
    els.horizon.addEventListener("click", (event) => {
      const marker = event && event.target ? event.target.closest("button[data-sat-id]") : null;
      if (!marker || !marker.dataset) return;
      setPointingActiveSatellite(marker.dataset.satId || null);
    });
  }
  if (els.compass) {
    els.compass.addEventListener("click", (event) => {
      const dot = event && event.target ? event.target.closest("button[data-sat-id]") : null;
      if (!dot || !dot.dataset) return;
      setPointingActiveSatellite(dot.dataset.satId || null);
    });
  }
  if (els.minElSlider) {
    const handler = (event) => onPointingMinElevationChange(event);
    els.minElSlider.addEventListener("input", handler);
    els.minElSlider.addEventListener("change", handler);
  }
  updatePointingCompassLegend();
}

function parsePointingSatellites(rawData) {
  const list = [];
  if (!Array.isArray(rawData)) return list;
  for (const entry of rawData) {
    const parsed = parsePointingTle(entry);
    if (parsed) list.push(parsed);
  }
  return list;
}

function parsePointingTle(entry) {
  if (!entry || !entry.line1 || !entry.line2) return null;
  try {
    const line1 = entry.line1;
    const line2 = entry.line2;
    const epochYearRaw = line1.slice(18, 20);
    const epochDayRaw = line1.slice(20, 32);
    const epochYear = parseInt(epochYearRaw, 10);
    const dayOfYear = Number(epochDayRaw.trim());
    if (!Number.isFinite(epochYear) || !Number.isFinite(dayOfYear)) return null;
    const fullYear = epochYear < 57 ? 2000 + epochYear : 1900 + epochYear;
    const epoch = tleDayToDate(fullYear, dayOfYear);

    const inclination = Number(line2.slice(8, 16).trim());
    const raan = Number(line2.slice(17, 25).trim());
    const eccRaw = line2.slice(26, 33).replace(/\s+/g, "");
    const eccentricity = eccRaw ? Number("0." + eccRaw) : 0;
    const argPerigee = Number(line2.slice(34, 42).trim());
    const meanAnomaly = Number(line2.slice(43, 51).trim());
    const meanMotion = Number(line2.slice(52, 63).trim());
    if (!Number.isFinite(meanMotion) || meanMotion <= 0) return null;

    const meanMotionRad = meanMotion * TWO_PI / 86400; // рад/с
    const semiMajorAxis = Math.cbrt(MU_EARTH / (meanMotionRad * meanMotionRad));

    const rev = line2.slice(63, 68).trim();
    return {
      id: String(entry.name || "SAT") + "|" + rev,
      name: entry.name || "SATELLITE",
      line1,
      line2,
      epoch,
      inclination,
      raan,
      eccentricity,
      argPerigee,
      meanAnomaly,
      meanMotion,
      meanMotionRad,
      semiMajorAxis,
    };
  } catch (err) {
    console.warn("[pointing] не удалось разобрать TLE", err, entry);
    return null;
  }
}

function tleDayToDate(year, dayOfYear) {
  const clampedDay = Math.max(1, dayOfYear);
  const base = Date.UTC(year, 0, 1);
  const ms = (clampedDay - 1) * 86400000;
  return new Date(base + ms);
}

function onPointingMinElevationChange(event) {
  const state = UI.state.pointing;
  const slider = event && event.target ? event.target : null;
  if (!slider) return;
  const value = Number(slider.value);
  if (!Number.isFinite(value)) return;
  state.minElevation = clampNumber(value, 0, 90);
  updatePointingMinElevationUi();
  updatePointingSatellites();
}

function updatePointingMinElevationUi() {
  const state = UI.state.pointing;
  const els = UI.els.pointing;
  if (!els) return;
  if (els.minElSlider) {
    els.minElSlider.value = String(state.minElevation);
  }
  if (els.minElValue) {
    els.minElValue.textContent = formatDegrees(state.minElevation, 0);
  }
}

// Поддержка ручного ввода координат в формате MGRS (100 км).
function pointingMgrsApi() {
  if (typeof window === "undefined") return null;
  const api = window.satMgrs;
  if (!api || typeof api.toLatLon100k !== "function") return null;
  return api;
}

function pointingFormatMgrs(text) {
  if (!text) return "—";
  const normalized = String(text).toUpperCase();
  const match = normalized.match(/^(\d{1,2}[C-HJ-NP-X])([A-HJ-NP-Z]{2})$/);
  if (!match) return normalized;
  return match[1] + " " + match[2];
}

function pointingCanonicalMgrs(value) {
  const api = pointingMgrsApi();
  if (!api || typeof api.normalize100k !== "function") return null;
  const normalized = api.normalize100k(value);
  return normalized ? normalized.text : null;
}

function pointingObserverFromMgrs(value, altitude) {
  const api = pointingMgrsApi();
  if (!api) return null;
  const solved = api.toLatLon100k(value);
  if (!solved) return null;
  const altNumber = Number(altitude);
  const heightM = Number.isFinite(altNumber) ? altNumber : 0;
  return {
    lat: solved.lat,
    lon: normalizeDegreesSigned(solved.lon),
    heightM,
    accuracy: null,
    source: "mgrs",
    timestamp: new Date(),
    mgrs: pointingFormatMgrs(solved.text),
    mgrsRaw: solved.text,
  };
}

function setPointingObserverFromMgrs(value, altitude, options = {}) {
  const state = UI.state.pointing;
  if (state.mgrsReady === false) {
    if (!options.silent) note("Конвертер MGRS ещё не загружен");
    return false;
  }
  const observer = pointingObserverFromMgrs(value, altitude);
  if (!observer) {
    if (!options.silent) note("Не удалось разобрать координаты MGRS");
    return false;
  }
  state.observer = observer;
  state.observerMgrs = observer.mgrs;
  if (!options.skipStore) {
    if (observer.mgrsRaw) storage.set("pointingMgrs", observer.mgrsRaw);
    storage.set("pointingAlt", String(observer.heightM || 0));
  }
  updatePointingObserverUi();
  updatePointingSatellites();
  if (!options.silent) {
    note("Координаты обновлены по MGRS");
  }
  return true;
}

function setPointingObserverAltitude(height, options = {}) {
  const state = UI.state.pointing;
  const observer = state && state.observer;
  if (!observer) {
    if (!options.silent) note("Сначала укажите квадрат MGRS");
    return false;
  }
  const altNumber = Number(height);
  const next = Number.isFinite(altNumber) ? altNumber : 0;
  const prev = Number.isFinite(observer.heightM) ? observer.heightM : 0;
  if (Math.abs(prev - next) < 0.01) {
    storage.set("pointingAlt", String(next || 0));
    updatePointingObserverUi();
    return true;
  }
  observer.heightM = next;
  storage.set("pointingAlt", String(next || 0));
  updatePointingObserverUi();
  updatePointingSatellites();
  if (!options.silent) note("Высота обновлена");
  return true;
}

function updatePointingObserverUi() {
  const els = UI.els.pointing;
  if (!els) return;
  const state = UI.state.pointing;
  const observer = state.observer;
  const errorText = state.mgrsError;
  const displayMgrs = errorText || state.observerMgrs || (observer && observer.mgrs) || "—";

  if (els.observerLabel) {
    els.observerLabel.textContent = displayMgrs;
  }
  if (els.mgrsInput) {
    const defaultPlaceholder = els.mgrsInput.dataset.defaultPlaceholder || "43U CR";
    if (document.activeElement !== els.mgrsInput) {
      if (errorText) {
        els.mgrsInput.value = state.observerMgrs || "";
      } else {
        const nextValue = observer ? (observer.mgrs || state.observerMgrs || "") : (state.observerMgrs || "");
        els.mgrsInput.value = nextValue;
      }
    }
    els.mgrsInput.placeholder = errorText || defaultPlaceholder;
    els.mgrsInput.disabled = state.mgrsReady === false;
  }
  if (els.manualAlt) {
    if (document.activeElement !== els.manualAlt) {
      els.manualAlt.value = observer ? String(observer.heightM || 0) : "";
    }
    els.manualAlt.disabled = state.mgrsReady === false;
  }
  if (els.mgrsApply) {
    els.mgrsApply.disabled = state.mgrsReady === false;
    if (state.mgrsReady === false) {
      els.mgrsApply.title = errorText || "Конвертер MGRS не загружен";
    } else {
      els.mgrsApply.removeAttribute("title");
    }
  }
  if (els.lat) {
    els.lat.textContent = observer ? formatLatitude(observer.lat, 4) : "—";
  }
  if (els.lon) {
    els.lon.textContent = observer ? formatLongitude(observer.lon, 4) : "—";
  }
  if (els.alt) {
    els.alt.textContent = observer ? formatMeters(observer.heightM) : "—";
  }
  updatePointingBadges();
}

function applyPointingMgrs() {
  const els = UI.els.pointing;
  if (!els) return;
  const state = UI.state.pointing;
  if (state.mgrsReady === false) {
    note("Конвертер MGRS ещё не загружен");
    return;
  }
  const inputValue = els.mgrsInput ? els.mgrsInput.value : "";
  let normalized = pointingCanonicalMgrs(inputValue);
  if (!normalized) {
    const fallback = state && state.observer ? (state.observer.mgrsRaw || state.observerMgrs) : null;
    normalized = pointingCanonicalMgrs(fallback);
    if (!normalized) {
      note("Введите квадрат MGRS вида 43U CR");
      return;
    }
  }
  const rawAlt = els.manualAlt ? String(els.manualAlt.value || "").trim() : "";
  let altValue;
  if (!rawAlt) {
    altValue = state && state.observer && Number.isFinite(state.observer.heightM) ? state.observer.heightM : 0;
  } else {
    const parsed = Number(rawAlt.replace(",", "."));
    if (!Number.isFinite(parsed)) {
      note("Высота должна быть числом");
      return;
    }
    altValue = parsed;
  }
  setPointingObserverFromMgrs(normalized, altValue, { silent: false });
}

function applyPointingAltitude() {
  const els = UI.els.pointing;
  if (!els) return;
  const state = UI.state.pointing;
  if (state.mgrsReady === false) {
    note("Сначала дождитесь загрузки конвертера MGRS");
    return;
  }
  const raw = els.manualAlt ? String(els.manualAlt.value || "").trim() : "";
  let value = 0;
  if (!raw) {
    value = state && state.observer && Number.isFinite(state.observer.heightM) ? state.observer.heightM : 0;
  } else {
    const parsed = Number(raw.replace(",", "."));
    if (!Number.isFinite(parsed)) {
      note("Высота должна быть числом");
      return;
    }
    value = parsed;
  }
  setPointingObserverAltitude(value, { silent: !raw });
}

function updatePointingSatellites() {
  const state = UI.state.pointing;
  const observer = state.observer;
  if (!observer || !Number.isFinite(observer.lat) || !Number.isFinite(observer.lon)) {
    state.visible = [];
    state.selectedSatId = null;
    renderPointingSatellites();
    updatePointingSatDetails(null);
    return;
  }

  const now = new Date();
  const gmst = computeGmst(now);
  const latRad = observer.lat * DEG2RAD;
  const lonRad = observer.lon * DEG2RAD;
  const sinLat = Math.sin(latRad);
  const cosLat = Math.cos(latRad);
  const sinLon = Math.sin(lonRad);
  const cosLon = Math.cos(lonRad);
  const observerRadius = EARTH_RADIUS_KM + (observer.heightM || 0) / 1000;
  const obsX = observerRadius * cosLat * cosLon;
  const obsY = observerRadius * cosLat * sinLon;
  const obsZ = observerRadius * sinLat;

  const visible = [];
  for (const sat of state.satellites) {
    const propagated = propagatePointingSatellite(sat, now, gmst);
    if (!propagated) continue;
    const { xEcef, yEcef, zEcef, subLon, subLat, radius } = propagated;

    const dx = xEcef - obsX;
    const dy = yEcef - obsY;
    const dz = zEcef - obsZ;

    const east = -sinLon * dx + cosLon * dy;
    const north = -sinLat * cosLon * dx - sinLat * sinLon * dy + cosLat * dz;
    const up = cosLat * cosLon * dx + cosLat * sinLon * dy + sinLat * dz;

    const azimuth = normalizeDegrees(Math.atan2(east, north) * RAD2DEG);
    const elevation = Math.atan2(up, Math.hypot(east, north)) * RAD2DEG;
    const rangeKm = Math.sqrt(dx * dx + dy * dy + dz * dz);
    const altitudeKm = radius - EARTH_RADIUS_KM;

    visible.push({
      id: sat.id,
      name: sat.name,
      azimuth,
      elevation,
      rangeKm,
      subLonDeg: subLon * RAD2DEG,
      subLatDeg: subLat * RAD2DEG,
      altitudeKm,
    });
  }

  const minEl = state.minElevation != null ? state.minElevation : POINTING_DEFAULT_MIN_ELEVATION;
  const filtered = visible.filter((sat) => sat.elevation >= minEl);
  filtered.sort((a, b) => b.elevation - a.elevation);
  state.visible = filtered;

  if (!filtered.some((sat) => sat.id === state.selectedSatId)) {
    state.selectedSatId = filtered.length ? filtered[0].id : null;
  }

  renderPointingSatellites();
  updatePointingSatDetails(getPointingSelectedSatellite());
}

function propagatePointingSatellite(sat, date, gmst) {
  if (!sat) return null;
  const deltaDays = (date.getTime() - sat.epoch.getTime()) / 86400000;
  const meanAnomalyDeg = sat.meanAnomaly + sat.meanMotion * 360 * deltaDays;
  const meanAnomalyRad = meanAnomalyDeg * DEG2RAD;
  const e = sat.eccentricity || 0;
  const E = solveKepler(meanAnomalyRad, e);
  const cosE = Math.cos(E);
  const sinE = Math.sin(E);
  const r = sat.semiMajorAxis * (1 - e * cosE);
  const nu = Math.atan2(Math.sqrt(1 - e * e) * sinE, cosE - e);

  const argPerigeeRad = sat.argPerigee * DEG2RAD;
  const inclinationRad = sat.inclination * DEG2RAD;
  const raanRad = sat.raan * DEG2RAD;
  const u = argPerigeeRad + nu;

  const cosRAAN = Math.cos(raanRad);
  const sinRAAN = Math.sin(raanRad);
  const cosInc = Math.cos(inclinationRad);
  const sinInc = Math.sin(inclinationRad);
  const cosU = Math.cos(u);
  const sinU = Math.sin(u);

  const xEci = r * (cosRAAN * cosU - sinRAAN * sinU * cosInc);
  const yEci = r * (sinRAAN * cosU + cosRAAN * sinU * cosInc);
  const zEci = r * (sinU * sinInc);

  const cosG = Math.cos(gmst);
  const sinG = Math.sin(gmst);
  const xEcef = xEci * cosG + yEci * sinG;
  const yEcef = -xEci * sinG + yEci * cosG;
  const zEcef = zEci;

  const subLon = normalizeRadians(Math.atan2(yEcef, xEcef));
  const subLat = Math.atan2(zEcef, Math.sqrt(xEcef * xEcef + yEcef * yEcef));

  return { xEcef, yEcef, zEcef, subLon, subLat, radius: r };
}

function computeGmst(date) {
  const jd = dateToJulian(date);
  const t = (jd - 2451545.0) / 36525.0;
  let gmst = 280.46061837 + 360.98564736629 * (jd - 2451545.0) + 0.000387933 * t * t - t * t * t / 38710000;
  gmst = normalizeDegrees(gmst);
  return gmst * DEG2RAD;
}

function dateToJulian(date) {
  return date.getTime() / 86400000 + 2440587.5;
}

function renderPointingSatellites() {
  const state = UI.state.pointing;
  const els = UI.els.pointing;
  if (!els) return;
  const visible = state.visible || [];
  const observer = state.observer;

  if (els.satSummary) {
    if (!state.tleReady) {
      els.satSummary.textContent = "Данные TLE не загружены — список спутников недоступен.";
    } else if (!observer) {
      els.satSummary.textContent = "Укажите координаты, чтобы увидеть доступные спутники.";
    } else if (!visible.length) {
      els.satSummary.textContent = "Видимых спутников нет (порог " + formatDegrees(state.minElevation, 0) + ").";
    } else {
      const maxEl = visible[0].elevation;
      const minEl = visible[visible.length - 1].elevation;
      els.satSummary.textContent = "Найдено " + visible.length + " спутника(ов). Возвышение от " +
        formatDegrees(minEl, 1) + " до " + formatDegrees(maxEl, 1) + ".";
    }
  }

  if (els.satSelect) {
    els.satSelect.innerHTML = "";
    if (!visible.length) {
      const opt = document.createElement("option");
      opt.value = "";
      opt.textContent = "Нет спутников";
      opt.selected = true;
      opt.disabled = true;
      els.satSelect.appendChild(opt);
    } else {
      for (const sat of visible) {
        const opt = document.createElement("option");
        opt.value = sat.id;
        opt.textContent = sat.name + " (" + formatDegrees(sat.elevation, 1) + ")";
        if (sat.id === state.selectedSatId) opt.selected = true;
        els.satSelect.appendChild(opt);
      }
    }
  }

  if (els.satList) {
    els.satList.innerHTML = "";
    if (!visible.length) {
      const empty = document.createElement("div");
      empty.className = "pointing-empty small muted";
      empty.textContent = observer ? "Спутники ниже заданного порога возвышения." : "Нет данных для отображения.";
      els.satList.appendChild(empty);
    } else {
      const groups = new Map();
      for (const sat of visible) {
        const colorInfo = pointingColorForSat(sat);
        const quadrant = colorInfo.quadrant || "unknown";
        if (!groups.has(quadrant)) {
          groups.set(quadrant, []);
        }
        groups.get(quadrant).push({ sat, color: colorInfo.color });
      }
      const order = ["north", "east", "south", "west", "unknown"];
      const handled = new Set();
      const processGroup = (quadrant) => {
        const entries = groups.get(quadrant);
        if (!entries || !entries.length) return;
        handled.add(quadrant);
        const groupSection = document.createElement("section");
        groupSection.className = "pointing-sat-group";
        groupSection.dataset.quadrant = quadrant;
        const groupColor = pointingColorForQuadrant(quadrant);
        if (groupColor) {
          groupSection.style.setProperty("--pointing-sat-color", groupColor);
        }
        const title = document.createElement("div");
        title.className = "pointing-sat-group-title";
        title.textContent = pointingQuadrantLabel(quadrant);
        groupSection.appendChild(title);
        const list = document.createElement("div");
        list.className = "pointing-sat-group-list";
        for (const entry of entries) {
          const { sat, color } = entry;
          const btn = document.createElement("button");
          btn.type = "button";
          btn.dataset.satId = sat.id;
          btn.className = "pointing-sat-entry" + (sat.id === state.selectedSatId ? " active" : "");
          if (color) {
            btn.style.setProperty("--pointing-sat-color", color);
          }
          if (quadrant && quadrant !== "unknown") {
            btn.dataset.quadrant = quadrant;
          } else {
            delete btn.dataset.quadrant;
          }

          const nameEl = document.createElement("div");
          nameEl.className = "pointing-sat-name";
          nameEl.textContent = sat.name;
          btn.appendChild(nameEl);

          const metaEl = document.createElement("div");
          metaEl.className = "pointing-sat-meta";

          const azEl = document.createElement("span");
          azEl.textContent = "Азимут " + formatDegrees(sat.azimuth, 1);
          metaEl.appendChild(azEl);

          const elEl = document.createElement("span");
          elEl.textContent = "Возвышение " + formatDegrees(sat.elevation, 1);
          metaEl.appendChild(elEl);

          const lonEl = document.createElement("span");
          lonEl.textContent = "Долгота " + formatLongitude(sat.subLonDeg, 1);
          metaEl.appendChild(lonEl);

          btn.appendChild(metaEl);
          list.appendChild(btn);
        }
        groupSection.appendChild(list);
        els.satList.appendChild(groupSection);
      };
      for (const quadrant of order) {
        processGroup(quadrant);
      }
      for (const [quadrant] of groups) {
        if (handled.has(quadrant)) continue;
        processGroup(quadrant);
      }
    }
  }
  renderPointingCompassRadar(visible);
  renderPointingHorizon(visible);
  updatePointingBadges();
}

function renderPointingHorizon(visible) {
  const els = UI.els.pointing;
  if (!els || !els.horizonTrack) return;
  const state = UI.state.pointing;
  els.horizonTrack.innerHTML = "";
  if (!Array.isArray(visible) || !visible.length) {
    if (els.horizonEmpty) {
      els.horizonEmpty.hidden = false;
      if (!state.tleReady) {
        els.horizonEmpty.textContent = "Ожидание загрузки TLE.";
      } else if (!state.observer) {
        els.horizonEmpty.textContent = "Укажите координаты для визуализации спутников.";
      } else {
        els.horizonEmpty.textContent = "Спутники ниже выбранного порога возвышения.";
      }
    }
    return;
  }
  if (els.horizonEmpty) {
    els.horizonEmpty.hidden = true;
  }
  const sorted = [...visible].sort((a, b) => a.azimuth - b.azimuth);
  const laneLast = [];
  const minGap = 6; // минимальный зазор между маркерами с учётом уменьшенного размера
  for (const sat of sorted) {
    const marker = document.createElement("button");
    marker.type = "button";
    marker.dataset.satId = sat.id;
    marker.className = "pointing-horizon-sat" + (sat.id === state.selectedSatId ? " active" : "");
    const leftPercent = clampNumber(sat.azimuth / 360, 0, 1) * 100;
    marker.style.left = leftPercent + "%";
    let laneIndex = 0;
    while (laneLast[laneIndex] != null && leftPercent - laneLast[laneIndex] < minGap) {
      laneIndex += 1;
    }
    laneLast[laneIndex] = leftPercent;
    const colorInfo = pointingColorForSat(sat);
    marker.style.setProperty("--pointing-sat-offset", String(laneIndex));
    marker.style.zIndex = String(10 + laneIndex);
    marker.style.setProperty("--pointing-sat-color", colorInfo.color);
    if (colorInfo.quadrant) {
      marker.dataset.quadrant = colorInfo.quadrant;
    } else {
      delete marker.dataset.quadrant;
    }
    marker.title = sat.name + " — азимут " + formatDegrees(sat.azimuth, 1) + ", возвышение " + formatDegrees(sat.elevation, 1);
    const label = document.createElement("span");
    label.className = "pointing-horizon-label";
    label.textContent = sat.name + " • " + formatDegrees(sat.azimuth, 0) + "/" + formatDegrees(sat.elevation, 0);
    marker.appendChild(label);
    els.horizonTrack.appendChild(marker);
  }
}

function renderPointingCompassRadar(visible) {
  const els = UI.els.pointing;
  if (!els || !els.compassRadar) return;
  const state = UI.state.pointing;
  const container = els.compassRadar;
  container.innerHTML = "";
  resetCompassDots();
  if (!Array.isArray(visible) || !visible.length) return;
  const sorted = [...visible].sort((a, b) => a.azimuth - b.azimuth);
  for (const sat of sorted) {
    const dot = document.createElement("button");
    dot.type = "button";
    dot.dataset.satId = sat.id;
    dot.className = "pointing-compass-sat" + (sat.id === state.selectedSatId ? " active" : "");
    const colorInfo = pointingColorForSat(sat);
    dot.style.setProperty("--pointing-sat-color", colorInfo.color);
    if (colorInfo.quadrant) {
      dot.dataset.quadrant = colorInfo.quadrant;
    } else {
      delete dot.dataset.quadrant;
    }
    const displayAngle = normalizeDegrees((Number.isFinite(sat.azimuth) ? sat.azimuth : 0) + POINTING_COMPASS_OFFSET_DEG);
    const angleRad = displayAngle * DEG2RAD;
    const elevationClamped = clampNumber(sat.elevation, 0, 90);
    const baseRadius = (1 - elevationClamped / 90) * 45;
    const placed = placeCompassDot(angleRad, baseRadius);
    const x = placed.x;
    const y = placed.y;
    dot.style.left = x + "%";
    dot.style.top = y + "%";
    dot.style.zIndex = String(50 + Math.round(60 - baseRadius));
    const description = sat.name + " — азимут " + formatDegrees(sat.azimuth, 1) + ", возвышение " + formatDegrees(sat.elevation, 1);
    dot.title = description;
    dot.setAttribute("aria-label", description);
    const label = document.createElement("span");
    label.className = "pointing-compass-label";
    label.textContent = sat.name + " • " + formatDegrees(sat.azimuth, 0) + "/" + formatDegrees(sat.elevation, 0);
    label.setAttribute("aria-hidden", "true");
    dot.appendChild(label);
    container.appendChild(dot);
  }
}

// Минимизируем перекрытие маркеров на радаре: сдвигаем их ближе к центру при конфликте.
const compassPlacedDots = [];

function placeCompassDot(angleRad, baseRadius) {
  const maxAttempts = 4;
  let radius = baseRadius;
  for (let attempt = 0; attempt < maxAttempts; attempt += 1) {
    const x = 50 + Math.sin(angleRad) * radius;
    const y = 50 - Math.cos(angleRad) * radius;
    const collision = compassPlacedDots.some((pt) => Math.hypot(pt.x - x, pt.y - y) < 5);
    if (!collision) {
      compassPlacedDots.push({ x, y });
      return { x, y };
    }
    radius = Math.max(4, radius - 5);
  }
  const fallbackX = 50 + Math.sin(angleRad) * baseRadius;
  const fallbackY = 50 - Math.cos(angleRad) * baseRadius;
  compassPlacedDots.push({ x: fallbackX, y: fallbackY });
  return { x: fallbackX, y: fallbackY };
}

function resetCompassDots() {
  compassPlacedDots.length = 0;
}

// Подбор цвета маркера с учётом квадранта и возвышения: так проще визуально различать группы спутников.
const POINTING_QUADRANT_META = {
  north: { hue: 205, label: "Север", short: "N", order: 0 },
  east: { hue: 130, label: "Восток", short: "E", order: 1 },
  south: { hue: 25, label: "Юг", short: "S", order: 2 },
  west: { hue: 285, label: "Запад", short: "W", order: 3 },
  unknown: { hue: 205, label: "Прочие", short: "?", order: 99 },
};

function pointingQuadrantMeta(quadrant) {
  return POINTING_QUADRANT_META[quadrant] || POINTING_QUADRANT_META.unknown;
}

function pointingColorFromHue(hue, elevation) {
  const normalized = clampNumber(((elevation || 0) + 5) / 65, 0, 1);
  const saturation = 78;
  const lightness = 64 - normalized * 20;
  return "hsl(" + Math.round(hue) + ", " + saturation + "%, " + Math.round(lightness) + "%)";
}

function pointingColorForSat(sat) {
  const elevation = sat && Number.isFinite(sat.elevation) ? sat.elevation : 0;
  const quadrant = pointingAzimuthToQuadrant(sat ? sat.azimuth : null);
  const meta = pointingQuadrantMeta(quadrant);
  return {
    color: pointingColorFromHue(meta.hue, elevation),
    quadrant,
  };
}

function pointingColorForQuadrant(quadrant) {
  const meta = pointingQuadrantMeta(quadrant);
  return pointingColorFromHue(meta.hue, 30);
}

function pointingQuadrantLabel(quadrant) {
  const meta = pointingQuadrantMeta(quadrant);
  if (quadrant === "unknown") {
    return "Прочие направления";
  }
  return meta.label + " • " + meta.short;
}

function updatePointingCompassLegend() {
  const els = UI.els.pointing;
  if (!els || !els.compassLegend) return;
  const entries = els.compassLegend.querySelectorAll("[data-quadrant]");
  entries.forEach((entry) => {
    const quadrant = entry.dataset.quadrant || "unknown";
    const color = pointingColorForQuadrant(quadrant);
    if (color) {
      entry.style.setProperty("--pointing-sat-color", color);
    }
  });
}

// Определение квадранта компаса по азимуту (С/В/Ю/З) с нормализацией угла.
function pointingAzimuthToQuadrant(azimuth) {
  const value = Number.isFinite(azimuth) ? normalizeDegrees(azimuth) : 0;
  if (value >= 315 || value < 45) return "north";
  if (value < 135) return "east";
  if (value < 225) return "south";
  if (value < 315) return "west";
  return "north";
}

function updatePointingBadges() {
  const els = UI.els.pointing;
  const state = UI.state.pointing;
  if (!els) return;
  const observer = state.observer;
  const hasObserver = !!(observer && Number.isFinite(observer.lat) && Number.isFinite(observer.lon));
  if (els.tleBadge) {
    const total = state.satellites ? state.satellites.length : 0;
    let status = "warn";
    let text = "TLE • нет данных";
    if (state.tleReady && total > 0) {
      status = "ok";
      text = "TLE • " + total;
    } else if (state.tleError) {
      text = "TLE • " + state.tleError;
    }
    els.tleBadge.dataset.state = status;
    if (els.tleBadgeText) {
      els.tleBadgeText.textContent = text;
    } else {
      els.tleBadge.textContent = text;
    }
  }
  if (els.locationBadge) {
    let label = "нет данных";
    if (observer) {
      if (observer.mgrs) {
        label = observer.mgrs;
      } else if (hasObserver) {
        label = formatLatitude(observer.lat, 2) + ", " + formatLongitude(observer.lon, 2);
      } else if (state.observerMgrs) {
        label = state.observerMgrs;
      }
    } else if (state.observerMgrs) {
      label = state.observerMgrs;
    }
    let status;
    let text;
    if (state.mgrsError) {
      status = "warn";
      text = "Позиция • " + state.mgrsError;
    } else if (hasObserver) {
      status = "ok";
      text = "Позиция • " + label;
    } else if (state.observerMgrs) {
      status = "idle";
      text = "Позиция • " + label;
    } else {
      status = "idle";
      text = "Позиция • нет данных";
    }
    els.locationBadge.dataset.state = status;
    if (els.locationBadgeText) {
      els.locationBadgeText.textContent = text;
    } else {
      els.locationBadge.textContent = text;
    }
  }
  if (els.satBadge) {
    const count = state.visible ? state.visible.length : 0;
    const status = count > 0 ? "ok" : hasObserver ? "warn" : (state.mgrsError ? "warn" : "idle");
    let text = "Спутники • —";
    if (count > 0) {
      text = "Спутники • " + count;
    } else if (hasObserver && state.tleReady) {
      text = "Спутники • вне порога";
    } else if (!state.tleReady) {
      text = "Спутники • ждём TLE";
    }
    els.satBadge.dataset.state = status;
    if (els.satBadgeText) {
      els.satBadgeText.textContent = text;
    } else {
      els.satBadge.textContent = text;
    }
  }
}

function setPointingActiveSatellite(id) {
  const state = UI.state.pointing;
  if (!id && state.visible && state.visible.length) {
    state.selectedSatId = state.visible[0].id;
  } else if (id && state.visible.some((sat) => sat.id === id)) {
    state.selectedSatId = id;
  } else {
    state.selectedSatId = state.visible && state.visible.length ? state.visible[0].id : null;
  }
  renderPointingSatellites();
  updatePointingSatDetails(getPointingSelectedSatellite());
}

function getPointingSelectedSatellite() {
  const state = UI.state.pointing;
  if (!state.visible || !state.visible.length || !state.selectedSatId) return null;
  return state.visible.find((sat) => sat.id === state.selectedSatId) || null;
}

function updatePointingSatDetails(sat) {
  const els = UI.els.pointing;
  if (!els) return;
  if (els.targetAz) els.targetAz.textContent = sat ? formatDegrees(sat.azimuth, 1) : "—";
  if (els.targetEl) els.targetEl.textContent = sat ? formatDegrees(sat.elevation, 1) : "—";
  if (els.satDetails) {
    els.satDetails.hidden = !sat;
  }
  if (!sat) {
    if (els.subLon) els.subLon.textContent = "—";
    if (els.subLat) els.subLat.textContent = "—";
    if (els.satAltitude) els.satAltitude.textContent = "—";
    if (els.range) els.range.textContent = "—";
    return;
  }
  if (els.subLon) els.subLon.textContent = formatLongitude(sat.subLonDeg, 2);
  if (els.subLat) els.subLat.textContent = formatLatitude(sat.subLatDeg, 2);
  if (els.satAltitude) els.satAltitude.textContent = formatKilometers(sat.altitudeKm, 0);
  if (els.range) els.range.textContent = formatKilometers(sat.rangeKm, 0);
}

function formatDegrees(value, digits = 1) {
  if (!Number.isFinite(value)) return "—";
  return value.toFixed(digits) + "°";
}

function isProbablyIos() {
  if (typeof navigator === "undefined") return false;
  const ua = navigator.userAgent || "";
  return /iPad|iPhone|iPod/i.test(ua);
}

function formatSignedDegrees(value, digits = 1) {
  if (!Number.isFinite(value)) return "—";
  const sign = value >= 0 ? "+" : "−";
  return sign + Math.abs(value).toFixed(digits) + "°";
}

function formatLongitude(value, digits = 1) {
  if (!Number.isFinite(value)) return "—";
  const suffix = value >= 0 ? "E" : "W";
  return Math.abs(value).toFixed(digits) + "°" + suffix;
}

function formatLatitude(value, digits = 1) {
  if (!Number.isFinite(value)) return "—";
  const suffix = value >= 0 ? "N" : "S";
  return Math.abs(value).toFixed(digits) + "°" + suffix;
}

function formatMeters(value) {
  if (!Number.isFinite(value)) return "—";
  return value.toFixed(0) + " м";
}

function formatKilometers(value, digits = 0) {
  if (!Number.isFinite(value)) return "—";
  return value.toFixed(digits) + " км";
}

function normalizeDegrees(value) {
  if (!Number.isFinite(value)) return value;
  let result = value % 360;
  if (result < 0) result += 360;
  return result;
}

function normalizeDegreesSigned(value) {
  if (!Number.isFinite(value)) return value;
  let result = normalizeDegrees(value);
  if (result > 180) result -= 360;
  return result;
}

function normalizeRadians(value) {
  if (!Number.isFinite(value)) return value;
  let result = value % TWO_PI;
  if (result <= -Math.PI) result += TWO_PI;
  else if (result > Math.PI) result -= TWO_PI;
  return result;
}

function angleDifference(target, current) {
  if (!Number.isFinite(target) || !Number.isFinite(current)) return null;
  let diff = normalizeDegrees(target) - normalizeDegrees(current);
  while (diff > 180) diff -= 360;
  while (diff < -180) diff += 360;
  return diff;
}

function clampNumber(value, min, max) {
  if (!Number.isFinite(value)) return min;
  if (value < min) return min;
  if (value > max) return max;
  return value;
}

function solveKepler(meanAnomaly, eccentricity) {
  let E = meanAnomaly;
  for (let i = 0; i < 10; i++) {
    const f = E - eccentricity * Math.sin(E) - meanAnomaly;
    const fPrime = 1 - eccentricity * Math.cos(E);
    const delta = f / fPrime;
    E -= delta;
    if (Math.abs(delta) < 1e-8) break;
  }
  return E;
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
function classifyDebugMessage(text) {
  const raw = text != null ? String(text) : "";
  const trimmed = raw.trim();
  const low = trimmed.toLowerCase();
  if (!trimmed) return "info";
  if (/[✗×]/.test(trimmed) || low.includes("fail") || low.includes("ошиб") || low.startsWith("err")) return "error";
  if (low.includes("warn") || low.includes("предупр")) return "warn";
  if (trimmed.includes("✓") || low.includes("успех") || /\bok\b/.test(low)) return "success";
  if (trimmed.startsWith("→") || low.startsWith("tx") || low.startsWith("cmd") || low.startsWith("send")) return "action";
  return "info";
}
function debugLog(text) {
  const log = UI.els.debugLog;
  if (!log) return;
  const line = document.createElement("div");
  const type = classifyDebugMessage(text);
  line.className = "debug-line debug-line--" + type;
  line.textContent = "[" + new Date().toLocaleTimeString() + "] " + text;
  log.appendChild(line);
  log.scrollTop = log.scrollHeight;
}

async function loadVersion() {
  let versionClearedExplicitly = false;
  const applyVersion = (value, opts) => {
    const normalized = normalizeVersionText(value);
    if (normalized) {
      UI.state.version = normalized;
      storage.set("appVersion", normalized);
    } else if (opts && opts.clear === true) {
      UI.state.version = null;
      storage.remove("appVersion");
      versionClearedExplicitly = true;
    }
    updateFooterVersion();
    return normalized;
  };

  if (!UI.state.version) {
    const meta = document.querySelector('meta[name="app-version"]');
    if (meta) {
      const metaValue = normalizeVersionText(meta.getAttribute("content"));
      if (metaValue) {
        UI.state.version = metaValue;
        storage.set("appVersion", metaValue);
        updateFooterVersion();
      }
    }
  } else {
    updateFooterVersion();
  }

  const initialVersion = UI.state.version;
  const targets = [];
  try {
    targets.push(new URL("/ver", window.location.href));
  } catch (err) {
    // окно может не иметь корректного href (например, file://)
  }
  try {
    const base = new URL(UI.cfg.endpoint || "http://192.168.4.1");
    targets.push(new URL("/ver", base));
  } catch (err) {
    targets.push(new URL("/ver", "http://192.168.4.1"));
  }
  const seen = new Set();
  let lastError = null;
  for (const url of targets) {
    const key = url.toString();
    if (seen.has(key)) continue;
    seen.add(key);
    try {
      const res = await fetch(key, { cache: "no-store" });
      if (!res.ok) throw new Error("HTTP " + res.status);
      const raw = await res.text();
      applyVersion(raw, { clear: true });
      return UI.state.version;
    } catch (err) {
      lastError = err;
    }
  }

  try {
    const res = await deviceFetch("VER", {}, 2000);
    if (res.ok) {
      applyVersion(res.text, { clear: true });
      return UI.state.version;
    }
    if (res.error) lastError = new Error(res.error);
  } catch (err) {
    lastError = err;
  }

  if (UI.state.version != null) {
    updateFooterVersion();
  } else if (!versionClearedExplicitly && initialVersion != null) {
    UI.state.version = initialVersion;
    updateFooterVersion();
  }

  if (lastError) throw lastError;
  throw new Error("version unavailable");
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
  text = text.replace(/^\uFEFF+/, "").trim();
  const eqIndex = text.lastIndexOf("=");
  if (eqIndex >= 0 && eqIndex < text.length - 1) {
    text = text.slice(eqIndex + 1).trim();
  }
  text = text.replace(/^['"]+/, "").replace(/['"]+$/, "").trim();
  text = text.replace(/^v+/i, "").trim();
  text = text.replace(/^(?:version|ver|release|build)[\s:=#-]*/i, "").trim();
  if (!text) return "";
  if (/^(unknown|undefined|none|-{1,3})$/i.test(text)) return "";
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

