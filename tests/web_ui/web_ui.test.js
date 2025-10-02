const test = require('node:test');
const assert = require('node:assert/strict');
const { createWebContext } = require('./context');
const debugLogSamples = require('./debug_log_types.json');

// Базовый контекст браузера для большинства проверок
const base = createWebContext();
const { context: ctx, document } = base;
const UI = ctx.UI;
const storage = ctx.storage;

function resetUiState() {
  if (storage && typeof storage.clear === 'function') storage.clear();
  UI.cfg.theme = 'dark';
  UI.cfg.autoNight = false;
  UI.cfg.accent = 'default';
  UI.state.autoNightActive = false;
  UI.els.themeToggle = document.createElement('button');
  UI.els.themeRedToggle = document.createElement('button');
  UI.els.autoNightSwitch = document.createElement('input');
  UI.els.autoNightHint = document.createElement('p');
}

// Проверяем хранение настроек
test('storage сохраняет значения и удаляет их', () => {
  resetUiState();
  storage.set('endpoint', 'http://example.local');
  assert.equal(storage.get('endpoint'), 'http://example.local');
  storage.remove('endpoint');
  assert.equal(storage.get('endpoint'), null);
});

// Проверяем резервное хранилище при недоступном localStorage
test('storage переходит в режим памяти при ошибке localStorage', () => {
  const brokenStorage = {
    setItem() {
      throw new Error('nope');
    },
    getItem() {
      throw new Error('nope');
    },
    removeItem() {},
    clear() {},
  };
  const { context: altCtx } = createWebContext({ localStorage: brokenStorage });
  assert.equal(altCtx.storage.available, false);
  altCtx.storage.set('key', 'value');
  assert.equal(altCtx.storage.get('key'), 'value');
});

// Проверяем определение темы по настройкам ОС
test('detectPreferredTheme учитывает matchMedia', () => {
  const { context: lightCtx } = createWebContext({
    matchMedia: (query) => ({
      matches: query.includes('light'),
      media: query,
      addEventListener() {},
      removeEventListener() {},
    }),
  });
  assert.equal(lightCtx.detectPreferredTheme(), 'light');
});

// Проверяем применение темы и индикацию элементов управления
test('applyTheme переключает класс и сохраняет выбор', () => {
  resetUiState();
  ctx.applyTheme('light');
  assert.equal(ctx.UI.cfg.theme, 'light');
  assert(document.documentElement.classList.contains('light'));
  assert.equal(storage.get('theme'), 'light');
  assert.equal(UI.els.themeToggle.getAttribute('aria-pressed'), 'false');
  assert.equal(UI.els.autoNightHint.textContent, 'Автоматическое переключение отключено.');

  ctx.applyTheme('dark');
  assert.equal(ctx.UI.cfg.theme, 'dark');
  assert.equal(document.documentElement.classList.contains('light'), false);
});

// Проверяем переключение акцента
test('toggleAccent меняет цветовую схему', () => {
  resetUiState();
  ctx.toggleAccent();
  assert.equal(ctx.UI.cfg.accent, 'red');
  assert(document.documentElement.classList.contains('red'));
  assert.equal(storage.get('accent'), 'red');
  assert.equal(UI.els.themeRedToggle.getAttribute('aria-pressed'), 'true');
});

// Проверяем разбор слэш-команд
test('parseSlashCommand корректно разбирает параметры', () => {
  const parsedAck = ctx.parseSlashCommand('ACK 0');
  assert.equal(parsedAck.cmd, 'ACK');
  assert.equal(parsedAck.params.v, '0');
  assert.equal(parsedAck.message, '');

  const parsedTx = ctx.parseSlashCommand('TX Привет мир');
  assert.equal(parsedTx.cmd, 'TX');
  assert.equal(Object.keys(parsedTx.params).length, 0);
  assert.equal(parsedTx.message, 'Привет мир');
});

// Проверяем нормализацию истории чата
test('normalizeChatEntries преобразует входящие записи RX', () => {
  const now = Date.now();
  const entries = [
    { a: 'you', m: 'Сообщение', t: now },
    { tag: 'tx-status', status: 'OK', t: now + 1 },
    { tag: 'rx-name', m: 'RX: GO-123', rx: { name: 'GO-123', hex: '48656c6c6f', len: 5 } },
  ];
  const normalized = ctx.normalizeChatEntries(entries);
  assert.equal(normalized.length, 2);
  assert.equal(normalized[0].txStatus.ok, true);
  assert.equal(normalized[1].tag, 'rx-message');
  assert.equal(normalized[1].role, 'rx');
  assert.equal(normalized[1].rx.name, 'GO-123');
  assert.equal(normalized[1].rx.len, 5);
  assert.equal(normalized[1].m, 'GO-123');
  assert.equal(normalized[1].rx.text, 'GO-123');
});

// Проверяем форматирование размеров
test('formatBytesShort даёт человеческое представление', () => {
  assert.equal(ctx.formatBytesShort(512), '512 Б');
  assert.equal(ctx.formatBytesShort(4096), '4 КиБ');
  assert.equal(ctx.formatBytesShort(6 * 1024 * 1024), '6 МиБ');
  assert.equal(ctx.formatBytesShort(0), '—');
});

// Проверяем формирование подписи изображения
test('formatChatImageCaption собирает подпись', () => {
  const meta = { name: 'img-1', jpegSize: 1536, frameWidth: 320, frameHeight: 240, profile: 'S', status: 'pending' };
  const caption = ctx.formatChatImageCaption(meta, null);
  assert.equal(caption, 'img-1 · 1.5 КиБ · 320×240 · профиль S · отправка…');
});

// Проверяем, что обновление статуса IRQ срабатывает на сообщении без символов после IRQ
test('debugLog сохраняет статус IRQ для сообщений RadioSX1262 без хвоста', () => {
  const wrap = document.createElement('div');
  const messageEl = document.createElement('span');
  const metaEl = document.createElement('span');
  wrap.classList.remove('active');
  UI.els.chatIrqStatus = wrap;
  UI.els.chatIrqMessage = messageEl;
  UI.els.chatIrqMeta = metaEl;
  UI.els.debugLog = document.createElement('div');

  ctx.debugLog('RadioSX1262: событие DIO1 без активных флагов IRQ', { origin: 'device', uptimeMs: 1234 });

  assert.equal(UI.state.irqStatus.message, 'RadioSX1262: событие DIO1 без активных флагов IRQ');
  assert.equal(UI.state.irqStatus.uptimeMs, 1234);
  assert.equal(wrap.classList.contains('active'), true);
  assert.equal(messageEl.textContent, 'RadioSX1262: событие DIO1 без активных флагов IRQ');
});

// Проверяем эвристику читаемости текста
test('evaluateTextCandidate и selectReadableTextCandidate выбирают кириллицу', () => {
  const good = ctx.evaluateTextCandidate('Привет, мир!');
  const bad = ctx.evaluateTextCandidate('ÃÂÃÂÃÂ¸');
  assert(good.score < bad.score);

  const chosen = ctx.selectReadableTextCandidate([
    { text: 'ÃÂÃÂÃÂ¸', source: 'bad' },
    { text: 'Привет', source: 'good' },
  ]);
  assert.equal(chosen.text, 'Привет');
  assert.equal(chosen.source, 'good');
});

// Проверяем преобразование HEX → байты → текст
test('hexToBytes и decodeBytesToText восстанавливают строку', () => {
  const bytes = ctx.hexToBytes('48656c6c6f');
  assert(bytes instanceof Uint8Array);
  assert.equal(bytes.length, 5);
  const text = ctx.decodeBytesToText(bytes);
  assert.equal(text, 'Hello');
});

// Проверяем парсинг ответа RSTS
test('parseReceivedResponse собирает элементы из JSON', () => {
  const response = JSON.stringify({ ready: [{ name: 'R-1', text: 'OK' }], split: [{ name: 'SP-1', hex: '4142' }] });
  const items = ctx.parseReceivedResponse(response);
  assert.equal(items.length, 2);
  const ready = items.find((item) => item.name === 'R-1');
  assert(ready);
  assert.equal(ready.type, 'ready');
  const split = items.find((item) => item.name === 'SP-1');
  assert(split);
  assert.equal(split.len, 2);
  assert.equal(split.text, 'AB');
});

// Проверяем оценку количества элементов JSON RSTS
test('countRstsJsonItems корректно считает массив', () => {
  assert.equal(ctx.countRstsJsonItems('[1,2,3]'), 3);
  assert.equal(ctx.countRstsJsonItems(JSON.stringify({ items: [1, 2] })), 2);
  assert.equal(ctx.countRstsJsonItems('{"foo":1}'), 0);
});

// Проверяем обработку ответов по ACK и Light pack
test('разбор ответов по радиопараметрам', () => {
  assert.equal(ctx.parseAckResponse('ACK:1'), true);
  assert.equal(ctx.parseAckResponse('выключено'), false);
  assert.equal(ctx.parseAckResponse('???'), null);

  assert.equal(ctx.clampAckRetry(15), 10);
  assert.equal(ctx.clampAckRetry(-1), 0);

  assert.equal(ctx.parseAckRetryResponse('ACKR=5'), 5);
  assert.equal(ctx.parsePauseResponse('PAUSE 1500'), 1500);
  assert.equal(ctx.parseAckTimeoutResponse('ACKT 99999'), ctx.ACK_TIMEOUT_MAX_MS);
  assert.equal(ctx.parseRxBoostedGainResponse('RXBG ON'), true);
  assert.equal(ctx.parseRxBoostedGainResponse('RXBG 0'), false);

  assert.equal(ctx.parseLightPackResponse('LIGHT:1'), true);
  assert.equal(ctx.parseLightPackResponse('выключен'), false);
  assert.equal(ctx.parseLightPackResponse('???'), null);
});

// Проверяем разбор диагностики ENCT и TESTRXM
test('parseEncTestResponse и parseTestRxmResponse распознают JSON и текст', () => {
  const encJson = ctx.parseEncTestResponse('{"status":"ok","plain":"data","cipher":"aabb"}');
  assert(encJson);
  assert.equal(encJson.status, 'ok');
  assert.equal(encJson.plain, 'data');
  assert.equal(encJson.cipher, 'aabb');
  assert.equal(encJson.legacy, false);
  const encLegacy = ctx.parseEncTestResponse('ENCT:ERR');
  assert(encLegacy);
  assert.equal(encLegacy.status, 'error');
  assert.equal(encLegacy.error, 'legacy');
  assert.equal(encLegacy.legacy, true);

  const rxmJson = ctx.parseTestRxmResponse('{"status":"ok","count":3,"intervalMs":250}');
  assert(rxmJson);
  assert.equal(rxmJson.status, 'ok');
  assert.equal(rxmJson.count, 3);
  assert.equal(rxmJson.intervalMs, 250);
  const rxmBusy = ctx.parseTestRxmResponse('busy');
  assert(rxmBusy);
  assert.equal(rxmBusy.status, 'busy');
  assert.equal(rxmBusy.count, 0);
  assert.equal(rxmBusy.intervalMs, 0);
});

// Проверяем нормализацию мощности и текстовых ответов KEYGEN
test('normalizePowerPreset и parseKeygenPlainResponse обрабатывают данные', () => {
  const preset = ctx.normalizePowerPreset('4');
  assert(preset);
  assert.equal(preset.index, 3);
  assert.equal(preset.value, ctx.POWER_PRESETS[3]);
  assert.equal(ctx.normalizePowerPreset('99'), null);

  const keygen = ctx.parseKeygenPlainResponse('KEYGEN OK ID=1a2b PUB=ffeeddcc backup=yes');
  assert(keygen);
  assert.equal(keygen.message.includes('ID 1A2B'), true);
  assert.equal(keygen.state.type, undefined);
  assert.equal(keygen.state.hasBackup, true);
});

// Проверяем вычисления вкладки наведения антенны
test('pointing функции разбирают TLE и выдают координаты', () => {
  const tle = {
    name: 'TESTSAT',
    line1: '1 25544U 98067A   21075.51005787  .00001264  00000-0  29621-4 0  9993',
    line2: '2 25544  51.6440  88.7421 0002396  96.0298 319.7034 15.48902037273910',
  };
  const sat = ctx.parsePointingTle(tle);
  assert(sat);
  const list = ctx.parsePointingSatellites([tle, null]);
  assert.equal(list.length, 1);

  const epoch = ctx.tleDayToDate(2021, 75.51005787);
  assert(epoch instanceof Date);

  const gmst = ctx.computeGmst(new Date('2021-03-16T12:14:28Z'));
  const propagated = ctx.propagatePointingSatellite(sat, new Date('2021-03-16T12:14:28Z'), gmst);
  assert(propagated);
  assert(Number.isFinite(propagated.subLat));
  assert(Number.isFinite(propagated.subLon));
});

// Проверяем форматирование геодезических величин и нормализацию углов
test('форматирование углов и расстояний', () => {
  assert.equal(ctx.formatDegrees(12.345, 1), '12.3°');
  assert.equal(ctx.formatLongitude(-45.5, 1), '45.5°W');
  assert.equal(ctx.formatLatitude(10.25, 2), '10.25°N');
  assert.equal(ctx.formatMeters(123.4), '123 м');
  assert.equal(ctx.formatKilometers(321.9, 1), '321.9 км');
  assert.equal(ctx.normalizeDegrees(-30), 330);
  assert.equal(ctx.normalizeDegreesSigned(190), -170);
  const normalizedRad = ctx.normalizeRadians(-Math.PI * 2);
  assert.equal(Object.is(normalizedRad, -0), true);
  assert.equal(ctx.angleDifference(10, 350), 20);
  assert.equal(ctx.clampNumber(120, 0, 100), 100);
  const solved = ctx.solveKepler(1, 0.1);
  const residual = solved - 0.1 * Math.sin(solved) - 1;
  assert(Math.abs(residual) < 1e-8);
});

// Проверяем форматирование времени
test('formatDurationMs, formatDeviceUptime и formatRelativeTime', () => {
  assert.equal(ctx.formatDurationMs(1500), '1,5 с');
  assert.equal(ctx.formatDeviceUptime(5000), '+5 с');
  const past = Date.now() - 90 * 1000;
  const rel = ctx.formatRelativeTime(past);
  assert.equal(rel, '2 мин назад');
});
// Проверяем отображение всех типов сообщений в debug-log
test('debugLog создает элементы для каждого типа сообщения', () => {
  const { context: logCtx, document: logDoc } = createWebContext();
  const debugRoot = logDoc.createElement('div');
  debugRoot.scrollHeight = 0;
  logCtx.UI.els.debugLog = debugRoot;

  debugLogSamples.forEach((sample, index) => {
    const timestamp = Date.UTC(2024, 0, 1, 0, 0, index);
    logCtx.debugLog(sample.text, { timestamp });
    const node = debugRoot.children[index];
    assert(node.className.includes('debug-card'));
    assert(node.className.includes(`debug-card--${sample.type}`));
    const messageEl = node.__messageEl;
    const timeEl = node.__timeEl;
    assert(messageEl && messageEl.textContent.includes(sample.text));
    assert(timeEl && timeEl.textContent.startsWith('['));
  });

  assert.equal(debugRoot.children.length, debugLogSamples.length);
});

// Проверяем, что повторяющиеся ошибки агрегируются и не засоряют журнал
test('logErrorEvent объединяет одинаковые ошибки', () => {
  const { context: errCtx, document: errDoc } = createWebContext();
  const logRoot = errDoc.createElement('div');
  errCtx.UI.els.debugLog = logRoot;

  errCtx.Date.now = () => 1000;
  errCtx.logErrorEvent('LOGS', 'тайм-аут запроса (4000 мс)');
  assert.equal(logRoot.children.length, 1);
  const first = logRoot.children[0];
  const firstMessage = first.__messageEl ? first.__messageEl.textContent : first.textContent;
  assert(firstMessage.includes('ERR LOGS:'));
  assert(firstMessage.includes('4000'));

  errCtx.logErrorEvent('LOGS', 'тайм-аут запроса (4000 мс)');
  assert.equal(logRoot.children.length, 1);
  assert(first.dataset.repeatCount === '2');

  errCtx.Date.now = () => 1000 + errCtx.ERROR_LOG_REPEAT_WINDOW_MS + 5;
  errCtx.logErrorEvent('LOGS', 'тайм-аут запроса (4000 мс)');
  assert.equal(logRoot.children.length, 2);
  const second = logRoot.children[1];
  const secondMessage = second.__messageEl ? second.__messageEl.textContent : second.textContent;
  assert(secondMessage.includes('ERR LOGS:'));
  assert(secondMessage.includes('4000'));
});

