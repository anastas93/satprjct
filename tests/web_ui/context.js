const { readFileSync } = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

// Русские вспомогательные функции для тестовой среды браузера

function createClassList(initial) {
  const set = new Set(initial || []);
  return {
    add(name) {
      set.add(name);
    },
    remove(name) {
      set.delete(name);
    },
    toggle(name, force) {
      if (force === undefined) {
        if (set.has(name)) {
          set.delete(name);
          return false;
        }
        set.add(name);
        return true;
      }
      if (force) set.add(name);
      else set.delete(name);
      return force;
    },
    contains(name) {
      return set.has(name);
    },
    has(name) {
      return set.has(name);
    },
    toString() {
      return Array.from(set).join(' ');
    },
    value: set,
  };
}

function createElementStub(tagName = 'div') {
  const element = {
    tagName: String(tagName).toUpperCase(),
    children: [],
    dataset: {},
    style: {},
    className: '',
    classList: createClassList(),
    attributes: {},
    textContent: '',
    innerHTML: '',
    value: '',
    checked: false,
    disabled: false,
    indeterminate: false,
    appendChild(child) {
      this.children.push(child);
      return child;
    },
    removeChild(child) {
      const index = this.children.indexOf(child);
      if (index >= 0) this.children.splice(index, 1);
    },
    setAttribute(name, value) {
      this.attributes[name] = String(value);
    },
    getAttribute(name) {
      return this.attributes[name];
    },
    removeAttribute(name) {
      delete this.attributes[name];
    },
    querySelector() {
      return null;
    },
    querySelectorAll() {
      return [];
    },
    addEventListener() {},
    removeEventListener() {},
    focus() {},
    blur() {},
    getContext() {
      return {
        canvas: this,
        clearRect() {},
        drawImage() {},
        getImageData() {
          return { data: new Uint8ClampedArray(0) };
        },
        putImageData() {},
      };
    },
  };
  return element;
}

function createLocalStorage() {
  const store = new Map();
  return {
    getItem(key) {
      return store.has(key) ? store.get(key) : null;
    },
    setItem(key, value) {
      store.set(key, String(value));
    },
    removeItem(key) {
      store.delete(key);
    },
    clear() {
      store.clear();
    },
  };
}

function createDocumentStub() {
  const documentElement = createElementStub('html');
  const body = createElementStub('body');
  documentElement.appendChild(body);
  const listeners = {};
  return {
    documentElement,
    body,
    createElement: createElementStub,
    createTextNode(text) {
      return { textContent: String(text) };
    },
    createDocumentFragment() {
      return createElementStub('#fragment');
    },
    querySelector() {
      return null;
    },
    querySelectorAll() {
      return [];
    },
    getElementById() {
      return null;
    },
    addEventListener(type, handler) {
      listeners[type] = handler;
    },
    removeEventListener(type) {
      delete listeners[type];
    },
    dispatchEvent(type) {
      if (typeof listeners[type] === 'function') {
        listeners[type]();
      }
    },
    _listeners: listeners,
  };
}

function createBaseWindow() {
  const localStorage = createLocalStorage();
  return {
    localStorage,
    navigator: { userAgent: 'node-test' },
    location: { href: 'http://localhost/' },
    history: { replaceState() {}, pushState() {} },
    requestAnimationFrame(cb) {
      return setTimeout(() => cb(Date.now()), 0);
    },
    cancelAnimationFrame(id) {
      clearTimeout(id);
    },
    setTimeout,
    clearTimeout,
    setInterval,
    clearInterval,
    matchMedia(query) {
      return {
        matches: false,
        media: query,
        addEventListener() {},
        removeEventListener() {},
      };
    },
    fetch: async () => {
      throw new Error('fetch не замокан для теста');
    },
    atob(data) {
      return Buffer.from(String(data), 'base64').toString('binary');
    },
    btoa(data) {
      return Buffer.from(String(data), 'binary').toString('base64');
    },
    EventSource: undefined,
    Image: class {
      set src(value) {
        this._src = value;
        if (typeof this.onload === 'function') {
          this.onload();
        }
      }
    },
    FileReader: class {
      constructor() {
        this.result = null;
        this.onerror = null;
        this.onload = null;
      }
      readAsDataURL() {
        if (typeof this.onload === 'function') {
          this.onload();
        }
      }
    },
    AudioContext: class {},
    URL: {
      createObjectURL() {
        return 'blob://dummy';
      },
      revokeObjectURL() {},
    },
    Blob,
  };
}

const scriptSource = readFileSync(path.resolve(__dirname, '../../src/web/script.js'), 'utf8');

function createWebContext(options = {}) {
  const document = createDocumentStub();
  const window = createBaseWindow();
  if (options.localStorage) {
    window.localStorage = options.localStorage;
  }
  if (typeof options.matchMedia === 'function') {
    window.matchMedia = options.matchMedia;
  }
  const base = {
    console,
    window,
    document,
    navigator: window.navigator,
    localStorage: window.localStorage,
    setTimeout,
    clearTimeout,
    setInterval,
    clearInterval,
    performance: { now: () => Date.now() },
    Blob: window.Blob,
    URL: window.URL,
    Image: window.Image,
    FileReader: window.FileReader,
    EventSource: window.EventSource,
    TextDecoder,
    TextEncoder,
    Uint8Array,
    Uint8ClampedArray,
    ArrayBuffer,
    DataView,
    Math,
    Date,
    JSON,
    Map,
    Set,
    WeakMap,
    WeakSet,
    Intl,
  };

  if (options.globals) {
    Object.assign(base, options.globals);
  }

  window.window = window;
  window.globalThis = window;
  window.document = document;
  window.console = console;
  window.navigator = window.navigator || { userAgent: 'node-test' };
  window.performance = base.performance;
  window.TextDecoder = TextDecoder;
  window.TextEncoder = TextEncoder;
  window.Uint8Array = Uint8Array;
  window.Uint8ClampedArray = Uint8ClampedArray;
  window.ArrayBuffer = ArrayBuffer;
  window.DataView = DataView;

  base.globalThis = window;
  base.self = window;
  window.self = window;
  base.matchMedia = window.matchMedia.bind(window);

  const context = vm.createContext(base);
  const script = new vm.Script(scriptSource, { filename: 'web/script.js' });
  script.runInContext(context);

  // Экспортируем нужные глобальные константы и структуры для удобного доступа из тестов
  const exportNames = [
    'storage',
    'UI',
    'POWER_PRESETS',
    'ACK_TIMEOUT_MAX_MS',
    'PAUSE_MIN_MS',
    'PAUSE_MAX_MS',
    'ACK_TIMEOUT_MIN_MS',
    'ACK_RETRY_DEFAULT',
    'ACK_RETRY_MAX',
    'logErrorEvent',
    'ERROR_LOG_REPEAT_WINDOW_MS',
  ];
  for (const name of exportNames) {
    try {
      context[name] = vm.runInContext(name, context);
    } catch (err) {
      context[name] = undefined;
    }
  }

  return { context, window, document };
}

module.exports = {
  createWebContext,
  createLocalStorage,
  createElementStub,
};
