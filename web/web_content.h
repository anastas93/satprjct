#pragma once
#include <pgmspace.h>
// Встроенные ресурсы веб-интерфейса.
// Файл сгенерирован автоматически скриптом tools/generate_web_content.py.

// web/index.html
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
    <div class="brand">SAT-COMMUNICATION<span class="tag">web</span></div>
    <nav class="nav" id="siteNav">
      <a href="#" data-tab="chat">Chat<span class="nav-badge" aria-label="Непрочитанные сообщения: 0" aria-live="off" hidden>0</span></a>
      <a href="#" data-tab="channels">Channels/Ping</a>
      <a href="#" data-tab="pointing">Antenna helper</a>
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
      <div class="chat-area">
        <div id="chatLog" class="chat-log"></div>
        <button type="button" id="chatScrollBottom" class="chat-scroll-down" aria-label="Прокрутить вниз"><span>↓</span></button>
        <div id="chatRxIndicator" class="chat-receiving-indicator" hidden>
          <span class="indicator-icon" aria-hidden="true"></span>
          <span class="indicator-text">Приём сообщения…</span>
        </div>
      </div>
      <div id="chatIrqStatus" class="chat-irq-status" aria-live="polite" role="status">
        <div class="chat-irq-text">
          <span class="chat-irq-message">IRQ: событий нет</span>
          <small class="chat-irq-meta" hidden>—</small>
        </div>
      </div>
      <div class="chat-input">
        <div class="send-row">
          <input id="chatInput" type="text" placeholder="Сообщение" maxlength="2000" />
          <button id="sendBtn" class="btn">Отправить</button>
        </div>
        <div class="chat-upload">
          <button type="button" id="chatImageBtn" class="icon-btn ghost" aria-label="Отправить изображение" title="Загрузить изображение"><span>📷</span></button>
          <select id="chatImageProfile" class="chat-image-profile" aria-label="Профиль изображений">
            <option value="S">S — 320×240, серый</option>
            <option value="M">M — 320×240, серый+</option>
            <option value="L">L — 320×240, цвет</option>
          </select>
        </div>
        <input id="chatImageInput" type="file" accept="image/jpeg,image/png,image/heic,image/heif" hidden multiple />
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
      <details id="chatCommands" class="chat-commands">
        <summary>Команды</summary>
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
      </details>
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
      <div id="channelRefStatus" class="channel-ref-status small" hidden></div>
      <div class="channel-layout">
        <div class="table-wrap pretty">
          <table id="channelsTable">
            <thead>
              <tr><th>CH</th><th>TX,MHz</th><th>RX,MHz</th><th>RSSI</th><th>SNR</th><th>ST</th><th>SCAN</th></tr>
            </thead>
            <tbody></tbody>
          </table>
        </div>
        <div id="channelInfoPanel" class="channel-info glass" hidden>
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
                <div><dt>Частота (MHz)</dt><dd id="channelInfoFrequency">—</dd></div>
                <div><dt>Система</dt><dd id="channelInfoSystem">—</dd></div>
                <div><dt>План</dt><dd id="channelInfoBand">—</dd></div>
                <div><dt>Полоса</dt><dd id="channelInfoBandwidth">—</dd></div>
                <div><dt>Назначение</dt><dd id="channelInfoPurpose">—</dd></div>
                <div><dt>Спутник</dt><dd id="channelInfoSatellite">—</dd></div>
                <div><dt>Позиция</dt><dd id="channelInfoPosition">—</dd></div>
                <div><dt>Модуляция</dt><dd id="channelInfoModulation">—</dd></div>
                <div><dt>Использование</dt><dd id="channelInfoUsage">—</dd></div>
                <div><dt>Комментарий</dt><dd id="channelInfoComments">—</dd></div>
              </dl>
            </div>
            <div class="channel-info-actions">
              <button id="channelInfoSetCurrent" class="btn">Установить текущим каналом</button>
            </div>
            <div class="channel-info-block channel-info-tests-block">
              <div class="channel-info-caption">Тесты</div>
              <div class="channel-info-tests">
                <button id="channelInfoStabilityTest" class="btn">Stability test</button>
                <button id="channelInfoCrTest" class="btn">CR test</button>
              </div>
              <div id="channelInfoTestsStatus" class="channel-info-status small muted" hidden></div>
              <div id="channelInfoStabilitySummary" class="channel-info-test-summary" hidden>
                <div class="channel-info-test-summary-main">
                  <div><span>Успешных пакетов</span><strong id="channelInfoStabilitySuccess">—</strong></div>
                  <div><span>Успешность</span><strong id="channelInfoStabilityPercent">—</strong></div>
                  <div><span>Средняя задержка</span><strong id="channelInfoStabilityLatencyAvg">—</strong></div>
                  <div><span>Джиттер (σ)</span><strong id="channelInfoStabilityLatencyJitter">—</strong></div>
                </div>
                <div class="channel-info-test-legend small">
                  <span data-type="rssi">RSSI</span>
                  <span data-type="snr">SNR</span>
                </div>
              </div>
              <div id="channelInfoStabilityIssues" class="channel-info-test-issues small muted" hidden></div>
              <div id="channelInfoStabilityCards" class="channel-info-test-cards" hidden>
                <div class="channel-info-test-card">
                  <div class="label">RSSI</div>
                  <div class="value" id="channelInfoStabilityRssiRange">—</div>
                  <div class="meta" id="channelInfoStabilityRssiMeta">—</div>
                </div>
                <div class="channel-info-test-card">
                  <div class="label">SNR</div>
                  <div class="value" id="channelInfoStabilitySnrRange">—</div>
                  <div class="meta" id="channelInfoStabilitySnrMeta">—</div>
                </div>
                <div class="channel-info-test-card">
                  <div class="label">Задержка</div>
                  <div class="value" id="channelInfoStabilityLatencyRange">—</div>
                  <div class="meta" id="channelInfoStabilityLatencyMeta">—</div>
                </div>
              </div>
              <div id="channelInfoStabilityResult" class="channel-info-test-chart" hidden>
                <canvas id="channelInfoStabilityChart" width="360" height="200"></canvas>
                <div id="channelInfoStabilityTooltip" class="channel-info-test-tooltip" hidden></div>
              </div>
              <div id="channelInfoStabilityHistory" class="channel-info-test-history" hidden>
                <div class="channel-info-test-history-header">
                  <div class="channel-info-test-history-title">История Stability</div>
                  <div class="channel-info-test-history-actions">
                    <button id="channelInfoStabilityExportCsv" class="btn ghost small">Экспорт CSV</button>
                    <button id="channelInfoStabilityExportJson" class="btn ghost small">Экспорт JSON</button>
                    <button id="channelInfoStabilityHistoryClear" class="btn ghost small">Очистить</button>
                  </div>
                </div>
                <div id="channelInfoStabilityHistoryEmpty" class="small muted">История пуста — выполните Stability test.</div>
                <ul id="channelInfoStabilityHistoryList" class="channel-info-test-history-list"></ul>
              </div>
              <div id="channelInfoCrResult" class="channel-info-cr" hidden>
                <table class="channel-info-cr-table">
                  <thead>
                    <tr><th>CR</th><th>Ping</th><th>RSSI</th><th>SNR</th></tr>
                  </thead>
                  <tbody id="channelInfoCrTableBody"></tbody>
                </table>
              </div>
            </div>
          </div>
        </div>
      </div>
    </section>
    <!-- Вкладка наведения антенны -->
    <section id="tab-pointing" class="tab" hidden>
      <!-- Заголовок вкладки скрыт по требованию макета -->
      <div id="pointingSummary" class="pointing-summary glass">
        <div class="pointing-summary-chip" id="pointingTleBadge" data-state="warn">
          <span class="pointing-summary-icon">🛰️</span>
          <span id="pointingTleText">TLE • нет данных</span>
        </div>
        <div class="pointing-summary-chip" id="pointingLocationBadge" data-state="idle">
          <span class="pointing-summary-icon">📍</span>
          <span id="pointingLocationText">Позиция • нет данных</span>
        </div>
        <div class="pointing-summary-chip" id="pointingSatBadge" data-state="idle">
          <span class="pointing-summary-icon">🗺️</span>
          <span id="pointingSatText">Спутники • —</span>
        </div>
      </div>
      <div class="pointing-grid">
        <article class="pointing-card glass">
          <h3>Активный спутник</h3>
          <label class="pointing-select">
            <span>Выбор спутника</span>
            <select id="pointingSatSelect"></select>
          </label>
          <div class="pointing-sat-details" id="pointingSatDetails" hidden>
            <div>Долгота подспутника: <strong id="pointingSubLon">—</strong></div>
            <div>Широта подспутника: <strong id="pointingSubLat">—</strong></div>
            <div>Высота орбиты: <strong id="pointingSatAltitude">—</strong></div>
            <div>Дистанция: <strong id="pointingRange">—</strong></div>
          </div>
        </article>
        <article class="pointing-card glass">
          <h3>Наведение на спутник</h3>
          <div class="pointing-target-grid">
            <div class="pointing-target-metric"><span class="label">Азимут</span><strong id="pointingTargetAz">—</strong></div>
            <div class="pointing-target-metric"><span class="label">Возвышение</span><strong id="pointingTargetEl">—</strong></div>
          </div>
          <div class="pointing-compass" id="pointingCompass">
            <div class="pointing-compass-dial">
              <div class="pointing-compass-radar" id="pointingCompassRadar"></div>
              <div class="pointing-compass-axes" aria-hidden="true"></div>
              <div class="pointing-compass-rose" aria-hidden="true">
                <span data-dir="south">S</span>
                <span data-dir="east">E</span>
                <span data-dir="north">N</span>
                <span data-dir="west">W</span>
              </div>
              <div class="pointing-compass-center"></div>
              <div class="pointing-compass-graduations"></div>
            </div>
            <div class="pointing-compass-legend" id="pointingCompassLegend" aria-hidden="true">
              <span data-quadrant="north">Север • N</span>
              <span data-quadrant="east">Восток • E</span>
              <span data-quadrant="south">Юг • S</span>
              <span data-quadrant="west">Запад • W</span>
            </div>
          </div>
          <label class="pointing-min-el pointing-min-el-footer">
            <span>Мин. возвышение</span>
            <input id="pointingMinElevation" type="range" min="0" max="30" step="1" value="10" />
            <span id="pointingMinElValue" class="pointing-min-el-value">10°</span>
          </label>
        </article>
        <article class="pointing-card glass pointing-card-wide">
          <h3>Доступные спутники</h3>
          <div class="pointing-horizon" id="pointingHorizon">
            <div class="pointing-horizon-azimuths" aria-hidden="true">
              <span data-az="0">0° • N</span>
              <span data-az="90">90° • E</span>
              <span data-az="180">180° • S</span>
              <span data-az="270">270° • W</span>
            </div>
            <div class="pointing-horizon-track" id="pointingHorizonTrack"></div>
            <div class="pointing-horizon-empty small muted" id="pointingHorizonEmpty">Укажите координаты, чтобы увидеть спутники.</div>
          </div>
          <p class="small muted" id="pointingSatSummary">После выбора квадрата MGRS появится список видимых спутников и их параметры.</p>
          <div class="pointing-sat-list" id="pointingSatList"></div>
        </article>
        <article class="pointing-card glass">
          <details id="pointingObserverDetails" class="pointing-observer">
            <summary class="pointing-observer-summary">
              <span class="title">Положение наблюдателя</span>
              <span class="value" id="pointingObserverLabel">—</span>
            </summary>
            <div class="pointing-observer-body">
              <p class="small muted">Укажите квадрат MGRS (100 км) и при необходимости высоту. По умолчанию используется сектор 43U CR.</p>
              <div class="pointing-observer-grid">
                <label>MGRS 100 км
                  <input id="pointingMgrsInput" type="text" maxlength="8" placeholder="43U CR" />
                </label>
                <label>Высота (м)
                  <input id="pointingManualAlt" type="number" step="1" min="-500" max="9000" placeholder="0" />
                </label>
              </div>
              <div class="pointing-observer-meta">
                <div><span class="label">Широта</span><strong id="pointingLat">—</strong></div>
                <div><span class="label">Долгота</span><strong id="pointingLon">—</strong></div>
                <div><span class="label">Высота</span><strong id="pointingAlt">—</strong></div>
              </div>
              <button id="pointingMgrsApply" class="btn ghost">Применить</button>
            </div>
          </details>
        </article>
      </div>
    </section>


    <!-- Вкладка настроек -->
    <section id="tab-settings" class="tab" hidden>
      <h2>Settings</h2>
      <form id="settingsForm" class="settings-form">
        <section class="settings-section">
          <h3>Подключение</h3>
          <div class="settings-group">
            <label>Endpoint (HTTP)
              <input type="url" id="endpoint" inputmode="url" placeholder="http://192.168.4.1" autocomplete="url" spellcheck="false" autocapitalize="none" />
              <div class="field-hint">Адрес HTTP-интерфейса радиомодуля. Значение сохраняется локально.</div>
            </label>
          </div>
        </section>
        <section class="settings-section">
          <h3>Канал</h3>
          <div class="settings-group two">
            <label>Bank
              <select id="BANK">
                <option value="e">EAST</option>
                <option value="w">WEST</option>
                <option value="t">TEST</option>
                <option value="a">ALL</option>
                <option value="h">HOME</option>
              </select>
            </label>
            <label>Channel
              <select id="CH"></select>
              <div id="channelSelectHint" class="field-hint"></div>
            </label>
          </div>
          <div class="settings-group two">
            <label>BW (kHz)
              <select id="BF">
                <option value="7.81">7.81</option>
                <option value="10.42">10.42</option>
                <option value="15.63">15.63</option>
                <option value="20.83" selected>20.83</option>
                <option value="31.25">31.25</option>
              </select>
            </label>
            <label>SF
              <select id="SF">
                <option>7</option><option>8</option><option>9</option><option>10</option><option>11</option><option>12</option>
              </select>
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
          </div>
          <div class="settings-toggle" id="rxBoostedGainControl">
            <label class="chip switch">
              <input type="checkbox" id="RXBG" />
              <span>RX boosted gain</span>
            </label>
            <div id="rxBoostedGainHint" class="field-hint">Состояние не загружено.</div>
          </div>
        </section>
        <section class="settings-section">
          <h3>ACK и задержки</h3>
          <div class="settings-group">
            <div class="settings-toggle" id="ackSettingControl">
              <label class="chip switch">
                <input type="checkbox" id="ACK" />
                <span>ACK (подтверждения)</span>
              </label>
              <div id="ackSettingHint" class="field-hint">Состояние не загружено.</div>
            </div>
            <div class="settings-group two">
              <label>ACK повторы
                <input id="ACKR" type="number" min="0" max="10" value="3" />
                <div id="ackRetryHint" class="field-hint">Доступно после включения ACK.</div>
              </label>
              <label>ACK тайм-аут (мс)
                <input id="ACKT" type="number" min="0" max="60000" value="320" />
                <div id="ackTimeoutHint" class="field-hint">Время ожидания ACK не загружено.</div>
              </label>
              <label>ACK задержка (мс)
                <input id="ACKD" type="number" min="0" max="5000" value="20" />
                <div id="ackDelayHint" class="field-hint">Задержка отправки ACK не загружена.</div>
              </label>
            </div>
            <label>Пауза между пакетами (мс)
              <input id="PAUSE" type="number" min="0" max="60000" value="370" />
              <div id="pauseHint" class="field-hint">Пауза между пакетами не загружена.</div>
            </label>
          </div>
        </section>
        <section class="settings-section">
          <h3>Режим передачи</h3>
          <div class="settings-group">
            <div class="settings-toggle" id="lightPackControl">
              <label class="chip switch">
                <input type="checkbox" id="LIGHTPACK" />
                <span>Light pack — прямой текст</span>
              </label>
              <div id="lightPackHint" class="field-hint">Сообщения передаются без служебного префикса и отображаются как есть.</div>
            </div>
          </div>
        </section>
        <section class="settings-section">
          <h3>Тестовые сообщения</h3>
          <div class="settings-group">
            <label>Сообщение для TESTRXM
              <input id="TESTRXMMSG" type="text" maxlength="2048" placeholder="Lorem ipsum dolor sit amet" />
              <div id="testRxmMessageHint" class="field-hint">Будет использован встроенный шаблон Lorem ipsum.</div>
            </label>
          </div>
          <div class="settings-toggle" id="testModeControl">
            <label class="chip switch">
              <input type="checkbox" id="TESTMODE" />
              <span>Тестовый режим TX/RX (SendMsg_BR/received_msg)</span>
            </label>
            <div id="testModeHint" class="field-hint">Состояние не загружено.</div>
          </div>
        </section>
        <div class="settings-actions actions">
          <button type="button" id="btnSaveSettings" class="btn">Сохранить</button>
          <button type="button" id="btnApplySettings" class="btn btn-primary">Применить</button>
          <button type="button" id="btnExportSettings" class="btn">Экспорт</button>
          <button type="button" id="btnImportSettings" class="btn">Импорт</button>
          <button type="button" id="btnClearCache" class="btn danger">Очистить</button>
        </div>
        <button type="button" id="INFO" class="btn" data-cmd="INFO">INFO</button>
        <section class="settings-section">
          <h3>Тема</h3>
          <div class="settings-group">
            <div class="settings-toggle" id="autoNightControl">
              <label class="chip switch">
                <input type="checkbox" id="autoNightMode" />
                <span>Авто ночной режим</span>
              </label>
              <div id="autoNightHint" class="field-hint">Ночная тема включается автоматически с 21:00 до 07:00.</div>
            </div>
          </div>
        </section>
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
        <button id="btnKeyGenPeer" class="btn" disabled>KEYGEN PEER</button>
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
      <div id="recvDiag" class="debug-metrics">
        <div class="debug-metrics__header">
          <h3>Мониторинг опроса RSTS</h3>
          <span id="recvDiagStatus" class="debug-metrics__status">Ожидание запуска</span>
          <div id="recvDiagMode" class="debug-metrics__mode small muted">Push-канал не активен</div>
        </div>
        <p class="small muted">Для push-подписки требуется новая прошивка с поддержкой SSE. Если канал недоступен, интерфейс автоматически использует прежний опрос командой RSTS.</p>
        <div class="debug-metrics-grid">
          <div class="debug-metrics-item">
            <span class="debug-metrics-item__label">Интервал опроса</span>
            <span id="recvDiagInterval" class="debug-metrics-item__value">—</span>
          </div>
          <div class="debug-metrics-item">
            <span class="debug-metrics-item__label">Последний старт</span>
            <span id="recvDiagLastStart" class="debug-metrics-item__value">—</span>
          </div>
          <div class="debug-metrics-item">
            <span class="debug-metrics-item__label">Последний ответ</span>
            <span id="recvDiagLastFinish" class="debug-metrics-item__value">—</span>
          </div>
          <div class="debug-metrics-item">
            <span class="debug-metrics-item__label">Длительность запроса</span>
            <span id="recvDiagDuration" class="debug-metrics-item__value">—</span>
          </div>
          <div class="debug-metrics-item">
            <span class="debug-metrics-item__label">Средняя длительность</span>
            <span id="recvDiagAvg" class="debug-metrics-item__value">—</span>
          </div>
          <div class="debug-metrics-item">
            <span class="debug-metrics-item__label">Фактический интервал</span>
            <span id="recvDiagGap" class="debug-metrics-item__value">—</span>
          </div>
          <div class="debug-metrics-item">
            <span class="debug-metrics-item__label">Выполнено/успехи/ошибки</span>
            <span id="recvDiagTotals" class="debug-metrics-item__value">0 / 0 ✓ / 0 ✗</span>
          </div>
          <div class="debug-metrics-item">
            <span class="debug-metrics-item__label">Таймауты</span>
            <span id="recvDiagTimeouts" class="debug-metrics-item__value">0</span>
          </div>
          <div class="debug-metrics-item">
            <span class="debug-metrics-item__label">Блокировки</span>
            <span id="recvDiagOverlaps" class="debug-metrics-item__value">0</span>
          </div>
        </div>
        <div class="debug-metrics-foot">
          <div id="recvDiagLastError" class="debug-metrics-foot__entry" hidden>Ошибок нет</div>
          <div id="recvDiagBlocked" class="debug-metrics-foot__entry" hidden>Блокировок не было</div>
        </div>
      </div>
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
  <script src="libs/geostat_tle.js"></script>
  <script src="libs/sha256.js"></script>
  <script src="libs/mgrs.js"></script>
  <script src="script.js"></script>
</body>
</html>
)~~~";

// web/style.css
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
  --stability-rssi: #f97316;
  --stability-snr: #38bdf8;
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
  --stability-rssi: #ea580c;
  --stability-snr: #2563eb;
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
  --stability-rssi: #fb7185;
  --stability-snr: #facc15;
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
.nav-overlay {
  position: fixed;
  inset: 0;
  background: rgba(9, 15, 28, .55);
  backdrop-filter: blur(6px);
  z-index: 40;
  opacity: 0;
  pointer-events: none;
  transition: opacity .2s ease;
}
.nav-overlay.visible { opacity: 1; pointer-events: auto; }
body.nav-open { overflow: hidden; }
.brand {
  display:flex;
  gap:.6rem;
  align-items:center;
  font-weight:800;
  letter-spacing:.3px;
  white-space: nowrap;
  font-size: clamp(1rem, 2.8vw, 1.5rem);
}
.brand .tag { font-size:.75rem; padding:.15rem .45rem; border-radius:.5rem; background: var(--panel-2); border:1px solid color-mix(in oklab, var(--panel-2) 70%, black 30%); color: var(--muted); }

/* Дополнительная подстройка логотипа на очень узких экранах */
@media (max-width: 480px) {
  .brand {
    gap: .45rem;
    font-size: clamp(.95rem, 6vw, 1.2rem);
  }

  .brand .tag {
    font-size: .62rem;
    padding: .1rem .3rem;
  }
}

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
.nav a .nav-badge {
  /* Бейдж непрочитанных сообщений в шапке */
  display: inline-flex;
  align-items: center;
  justify-content: center;
  min-width: 1.35rem;
  height: 1.35rem;
  padding: 0 .35rem;
  border-radius: 999px;
  background: color-mix(in oklab, var(--danger) 78%, black 15%);
  color: #fff;
  font-size: .68rem;
  font-weight: 800;
  line-height: 1;
  letter-spacing: .02em;
  box-shadow: 0 4px 10px rgba(239, 68, 68, .35);
  transform-origin: center;
  will-change: transform, opacity;
}
:root.light .nav a .nav-badge {
  background: color-mix(in oklab, var(--danger) 85%, white 20%);
  color: #fff;
  box-shadow: 0 4px 10px rgba(220, 38, 38, .35);
}
:root.red .nav a .nav-badge {
  background: color-mix(in oklab, var(--accent-2) 80%, black 10%);
  box-shadow: 0 4px 10px rgba(244, 63, 94, .4);
}
.nav a .nav-badge.nav-badge-pop {
  animation: nav-badge-pop .45s cubic-bezier(.2, .75, .3, 1.2);
}
@keyframes nav-badge-pop {
  0% { transform: scale(.6); opacity: 0; }
  55% { transform: scale(1.08); opacity: 1; }
  75% { transform: scale(.95); }
  100% { transform: scale(1); opacity: 1; }
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
.chat-area { position: relative; }
.chat-area.drop-active::after {
  content: "Отпустите, чтобы отправить";
  position: absolute;
  inset: 0;
  border: 2px dashed var(--accent);
  border-radius: .8rem;
  background: color-mix(in oklab, var(--accent) 12%, transparent 88%);
  color: var(--text);
  font-weight: 600;
  display: grid;
  place-items: center;
  text-align: center;
  pointer-events: none;
  padding: 1rem;
  backdrop-filter: blur(2px);
}
.chat-log {
  height: min(52vh, 560px);
  overflow-y: auto;
  overflow-x: hidden;
  display:flex; flex-direction:column; gap:.55rem;
  padding:.5rem;
  border-radius: .8rem;
  background: var(--panel-2);
  border: 1px solid color-mix(in oklab, var(--panel-2) 70%, black 30%);
}
.chat-scroll-down {
  position: absolute;
  right: .85rem;
  bottom: .9rem;
  width: 46px;
  height: 46px;
  border-radius: 50%;
  border: 1px solid color-mix(in oklab, var(--panel-2) 70%, black 30%);
  background: linear-gradient(180deg, color-mix(in oklab, var(--panel-2) 92%, white 8%), var(--panel-2));
  color: var(--text);
  display: grid;
  place-items: center;
  cursor: pointer;
  box-shadow: 0 12px 24px rgba(0,0,0,.35);
  transition: transform .15s ease, box-shadow .15s ease;
}
.chat-scroll-down span { font-size: 1.3rem; line-height: 1; }
.chat-scroll-down:hover { transform: translateY(-2px); box-shadow: 0 18px 30px rgba(0,0,0,.38); }
.chat-scroll-down:active { transform: translateY(0); box-shadow: 0 10px 18px rgba(0,0,0,.32); }
.chat-scroll-down[hidden] { display: none; }
.chat-receiving-indicator {
  position: absolute;
  left: .9rem;
  bottom: .95rem;
  display: inline-flex;
  align-items: center;
  gap: .55rem;
  padding: .45rem .7rem;
  border-radius: .85rem;
  border: 1px solid color-mix(in oklab, var(--panel-2) 70%, black 30%);
  background: linear-gradient(180deg, color-mix(in oklab, var(--panel-2) 92%, white 8%), var(--panel-2));
  color: var(--text);
  font-size: .82rem;
  font-weight: 600;
  box-shadow: 0 14px 26px rgba(0,0,0,.32);
  pointer-events: none;
  opacity: 0;
  transform: translateY(6px);
  transition: opacity .2s ease, transform .2s ease;
  z-index: 5;
}
.chat-receiving-indicator.active {
  opacity: 1;
  transform: translateY(0);
}
.chat-receiving-indicator .indicator-icon {
  position: relative;
  width: 18px;
  height: 18px;
  flex: 0 0 18px;
}
.chat-receiving-indicator .indicator-icon::before {
  content: "";
  position: absolute;
  inset: 5px;
  border-radius: 50%;
  background: var(--accent);
  box-shadow: 0 0 12px color-mix(in oklab, var(--accent) 85%, black 15%);
}
.chat-receiving-indicator .indicator-icon::after {
  content: "";
  position: absolute;
  inset: 0;
  border-radius: 50%;
  border: 2px solid color-mix(in oklab, var(--accent) 65%, transparent 35%);
  animation: chat-receive-pulse 1.4s ease-out infinite;
}
.chat-receiving-indicator .indicator-text {
  white-space: nowrap;
}
.chat-irq-status {
  margin-top: .75rem;
  padding: .65rem .95rem;
  border-radius: .85rem;
  background: color-mix(in oklab, var(--panel) 88%, black 12%);
  border: 1px solid color-mix(in oklab, var(--panel-2) 68%, black 32%);
  display: flex;
  align-items: flex-start;
  gap: .75rem;
  font-size: .92rem;
  color: var(--muted);
  box-shadow: 0 12px 26px rgba(0,0,0,.18);
  transition: border-color .2s ease, color .2s ease, background .2s ease, box-shadow .2s ease;
}
.chat-irq-status::before {
  content: "⚡";
  font-size: 1.15rem;
  line-height: 1;
  color: color-mix(in oklab, var(--accent-2) 70%, white 30%);
}
.chat-irq-status .chat-irq-text {
  display: flex;
  flex-direction: column;
  gap: .25rem;
}
.chat-irq-status .chat-irq-message {
  font-weight: 600;
}
.chat-irq-status .chat-irq-meta {
  font-size: .78rem;
  color: var(--muted);
}
.chat-irq-status.active {
  color: var(--text);
  border-color: color-mix(in oklab, var(--accent) 50%, transparent);
  background: color-mix(in oklab, var(--panel) 82%, black 18%);
  box-shadow: 0 16px 30px rgba(0,0,0,.22);
}
.chat-irq-status.active::before {
  color: color-mix(in oklab, var(--accent) 70%, white 30%);
}
.chat-irq-status.active .chat-irq-meta {
  color: color-mix(in oklab, var(--accent-2) 55%, var(--muted) 45%);
}
@keyframes chat-receive-pulse {
  0% {
    transform: scale(.6);
    opacity: .85;
  }
  60% {
    transform: scale(1.4);
    opacity: 0;
  }
  100% {
    transform: scale(1.55);
    opacity: 0;
  }
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
  overflow-wrap: anywhere;
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
.msg.rx .bubble {
  background: color-mix(in oklab, var(--good) 18%, var(--panel) 82%);
  border-color: color-mix(in oklab, var(--good) 45%, var(--panel) 55%);
}
.msg.rx .bubble-text { font-weight:600; }
.msg.rx time { color: color-mix(in oklab, var(--good) 55%, var(--muted) 45%); }
.msg.rx.progress .bubble {
  background: color-mix(in oklab, var(--good) 12%, var(--panel) 88%);
  border-style: dashed;
}
.msg.rx.progress .bubble-text { font-weight:600; }
.bubble-text.with-spinner {
  display: inline-flex;
  align-items: center;
  gap: .45rem;
}
.bubble-spinner {
  width: .85rem;
  height: .85rem;
  border-radius: 50%;
  border: 2px solid color-mix(in oklab, var(--good) 80%, transparent 20%);
  border-top-color: color-mix(in oklab, var(--good) 20%, transparent 80%);
  animation: bubble-spinner 0.8s linear infinite;
  flex: 0 0 auto;
}
@keyframes bubble-spinner {
  0% { transform: rotate(0deg); }
  100% { transform: rotate(360deg); }
}
.bubble-text { line-height:1.55; }
/* Полный текст системного ответа */
.bubble-detail {
  margin-top:.45rem;
  padding:.45rem .55rem;
  background: color-mix(in oklab, var(--panel) 88%, black 12%);
  border-radius:.6rem;
  border:1px dashed color-mix(in oklab, var(--panel-2) 70%, black 30%);
  font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", "Courier New", monospace;
  font-size:.78rem;
  line-height:1.5;
  white-space:pre-wrap;
  word-break:break-word;
}
.bubble-meta {
  display:flex;
  align-items:center;
  gap:.35rem;
  margin-top:.45rem;
  font-size:.78rem;
  color: var(--muted);
  justify-content:flex-start;
}
.bubble-meta.bubble-rx {
  color: color-mix(in oklab, var(--good) 55%, var(--muted) 45%);
  gap:.45rem;
  font-size:.72rem;
  flex-wrap:wrap;
}
.bubble-rx-name {
  font-weight:600;
  letter-spacing:.02em;
}
.bubble-rx-len {
  font-variant-numeric: tabular-nums;
  opacity:.85;
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

.chat-input { display:flex; gap:.6rem; margin-top:.8rem; align-items:center; flex-wrap:wrap; }
.chat-input button { min-width: 7.5rem; }
.chat-upload { display:flex; align-items:center; gap:.5rem; }
.chat-upload .icon-btn { min-width: 2.7rem; min-height: 2.7rem; font-size: 1.25rem; }
.chat-upload .icon-btn span { pointer-events: none; }
.chat-image-profile {
  background: var(--panel-2);
  border: 1px solid color-mix(in oklab, var(--panel-2) 70%, black 30%);
  border-radius: .6rem;
  padding: .35rem .6rem;
  color: var(--text);
  font-weight: 600;
  min-width: 6rem;
}
.chat-image-profile:focus {
  outline: none;
  box-shadow: 0 0 0 2px var(--ring);
}
.bubble-image { margin-top: .6rem; }
.bubble-image-frame {
  border-radius: .75rem;
  overflow: hidden;
  border: 1px solid color-mix(in oklab, var(--panel-2) 70%, black 30%);
  background: #000;
}
.bubble-image-frame img { display:block; width:100%; height:auto; }
.bubble-image-caption {
  margin-top: .35rem;
  font-size: .78rem;
  color: var(--muted);
  display:flex;
  gap:.4rem;
  align-items:center;
  flex-wrap:wrap;
}
.chat-input-row { display:grid; gap:.6rem; grid-template-columns: 1fr; margin-top:.8rem; }
.cmd-buttons { display:flex; gap:.4rem; flex-wrap:wrap; }
.send-row { position: relative; display:flex; gap:.5rem; }
#chatInput { flex:1; background: var(--panel-2); color: var(--text); border: 1px solid transparent; border-radius: .9rem; padding: .7rem .9rem; outline: none; }
#chatInput:focus { border-color: var(--ring); box-shadow: 0 0 0 3px var(--ring); }
.chat-commands {
  margin-top: 1rem;
  padding: .9rem 1rem;
  border-radius: .95rem;
  border: 1px solid color-mix(in oklab, var(--panel-2) 70%, black 30%);
  background: color-mix(in oklab, var(--panel-2) 92%, black 8%);
}
.chat-commands summary {
  display:flex;
  align-items:center;
  gap:.45rem;
  font-weight:700;
  cursor:pointer;
  list-style:none;
  text-transform:uppercase;
  letter-spacing:.08em;
  color: color-mix(in oklab, var(--text) 85%, var(--muted) 15%);
}
.chat-commands summary::-webkit-details-marker { display:none; }
.chat-commands summary::after {
  content:"▾";
  margin-left:auto;
  transition: transform .2s ease;
  font-size:.9rem;
}
.chat-commands[open] summary::after { transform: rotate(180deg); }
.chat-commands .cmd-buttons { margin-top:.75rem; }
.chat-commands .cmd-inline { margin-top:.6rem; }
@media (max-width: 640px) {
  .chat-input { flex-direction: column; align-items: stretch; }
  .chat-input button { width: 100%; min-width: 0; }
  .send-row { flex-direction: column; align-items: stretch; gap: .4rem; }
  .chat-upload {
    width: 100%;
    display: grid;
    grid-template-columns: minmax(2.75rem, 3rem) 1fr;
    gap: .5rem;
    align-items: center;
  }
  .chat-upload .icon-btn {
    width: 100%;
    min-width: 0;
  }
  .chat-image-profile {
    width: 100%;
  }
  .chat-irq-status {
    flex-direction: column;
    align-items: stretch;
    gap: .5rem;
  }
  .chat-irq-status::before {
    margin-bottom: -.15rem;
  }
}
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
.btn.waiting {
  color: #001018;
  border-color: transparent;
  animation: key-wait 1.1s ease-in-out infinite;
  background: color-mix(in oklab, var(--accent) 45%, var(--panel) 55%);
}
.btn.waiting:hover { background: color-mix(in oklab, var(--danger) 45%, var(--panel) 55%); }
.btn.waiting:disabled { opacity: 1; }
@keyframes key-wait {
  0% { background: color-mix(in oklab, var(--accent) 45%, var(--panel) 55%); box-shadow: 0 0 0 rgba(248,113,113,0); }
  50% { background: color-mix(in oklab, var(--danger) 45%, var(--panel) 55%); box-shadow: 0 0 12px rgba(248,113,113,.35); }
  100% { background: color-mix(in oklab, var(--accent) 45%, var(--panel) 55%); box-shadow: 0 0 0 rgba(248,113,113,0); }
}

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

/* Pointing tab */
.pointing-intro { margin-top:.4rem; max-width:640px; }
.pointing-title { margin:0; font-size:clamp(2.6rem, 6vw, 3.4rem); font-weight:800; letter-spacing:.12em; line-height:1.1; }
.pointing-summary { margin-top:.8rem; padding:.7rem .85rem; border-radius:.95rem; display:flex; flex-wrap:wrap; gap:.55rem .7rem; align-items:center; }
.pointing-summary-chip { display:inline-flex; align-items:center; gap:.35rem; padding:.45rem .7rem; border-radius:.75rem; border:1px solid color-mix(in oklab, var(--panel-2) 70%, black 30%); background: color-mix(in oklab, var(--panel-2) 92%, black 8%); font-weight:600; font-size:.82rem; transition:background .2s ease, border-color .2s ease, transform .2s ease; }
.pointing-summary-chip .pointing-summary-icon { font-size:.6rem; line-height:1; }
.pointing-summary-chip[data-state="ok"] { border-color: color-mix(in oklab, var(--good) 45%, var(--panel-2) 55%); background: color-mix(in oklab, var(--good) 20%, var(--panel-2) 80%); color: color-mix(in oklab, var(--good) 75%, white 25%); }
.pointing-summary-chip[data-state="warn"] { border-color: color-mix(in oklab, var(--danger) 50%, var(--panel-2) 50%); background: color-mix(in oklab, var(--danger) 18%, var(--panel-2) 82%); color: color-mix(in oklab, var(--danger) 80%, white 20%); }
.pointing-summary-chip[data-state="pending"] { border-color: color-mix(in oklab, var(--accent) 55%, var(--panel-2) 45%); background: color-mix(in oklab, var(--accent) 20%, var(--panel-2) 80%); color: color-mix(in oklab, var(--accent) 70%, white 30%); }
.pointing-summary-chip[data-state="idle"] { opacity:.75; }
.pointing-summary-chip:hover { transform:translateY(-2px); }
@media (max-width: 520px) {
  .pointing-summary { flex-wrap: nowrap; gap:.45rem; }
  .pointing-summary-chip { flex:1 1 0; justify-content:center; min-width:0; }
}
.pointing-grid { display:grid; gap:1rem; margin-top:1rem; }
.pointing-card { border-radius:.95rem; padding:1rem; display:flex; flex-direction:column; gap:.8rem; }
.pointing-card h3 { margin:0; font-size:1.05rem; }
.pointing-observer { border:none; padding:0; background:transparent; }
.pointing-observer summary { list-style:none; display:flex; align-items:center; justify-content:space-between; gap:.75rem; cursor:pointer; font-weight:600; font-size:1.05rem; color: var(--text); }
.pointing-observer summary::-webkit-details-marker { display:none; }
.pointing-observer summary .value { font-family:'JetBrains Mono','Fira Code','SFMono-Regular',monospace; letter-spacing:.08em; color: color-mix(in oklab, var(--accent) 70%, white 30%); text-transform:uppercase; }
.pointing-observer[open] summary { border-bottom:1px solid color-mix(in oklab, var(--panel-2) 70%, black 30%); padding-bottom:.6rem; }
.pointing-observer-body { display:flex; flex-direction:column; gap:.75rem; padding-top:.75rem; }
.pointing-observer-grid { display:grid; gap:.6rem; grid-template-columns:repeat(auto-fit, minmax(160px, 1fr)); }
.pointing-observer-grid input { width:100%; background: var(--panel-2); border:1px solid color-mix(in oklab, var(--panel-2) 70%, black 30%); border-radius:.6rem; padding:.5rem .65rem; color: var(--text); font-family:'JetBrains Mono','Fira Code','SFMono-Regular',monospace; letter-spacing:.06em; text-transform:uppercase; }
.pointing-observer-grid input:focus { border-color: var(--ring); box-shadow:0 0 0 2px var(--ring); outline:none; }
.pointing-observer-meta { display:flex; flex-wrap:wrap; gap:1rem; font-size:.8rem; color: var(--muted); }
.pointing-observer-meta div { display:flex; flex-direction:column; gap:.2rem; min-width:120px; }
.pointing-observer-meta .label { font-size:.7rem; text-transform:uppercase; letter-spacing:.05em; color: color-mix(in oklab, var(--muted) 80%, var(--text) 20%); }
.pointing-observer-meta strong { font-family:'JetBrains Mono','Fira Code','SFMono-Regular',monospace; color: var(--text); letter-spacing:.05em; }
.pointing-target-grid { display:flex; flex-wrap:wrap; gap:1.25rem; }
.pointing-target-metric { display:flex; flex-direction:column; gap:.25rem; }
.pointing-target-metric .label { font-size:.75rem; text-transform:uppercase; letter-spacing:.05em; color: var(--muted); }
.pointing-target-metric strong { font-size:1.45rem; font-weight:700; font-family:'JetBrains Mono','Fira Code','SFMono-Regular',monospace; letter-spacing:.08em; color: color-mix(in oklab, var(--accent) 65%, white 35%); }
.pointing-compass { display:flex; flex-direction:column; align-items:center; justify-content:center; gap:.55rem; }
.pointing-compass-dial { position:relative; width:min(260px, 70vw); aspect-ratio:1 / 1; border-radius:50%; border:1px solid color-mix(in oklab, var(--panel-2) 68%, black 32%); background: radial-gradient(circle at 50% 50%, color-mix(in oklab, var(--panel-2) 96%, white 4%), color-mix(in oklab, var(--panel) 88%, black 12%)); display:flex; align-items:center; justify-content:center; overflow:hidden; }
.pointing-compass-dial::before {
  content:"";
  position:absolute;
  inset:12%;
  border-radius:50%;
  background: conic-gradient(from 180deg, color-mix(in oklab, var(--accent) 15%, transparent) 0deg 90deg, color-mix(in oklab, var(--good) 16%, transparent) 90deg 180deg, color-mix(in oklab, var(--accent-2) 15%, transparent) 180deg 270deg, color-mix(in oklab, var(--danger) 18%, transparent) 270deg 360deg);
  opacity:.15;
  pointer-events:none;
}
.pointing-compass-radar { position:absolute; left:14%; top:14%; width:72%; height:72%; border-radius:50%; pointer-events:none; }
.pointing-compass-sat { position:absolute; left:50%; top:50%; width:10px; height:10px; border-radius:50%; padding:0; display:flex; align-items:center; justify-content:center; transform:translate(-50%, -50%) scale(.55); background: color-mix(in oklab, var(--pointing-sat-color, var(--accent)) 50%, var(--panel) 50%); border:2px solid color-mix(in oklab, var(--pointing-sat-color, var(--accent)) 60%, black 40%); box-shadow:0 3px 8px rgba(0,0,0,.28); pointer-events:auto; cursor:pointer; transition:transform .2s ease, box-shadow .2s ease, border-color .2s ease, filter .2s ease; box-sizing:border-box; line-height:1; aspect-ratio:1 / 1; }
.pointing-compass-sat:hover { transform:translate(-50%, -50%) scale(.8); box-shadow:0 6px 14px rgba(0,0,0,.32); }
.pointing-compass-sat:focus-visible { outline:2px solid var(--ring); outline-offset:2px; }
.pointing-compass-sat.active { transform:translate(-50%, -52%) scale(.88); border-color: color-mix(in oklab, var(--pointing-sat-color, var(--accent)) 72%, white 28%); filter:brightness(1.08); box-shadow:0 0 0 2px color-mix(in oklab, var(--pointing-sat-color, var(--accent)) 38%, white 62%); }
.pointing-compass-sat .pointing-compass-label { position:absolute; left:50%; top:-.4rem; transform:translate(-50%, -100%); padding:.25rem .55rem; border-radius:.55rem; background: color-mix(in oklab, var(--pointing-sat-color, var(--accent)) 22%, var(--panel-2) 78%); border:1px solid color-mix(in oklab, var(--pointing-sat-color, var(--accent)) 55%, black 45%); color: var(--text); font-size:.68rem; font-weight:600; text-shadow:0 1px 2px rgba(0,0,0,.28); white-space:nowrap; pointer-events:none; opacity:0; transition:opacity .2s ease, transform .2s ease; z-index:5; }
.pointing-compass-sat .pointing-compass-label::after { content:""; position:absolute; left:50%; top:100%; width:8px; height:8px; transform:translate(-50%, -5px) rotate(45deg); background: color-mix(in oklab, var(--pointing-sat-color, var(--accent)) 22%, var(--panel-2) 78%); border:1px solid color-mix(in oklab, var(--pointing-sat-color, var(--accent)) 55%, black 45%); border-left:none; border-top:none; box-shadow:0 3px 6px rgba(0,0,0,.22); }
.pointing-compass-sat:hover .pointing-compass-label, .pointing-compass-sat:focus-visible .pointing-compass-label, .pointing-compass-sat.active .pointing-compass-label { opacity:1; transform:translate(-50%, -115%); }
.pointing-compass-axes {
  position:absolute;
  inset:16%;
  pointer-events:none;
  background:
    linear-gradient(color-mix(in oklab, var(--panel-2) 70%, black 30%), color-mix(in oklab, var(--panel-2) 70%, black 30%)) center/2px 100% no-repeat,
    linear-gradient(color-mix(in oklab, var(--panel-2) 70%, black 30%), color-mix(in oklab, var(--panel-2) 70%, black 30%)) center/100% 2px no-repeat;
  opacity:.45;
}
.pointing-compass-rose {
  position:absolute;
  inset:6%;
  pointer-events:none;
  font-weight:700;
  font-size:.7rem;
  letter-spacing:.16em;
  text-transform:uppercase;
  color: color-mix(in oklab, var(--text) 80%, var(--panel-2) 20%);
}
.pointing-compass-rose span {
  position:absolute;
  transform:translate(-50%, -50%);
}
.pointing-compass-rose span[data-dir="south"] { top:8%; left:50%; }
.pointing-compass-rose span[data-dir="north"] { bottom:8%; left:50%; }
.pointing-compass-rose span[data-dir="east"] { top:50%; right:6%; }
.pointing-compass-rose span[data-dir="west"] { top:50%; left:6%; }
.pointing-compass-legend { display:flex; flex-wrap:wrap; justify-content:center; gap:.5rem .9rem; margin-top:.6rem; }
.pointing-compass-legend span { display:flex; align-items:center; gap:.35rem; font-size:.7rem; text-transform:uppercase; letter-spacing:.06em; color: color-mix(in oklab, var(--text) 78%, var(--panel-2) 22%); opacity:.85; }
.pointing-compass-legend span::before { content:""; width:8px; height:8px; border-radius:50%; background: color-mix(in oklab, var(--pointing-sat-color, var(--accent)) 78%, white 22%); box-shadow:0 0 0 2px color-mix(in oklab, var(--pointing-sat-color, var(--accent)) 45%, black 55%); }
.pointing-compass-center { width:14px; height:14px; border-radius:50%; background: color-mix(in oklab, var(--panel-2) 85%, black 15%); border:2px solid color-mix(in oklab, var(--panel-2) 70%, black 30%); }
.pointing-compass-graduations { position:absolute; inset:16%; border-radius:50%; border:1px dashed color-mix(in oklab, var(--panel-2) 70%, black 30%); opacity:.45; }
.pointing-min-el { display:flex; align-items:center; gap:.4rem; font-size:.85rem; color: var(--muted); }
.pointing-min-el input { width:140px; }
.pointing-min-el-value { font-weight:700; color: var(--text); min-width:2.5rem; text-align:right; }
.pointing-min-el-footer { margin-top:auto; padding-top:.6rem; border-top:1px solid color-mix(in oklab, var(--panel-2) 55%, black 45%); }
.pointing-select { display:flex; flex-direction:column; gap:.35rem; font-weight:600; }
.pointing-select select { background: var(--panel-2); color: var(--text); border:1px solid color-mix(in oklab, var(--panel-2) 70%, black 30%); border-radius:.6rem; padding:.55rem .7rem; outline:none; }
.pointing-select select:focus { border-color: var(--ring); box-shadow:0 0 0 3px var(--ring); }
.pointing-sat-details { display:grid; grid-template-columns:repeat(auto-fit, minmax(180px, 1fr)); gap:.4rem .75rem; font-size:.85rem; color: var(--muted); }
.pointing-sat-details strong { color: var(--text); }
.pointing-sat-list { display:flex; flex-direction:column; gap:1rem; margin-top:.5rem; }
.pointing-sat-group { display:flex; flex-direction:column; gap:.65rem; padding: .35rem 0 .45rem 1rem; border-left:3px solid color-mix(in oklab, var(--pointing-sat-color, var(--accent)) 45%, var(--panel-2) 55%); border-radius:.75rem; background: color-mix(in oklab, var(--pointing-sat-color, var(--accent)) 8%, transparent); }
.pointing-sat-group-title { font-size:.78rem; text-transform:uppercase; letter-spacing:.08em; font-weight:700; color: color-mix(in oklab, var(--pointing-sat-color, var(--accent)) 60%, var(--text) 40%); display:flex; align-items:center; gap:.45rem; }
.pointing-sat-group-title::before { content:""; width:12px; height:12px; border-radius:50%; background: color-mix(in oklab, var(--pointing-sat-color, var(--accent)) 80%, white 20%); box-shadow:0 0 0 2px color-mix(in oklab, var(--pointing-sat-color, var(--accent)) 45%, black 55%); }
.pointing-sat-group-list { display:flex; flex-direction:column; gap:.6rem; }
.pointing-sat-entry { border-radius:.85rem; padding:.65rem .8rem; border:1px solid color-mix(in oklab, var(--pointing-sat-color, var(--accent)) 35%, black 65%); background: color-mix(in oklab, var(--pointing-sat-color, var(--accent)) 12%, var(--panel-2) 88%); display:flex; flex-direction:column; gap:.35rem; text-align:left; cursor:pointer; transition:background .2s ease, transform .2s ease, border-color .2s ease, box-shadow .2s ease; }
.pointing-sat-entry:hover { background: color-mix(in oklab, var(--pointing-sat-color, var(--accent)) 22%, var(--panel-2) 78%); transform:translateY(-1px); }
.pointing-sat-entry:focus-visible { outline:2px solid var(--ring); outline-offset:2px; }
.pointing-sat-entry.active { border-color: color-mix(in oklab, var(--pointing-sat-color, var(--accent)) 70%, white 30%); box-shadow: inset 0 0 0 1px color-mix(in oklab, var(--pointing-sat-color, var(--accent)) 45%, white 55%); }
.pointing-sat-name { font-weight:700; }
.pointing-sat-meta { display:flex; flex-wrap:wrap; gap:.5rem; font-size:.78rem; color: var(--muted); }
.pointing-empty { padding:.8rem; border-radius:.8rem; background: color-mix(in oklab, var(--panel-2) 90%, black 10%); border:1px dashed color-mix(in oklab, var(--panel-2) 65%, black 35%); text-align:center; }
.pointing-card-header { display:flex; flex-wrap:wrap; align-items:center; justify-content:space-between; gap:.5rem; }
.pointing-card-wide { grid-column:1 / -1; }
.pointing-horizon { position:relative; border-radius:1rem; padding:1.25rem 1.1rem 1.5rem; border:1px solid color-mix(in oklab, var(--panel-2) 70%, black 30%); background: linear-gradient(180deg, color-mix(in oklab, var(--panel-2) 72%, var(--accent-2) 6%), color-mix(in oklab, var(--panel) 90%, black 10%)); overflow:hidden; min-height:160px; }
.pointing-horizon::before { content:""; position:absolute; inset:auto 0 0; height:32%; background: linear-gradient(180deg, color-mix(in oklab, var(--panel) 82%, black 18%), color-mix(in oklab, #020617 88%, black 12%)); opacity:.85; z-index:0; pointer-events:none; }
.pointing-horizon::after { content:""; position:absolute; inset:42px 18px 22px; background:
    repeating-linear-gradient(to right, color-mix(in oklab, var(--panel-2) 65%, black 35%) 0, color-mix(in oklab, var(--panel-2) 65%, black 35%) 1px, transparent 1px, transparent calc(100% / 36)),
    repeating-linear-gradient(to top, color-mix(in oklab, var(--panel-2) 65%, black 35%) 0, color-mix(in oklab, var(--panel-2) 65%, black 35%) 1px, transparent 1px, transparent calc(100% / 9));
  opacity:.32; pointer-events:none; z-index:0; }
.pointing-horizon-azimuths { position:absolute; top:14px; left:18px; right:18px; display:flex; justify-content:space-between; font-size:.66rem; text-transform:uppercase; letter-spacing:.08em; color: color-mix(in oklab, var(--muted) 70%, var(--text) 30%); pointer-events:none; z-index:0; opacity:.85; }
.pointing-horizon-azimuths span { display:flex; align-items:center; gap:.35rem; }
.pointing-horizon-azimuths span::before { content:""; display:block; width:1px; height:14px; background: color-mix(in oklab, var(--panel-2) 60%, black 40%); }
.pointing-horizon-track { position:absolute; inset:46px 18px 24px; z-index:1; }
.pointing-horizon-empty { position:relative; z-index:1; margin:0; text-align:center; padding-top:2.1rem; }
.pointing-horizon-sat { position:absolute; bottom:0; --pointing-sat-offset:0; transform:translate(-50%, calc(var(--pointing-sat-offset, 0) * -1.25rem)) scale(.55); border:none; width:18px; height:18px; border-radius:50%; padding:0; display:flex; align-items:center; justify-content:center; line-height:1; aspect-ratio:1 / 1; background: color-mix(in oklab, var(--pointing-sat-color, var(--accent)) 48%, var(--panel) 52%); border:3px solid color-mix(in oklab, var(--pointing-sat-color, var(--accent)) 65%, black 35%); box-shadow:0 4px 12px rgba(0,0,0,.32); cursor:pointer; transition:transform .2s ease, box-shadow .2s ease, filter .2s ease; z-index:2; box-sizing:border-box; }
.pointing-horizon-sat:hover { transform:translate(-50%, calc(var(--pointing-sat-offset, 0) * -1.25rem - 4px)) scale(.8); box-shadow:0 10px 20px rgba(0,0,0,.4); }
.pointing-horizon-sat.active { filter:brightness(1.08); box-shadow:0 0 0 3px color-mix(in oklab, var(--pointing-sat-color, var(--accent)) 45%, white 55%); }
.pointing-horizon-sat:focus-visible { outline:2px solid var(--ring); outline-offset:4px; }
.pointing-horizon-sat .pointing-horizon-label { position:absolute; left:50%; bottom:130%; transform:translate(-50%, 6px); padding:.3rem .55rem; border-radius:.65rem; background: color-mix(in oklab, var(--pointing-sat-color, var(--accent)) 22%, var(--panel-2) 78%); border:1px solid color-mix(in oklab, var(--pointing-sat-color, var(--accent)) 55%, black 45%); color: var(--text); font-weight:600; font-size:.72rem; white-space:nowrap; pointer-events:none; opacity:0; transition:opacity .2s ease, transform .2s ease; text-shadow:0 1px 2px rgba(0,0,0,.28); }
.pointing-horizon-sat .pointing-horizon-label::after { content:""; position:absolute; left:50%; top:100%; width:8px; height:8px; transform:translate(-50%, -5px) rotate(45deg); background: color-mix(in oklab, var(--pointing-sat-color, var(--accent)) 22%, var(--panel-2) 78%); border:1px solid color-mix(in oklab, var(--pointing-sat-color, var(--accent)) 55%, black 45%); border-left:none; border-top:none; box-shadow:0 3px 6px rgba(0,0,0,.22); }
.pointing-horizon-sat:hover .pointing-horizon-label, .pointing-horizon-sat:focus-visible .pointing-horizon-label, .pointing-horizon-sat.active .pointing-horizon-label { opacity:1; transform:translate(-50%, -4px); }
@media (min-width: 960px) { .pointing-grid { grid-template-columns:repeat(2, minmax(0, 1fr)); } }


/* Tables */
.table-wrap { overflow:auto; border-radius:.8rem; border:1px solid color-mix(in oklab, var(--panel-2) 70%, black 30%); background: var(--panel-2); }
.table-wrap.pretty table thead th { position: sticky; top: 0; background: linear-gradient(180deg, var(--panel-2), color-mix(in oklab, var(--panel-2) 80%, white 20%)); }
table { border-collapse: collapse; width: 100%; min-width: 680px; }
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
tbody tr.signal {
  background: color-mix(in oklab, var(--good) 18%, var(--panel-2) 82%);
  box-shadow: inset 4px 0 0 color-mix(in oklab, var(--good) 65%, white 35%);
  color: color-mix(in oklab, var(--text) 85%, white 15%);
}
tbody tr.signal td { font-weight:600; }
tbody tr.crc-error { background: color-mix(in oklab, #f97316 20%, white); /* ~#fed7aa, оранжевый фон */ }
tbody tr.no-response { background: color-mix(in oklab, var(--muted) 15%, white); color:#374151; /* ~#e5e7eb, серый фон и тёмный текст */ }
.channel-layout {
  display: grid;
  gap: 1rem;
  margin-top: 1rem;
}
.channel-ref-status {
  margin: .75rem 0 0;
  padding: .55rem .85rem;
  border-radius: .75rem;
  background: color-mix(in oklab, var(--panel-2) 85%, black 15%);
  border: 1px solid color-mix(in oklab, var(--panel-2) 60%, black 40%);
  font-weight: 600;
  line-height: 1.35;
}
.channel-ref-status[data-state="loading"] {
  color: color-mix(in oklab, var(--muted) 85%, var(--text) 15%);
}
.channel-ref-status[data-state="fallback"] {
  color: color-mix(in oklab, var(--danger) 70%, var(--text) 30%);
  border-color: color-mix(in oklab, var(--danger) 55%, var(--text) 45%);
}
.channel-ref-status[data-state="error"] {
  color: color-mix(in oklab, var(--danger) 80%, var(--text) 20%);
  border-color: color-mix(in oklab, var(--danger) 65%, var(--text) 35%);
  background: color-mix(in oklab, var(--danger) 20%, black 80%);
}
.channel-info {
  border-radius: .9rem;
  padding: 1rem;
  display: flex;
  flex-direction: column;
  gap: .75rem;
  color: var(--text);
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
.channel-info-tests-block { gap: .75rem; }
.channel-info-tests {
  display: flex;
  flex-wrap: wrap;
  gap: .5rem;
}
.channel-info-tests .btn {
  flex: 1 1 45%;
  min-width: 9rem;
}
.channel-info-test-summary {
  display: grid;
  gap: .6rem;
  padding: .45rem .6rem;
  border-radius: .7rem;
  background: color-mix(in oklab, var(--panel-2) 82%, black 18%);
}
.channel-info-test-summary-main {
  display: grid;
  gap: .5rem;
  grid-template-columns: repeat(auto-fit, minmax(9.5rem, 1fr));
  align-items: start;
}
.channel-info-test-summary-main > div {
  display: grid;
  gap: .1rem;
  font-size: .9rem;
}
.channel-info-test-summary strong {
  font-size: 1rem;
}
.channel-info-test-summary strong + span,
.channel-info-test-summary span + strong {
  font-size: .95rem;
}
.channel-info-test-legend {
  display: flex;
  gap: .65rem;
  align-items: center;
}
.channel-info-test-legend span {
  display: inline-flex;
  align-items: center;
  gap: .4rem;
}
.channel-info-test-legend span::before {
  content: "";
  width: .75rem;
  height: .75rem;
  border-radius: 999px;
  box-shadow: 0 0 0 1px rgba(0,0,0,.2);
}
.channel-info-test-legend span[data-type="rssi"]::before { background: var(--stability-rssi); }
.channel-info-test-legend span[data-type="snr"]::before { background: var(--stability-snr); }
.channel-info-test-issues {
  padding: .3rem .1rem;
}
.channel-info-test-chart {
  position: relative;
  padding: .4rem;
  border-radius: .8rem;
  background: color-mix(in oklab, var(--panel-2) 88%, black 12%);
}
.channel-info-test-chart canvas {
  width: 100%;
  display: block;
  border-radius: .6rem;
}
.channel-info-test-tooltip {
  position: absolute;
  top: 0;
  left: 0;
  transform: translate3d(0,0,0);
  pointer-events: none;
  padding: .55rem .65rem;
  border-radius: .65rem;
  background: color-mix(in oklab, var(--panel-2) 92%, black 8%);
  border: 1px solid color-mix(in oklab, var(--panel-2) 70%, black 30%);
  box-shadow: 0 8px 18px rgba(0,0,0,.25);
  max-width: 220px;
  font-size: .85rem;
  display: grid;
  gap: .4rem;
  transition: opacity .12s ease;
}
.channel-info-test-tooltip-title {
  font-weight: 600;
}
.channel-info-test-tooltip-grid {
  display: grid;
  gap: .3rem;
}
.channel-info-test-tooltip-grid > div {
  display: flex;
  justify-content: space-between;
  gap: .8rem;
}
.channel-info-test-tooltip-grid span {
  color: var(--muted);
  font-size: .8rem;
}
.channel-info-test-tooltip-grid strong {
  font-weight: 600;
}
.channel-info-test-tooltip-raw {
  font-family: var(--mono-font, ui-monospace, SFMono-Regular, Menlo, Consolas, 'Liberation Mono', 'Courier New', monospace);
  font-size: .78rem;
  background: color-mix(in oklab, var(--panel-2) 85%, black 15%);
  padding: .35rem .45rem;
  border-radius: .5rem;
  color: var(--muted);
  word-break: break-word;
}
.channel-info-test-cards {
  display: grid;
  gap: .6rem;
  grid-template-columns: repeat(auto-fit, minmax(10rem, 1fr));
}
.channel-info-test-card {
  border-radius: .75rem;
  padding: .55rem .65rem;
  background: color-mix(in oklab, var(--panel-2) 88%, black 12%);
  border: 1px solid color-mix(in oklab, var(--panel-2) 70%, black 30%);
  display: grid;
  gap: .25rem;
}
.channel-info-test-card .label {
  font-size: .8rem;
  color: var(--muted);
  letter-spacing: .02em;
  text-transform: uppercase;
}
.channel-info-test-card .value {
  font-size: 1rem;
  font-weight: 600;
}
.channel-info-test-card .meta {
  font-size: .8rem;
  color: var(--muted);
}
.channel-info-test-history {
  display: grid;
  gap: .6rem;
  padding: .4rem 0 .2rem;
}
.channel-info-test-history-header {
  display: flex;
  flex-wrap: wrap;
  justify-content: space-between;
  gap: .5rem;
  align-items: center;
}
.channel-info-test-history-title {
  font-weight: 600;
}
.channel-info-test-history-actions {
  display: flex;
  flex-wrap: wrap;
  gap: .4rem;
}
.channel-info-test-history-list {
  list-style: none;
  margin: 0;
  padding: 0;
  display: grid;
  gap: .5rem;
}
.channel-info-test-history-item {
  border: 1px solid color-mix(in oklab, var(--panel-2) 70%, black 30%);
  border-radius: .7rem;
  padding: .55rem .65rem;
  background: color-mix(in oklab, var(--panel-2) 85%, black 15%);
  display: grid;
  gap: .35rem;
}
.channel-info-test-history-line {
  display: flex;
  flex-wrap: wrap;
  gap: .4rem .8rem;
  align-items: baseline;
  justify-content: space-between;
}
.channel-info-test-history-time {
  font-weight: 600;
}
.channel-info-test-history-success {
  font-size: .9rem;
}
.channel-info-test-history-diff {
  font-size: .85rem;
  font-weight: 600;
}
.channel-info-test-history-diff.up {
  color: var(--good);
}
.channel-info-test-history-diff.down {
  color: var(--danger);
}
.channel-info-test-history-metrics {
  display: flex;
  flex-wrap: wrap;
  gap: .4rem .8rem;
}
.channel-info-test-history-issues {
  color: var(--muted);
}
.channel-info-cr {
  border-radius: .8rem;
  overflow: hidden;
  border: 1px solid color-mix(in oklab, var(--panel-2) 75%, black 25%);
}
.channel-info-cr-table {
  width: 100%;
  border-collapse: collapse;
  font-size: .85rem;
}
.channel-info-cr-table th,
.channel-info-cr-table td {
  padding: .5rem .6rem;
  border-bottom: 1px solid color-mix(in oklab, var(--panel-2) 70%, black 30%);
  text-align: left;
}
.channel-info-cr-table thead { background: color-mix(in oklab, var(--panel-2) 90%, black 10%); }
.channel-info-cr-table tbody tr:last-child td { border-bottom: none; }
.channel-info-cr-table td.ok { color: var(--good); font-weight: 600; }
.channel-info-cr-table td.fail { color: var(--danger); font-weight: 600; }
.channel-info-status[data-state="warn"] { color: var(--danger); }
.channel-info-status[data-state="success"] { color: var(--good); }
.channel-info-status[data-state="info"] { color: var(--muted); }
.channel-info-row td {
  padding: 0;
  border-bottom: none;
  background: transparent;
}
.channel-info-row .channel-info {
  width: 100%;
  margin: .8rem 0;
}
.channel-info-cell { background: transparent; }
@media (max-width: 720px) {
  .channel-layout { gap: .75rem; }
  .table-wrap.pretty table { min-width: 0; }
  #channelsTable thead { display: none; }
  #channelsTable tbody { display: grid; gap: .9rem; }
  #channelsTable tbody tr {
    display: grid;
    grid-template-columns: repeat(2, minmax(0, 1fr));
    gap: .6rem .8rem;
    padding: .85rem;
    border-radius: .95rem;
    border: 1px solid color-mix(in oklab, var(--panel-2) 68%, black 32%);
    background: color-mix(in oklab, var(--panel-2) 95%, black 5%);
  }
  #channelsTable tbody tr td {
    border-bottom: none;
    padding: .35rem 0;
    display: flex;
    justify-content: space-between;
    align-items: center;
    gap: .5rem;
    font-size: .85rem;
  }
  #channelsTable tbody tr td::before {
    content: attr(data-label);
    font-weight: 600;
    color: var(--muted);
    text-transform: uppercase;
    letter-spacing: .05em;
  }
  #channelsTable tbody tr.channel-info-row {
    display: block;
    padding: 0;
    background: transparent;
    border: none;
    box-shadow: none;
  }
  #channelsTable tbody tr.channel-info-row td {
    display: block;
    justify-content: initial;
    align-items: initial;
    gap: 0;
  }
  #channelsTable tbody tr.channel-info-row td::before {
    content: none;
  }
  .channel-info-cell {
    max-width: 100%;
    width: 100%;
  }
  .channel-info-row .channel-info {
    width: 100%;
    max-width: 100%;
    margin: 0;
  }
  #channelsTable tbody tr.selected-info {
    background: color-mix(in oklab, var(--accent-2) 25%, var(--panel-2) 75%);
    box-shadow: inset 0 0 0 1px color-mix(in oklab, var(--accent-2) 45%, var(--panel-2) 55%);
    color: color-mix(in oklab, var(--text) 90%, white 10%);
  }
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
  border-color: color-mix(in oklab, var(--good) 45%, var(--panel-2) 55%);
  background: color-mix(in oklab, var(--good) 18%, var(--panel-2) 82%);
  box-shadow: inset 0 0 0 1px color-mix(in oklab, var(--good) 30%, var(--panel-2) 70%);
}
.enc-test[data-status="error"] {
  border-color: color-mix(in oklab, var(--danger) 55%, var(--panel-2) 45%);
  background: color-mix(in oklab, var(--danger) 22%, var(--panel-2) 78%);
  box-shadow: inset 0 0 0 1px color-mix(in oklab, var(--danger) 35%, black 15%);
  color: color-mix(in oklab, var(--danger) 75%, white 25%);
}
.enc-test-header {
  display:flex;
  flex-wrap:wrap;
  align-items:center;
  justify-content:space-between;
  gap:.35rem 1rem;
}
.enc-test-header .btn { padding:.4rem .9rem; }
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
.settings-form { display:grid; gap:1.25rem; max-width:520px; }
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
.settings-section {
  border-radius: .95rem;
  border: 1px solid color-mix(in oklab, var(--panel-2) 70%, black 30%);
  background: color-mix(in oklab, var(--panel-2) 92%, black 8%);
  padding: 1rem;
  display: flex;
  flex-direction: column;
  gap: .8rem;
}
.settings-section h3 {
  margin: 0;
  font-size: .78rem;
  text-transform: uppercase;
  letter-spacing: .08em;
  color: var(--muted);
}
.settings-group { display:grid; gap:.75rem; }
.settings-group.two { grid-template-columns: repeat(auto-fit, minmax(160px, 1fr)); gap:.75rem; }
.settings-group.inline { display:flex; flex-wrap:wrap; gap:.75rem; }
.settings-section:last-of-type { margin-bottom: 0; }
.settings-actions { margin-top:.25rem; }
@media (max-width: 640px) {
  .settings-group.two { grid-template-columns: 1fr; }
  .settings-group.inline { flex-direction: column; }
}

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
  .nav { position: fixed; top: 3.6rem; right: clamp(.75rem, 8vw, 2rem); left: clamp(.75rem, 8vw, 2rem);
         width: auto; z-index: 50; background: color-mix(in oklab, var(--panel) 94%, black 6%);
         border-radius:1rem; padding:.75rem .85rem;
         border:1px solid color-mix(in oklab, var(--panel-2) 70%, black 30%);
         box-shadow: 0 20px 45px rgba(0,0,0,.25); }
  .nav { display:none; flex-direction:column; align-items:stretch; text-align:left; gap:.35rem; }
  .nav.open { display:flex; }
  .nav a { font-size:1rem; justify-content:space-between; padding:.65rem .85rem; border-radius:.85rem; }
  .chip input { width: 42vw; }
  .chip.input-chip input { width: min(6rem, 34vw); }
  table { min-width: 640px; }
  .key-grid { grid-template-columns: 1fr; }
}

/* Отладочный вывод */
.debug-log {
  height: min(52vh, 560px);
  overflow-y: auto;
  overflow-x: hidden;
  display: flex;
  flex-direction: column;
  gap: .25rem;
  padding: .5rem;
  border-radius: .8rem;
  background: var(--panel-2);
  border: 1px solid color-mix(in oklab, var(--panel-2) 70%, black 30%);
  font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, 'Liberation Mono', 'Courier New', monospace;
  font-size: .85rem;
  word-break: break-word;
  overflow-wrap: anywhere;
}
.debug-line { opacity:.85; color: var(--text); word-break: break-word; overflow-wrap: anywhere; }
.debug-line--success { color: color-mix(in oklab, var(--good) 65%, var(--text) 35%); }
.debug-line--error { color: color-mix(in oklab, var(--danger) 75%, var(--text) 25%); }
.debug-line--warn { color: color-mix(in oklab, #facc15 70%, var(--text) 30%); }
.debug-line--action { color: color-mix(in oklab, var(--accent) 70%, var(--text) 30%); }
.debug-metrics { background: var(--panel-2); border:1px solid color-mix(in oklab, var(--panel-2) 70%, black 30%); border-radius:.9rem; padding:1rem; margin-bottom:1.5rem; }
.debug-metrics__header { display:flex; align-items:center; justify-content:space-between; gap:.75rem; margin-bottom:1rem; }
.debug-metrics__status { font-size:.9rem; color: var(--muted); }
.debug-metrics__status.warn { color: color-mix(in oklab, #facc15 70%, var(--text) 30%); }
.debug-metrics__status.error { color: color-mix(in oklab, var(--danger) 75%, var(--text) 25%); }
.debug-metrics-grid { display:grid; grid-template-columns: repeat(auto-fit, minmax(160px, 1fr)); gap:.75rem 1rem; }
.debug-metrics-item { background: color-mix(in oklab, var(--panel) 70%, var(--panel-2) 30%); border-radius:.8rem; padding:.75rem; display:flex; flex-direction:column; gap:.25rem; }
.debug-metrics-item__label { font-size:.75rem; text-transform:uppercase; letter-spacing:.05em; color: var(--muted); }
.debug-metrics-item__value { font-size:.95rem; color: var(--text); }
.debug-metrics-item__value.warn { color: color-mix(in oklab, #facc15 70%, var(--text) 30%); }
.debug-metrics-item__value.error { color: color-mix(in oklab, var(--danger) 75%, var(--text) 25%); }
.debug-metrics-foot { margin-top:1rem; display:flex; flex-direction:column; gap:.5rem; font-size:.85rem; color: var(--muted); }
.debug-metrics-foot__entry.error { color: color-mix(in oklab, var(--danger) 70%, var(--text) 30%); }
)~~~";

// web/script.js
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

// Константы для вкладки наведения антенны
const EARTH_RADIUS_KM = 6378.137; // экваториальный радиус Земли
const GEO_ALTITUDE_KM = 35786.0;   // высота геостационарной орбиты над поверхностью
const MU_EARTH = 398600.4418;      // гравитационный параметр Земли (км^3/с^2)
const DEG2RAD = Math.PI / 180;
const RAD2DEG = 180 / Math.PI;
const TWO_PI = Math.PI * 2;
const POINTING_DEFAULT_MIN_ELEVATION = 10; // минимальный угол возвышения по умолчанию
const POINTING_COMPASS_OFFSET_DEG = 180; // отображаем юг в верхней части компаса

// Константы счётчика непрочитанных сообщений чата выносим наверх, чтобы избежать ошибок инициализации
const CHAT_UNREAD_STORAGE_KEY = "chatUnread"; // ключ хранения количества непрочитанных сообщений
const CHAT_UNREAD_MAX = 999; // максимальное значение счётчика, отображаемое в бейдже
const CHAT_HISTORY_LIMIT = 500; // максимальное количество сообщений, сохраняемых в истории чата
const CHANNELS_CACHE_STORAGE_KEY = "channelsCache"; // ключ localStorage для кеша списка каналов

/* Состояние интерфейса */
const UI = {
  tabs: ["chat", "channels", "pointing", "settings", "security", "debug"],
  els: {},
  cfg: {
    endpoint: storage.get("endpoint") || "http://192.168.4.1",
    theme: storage.get("theme") || detectPreferredTheme(),
    accent: (storage.get("accent") === "red") ? "red" : "default",
    autoNight: storage.get("autoNight") !== "0",
    imageProfile: (storage.get("imageProfile") || "S").toUpperCase(),
  },
  key: {
    state: null,
    lastMessage: "",
  },
  state: {
    activeTab: null,
    bank: null,
    channel: null,
    ack: null,
    ackRetry: null,
    ackBusy: false,
    lightPack: null,
    lightPackBusy: false,
    rxBoostedGain: null,
    encBusy: false,
    encryption: null,
    infoChannel: null,
    infoChannelTx: null,
    infoChannelRx: null,
    channelTests: {
      message: "",
      messageType: null,
      messageChannel: null,
      stability: {
        running: false,
        channel: null,
        total: 0,
        success: 0,
        points: [],
        stats: null,
        startedAt: null,
        finishedAt: null,
      },
      stabilityHistory: {
        loaded: false,
        map: new Map(),
      },
      cr: {
        running: false,
        channel: null,
        results: [],
        previous: null,
      },
    },
    chatHistory: [],
    chatHydrating: false,
    chatScrollPinned: true,
    chatUnread: (() => {
      const raw = storage.get(CHAT_UNREAD_STORAGE_KEY);
      const parsed = raw != null ? parseInt(raw, 10) : NaN;
      if (!Number.isFinite(parsed) || parsed <= 0) return 0;
      return Math.min(Math.max(parsed, 0), CHAT_UNREAD_MAX);
    })(),
    chatSoundCtx: null,
    chatSoundLast: 0,
    chatImages: null,
    irqStatus: {
      message: "",
      uptimeMs: null,
      timestamp: null,
    },
    testRxm: {
      polling: false,
      lastName: null,
    },
    received: {
      timer: null,
      running: false,
      known: new Set(),
      limit: null,
      awaiting: false,
      progress: new Map(),
    },
    deviceLog: {
      initialized: false,
      loading: false,
      known: new Set(),
      lastId: 0,
      queue: [],
    },
    version: normalizeVersionText(storage.get("appVersion") || "") || null,
    pauseMs: null,
    ackTimeout: null,
    encTest: null,
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
// Настройки тестов вкладки Channels/Ping
const CHANNEL_STABILITY_ATTEMPTS = 20; // количество ping для Stability test
const CHANNEL_STABILITY_INTERVAL_MS = 300; // задержка между ping в миллисекундах
const CHANNEL_STABILITY_HISTORY_KEY = "channelStabilityHistory"; // ключ localStorage для архива Stability test
const CHANNEL_STABILITY_HISTORY_VERSION = 1; // версия структуры архива
const CHANNEL_STABILITY_HISTORY_LIMIT = 20; // максимальное число записей истории на канал
const CHANNEL_STABILITY_TOOLTIP_RADIUS = 18; // радиус поиска точек на графике для подсказки
const CHANNEL_CR_PRESETS = [ // допустимые значения CR для перебора
  { value: 5, label: "4/5" },
  { value: 6, label: "4/6" },
  { value: 7, label: "4/7" },
  { value: 8, label: "4/8" },
];

// Профили подготовки изображений перед отправкой
const IMAGE_PROFILES = {
  S: { id: "S", label: "S — 320×240, серый", maxWidth: 320, maxHeight: 240, quality: 0.3, grayscale: true },
  M: { id: "M", label: "M — 320×240, серый", maxWidth: 320, maxHeight: 240, quality: 0.4, grayscale: true },
  L: { id: "L", label: "L — 320×240, цвет", maxWidth: 320, maxHeight: 240, quality: 0.4, grayscale: false },
};
const IMAGE_ALLOWED_TYPES = new Set(["image/jpeg", "image/png", "image/heic", "image/heif"]);
const IMAGE_MAX_SOURCE_SIZE = 20 * 1024 * 1024; // 20 МБ — верхний предел исходного файла
const IMAGE_CACHE_LIMIT = 32; // ограничение количества кэшированных изображений в памяти
let chatDropCounter = 0; // глубина вложенных dragenter для зоны чата

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
  UI.els.settingsForm = $("#settingsForm");
  UI.els.debugLog = $("#debugLog");
  UI.els.receivedDiag = {
    root: $("#recvDiag"),
    status: $("#recvDiagStatus"),
    mode: $("#recvDiagMode"),
    interval: $("#recvDiagInterval"),
    lastStart: $("#recvDiagLastStart"),
    lastFinish: $("#recvDiagLastFinish"),
    duration: $("#recvDiagDuration"),
    avg: $("#recvDiagAvg"),
    gap: $("#recvDiagGap"),
    runningSince: $("#recvDiagRunning"),
    totals: $("#recvDiagTotals"),
    timeouts: $("#recvDiagTimeouts"),
    overlaps: $("#recvDiagOverlaps"),
    errorBox: $("#recvDiagLastError"),
    blockedBox: $("#recvDiagBlocked"),
  };
  updateReceivedMonitorDiagnostics();
  UI.els.rstsFullBtn = $("#btnRstsFull");
  UI.els.rstsJsonBtn = $("#btnRstsJson");
  UI.els.rstsDownloadBtn = $("#btnRstsDownloadJson");
  UI.els.chatLog = $("#chatLog");
  UI.els.chatInput = $("#chatInput");
  UI.els.chatScrollBtn = $("#chatScrollBottom");
  UI.els.chatRxIndicator = $("#chatRxIndicator");
  if (UI.els.chatRxIndicator) {
    setChatReceivingIndicatorState(getReceivedMonitorState().awaiting);
  }
  UI.els.chatIrqStatus = $("#chatIrqStatus");
  UI.els.chatIrqMessage = UI.els.chatIrqStatus ? UI.els.chatIrqStatus.querySelector(".chat-irq-message") : null;
  UI.els.chatIrqMeta = UI.els.chatIrqStatus ? UI.els.chatIrqStatus.querySelector(".chat-irq-meta") : null;
  renderChatIrqStatus();
  UI.els.sendBtn = $("#sendBtn");
  UI.els.chatImageBtn = $("#chatImageBtn");
  UI.els.chatImageInput = $("#chatImageInput");
  UI.els.chatImageProfile = $("#chatImageProfile");
  UI.els.ackChip = $("#ackStateChip");
  UI.els.ackText = $("#ackStateText");
  UI.els.encChip = $("#encStateChip");
  UI.els.encText = $("#encStateText");
  UI.els.ackSetting = $("#ACK");
  UI.els.ackSettingWrap = $("#ackSettingControl");
  UI.els.ackSettingHint = $("#ackSettingHint");
  UI.els.ackRetry = $("#ACKR");
  UI.els.ackRetryHint = $("#ackRetryHint");
  UI.els.lightPack = $("#LIGHTPACK");
  UI.els.lightPackHint = $("#lightPackHint");
  UI.els.lightPackControl = $("#lightPackControl");
  UI.els.pauseInput = $("#PAUSE");
  UI.els.pauseHint = $("#pauseHint");
  UI.els.ackTimeout = $("#ACKT");
  UI.els.ackTimeoutHint = $("#ackTimeoutHint");
  UI.els.rxBoostedGain = $("#RXBG");
  UI.els.rxBoostedGainHint = $("#rxBoostedGainHint");
  UI.els.testRxmMessage = $("#TESTRXMMSG");
  UI.els.testRxmMessageHint = $("#testRxmMessageHint");
  UI.els.channelSelect = $("#CH");
  UI.els.channelSelectHint = $("#channelSelectHint");
  UI.els.txlInput = $("#txlSize");
  UI.els.autoNightSwitch = $("#autoNightMode");
  UI.els.autoNightHint = $("#autoNightHint");
  UI.els.channelInfoPanel = $("#channelInfoPanel");
  UI.els.channelInfoTitle = $("#channelInfoTitle");
  UI.els.channelInfoBody = $("#channelInfoBody");
  UI.els.channelInfoEmpty = $("#channelInfoEmpty");
  UI.els.channelInfoStatus = $("#channelInfoStatus");
  UI.els.channelRefStatus = $("#channelRefStatus");
  UI.els.channelInfoSetBtn = $("#channelInfoSetCurrent");
  UI.els.channelInfoStabilityBtn = $("#channelInfoStabilityTest");
  UI.els.channelInfoCrBtn = $("#channelInfoCrTest");
  UI.els.channelInfoTestsStatus = $("#channelInfoTestsStatus");
  UI.els.channelInfoStabilitySummary = $("#channelInfoStabilitySummary");
  UI.els.channelInfoStabilitySuccess = $("#channelInfoStabilitySuccess");
  UI.els.channelInfoStabilityPercent = $("#channelInfoStabilityPercent");
  UI.els.channelInfoStabilityLatencyAvg = $("#channelInfoStabilityLatencyAvg");
  UI.els.channelInfoStabilityLatencyJitter = $("#channelInfoStabilityLatencyJitter");
  UI.els.channelInfoStabilityCards = $("#channelInfoStabilityCards");
  UI.els.channelInfoStabilityRssiRange = $("#channelInfoStabilityRssiRange");
  UI.els.channelInfoStabilityRssiMeta = $("#channelInfoStabilityRssiMeta");
  UI.els.channelInfoStabilitySnrRange = $("#channelInfoStabilitySnrRange");
  UI.els.channelInfoStabilitySnrMeta = $("#channelInfoStabilitySnrMeta");
  UI.els.channelInfoStabilityLatencyRange = $("#channelInfoStabilityLatencyRange");
  UI.els.channelInfoStabilityLatencyMeta = $("#channelInfoStabilityLatencyMeta");
  UI.els.channelInfoStabilityIssues = $("#channelInfoStabilityIssues");
  UI.els.channelInfoStabilityWrap = $("#channelInfoStabilityResult");
  UI.els.channelInfoStabilityChart = $("#channelInfoStabilityChart");
  UI.els.channelInfoStabilityTooltip = $("#channelInfoStabilityTooltip");
  UI.els.channelInfoStabilityHistory = $("#channelInfoStabilityHistory");
  UI.els.channelInfoStabilityHistoryList = $("#channelInfoStabilityHistoryList");
  UI.els.channelInfoStabilityHistoryEmpty = $("#channelInfoStabilityHistoryEmpty");
  UI.els.channelInfoStabilityExportCsv = $("#channelInfoStabilityExportCsv");
  UI.els.channelInfoStabilityExportJson = $("#channelInfoStabilityExportJson");
  UI.els.channelInfoStabilityHistoryClear = $("#channelInfoStabilityHistoryClear");
  UI.els.channelInfoCrResult = $("#channelInfoCrResult");
  UI.els.channelInfoCrTableBody = $("#channelInfoCrTableBody");
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
  if (UI.els.channelInfoStabilityBtn) {
    UI.els.channelInfoStabilityBtn.addEventListener("click", onChannelStabilityTest);
  }
  if (UI.els.channelInfoCrBtn) {
    UI.els.channelInfoCrBtn.addEventListener("click", onChannelCrTest);
  }

  const restoredChannels = restoreChannelsFromStorage();
  if (restoredChannels) {
    renderChannels();
    updateChannelSelect();
    updateChannelSelectHint();
  }

  if (UI.els.channelInfoStabilityExportCsv) {
    UI.els.channelInfoStabilityExportCsv.addEventListener("click", onChannelStabilityExportCsv);
  }
  if (UI.els.channelInfoStabilityExportJson) {
    UI.els.channelInfoStabilityExportJson.addEventListener("click", onChannelStabilityExportJson);
  }
  if (UI.els.channelInfoStabilityHistoryClear) {
    UI.els.channelInfoStabilityHistoryClear.addEventListener("click", onChannelStabilityHistoryClear);
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
  updateChannelTestsUi();
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
  UI.state.activeTab = initialTab;
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
  updateChatUnreadBadge();
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
  if (UI.els.settingsForm) {
    // Блокируем стандартную отправку формы, чтобы страница не перезагружалась при нажатии Enter
    UI.els.settingsForm.addEventListener("submit", (event) => event.preventDefault());
  }
  if (UI.els.autoNightSwitch) {
    UI.els.autoNightSwitch.checked = UI.cfg.autoNight;
    UI.els.autoNightSwitch.addEventListener("change", () => setAutoNight(UI.els.autoNightSwitch.checked));
  }

  // Настройка endpoint
  if (UI.els.endpoint) {
    UI.els.endpoint.value = UI.cfg.endpoint;
    UI.els.endpoint.addEventListener("keydown", (event) => {
      if (event.key === "Enter") {
        event.preventDefault();
        UI.els.endpoint.blur();
      }
    });
    UI.els.endpoint.addEventListener("change", () => {
      const fallback = UI.cfg.endpoint || "http://192.168.4.1";
      const input = UI.els.endpoint.value.trim();
      const candidate = input || "http://192.168.4.1";
      let parsed = null;
      try {
        parsed = new URL(candidate);
      } catch (err) {
        console.warn("[endpoint] некорректный URL", err);
      }
      if (!parsed) {
        note("Endpoint: введён некорректный URL, используется предыдущее значение");
        UI.els.endpoint.value = fallback;
        return;
      }
      const protocol = (parsed.protocol || "").toLowerCase();
      if (protocol !== "http:" && protocol !== "https:") {
        console.warn("[endpoint] неподдерживаемая схема", protocol);
        note("Endpoint: схема не поддерживается, используется предыдущее значение");
        UI.els.endpoint.value = fallback;
        return;
      }
      const value = parsed.toString();
      UI.cfg.endpoint = value;
      UI.els.endpoint.value = value;
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
  if (UI.els.chatImageBtn && UI.els.chatImageInput) {
    UI.els.chatImageBtn.addEventListener("click", (event) => {
      event.preventDefault();
      onChatImageButton();
    });
    UI.els.chatImageInput.addEventListener("change", onChatImageInputChange);
  }
  if (UI.els.chatImageProfile) {
    const profile = normalizeImageProfile(UI.cfg.imageProfile);
    UI.els.chatImageProfile.value = profile.id;
    UI.els.chatImageProfile.addEventListener("change", onChatImageProfileChange);
  }
  const chatArea = $(".chat-area");
  if (chatArea) {
    UI.els.chatArea = chatArea;
    chatArea.addEventListener("dragenter", onChatDragEnter, false);
    chatArea.addEventListener("dragover", onChatDragOver, false);
    chatArea.addEventListener("dragleave", onChatDragLeave, false);
    chatArea.addEventListener("drop", onChatDrop, false);
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

  loadChatHistory();
  startReceivedMonitor({ immediate: true });
  openReceivedPushChannel();

  // Управление ACK и тестами
  if (UI.els.ackChip) UI.els.ackChip.addEventListener("click", onAckChipToggle);
  if (UI.els.encChip) UI.els.encChip.addEventListener("click", onEncChipToggle);
  if (UI.els.ackSetting) {
    UI.els.ackSetting.addEventListener("change", () => {
      UI.els.ackSetting.indeterminate = false;
      void withAckLock(() => setAck(UI.els.ackSetting.checked));
    });
  }
  if (UI.els.lightPack) {
    UI.els.lightPack.addEventListener("change", onLightPackToggle);
  }
  if (UI.els.ackRetry) UI.els.ackRetry.addEventListener("change", onAckRetryInput);
  if (UI.els.pauseInput) UI.els.pauseInput.addEventListener("change", onPauseInputChange);
  if (UI.els.ackTimeout) UI.els.ackTimeout.addEventListener("change", onAckTimeoutInputChange);
  if (UI.els.rxBoostedGain) {
    UI.els.rxBoostedGain.addEventListener("change", () => {
      UI.els.rxBoostedGain.indeterminate = false;
      UI.state.rxBoostedGain = UI.els.rxBoostedGain.checked;
      updateRxBoostedGainUi();
    });
  }
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
  const btnKeyGenPeer = $("#btnKeyGenPeer");
  if (btnKeyGenPeer) {
    UI.els.keyGenPeerBtn = btnKeyGenPeer;
    btnKeyGenPeer.addEventListener("click", () => requestKeyGenPeer());
  }
  const btnKeyRestore = $("#btnKeyRestore"); if (btnKeyRestore) btnKeyRestore.addEventListener("click", () => requestKeyRestore());
  const btnKeySend = $("#btnKeySend"); if (btnKeySend) btnKeySend.addEventListener("click", () => requestKeySend());
  const btnKeyRecv = $("#btnKeyRecv"); if (btnKeyRecv) btnKeyRecv.addEventListener("click", () => requestKeyReceive());
  const criticalInitTasks = [
    { name: "refreshKeyState", run: () => refreshKeyState({ silent: true }) },
    { name: "syncSettingsFromDevice", run: () => syncSettingsFromDevice() },
    { name: "refreshAckState", run: () => refreshAckState() },
    { name: "refreshAckRetry", run: () => refreshAckRetry() },
    { name: "refreshLightPackState", run: () => refreshLightPackState() },
    { name: "refreshEncryptionState", run: () => refreshEncryptionState() },
  ];
  const criticalResults = await Promise.allSettled(criticalInitTasks.map((task) => {
    try {
      return task.run();
    } catch (err) {
      console.warn("[init] синхронное исключение при запуске задачи", task.name, err);
      return Promise.reject(err);
    }
  }));
  criticalResults.forEach((result, index) => {
    if (result.status === "rejected") {
      console.warn("[init] задача инициализации завершилась ошибкой", criticalInitTasks[index].name, result.reason);
    }
  });

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

/* Обновляем бейдж непрочитанных сообщений в меню */
function updateChatUnreadBadge(opts) {
  const options = opts || {};
  const nav = UI.els.nav || $("#siteNav");
  const link = nav ? nav.querySelector('[data-tab="chat"]') : $(".nav [data-tab=\"chat\"]");
  if (!link) return;
  const badge = link.querySelector(".nav-badge");
  if (!badge) return;
  if (!badge.dataset.listener) {
    badge.addEventListener("animationend", () => {
      badge.classList.remove("nav-badge-pop");
    });
    badge.dataset.listener = "1";
  }
  const unreadRaw = Number(UI.state.chatUnread || 0);
  const unread = Math.max(0, Math.min(unreadRaw, CHAT_UNREAD_MAX));
  if (unread <= 0) {
    badge.hidden = true;
    badge.textContent = "";
    badge.setAttribute("aria-live", "off");
    badge.setAttribute("aria-label", "Непрочитанных сообщений нет");
    badge.classList.remove("nav-badge-pop");
    badge.dataset.count = "0";
    return;
  }
  const text = unread > 99 ? "99+" : String(unread);
  badge.hidden = false;
  badge.textContent = text;
  badge.setAttribute("aria-live", "polite");
  badge.setAttribute("aria-label", `Непрочитанные сообщения: ${text}`);
  badge.dataset.count = String(unread);
  if (options.animate) {
    badge.classList.remove("nav-badge-pop");
    // Перезапуск анимации появления бейджа
    void badge.offsetWidth;
    badge.classList.add("nav-badge-pop");
  }
}

/* Вкладки */
function setTab(tab) {
  UI.state.activeTab = tab;
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
  if (tab === "chat") {
    if (UI.state.chatUnread !== 0) {
      UI.state.chatUnread = 0;
    }
    storage.set(CHAT_UNREAD_STORAGE_KEY, "0");
    updateChatUnreadBadge();
    updateChatScrollButton();
  } else {
    updateChatUnreadBadge();
  }
  if (tab === "debug") {
    hydrateDeviceLog({ limit: 150 }).catch((err) => console.warn("[debug] hydrateDeviceLog", err));
  }
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

// Сдвигаем индексы элементов прогресса при обрезке истории чата
function adjustChatProgressAfterTrim(removed) {
  if (!Number.isFinite(removed) || removed <= 0) return;
  const receivedState = UI.state && UI.state.received ? UI.state.received : null;
  if (!receivedState || !(receivedState.progress instanceof Map)) return;
  let changed = false;
  for (const [key, info] of Array.from(receivedState.progress.entries())) {
    if (!info || typeof info.index !== "number") {
      receivedState.progress.delete(key);
      changed = true;
      continue;
    }
    const nextIndex = info.index - removed;
    if (nextIndex < 0) {
      receivedState.progress.delete(key);
      changed = true;
    } else if (nextIndex !== info.index) {
      info.index = nextIndex;
      changed = true;
    }
  }
  if (changed && receivedState.progress.size === 0) {
    setChatReceivingIndicatorState(false);
  }
}

// Перестраиваем DOM истории чата и атрибуты dataset.index после обрезки
function adjustChatDomAfterTrim(removed) {
  if (!Number.isFinite(removed) || removed <= 0) return;
  if (!UI.els || !UI.els.chatLog) return;
  const currentMessages = UI.els.chatLog.querySelectorAll(".msg");
  let removedCount = 0;
  for (let i = 0; i < currentMessages.length && removedCount < removed; i += 1) {
    const node = currentMessages[i];
    if (node && node.parentNode) {
      node.parentNode.removeChild(node);
      removedCount += 1;
    }
  }
  const nodes = UI.els.chatLog.querySelectorAll(".msg");
  for (let i = 0; i < nodes.length; i += 1) {
    nodes[i].dataset.index = String(i);
  }
}

// Универсальная функция ограничения размера истории чата с синхронизацией индексов
function applyChatHistoryLimit(entries, options) {
  const opts = options || {};
  const limitRaw = Number.isFinite(opts.limit) ? Number(opts.limit) : CHAT_HISTORY_LIMIT;
  const limit = limitRaw > 0 ? Math.floor(limitRaw) : CHAT_HISTORY_LIMIT;
  if (!Array.isArray(entries) || !Number.isFinite(limit) || limit <= 0) return 0;
  const overflow = entries.length - limit;
  if (overflow <= 0) return 0;
  entries.splice(0, overflow);
  if (opts.updateProgress !== false) adjustChatProgressAfterTrim(overflow);
  if (opts.updateDom !== false) adjustChatDomAfterTrim(overflow);
  return overflow;
}

function normalizeImageProfile(value) {
  const key = typeof value === "string" && value ? value.toUpperCase() : "S";
  if (Object.prototype.hasOwnProperty.call(IMAGE_PROFILES, key)) {
    return IMAGE_PROFILES[key];
  }
  return IMAGE_PROFILES.S;
}

function getCurrentImageProfileId() {
  const profile = normalizeImageProfile(UI.cfg && UI.cfg.imageProfile);
  return profile.id;
}

function setCurrentImageProfile(id) {
  const profile = normalizeImageProfile(id);
  UI.cfg.imageProfile = profile.id;
  storage.set("imageProfile", profile.id);
  return profile;
}

function makeImageNameFromId(id) {
  const num = Number(id);
  if (!Number.isFinite(num) || num < 0) return null;
  const normalized = Math.abs(Math.trunc(num)) % 100000;
  return "IMG-" + normalized.toString().padStart(5, "0");
}

function imageNameFromRawName(rawName) {
  const text = rawName != null ? String(rawName) : "";
  const match = text.match(/GO-(\d+)/i);
  if (match) return makeImageNameFromId(match[1]);
  return null;
}
function saveChatHistory() {
  const entries = getChatHistory();
  applyChatHistoryLimit(entries, { updateDom: false, updateProgress: false });
  try {
    storage.set("chatHistory", JSON.stringify(entries));
  } catch (err) {
    console.warn("[chat] не удалось сохранить историю:", err);
  }
}

function normalizeChatImageMeta(raw) {
  const src = raw && typeof raw === "object" ? raw : {};
  const profile = normalizeImageProfile(src.profile);
  const toNumber = (value) => {
    const num = Number(value);
    return Number.isFinite(num) ? num : 0;
  };
  const meta = {
    key: src.key ? String(src.key) : "",
    name: src.name ? String(src.name) : "",
    profile: profile.id,
    frameWidth: toNumber(src.frameWidth),
    frameHeight: toNumber(src.frameHeight),
    drawWidth: toNumber(src.drawWidth),
    drawHeight: toNumber(src.drawHeight),
    offsetX: toNumber(src.offsetX),
    offsetY: toNumber(src.offsetY),
    grayscale: src.grayscale === true || src.grayscale === 1 || src.grayscale === "1",
    jpegSize: toNumber(src.jpegSize),
    originalSize: toNumber(src.originalSize),
    sourceName: src.sourceName ? String(src.sourceName) : "",
    source: src.source === "rx" ? "rx" : "tx",
    status: src.status ? String(src.status) : "pending",
    createdAt: toNumber(src.createdAt) || Date.now(),
    naturalWidth: src.naturalWidth != null ? toNumber(src.naturalWidth) : null,
    naturalHeight: src.naturalHeight != null ? toNumber(src.naturalHeight) : null,
    msgId: src.msgId != null ? toNumber(src.msgId) : null,
  };
  if (!meta.key) meta.key = meta.name || ("img:" + meta.createdAt);
  return meta;
}

function sanitizeChatImageMeta(meta) {
  const normalized = normalizeChatImageMeta(meta);
  return {
    key: normalized.key,
    name: normalized.name,
    profile: normalized.profile,
    frameWidth: normalized.frameWidth,
    frameHeight: normalized.frameHeight,
    drawWidth: normalized.drawWidth,
    drawHeight: normalized.drawHeight,
    offsetX: normalized.offsetX,
    offsetY: normalized.offsetY,
    grayscale: normalized.grayscale ? 1 : 0,
    jpegSize: normalized.jpegSize,
    originalSize: normalized.originalSize,
    sourceName: normalized.sourceName,
    source: normalized.source,
    status: normalized.status,
    createdAt: normalized.createdAt,
    naturalWidth: normalized.naturalWidth,
    naturalHeight: normalized.naturalHeight,
    msgId: normalized.msgId,
  };
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
    if (obj.image) {
      obj.image = normalizeChatImageMeta(obj.image);
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
  applyChatHistoryLimit(normalized, { updateDom: false });
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
    if (meta.image) record.image = sanitizeChatImageMeta(meta.image);
  }
  if (!record.role) record.role = author === "you" ? "user" : "system";
  entries.push(record);
  applyChatHistoryLimit(entries);
  const index = entries.length - 1;
  saveChatHistory();
  return { record, index };
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
  const tag = data && data.tag ? String(data.tag) : "";
  if (tag === "rx-progress") wrap.classList.add("progress");
  else if (tag === "rx-image") wrap.classList.add("image");
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

function getChatImageState() {
  if (!UI.state.chatImages || typeof UI.state.chatImages !== "object") {
    UI.state.chatImages = {
      cache: new Map(),
      order: [],
      elements: new Map(),
      limit: IMAGE_CACHE_LIMIT,
    };
  }
  const state = UI.state.chatImages;
  if (!(state.cache instanceof Map)) state.cache = new Map();
  if (!Array.isArray(state.order)) state.order = [];
  if (!(state.elements instanceof Map)) state.elements = new Map();
  if (!Number.isFinite(state.limit) || state.limit <= 0) state.limit = IMAGE_CACHE_LIMIT;
  return state;
}

function trimChatImageCache(state) {
  while (state.order.length > state.limit) {
    const oldest = state.order.shift();
    if (!oldest) continue;
    const record = state.cache.get(oldest);
    if (record && record.url) {
      URL.revokeObjectURL(record.url);
    }
    state.cache.delete(oldest);
    state.elements.delete(oldest);
  }
}

function cacheChatImage(key, blob, meta) {
  const state = getChatImageState();
  const normalizedKey = key || (meta && meta.key) || ("img:" + Date.now());
  if (!blob) return null;
  const existing = state.cache.get(normalizedKey);
  if (existing && existing.url) {
    URL.revokeObjectURL(existing.url);
  }
  const record = {
    key: normalizedKey,
    blob,
    url: URL.createObjectURL(blob),
    size: blob.size,
    created: Date.now(),
  };
  state.cache.set(normalizedKey, record);
  state.order = state.order.filter((item) => item !== normalizedKey);
  state.order.push(normalizedKey);
  trimChatImageCache(state);
  updateChatImageElements(normalizedKey);
  return record;
}

function renameChatImageKey(oldKey, newKey) {
  if (!oldKey || !newKey || oldKey === newKey) return;
  const state = getChatImageState();
  const record = state.cache.get(oldKey);
  if (record) {
    state.cache.delete(oldKey);
    record.key = newKey;
    state.cache.set(newKey, record);
  }
  state.order = state.order.map((item) => (item === oldKey ? newKey : item));
  const bindings = state.elements.get(oldKey);
  if (bindings) {
    state.elements.delete(oldKey);
    state.elements.set(newKey, bindings);
    bindings.forEach((binding) => {
      binding.meta.key = newKey;
      if (binding.element) binding.element.dataset.imageKey = newKey;
    });
  }
  updateChatImageElements(newKey);
}

function formatBytesShort(value) {
  const num = Number(value);
  if (!Number.isFinite(num) || num <= 0) return "—";
  if (num < 1024) return num + " Б";
  const kb = num / 1024;
  if (kb < 1024) return (kb >= 10 ? Math.round(kb) : Math.round(kb * 10) / 10) + " КиБ";
  const mb = kb / 1024;
  return (mb >= 10 ? Math.round(mb) : Math.round(mb * 10) / 10) + " МиБ";
}

function resolveImageDimensions(meta) {
  if (!meta) return "";
  if (meta.naturalWidth && meta.naturalHeight) {
    return meta.naturalWidth + "×" + meta.naturalHeight;
  }
  if (meta.drawWidth && meta.drawHeight) {
    return meta.drawWidth + "×" + meta.drawHeight;
  }
  if (meta.frameWidth && meta.frameHeight) {
    return meta.frameWidth + "×" + meta.frameHeight;
  }
  return "";
}

function formatChatImageCaption(meta, record) {
  const parts = [];
  if (meta && meta.name) parts.push(meta.name);
  else if (meta && meta.key) parts.push(meta.key);
  const size = record ? record.size : (meta && meta.jpegSize ? meta.jpegSize : 0);
  if (size) parts.push(formatBytesShort(size));
  const dims = resolveImageDimensions(meta);
  if (dims) parts.push(dims);
  if (meta && meta.profile) parts.push("профиль " + meta.profile);
  if (meta && meta.status === "pending") parts.push("отправка…");
  else if (meta && meta.status === "error") parts.push("ошибка");
  return parts.length ? parts.join(" · ") : "Изображение";
}

function ensureImageDimensions(meta, img, caption) {
  if (!meta || !img) return;
  const apply = () => {
    if (!meta.naturalWidth || !meta.naturalHeight) {
      if (img.naturalWidth && img.naturalHeight) {
        meta.naturalWidth = img.naturalWidth;
        meta.naturalHeight = img.naturalHeight;
        saveChatHistory();
        if (caption) caption.textContent = formatChatImageCaption(meta, getChatImageState().cache.get(meta.key) || null);
      }
    }
  };
  if (img.complete) {
    apply();
  } else {
    const handler = () => {
      img.removeEventListener("load", handler);
      apply();
    };
    img.addEventListener("load", handler, { once: true });
  }
}

function applyChatImageBinding(binding) {
  if (!binding || !binding.meta) return;
  const state = getChatImageState();
  const record = state.cache.get(binding.meta.key);
  if (record) {
    binding.element.src = record.url;
    if (binding.caption) binding.caption.textContent = formatChatImageCaption(binding.meta, record);
    ensureImageDimensions(binding.meta, binding.element, binding.caption);
  } else {
    binding.element.removeAttribute("src");
    if (binding.caption) binding.caption.textContent = formatChatImageCaption(binding.meta, null);
  }
}

function updateChatImageElements(key) {
  if (!key) return;
  const state = getChatImageState();
  const bindings = state.elements.get(key);
  if (!bindings) return;
  bindings.forEach((binding) => {
    applyChatImageBinding(binding);
  });
}

function registerChatImageElement(meta, element, caption) {
  if (!meta || !element) return;
  const normalized = normalizeChatImageMeta(meta);
  meta.key = normalized.key;
  meta.name = normalized.name;
  meta.profile = normalized.profile;
  meta.frameWidth = normalized.frameWidth;
  meta.frameHeight = normalized.frameHeight;
  meta.drawWidth = normalized.drawWidth;
  meta.drawHeight = normalized.drawHeight;
  meta.offsetX = normalized.offsetX;
  meta.offsetY = normalized.offsetY;
  meta.grayscale = normalized.grayscale;
  meta.jpegSize = normalized.jpegSize;
  meta.originalSize = normalized.originalSize;
  meta.sourceName = normalized.sourceName;
  meta.source = normalized.source;
  meta.status = normalized.status;
  meta.createdAt = normalized.createdAt;
  meta.naturalWidth = normalized.naturalWidth;
  meta.naturalHeight = normalized.naturalHeight;
  meta.msgId = normalized.msgId;
  const key = meta.key || meta.name;
  if (!key) return;
  element.dataset.imageKey = key;
  const state = getChatImageState();
  let bindings = state.elements.get(key);
  if (!bindings) {
    bindings = new Set();
    state.elements.set(key, bindings);
  }
  const binding = { element, caption, meta };
  bindings.add(binding);
  applyChatImageBinding(binding);
}

function createChatImageFigure(meta) {
  const figure = document.createElement("figure");
  figure.className = "bubble-image";
  const frame = document.createElement("div");
  frame.className = "bubble-image-frame";
  const img = document.createElement("img");
  img.alt = meta && meta.name ? meta.name : "Изображение";
  img.loading = "lazy";
  frame.appendChild(img);
  figure.appendChild(frame);
  const caption = document.createElement("figcaption");
  caption.className = "bubble-image-caption";
  caption.textContent = formatChatImageCaption(meta, getChatImageState().cache.get(meta && meta.key ? meta.key : "") || null);
  figure.appendChild(caption);
  registerChatImageElement(meta, img, caption);
  return figure;
}

function setChatDropState(active) {
  if (!UI.els || !UI.els.chatArea) return;
  if (active) UI.els.chatArea.classList.add("drop-active");
  else UI.els.chatArea.classList.remove("drop-active");
}

function onChatImageButton() {
  if (UI.els && UI.els.chatImageInput) {
    UI.els.chatImageInput.click();
  }
}

function onChatImageInputChange(event) {
  const input = event && event.target ? event.target : UI.els.chatImageInput;
  const files = input && input.files ? Array.from(input.files) : [];
  if (files.length > 0) {
    handleSelectedImages(files, { source: "picker" }).catch((err) => {
      console.error("[chat-image] ошибка обработки выбранных файлов:", err);
      note("Не удалось подготовить изображение");
    });
  }
  if (input) input.value = "";
}

function onChatImageProfileChange(event) {
  const select = event && event.target ? event.target : UI.els.chatImageProfile;
  const profile = setCurrentImageProfile(select ? select.value : null);
  note("Профиль изображений: " + profile.id);
}

function onChatDragEnter(event) {
  if (!event || !event.dataTransfer) return;
  const hasFiles = Array.from(event.dataTransfer.types || []).includes("Files");
  if (!hasFiles) return;
  chatDropCounter += 1;
  event.preventDefault();
  setChatDropState(true);
}

function onChatDragOver(event) {
  if (!event || !event.dataTransfer) return;
  const hasFiles = Array.from(event.dataTransfer.types || []).includes("Files");
  if (!hasFiles) return;
  event.preventDefault();
  event.dataTransfer.dropEffect = "copy";
  setChatDropState(true);
}

function onChatDragLeave(event) {
  if (!event || !event.dataTransfer) return;
  const hasFiles = Array.from(event.dataTransfer.types || []).includes("Files");
  if (!hasFiles) return;
  chatDropCounter = Math.max(0, chatDropCounter - 1);
  if (chatDropCounter === 0) setChatDropState(false);
}

function onChatDrop(event) {
  if (!event || !event.dataTransfer) return;
  event.preventDefault();
  const files = Array.from(event.dataTransfer.files || []);
  chatDropCounter = 0;
  setChatDropState(false);
  if (files.length > 0) {
    handleSelectedImages(files, { source: "drop" }).catch((err) => {
      console.error("[chat-image] ошибка обработки файлов drop:", err);
      note("Не удалось подготовить изображение");
    });
  }
}

async function handleSelectedImages(files, options) {
  const list = Array.from(files || []);
  if (list.length === 0) return;
  for (const file of list) {
    try {
      await processImageFile(file, options || {});
    } catch (err) {
      console.error("[chat-image] ошибка обработки файла:", err);
      note("Не удалось подготовить изображение");
    }
  }
}

async function processImageFile(file, options) {
  if (!(file instanceof File)) {
    note("Неизвестный источник данных");
    return;
  }
  if (file.size > IMAGE_MAX_SOURCE_SIZE) {
    note("Файл слишком большой для обработки (" + formatBytesShort(file.size) + ")");
    return;
  }
  const type = (file.type || "").toLowerCase();
  if (type && !IMAGE_ALLOWED_TYPES.has(type)) {
    note("Формат изображения не поддерживается: " + type);
    return;
  }
  const profile = normalizeImageProfile(UI.cfg && UI.cfg.imageProfile);
  try {
    const prepared = await prepareImageForSend(file, profile);
    if (!prepared || !prepared.blob) {
      note("Не удалось подготовить изображение");
      return;
    }
    await queuePreparedImage(prepared, file, profile, options || {});
  } catch (err) {
    console.error("[chat-image] ошибка подготовки изображения:", err);
    note("Ошибка обработки изображения");
  }
}

async function prepareImageForSend(file, profile) {
  const preset = normalizeImageProfile(profile && profile.id ? profile.id : profile);
  const bitmap = await loadImageBitmap(file);
  if (!bitmap) throw new Error("bitmap");
  const frameWidth = preset.maxWidth;
  const frameHeight = preset.maxHeight;
  const canvasData = drawImageToCanvas(bitmap, frameWidth, frameHeight);
  if (preset.grayscale) {
    applyCanvasGrayscale(canvasData.ctx, canvasData.canvas.width, canvasData.canvas.height);
  }
  const blob = await canvasToJpegBlob(canvasData.canvas, preset.quality);
  if (typeof bitmap.close === "function") {
    try { bitmap.close(); } catch (err) { console.warn("[chat-image] close bitmap", err); }
  }
  return {
    blob,
    frameWidth: canvasData.canvas.width,
    frameHeight: canvasData.canvas.height,
    drawWidth: canvasData.drawWidth,
    drawHeight: canvasData.drawHeight,
    offsetX: canvasData.offsetX,
    offsetY: canvasData.offsetY,
    grayscale: !!preset.grayscale,
  };
}

async function queuePreparedImage(prepared, file, profile, options) {
  const timestamp = Date.now();
  const tempKey = "local:" + timestamp + ":" + Math.random().toString(36).slice(2, 8);
  const meta = normalizeChatImageMeta({
    key: tempKey,
    name: tempKey,
    profile: profile.id,
    frameWidth: prepared.frameWidth,
    frameHeight: prepared.frameHeight,
    drawWidth: prepared.drawWidth,
    drawHeight: prepared.drawHeight,
    offsetX: prepared.offsetX,
    offsetY: prepared.offsetY,
    grayscale: prepared.grayscale,
    jpegSize: prepared.blob.size,
    originalSize: file.size,
    sourceName: file.name || "",
    source: "tx",
    status: "pending",
    createdAt: timestamp,
  });
  cacheChatImage(meta.key, prepared.blob, meta);
  const label = meta.profile ? "Изображение (" + meta.profile + ")" : "Изображение";
  const saved = persistChat(label, "you", { role: "user", tag: "img-out", image: meta });
  addChatMessage(saved.record, saved.index);
  await sendImageMessage(prepared, {
    meta,
    originIndex: saved.index,
    file,
    options,
  });
}

async function loadImageBitmap(file) {
  if (typeof createImageBitmap === "function") {
    try {
      return await createImageBitmap(file);
    } catch (err) {
      console.warn("[chat-image] createImageBitmap недоступен:", err);
    }
  }
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onerror = () => reject(reader.error || new Error("reader"));
    reader.onload = () => {
      const img = new Image();
      img.onload = () => resolve(img);
      img.onerror = () => reject(new Error("image"));
      img.src = reader.result;
    };
    reader.readAsDataURL(file);
  });
}

function drawImageToCanvas(bitmap, frameWidth, frameHeight) {
  const canvas = document.createElement("canvas");
  canvas.width = frameWidth;
  canvas.height = frameHeight;
  const ctx = canvas.getContext("2d", { willReadFrequently: true });
  ctx.fillStyle = "#000";
  ctx.fillRect(0, 0, frameWidth, frameHeight);
  const scale = Math.min(1, Math.min(frameWidth / bitmap.width, frameHeight / bitmap.height));
  const drawWidth = Math.max(1, Math.round(bitmap.width * scale));
  const drawHeight = Math.max(1, Math.round(bitmap.height * scale));
  const offsetX = Math.floor((frameWidth - drawWidth) / 2);
  const offsetY = Math.floor((frameHeight - drawHeight) / 2);
  ctx.drawImage(bitmap, offsetX, offsetY, drawWidth, drawHeight);
  return { canvas, ctx, drawWidth, drawHeight, offsetX, offsetY };
}

function applyCanvasGrayscale(ctx, width, height) {
  if (!ctx) return;
  const image = ctx.getImageData(0, 0, width, height);
  const data = image.data;
  for (let i = 0; i < data.length; i += 4) {
    const r = data[i];
    const g = data[i + 1];
    const b = data[i + 2];
    const y = Math.round(0.299 * r + 0.587 * g + 0.114 * b);
    data[i] = y;
    data[i + 1] = y;
    data[i + 2] = y;
  }
  ctx.putImageData(image, 0, 0);
}

function canvasToJpegBlob(canvas, quality) {
  return new Promise((resolve, reject) => {
    canvas.toBlob((blob) => {
      if (blob) resolve(blob);
      else reject(new Error("toBlob"));
    }, "image/jpeg", quality);
  });
}
function applyChatBubbleContent(node, entry) {
  const raw = entry && entry.m != null ? String(entry.m) : "";
  const isRx = entry && entry.role === "rx";
  const tag = entry && entry.tag ? String(entry.tag) : "";
  const isProgress = tag === "rx-progress";
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
  if (isProgress) {
    textBox.classList.add("with-spinner");
    const spinner = document.createElement("span");
    spinner.className = "bubble-spinner";
    textBox.insertBefore(spinner, textBox.firstChild);
  }
  node.appendChild(textBox);
  const imageMeta = entry && entry.image && typeof entry.image === "object" ? entry.image : null;
  if (imageMeta) {
    node.appendChild(createChatImageFigure(imageMeta));
  }
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
  const entry = entries[index];
  const tag = entry && entry.tag ? String(entry.tag) : "";
  wrap.classList.toggle("progress", tag === "rx-progress");
  wrap.classList.toggle("image", tag === "rx-image");
  if (entry && entry.role === "rx") wrap.classList.add("rx");
  else wrap.classList.remove("rx");
  if (entry && entry.role === "user") wrap.classList.remove("system");
  else if (!wrap.classList.contains("you")) wrap.classList.add("system");
  applyChatBubbleContent(bubble, entry);
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
  const candidates = [];
  const candidatePaths = ["/cmd", "/api/cmd", "/"];
  for (const path of candidatePaths) {
    try {
      candidates.push(new URL(path, base));
    } catch (err) {
      console.warn("[deviceFetch] базовый URL не поддерживает относительный путь", path, err);
    }
  }
  if (candidates.length === 0) {
    console.warn("[deviceFetch] не удалось построить относительные пути, используется базовый адрес как есть");
    candidates.push(base);
  }
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
      const name = e && e.name ? String(e.name) : "";
      const message = e && e.message ? String(e.message) : "";
      // Преобразуем ошибки fetch в более понятные пользователю сообщения
      if (name === "AbortError") {
        lastErr = `тайм-аут запроса (${timeout} мс)`;
      } else if (name === "TypeError" && (!message || /Failed to fetch/i.test(message))) {
        lastErr = "сетевой запрос не выполнен (проверьте подключение к устройству)";
      } else {
        lastErr = message || String(e);
      }
    }
  }
  return { ok: false, error: lastErr || "Unknown error" };
}

// Получение последних строк журнала устройства через HTTP
async function fetchDeviceLogHistory(limit, timeoutMs) {
  const timeout = timeoutMs || 4000;
  let base;
  try {
    base = new URL(UI.cfg.endpoint || "http://192.168.4.1");
  } catch (e) {
    base = new URL("http://192.168.4.1");
  }
  const url = new URL("/api/logs", base);
  if (Number.isFinite(limit) && limit > 0) {
    url.searchParams.set("n", String(Math.round(limit)));
  }
  const ctrl = new AbortController();
  const timer = setTimeout(() => ctrl.abort(), timeout);
  try {
    const res = await fetch(url.toString(), { signal: ctrl.signal, headers: { "Accept": "application/json" } });
    clearTimeout(timer);
    if (!res.ok) return { ok: false, error: "HTTP " + res.status };
    let data = null;
    try {
      data = await res.json();
    } catch (err) {
      return { ok: false, error: "invalid JSON" };
    }
    if (!data || !Array.isArray(data.logs)) {
      return { ok: false, error: "missing logs" };
    }
    return { ok: true, logs: data.logs };
  } catch (err) {
    clearTimeout(timer);
    const name = err && err.name ? String(err.name) : "";
    const message = err && err.message ? String(err.message) : "";
    if (name === "AbortError") {
      return { ok: false, error: `тайм-аут запроса (${timeout} мс)` };
    }
    if (name === "TypeError" && (!message || /Failed to fetch/i.test(message))) {
      return { ok: false, error: "сетевой запрос не выполнен (проверьте подключение к устройству)" };
    }
    return { ok: false, error: message || String(err) };
  }
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
const DEVICE_COMMAND_TIMEOUT_MS = 4000; // Базовый тайм-аут для команд и фонового опроса

async function sendCommand(cmd, params, opts) {
  const options = opts || {};
  const silent = options.silent === true;
  const timeout = options.timeoutMs || DEVICE_COMMAND_TIMEOUT_MS;
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
  const errText = res.error != null ? String(res.error) : "ошибка";
  if (!silent) {
    status("✗ " + cmd);
    note("Команда " + cmd + ": " + summarizeResponse(errText, "ошибка"));
    logSystemCommand(cmd, errText, "error");
  }
  logErrorEvent(debugLabel || cmd, errText);
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
    const name = e && e.name ? String(e.name) : "";
    const message = e && e.message ? String(e.message) : "";
    // Сообщаем о сетевых ошибках на понятном языке
    if (name === "AbortError") {
      return { ok: false, error: `тайм-аут запроса (${timeout} мс)` };
    }
    if (name === "TypeError" && (!message || /Failed to fetch/i.test(message))) {
      return { ok: false, error: "сетевой запрос не выполнен (проверьте подключение к устройству)" };
    }
    return { ok: false, error: message || String(e) };
  }
}

async function postImage(blob, meta, timeoutMs) {
  const timeout = timeoutMs || 8000;
  let base;
  try {
    base = new URL(UI.cfg.endpoint || "http://192.168.4.1");
  } catch (e) {
    base = new URL("http://192.168.4.1");
  }
  const url = new URL("/api/tx-image", base);
  const headers = buildImageHeaders(meta);
  const ctrl = new AbortController();
  const timer = setTimeout(() => ctrl.abort(), timeout);
  try {
    const res = await fetch(url.toString(), {
      method: "POST",
      headers,
      body: blob,
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
  } catch (err) {
    clearTimeout(timer);
    const name = err && err.name ? String(err.name) : "";
    const message = err && err.message ? String(err.message) : "";
    if (name === "AbortError") {
      return { ok: false, error: `тайм-аут запроса (${timeout} мс)` };
    }
    if (name === "TypeError" && (!message || /Failed to fetch/i.test(message))) {
      return { ok: false, error: "сетевой запрос не выполнен (проверьте подключение к устройству)" };
    }
    return { ok: false, error: message || String(err) };
  }
}

function buildImageHeaders(meta) {
  const headers = new Headers();
  headers.set("Content-Type", "image/jpeg");
  const profile = normalizeImageProfile(meta && meta.profile ? meta.profile : UI.cfg.imageProfile);
  headers.set("X-Image-Profile", profile.id);
  const setIfPositive = (key, value) => {
    const num = Number(value);
    if (Number.isFinite(num) && num > 0) headers.set(key, String(Math.round(num)));
  };
  setIfPositive("X-Image-Frame-Width", meta && meta.frameWidth);
  setIfPositive("X-Image-Frame-Height", meta && meta.frameHeight);
  setIfPositive("X-Image-Scaled-Width", meta && meta.drawWidth);
  setIfPositive("X-Image-Scaled-Height", meta && meta.drawHeight);
  setIfPositive("X-Image-Offset-X", meta && meta.offsetX);
  setIfPositive("X-Image-Offset-Y", meta && meta.offsetY);
  setIfPositive("X-Image-Original-Size", meta && meta.originalSize);
  headers.set("X-Image-Grayscale", meta && meta.grayscale ? "1" : "0");
  return headers;
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
  logErrorEvent("TX", errorText || "ошибка");
  let attached = false;
  if (originIndex !== null) {
    attached = attachTxStatus(originIndex, { ok: false, text: errorText, raw: res.error });
  }
  if (!attached) {
    logSystemCommand("TX", errorText, "error");
  }
  return null;
}

async function sendImageMessage(prepared, context) {
  const ctx = context || {};
  const meta = ctx.meta ? ctx.meta : null;
  const originIndex = typeof ctx.originIndex === "number" ? ctx.originIndex : null;
  if (!prepared || !prepared.blob || !meta) {
    note("Не удалось отправить изображение");
    return null;
  }
  status("→ IMG");
  const res = await postImage(prepared.blob, meta, 9000);
  const entries = getChatHistory();
  const entry = originIndex != null && entries[originIndex] ? entries[originIndex] : null;
  if (res.ok) {
    status("✓ IMG");
    const value = res.text != null ? String(res.text) : "";
    note("Изображение: " + summarizeResponse(value, "успешно"));
    const match = value.match(/id\s*=\s*(\d+)/i);
    if (match) {
      meta.msgId = Number(match[1]);
      const newName = makeImageNameFromId(meta.msgId);
      if (newName) {
        const oldKey = meta.key;
        meta.name = newName;
        meta.key = newName;
        renameChatImageKey(oldKey, newName);
      }
    }
    meta.status = "sent";
    if (entry) {
      entry.image = sanitizeChatImageMeta(meta);
      if (entry.image && entry.image.name) {
        entry.m = "Изображение " + entry.image.name;
      }
      saveChatHistory();
      updateChatMessageContent(originIndex);
    }
    if (originIndex != null) {
      attachTxStatus(originIndex, { ok: true, text: value, raw: res.text });
    }
    return value;
  }
  status("✗ IMG");
  const errorText = res.error != null ? String(res.error) : "";
  note("Изображение: " + summarizeResponse(errorText, "ошибка"));
  meta.status = "error";
  if (entry) {
    entry.image = sanitizeChatImageMeta(meta);
    saveChatHistory();
    updateChatMessageContent(originIndex);
  }
  if (originIndex != null) {
    attachTxStatus(originIndex, { ok: false, text: errorText, raw: res.error });
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
  } else if (upper === "RXBG") {
    const state = parseRxBoostedGainResponse(text);
    if (state !== null) {
      UI.state.rxBoostedGain = state;
      storage.set("set.RXBG", state ? "1" : "0");
      updateRxBoostedGainUi();
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
        note("TESTRXM запланирован");
        pollReceivedAfterTestRxm(info.count).catch((err) => {
          console.warn("[testrxm] не удалось запустить автоматический опрос:", err);
        });
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
      const out = [];
      const seen = new Set();
      const pushEntry = (sourceEntry, fallbackType) => {
        if (sourceEntry == null) return;
        const src = (sourceEntry && typeof sourceEntry === "object") ? sourceEntry : { name: sourceEntry };
        const nameRaw = src.name != null ? String(src.name) : "";
        const name = nameRaw.trim();
        const typeHint = src.type != null ? src.type : (src.kind != null ? src.kind : (src.queue != null ? src.queue : fallbackType));
        const type = normalizeReceivedType(name, typeHint);
        let text = "";
        if (src.text != null) text = String(src.text);
        else if (src.payload && typeof src.payload === "object" && src.payload.text != null) text = String(src.payload.text);
        else if (src.data && typeof src.data === "object" && src.data.text != null) text = String(src.data.text);
        let hex = "";
        if (src.hex != null) hex = String(src.hex);
        else if (src.payload && typeof src.payload === "object" && src.payload.hex != null) hex = String(src.payload.hex);
        else if (src.data && typeof src.data === "object" && src.data.hex != null) hex = String(src.data.hex);
        hex = hex.trim();
        let lenValue = src.len;
        if (lenValue == null && src.length != null) lenValue = src.length;
        if (lenValue == null && src.payload && typeof src.payload === "object" && src.payload.len != null) lenValue = src.payload.len;
        if (lenValue == null && src.data && typeof src.data === "object" && src.data.len != null) lenValue = src.data.len;
        let lenNum = Number(lenValue);
        if (!Number.isFinite(lenNum) || lenNum < 0) lenNum = null;
        let bytes = null;
        if (src._hexBytes instanceof Uint8Array) bytes = src._hexBytes;
        if ((!bytes || !bytes.length) && Array.isArray(src.bytes)) {
          const collected = [];
          for (let i = 0; i < src.bytes.length; i += 1) {
            const value = Number(src.bytes[i]);
            if (!Number.isFinite(value)) continue;
            collected.push(Math.max(0, Math.min(255, Math.round(value))));
          }
          if (collected.length) bytes = new Uint8Array(collected);
        }
        if ((!bytes || !bytes.length) && hex) bytes = hexToBytes(hex);
        if (bytes && bytes.length) {
          if (!hex) {
            let hexString = "";
            for (let i = 0; i < bytes.length; i += 1) {
              hexString += bytes[i].toString(16).padStart(2, "0");
            }
            hex = hexString;
          }
          if (lenNum == null) lenNum = bytes.length;
        }
        if (lenNum == null && text) lenNum = text.length;
        const normalized = {
          name,
          type,
          text: text != null ? String(text) : "",
          hex,
          len: Number.isFinite(lenNum) && lenNum >= 0 ? lenNum : 0,
        };
        if (bytes && bytes.length) normalized._hexBytes = bytes;
        const resolved = resolveReceivedText(normalized);
        if (resolved && !normalized.text) normalized.text = resolved;
        normalized.resolvedText = resolved;
        normalized.resolvedLen = getReceivedLength(normalized);
        const key = (normalized.name || "") + "|" + normalized.type + "|" + (normalized.hex ? normalized.hex : normalized.text);
        if (seen.has(key)) return;
        seen.add(key);
        out.push(normalized);
      };
      const processList = (value, fallbackType) => {
        if (!value) return;
        if (Array.isArray(value)) {
          value.forEach((item) => pushEntry(item, fallbackType));
          return;
        }
        if (value && typeof value === "object" && Array.isArray(value.items)) {
          value.items.forEach((item) => pushEntry(item, fallbackType));
        }
      };
      if (Array.isArray(parsed)) {
        processList(parsed, null);
      } else if (parsed && typeof parsed === "object") {
        if (Array.isArray(parsed.items)) processList(parsed.items, null);
        else if (parsed.items && typeof parsed.items === "object" && Array.isArray(parsed.items.items)) {
          processList(parsed.items.items, null);
        }
        Object.keys(parsed).forEach((key) => {
          if (key === "items") return;
          processList(parsed[key], key);
        });
      }
      if (out.length) return out;
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

// Периодически опрашиваем список полученных сообщений после TESTRXM и выводим новые пакеты в чат
async function pollReceivedAfterTestRxm(threshold, options) {
  const state = UI.state && UI.state.testRxm ? UI.state.testRxm : null;
  if (!state || state.polling) return;
  state.polling = true;
  const opts = options || {};
  const limitRaw = Number(threshold);
  const limit = Number.isFinite(limitRaw) && limitRaw > 0 ? Math.ceil(limitRaw) : 5;
  const maxAttemptsRaw = Number(opts.maxAttempts);
  const maxAttempts = Number.isFinite(maxAttemptsRaw) && maxAttemptsRaw > 0 ? Math.ceil(maxAttemptsRaw) : 6;
  const delayRaw = Number(opts.delayMs);
  const delayMs = Number.isFinite(delayRaw) && delayRaw >= 0 ? delayRaw : 320;
  const ensureLastName = () => {
    if (state.lastName) return state.lastName;
    const history = getChatHistory();
    for (let i = history.length - 1; i >= 0; i -= 1) {
      const entry = history[i];
      if (entry && entry.role === "rx" && entry.rx && typeof entry.rx === "object" && entry.rx.name) {
        state.lastName = String(entry.rx.name);
        break;
      }
    }
    return state.lastName;
  };
  ensureLastName();
  try {
    let attempt = 0;
    let lastName = state.lastName || null;
    while (attempt < maxAttempts) {
      attempt += 1;
      const res = await deviceFetch("RSTS", { json: "1", n: String(limit) }, 4000);
      if (res.ok) {
        const items = parseReceivedResponse(res.text) || [];
        const readyItems = items.filter((item) => item && item.type === "ready");
        let startIndex = 0;
        if (lastName) {
          const existingIdx = readyItems.findIndex((item) => item && item.name === lastName);
          if (existingIdx >= 0) startIndex = existingIdx + 1;
        }
        const fresh = readyItems.slice(startIndex);
        if (fresh.length > 0) {
          for (let i = 0; i < fresh.length; i += 1) {
            const entry = fresh[i];
            if (!entry) continue;
            const rxName = entry.name != null ? String(entry.name) : "";
            const rxTextRaw = entry.text != null ? String(entry.text) : "";
            const rxText = rxTextRaw.trim() || (entry.resolvedText != null ? String(entry.resolvedText).trim() : "");
            const rxHex = entry.hex != null ? String(entry.hex) : "";
            const rxLenRaw = Number(entry.len);
            const rxLen = Number.isFinite(rxLenRaw) && rxLenRaw >= 0 ? rxLenRaw : getReceivedLength(entry);
            const messageText = rxText || rxName || "RX сообщение";
            const saved = persistChat(messageText, "dev", {
              role: "rx",
              tag: "rx-message",
              rx: {
                name: rxName,
                text: rxText || messageText,
                hex: rxHex,
                len: rxLen != null ? rxLen : 0,
              },
            });
            addChatMessage(saved.record, saved.index);
            if (rxName) {
              lastName = rxName;
              state.lastName = rxName;
            }
          }
          break;
        }
      } else {
        console.warn("[testrxm] RSTS недоступен:", res.error);
      }
      if (attempt < maxAttempts) {
        await new Promise((resolve) => setTimeout(resolve, delayMs));
      }
    }
  } catch (err) {
    console.warn("[testrxm] ошибка опроса TESTRXM:", err);
  } finally {
    state.polling = false;
  }
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

function parseReceivedNumericId(name) {
  const raw = name != null ? String(name).trim() : "";
  if (!raw) return null;
  const match = raw.match(/(?:GO|SP|R)[^0-9]*(\d{1,6})/i);
  if (!match) return null;
  const digits = match[1];
  if (!digits) return null;
  return digits.padStart(5, "0");
}

function makeReadyNameFromId(id) {
  const digits = id != null ? String(id).replace(/\D+/g, "") : "";
  if (!digits) return null;
  return "GO-" + digits.padStart(5, "0");
}

function clearSplitProgressTracking(name) {
  if (!name) return;
  const state = getReceivedMonitorState();
  if (!(state.progress instanceof Map)) return;
  if (state.progress.delete(name)) {
    if (state.progress.size === 0) setChatReceivingIndicatorState(false);
    return;
  }
  for (const [key, info] of state.progress.entries()) {
    if (info && info.name === name) {
      state.progress.delete(key);
      if (state.progress.size === 0) setChatReceivingIndicatorState(false);
      break;
    }
  }
}

function upsertSplitProgress(entry) {
  if (!entry) return;
  const nameRaw = entry.name != null ? String(entry.name).trim() : "";
  if (!nameRaw) return;
  const state = getReceivedMonitorState();
  setChatReceivingIndicatorState(true);
  const history = getChatHistory();
  const id = parseReceivedNumericId(nameRaw);
  const readyName = makeReadyNameFromId(id);
  const key = readyName || nameRaw;
  const length = getReceivedLength(entry);
  const lengthText = Number.isFinite(length) && length > 0 ? `${length} байт` : "ожидание данных";
  const caption = readyName ? `Приём пакета ${readyName}` : `Приём пакета ${nameRaw}`;
  const detail = readyName ? `${readyName}: получено ${lengthText}` : `Получено ${lengthText}`;
  const rxText = `${caption} · ${lengthText}`;
  let index = -1;
  const known = state.progress instanceof Map ? state.progress.get(key) : null;
  if (known && typeof known.index === "number") {
    index = known.index;
  } else if (readyName) {
    index = history.findIndex((item) => item && item.rx && item.rx.name === readyName && item.tag === "rx-progress");
  }
  if (index >= 0 && history[index]) {
    const record = history[index];
    let changed = false;
    if (record.m !== caption) { record.m = caption; changed = true; }
    if (record.role !== "rx") { record.role = "rx"; changed = true; }
    if (record.tag !== "rx-progress") { record.tag = "rx-progress"; changed = true; }
    if (!record.rx || typeof record.rx !== "object") {
      record.rx = {};
      changed = true;
    }
    if (record.rx.name !== (readyName || nameRaw)) { record.rx.name = readyName || nameRaw; changed = true; }
    if (record.rx.type !== "split") { record.rx.type = "split"; changed = true; }
    if (Number.isFinite(length) && record.rx.len !== length) { record.rx.len = length; changed = true; }
    if (record.rx.text !== rxText) { record.rx.text = rxText; changed = true; }
    if (record.detail !== detail) { record.detail = detail; changed = true; }
    if (changed) saveChatHistory();
    updateChatMessageContent(index);
    state.progress.set(key, { index, name: readyName || nameRaw });
    return;
  }
  const meta = {
    role: "rx",
    tag: "rx-progress",
    rx: {
      name: readyName || nameRaw,
      type: "split",
      len: Number.isFinite(length) && length >= 0 ? length : 0,
      text: rxText,
    },
    detail,
  };
  const saved = persistChat(caption, "dev", meta);
  addChatMessage(saved.record, saved.index);
  state.progress.set(key, { index: saved.index, name: readyName || nameRaw });
}

/* Фоновый монитор входящих сообщений */
function logReceivedMessage(entry, opts) {
  if (!entry) return;
  const options = opts || {};
  const name = entry.name != null ? String(entry.name).trim() : "";
  const entryType = normalizeReceivedType(name, entry.type);
  if (entryType === "split") {
    upsertSplitProgress(entry);
    return;
  }
  if (!name || entryType !== "ready") {
    // В чат попадают только элементы со статусом ready; без имени их сложно отслеживать.
    return;
  }
  if (options.isNew && UI.state.activeTab !== "chat") {
    const current = Math.max(0, Number(UI.state.chatUnread) || 0);
    const next = Math.min(current + 1, CHAT_UNREAD_MAX);
    if (next !== current) {
      UI.state.chatUnread = next;
      storage.set(CHAT_UNREAD_STORAGE_KEY, String(next));
    }
    updateChatUnreadBadge({ animate: true });
  } else if (options.isNew && UI.state.activeTab === "chat") {
    if (UI.state.chatUnread !== 0) {
      UI.state.chatUnread = 0;
      storage.set(CHAT_UNREAD_STORAGE_KEY, "0");
    }
    updateChatUnreadBadge();
  }
  const bytes = entry && entry._hexBytes instanceof Uint8Array ? entry._hexBytes : hexToBytes(entry.hex);
  const isImage = entryType === "ready" && bytes && bytes.length >= 3 && bytes[0] === 0xFF && bytes[1] === 0xD8 && bytes[2] === 0xFF;
  const textValue = resolveReceivedText(entry);
  const fallbackTextRaw = entry.text != null ? String(entry.text) : "";
  const fallbackText = fallbackTextRaw.trim();
  const messageBody = textValue || fallbackText;
  let length = getReceivedLength(entry);
  if (isImage && bytes && bytes.length) length = bytes.length;
  if (!Number.isFinite(length) || length < 0) length = null;
  if (length == null && messageBody) length = messageBody.length;
  const rxMeta = {
    name,
    type: entryType,
    hex: entry.hex || "",
  };
  if (length != null) rxMeta.len = length;
  let imageMeta = null;
  let message = "";
  if (isImage && bytes && bytes.length) {
    const blob = new Blob([bytes], { type: "image/jpeg" });
    const key = imageNameFromRawName(name) || (name ? name : "img:" + Date.now());
    imageMeta = normalizeChatImageMeta({
      key,
      name: imageNameFromRawName(name) || key,
      profile: getCurrentImageProfileId(),
      jpegSize: bytes.length,
      source: "rx",
      status: "received",
      createdAt: Date.now(),
    });
    cacheChatImage(imageMeta.key, blob, imageMeta);
    message = imageMeta.name ? "Изображение " + imageMeta.name : "Изображение";
    rxMeta.text = message;
    rxMeta.image = { name: imageMeta.name || imageMeta.key, size: bytes.length };
  } else if (messageBody) {
    message = messageBody;
    rxMeta.text = message;
  } else {
    const base = name || "RX сообщение";
    const lengthText = Number.isFinite(length) && length >= 0 ? `${length} байт` : "";
    message = lengthText ? `${base} · ${lengthText}` : base;
    rxMeta.text = message;
  }
  const history = getChatHistory();
  let existingIndex = -1;
  if (name && Array.isArray(history)) {
    existingIndex = history.findIndex((item) => item && item.rx && item.rx.name === name);
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
    const desiredTag = imageMeta ? "rx-image" : "rx-message";
    if (existing.tag !== desiredTag) {
      existing.tag = desiredTag;
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
    if (existing.tag === "rx-progress" && existing.detail) {
      existing.detail = "";
      changed = true;
    }
    if (imageMeta) {
      if (!existing.image || typeof existing.image !== "object") {
        existing.image = sanitizeChatImageMeta(imageMeta);
        changed = true;
      } else {
        const sanitized = sanitizeChatImageMeta(imageMeta);
        const keys = Object.keys(sanitized);
        for (let i = 0; i < keys.length; i += 1) {
          const key = keys[i];
          if (existing.image[key] !== sanitized[key]) {
            existing.image[key] = sanitized[key];
            changed = true;
          }
        }
      }
    }
    if (!existing.t) {
      existing.t = Date.now();
      changed = true;
    }
    if (changed) saveChatHistory();
    updateChatMessageContent(existingIndex);
    clearSplitProgressTracking(name);
    return;
  }
  if (options.isNew === false && !messageBody && !imageMeta) {
    // Для старых записей без текста не добавляем дубль.
    return;
  }
  if (!imageMeta && !messageBody && name && message === name) {
    // Для пакетов без payload добавляем подпись с длиной, чтобы было видно объём данных
    const lengthText = Number.isFinite(length) && length >= 0 ? `${length} байт` : "нет данных";
    rxMeta.text = `${name} · ${lengthText}`;
    message = rxMeta.text;
  }
  const meta = { role: "rx", tag: imageMeta ? "rx-image" : "rx-message", rx: rxMeta };
  if (imageMeta) meta.image = sanitizeChatImageMeta(imageMeta);
  const saved = persistChat(message, "dev", meta);
  addChatMessage(saved.record, saved.index);
  clearSplitProgressTracking(name);
}

function getReceivedMonitorState() {
  if (!UI.state || typeof UI.state !== "object") UI.state = {};
  let state = UI.state.received;
  if (!state || typeof state !== "object") {
    state = { timer: null, metricsTimer: null, running: false, known: new Set(), limit: null, awaiting: false, progress: new Map() };
    UI.state.received = state;
  }
  if (!(state.known instanceof Set)) {
    state.known = new Set(state.known ? Array.from(state.known) : []);
  }
  if (!(state.progress instanceof Map)) {
    // Карта отслеживания промежуточных сообщений (SP-xxxxx → индекс в истории чата)
    state.progress = new Map();
  }
  if (!state.metrics || typeof state.metrics !== "object") {
    // Метрики фонового опроса RSTS для вкладки Debug
    state.metrics = {
      configuredIntervalMs: null,
      lastStartedAt: null,
      lastFinishedAt: null,
      lastDurationMs: null,
      averageDurationMs: null,
      samples: 0,
      totalAttempts: 0,
      totalSuccess: 0,
      totalErrors: 0,
      totalTimeouts: 0,
      skippedOverlaps: 0,
      lastGapMs: null,
      lastOkAt: null,
      lastErrorAt: null,
      lastErrorText: null,
      lastBlockedAt: null,
      lastBlockedReason: null,
      consecutiveErrors: 0,
      runningSince: null,
    };
  }
  if (!state.push || typeof state.push !== "object") {
    state.push = {
      supported: typeof EventSource !== "undefined",
      connected: false,
      connecting: false,
      source: null,
      mode: "poll",
      lastEventAt: null,
      lastOpenAt: null,
      lastErrorAt: null,
      retryCount: 0,
      pendingRefresh: false,
      refreshScheduled: false,
      lastHint: null,
    };
  } else if (typeof EventSource === "undefined") {
    state.push.supported = false;
  }
  let limit = Number(state.limit);
  if (!Number.isFinite(limit) || limit <= 0) {
    const storedRaw = storage.get("recvLimit");
    const parsed = storedRaw != null ? parseInt(storedRaw, 10) : NaN;
    limit = Number.isFinite(parsed) && parsed > 0 ? parsed : 20;
  }
  if (!Number.isFinite(limit) || limit <= 0) limit = 20;
  state.limit = Math.min(Math.max(Math.round(limit), 1), 200);
  return state;
}

// Состояние буфера журнала устройства для вкладки Debug
function getDeviceLogState() {
  if (!UI.state || typeof UI.state !== "object") UI.state = {};
  let state = UI.state.deviceLog;
  if (!state || typeof state !== "object") {
    state = { initialized: false, loading: false, known: new Set(), lastId: 0, queue: [] };
    UI.state.deviceLog = state;
  }
  if (!(state.known instanceof Set)) {
    state.known = new Set(state.known ? Array.from(state.known) : []);
  }
  if (!Number.isFinite(state.lastId)) state.lastId = 0;
  if (!Array.isArray(state.queue)) state.queue = [];
  return state;
}

// Форматирование продолжительности запроса в человеко-понятный вид
function formatDurationMs(ms) {
  if (!Number.isFinite(ms)) return "—";
  if (ms < 1) return ms.toFixed(2) + " мс";
  if (ms < 1000) return Math.round(ms) + " мс";
  const seconds = ms / 1000;
  if (seconds < 60) return seconds.toFixed(seconds >= 10 ? 1 : 2) + " с";
  const minutes = Math.floor(seconds / 60);
  const rest = seconds - minutes * 60;
  if (minutes < 60) return `${minutes} мин ${rest.toFixed(0)} с`;
  const hours = Math.floor(minutes / 60);
  const restMin = minutes - hours * 60;
  return `${hours} ч ${restMin} мин`;
}

// Форматируем аппаратное uptime устройства с префиксом «+»
function formatDeviceUptime(ms) {
  if (!Number.isFinite(ms)) return "+0 мс";
  const normalized = ms < 0 ? 0 : ms;
  return "+" + formatDurationMs(normalized);
}

// Форматирование относительного времени (например, «5 с назад»)
function formatRelativeTime(ts) {
  if (!Number.isFinite(ts)) return "—";
  const now = Date.now();
  const delta = Math.max(0, now - ts);
  if (delta < 5000) return "только что";
  if (delta < 60000) return `${Math.round(delta / 1000)} с назад`;
  if (delta < 3600000) {
    const minutes = Math.round(delta / 60000);
    return `${minutes} мин назад`;
  }
  const hours = Math.round(delta / 3600000);
  return `${hours} ч назад`;
}

// Обновление панели диагностики опроса RSTS во вкладке Debug
function updateReceivedMonitorDiagnostics() {
  const els = UI.els && UI.els.receivedDiag ? UI.els.receivedDiag : null;
  if (!els || !els.root) return;
  const state = UI.state && UI.state.received ? UI.state.received : getReceivedMonitorState();
  const metrics = state.metrics || {};
  const statusEl = els.status;
  const now = Date.now();
  const push = state.push || {};
  let statusText = "Ожидание запуска";
  let statusClass = "";
  if (push.connected) {
    const last = push.lastEventAt ? formatRelativeTime(push.lastEventAt) : "недавно";
    statusText = "Push подписка активна · " + last;
    statusClass = "";
  } else if (push.connecting) {
    statusText = "Подключение к push-каналу…";
    statusClass = "";
  } else if (state.running) {
    statusText = "Выполняется запрос";
    statusClass = "warn";
  } else if (metrics.consecutiveErrors >= 3) {
    statusText = "Нет ответа, проверьте соединение";
    statusClass = "error";
  } else if (metrics.consecutiveErrors > 0) {
    statusText = "Есть ошибки опроса";
    statusClass = "warn";
  } else if (metrics.lastOkAt) {
    statusText = "Ответ получен " + formatRelativeTime(metrics.lastOkAt);
    statusClass = "";
  } else if (metrics.totalAttempts > 0) {
    statusText = "Ответ не получен";
    statusClass = "warn";
  }
  if (statusEl) {
    statusEl.textContent = statusText;
    statusEl.classList.remove("warn", "error");
    if (statusClass) statusEl.classList.add(statusClass);
  }
  if (els.mode) {
    let modeText = "Push-канал не активен";
    if (push.supported === false) {
      modeText = "Push недоступен в этом браузере, используется опрос";
    } else if (push.connected) {
      modeText = "Push активен, резервный опрос каждые 30 с";
    } else if (push.connecting) {
      modeText = "Push: ожидаем подключение, работает стандартный опрос";
    } else {
      modeText = "Используется опрос каждые 5 с";
    }
    els.mode.textContent = modeText;
  }
  const intervalMs = Number(metrics.configuredIntervalMs);
  if (els.interval) {
    els.interval.textContent = formatDurationMs(metrics.configuredIntervalMs);
    els.interval.classList.remove("warn", "error");
  }
  if (els.lastStart) els.lastStart.textContent = metrics.lastStartedAt ? formatRelativeTime(metrics.lastStartedAt) : "—";
  if (els.lastFinish) els.lastFinish.textContent = metrics.lastFinishedAt ? formatRelativeTime(metrics.lastFinishedAt) : "—";
  if (els.duration) {
    els.duration.textContent = formatDurationMs(metrics.lastDurationMs);
    els.duration.classList.remove("warn", "error");
    if (Number.isFinite(metrics.lastDurationMs) && Number.isFinite(intervalMs) && intervalMs > 0) {
      if (metrics.lastDurationMs > intervalMs * 1.5) els.duration.classList.add("error");
      else if (metrics.lastDurationMs > intervalMs) els.duration.classList.add("warn");
    }
  }
  if (els.avg) {
    els.avg.textContent = formatDurationMs(metrics.averageDurationMs);
    els.avg.classList.remove("warn", "error");
    if (Number.isFinite(metrics.averageDurationMs) && Number.isFinite(intervalMs) && intervalMs > 0) {
      if (metrics.averageDurationMs > intervalMs * 1.3) els.avg.classList.add("warn");
    }
  }
  if (els.gap) {
    const gapText = Number.isFinite(metrics.lastGapMs) ? formatDurationMs(metrics.lastGapMs) : "—";
    els.gap.textContent = gapText;
    els.gap.classList.remove("warn", "error");
    if (Number.isFinite(metrics.lastGapMs) && Number.isFinite(intervalMs) && intervalMs > 0) {
      if (metrics.lastGapMs > intervalMs * 2.5) els.gap.classList.add("error");
      else if (metrics.lastGapMs > intervalMs * 1.5) els.gap.classList.add("warn");
    }
  }
  if (els.runningSince) {
    els.runningSince.textContent = state.running && metrics.runningSince ? formatRelativeTime(metrics.runningSince) : "—";
  }
  if (els.totals) {
    const ok = metrics.totalSuccess || 0;
    const fail = metrics.totalErrors || 0;
    els.totals.textContent = `${metrics.totalAttempts || 0} / ${ok} ✓ / ${fail} ✗`;
    els.totals.classList.toggle("warn", fail > 0);
  }
  if (els.timeouts) {
    els.timeouts.textContent = String(metrics.totalTimeouts || 0);
    els.timeouts.classList.toggle("warn", (metrics.totalTimeouts || 0) > 0);
  }
  if (els.overlaps) {
    const overlaps = metrics.skippedOverlaps || 0;
    els.overlaps.textContent = String(overlaps);
    els.overlaps.classList.toggle("warn", overlaps > 0);
  }
  if (els.errorBox) {
    const hasError = !!metrics.lastErrorText;
    els.errorBox.textContent = hasError
      ? `${formatRelativeTime(metrics.lastErrorAt || now)} · ${metrics.lastErrorText}`
      : "Ошибок нет";
    els.errorBox.hidden = !hasError;
    els.errorBox.classList.toggle("error", hasError);
  }
  if (els.blockedBox) {
    const hasBlock = !!metrics.lastBlockedAt;
    els.blockedBox.textContent = hasBlock
      ? `${formatRelativeTime(metrics.lastBlockedAt)} · ${metrics.lastBlockedReason || "Блокировка"}`
      : "Блокировок не было";
    els.blockedBox.hidden = !hasBlock;
  }
}

// Обновляем визуальный индикатор, показывающий ожидание оставшихся пакетов SP-...
function setChatReceivingIndicatorState(active) {
  const indicator = UI.els && UI.els.chatRxIndicator ? UI.els.chatRxIndicator : null;
  const state = getReceivedMonitorState();
  state.awaiting = !!active;
  if (!indicator) return;
  const flag = !!active;
  indicator.classList.toggle("active", flag);
  indicator.hidden = !flag;
}

// Проверяем список RSTS и решаем, нужно ли показывать анимацию приёма
function updateChatReceivingIndicatorFromRsts(items) {
  const list = Array.isArray(items) ? items : [];
  const awaiting = list.some((entry) => {
    if (!entry) return false;
    const name = entry.name != null ? String(entry.name).trim() : "";
    const entryType = normalizeReceivedType(name, entry.type);
    return entryType === "split";
  });
  setChatReceivingIndicatorState(awaiting);
}

function handleReceivedSnapshot(items) {
  const state = getReceivedMonitorState();
  const prev = state.known;
  const next = new Set();
  const list = Array.isArray(items) ? items : [];
  const firstLoad = !state.snapshotReady;
  for (let i = 0; i < list.length; i += 1) {
    const entry = list[i];
    const name = entry && entry.name ? String(entry.name).trim() : "";
    if (name) next.add(name);
    const isNew = !firstLoad && !!(name && !prev.has(name));
    logReceivedMessage(entry, { isNew });
  }
  state.known = next;
  state.snapshotReady = true;
  updateChatReceivingIndicatorFromRsts(list);
}

function scheduleReceivedRefreshFromPush(hint) {
  const state = getReceivedMonitorState();
  const push = state.push;
  if (!push) return;
  push.lastHint = hint || null;
  if (push.refreshScheduled) return;
  push.refreshScheduled = true;
  Promise.resolve().then(() => {
    push.refreshScheduled = false;
    if (state.running) {
      push.pendingRefresh = true;
      return;
    }
    pollReceivedMessages({ silentError: true }).catch((err) => {
      console.warn("[push] ошибка обновления списка сообщений:", err);
    });
  });
}

function handleReceivedPushMessage(event) {
  const state = getReceivedMonitorState();
  const push = state.push;
  if (!push) return;
  const raw = event && typeof event.data === "string" ? event.data : "";
  let payload = null;
  if (raw) {
    try {
      payload = JSON.parse(raw);
    } catch (err) {
      push.lastHint = raw;
    }
  }
  if (payload && payload.kind) {
    const kind = String(payload.kind).toLowerCase();
    if (kind === "split") {
      state.awaiting = true;
      setChatReceivingIndicatorState(true);
    } else if (kind === "ready") {
      state.awaiting = false;
    }
  }
  scheduleReceivedRefreshFromPush(payload);
}

function handleDeviceLogPushMessage(event) {
  const raw = event && typeof event.data === "string" ? event.data : "";
  if (!raw) return;
  let payload = null;
  try {
    payload = JSON.parse(raw);
  } catch (err) {
    console.warn("[push] не удалось распарсить log-сообщение:", err, raw);
    return;
  }
  if (!payload || typeof payload.text === "undefined") return;
  const state = getDeviceLogState();
  const entry = { id: payload.id, uptime: payload.uptime, text: payload.text };
  if (state.loading) {
    state.queue.push(entry);
    return;
  }
  appendDeviceLogEntries([entry]);
}

function closeReceivedPushChannel(opts) {
  const options = opts || {};
  const state = getReceivedMonitorState();
  const push = state.push;
  if (!push) return;
  if (push.source && typeof push.source.close === "function") {
    try {
      push.source.close();
    } catch (err) {
      if (!options.silent) console.warn("[push] ошибка закрытия EventSource:", err);
    }
  }
  push.source = null;
  push.connected = false;
  push.connecting = false;
  push.mode = "poll";
}

function openReceivedPushChannel() {
  const state = getReceivedMonitorState();
  const push = state.push;
  if (!push || push.supported === false || typeof EventSource === "undefined") {
    if (push) push.mode = "poll";
    startReceivedMonitor({ intervalMs: 5000, immediate: false });
    updateReceivedMonitorDiagnostics();
    return;
  }
  closeReceivedPushChannel({ silent: true });
  let url;
  try {
    const base = new URL(UI.cfg.endpoint || "http://192.168.4.1");
    url = new URL("/events", base).toString();
  } catch (err) {
    url = "http://192.168.4.1/events";
  }
  try {
    const source = new EventSource(url);
    push.source = source;
    push.connecting = true;
    push.retryCount = 0;
    push.mode = "push";
    updateReceivedMonitorDiagnostics();
    source.addEventListener("open", () => {
      push.connected = true;
      push.connecting = false;
      push.lastOpenAt = Date.now();
      push.lastErrorAt = null;
      push.retryCount = 0;
      push.mode = "push";
      startReceivedMonitor({ intervalMs: 30000, immediate: false });
      updateReceivedMonitorDiagnostics();
    });
    source.addEventListener("received", (event) => {
      push.connected = true;
      push.lastEventAt = Date.now();
      handleReceivedPushMessage(event);
      updateReceivedMonitorDiagnostics();
    });
    source.addEventListener("log", (event) => {
      push.connected = true;
      push.lastEventAt = Date.now();
      handleDeviceLogPushMessage(event);
      updateReceivedMonitorDiagnostics();
    });
    source.addEventListener("error", () => {
      push.connected = false;
      push.connecting = true;
      push.lastErrorAt = Date.now();
      push.retryCount = (push.retryCount || 0) + 1;
      if (push.retryCount >= 3) {
        push.mode = "poll";
        startReceivedMonitor({ intervalMs: 5000, immediate: false });
      }
      updateReceivedMonitorDiagnostics();
    });
  } catch (err) {
    console.warn("[push] не удалось открыть EventSource:", err);
    push.supported = false;
    push.mode = "poll";
    updateReceivedMonitorDiagnostics();
  }
}

// Фоновый запрос RSTS использует тот же базовый тайм-аут, что и sendCommand,
// чтобы избежать преждевременного прерывания при нагруженных каналах.
async function pollReceivedMessages(opts) {
  const state = getReceivedMonitorState();
  const metrics = state.metrics;
  if (state.running) {
    metrics.skippedOverlaps += 1;
    metrics.lastBlockedAt = Date.now();
    metrics.lastBlockedReason = "Предыдущий запрос ещё выполняется";
    updateReceivedMonitorDiagnostics();
    return null;
  }
  state.running = true;
  const options = opts || {};
  const params = { full: "1", json: "1", n: String(state.limit) };
  const startedAt = Date.now();
  const startedPrecise = typeof performance !== "undefined" && performance.now ? performance.now() : startedAt;
  metrics.totalAttempts += 1;
  metrics.lastGapMs = metrics.lastStartedAt != null ? Math.max(0, startedAt - metrics.lastStartedAt) : null;
  metrics.lastStartedAt = startedAt;
  metrics.runningSince = startedAt;
  try {
    const res = await deviceFetch("RSTS", params, options.timeoutMs || DEVICE_COMMAND_TIMEOUT_MS);
    if (!res.ok) {
      if (res.error) console.warn("[recv] RSTS недоступен:", res.error);
      metrics.totalErrors += 1;
      metrics.consecutiveErrors += 1;
      const errText = res.error != null ? String(res.error) : "Unknown error";
      metrics.lastErrorText = errText;
      metrics.lastErrorAt = Date.now();
      if (/abort/i.test(errText)) metrics.totalTimeouts += 1;
      updateReceivedMonitorDiagnostics();
      return null;
    }
    const items = parseReceivedResponse(res.text);
    handleReceivedSnapshot(items);
    metrics.totalSuccess += 1;
    metrics.consecutiveErrors = 0;
    metrics.lastOkAt = Date.now();
    metrics.lastErrorText = null;
    metrics.lastErrorAt = null;
    return items;
  } catch (err) {
    console.warn("[recv] ошибка фонового опроса:", err);
    const errText = err != null ? String(err) : "Unknown error";
    metrics.totalErrors += 1;
    metrics.consecutiveErrors += 1;
    metrics.lastErrorText = errText;
    metrics.lastErrorAt = Date.now();
    if (/abort/i.test(errText)) metrics.totalTimeouts += 1;
    updateReceivedMonitorDiagnostics();
    return null;
  } finally {
    const finishedAt = Date.now();
    const finishedPrecise = typeof performance !== "undefined" && performance.now ? performance.now() : finishedAt;
    const durationMs = Math.max(0, finishedPrecise - startedPrecise);
    metrics.lastFinishedAt = finishedAt;
    metrics.lastDurationMs = durationMs;
    metrics.samples += 1;
    if (Number.isFinite(durationMs)) {
      if (!Number.isFinite(metrics.averageDurationMs)) {
        metrics.averageDurationMs = durationMs;
      } else {
        const alpha = 1 / Math.min(metrics.samples, 30);
        metrics.averageDurationMs = (1 - alpha) * metrics.averageDurationMs + alpha * durationMs;
      }
    }
    metrics.runningSince = null;
    updateReceivedMonitorDiagnostics();
    state.running = false;
    if (state.push && state.push.pendingRefresh) {
      state.push.pendingRefresh = false;
      scheduleReceivedRefreshFromPush(state.push.lastHint);
    }
  }
}

function stopReceivedMonitor() {
  const state = UI.state && UI.state.received;
  if (state && state.timer) {
    clearInterval(state.timer);
    state.timer = null;
  }
  if (state && state.metricsTimer) {
    clearInterval(state.metricsTimer);
    state.metricsTimer = null;
  }
}

function startReceivedMonitor(opts) {
  const options = opts || {};
  const state = getReceivedMonitorState();
  const intervalRaw = Number(options.intervalMs);
  const interval = Number.isFinite(intervalRaw) && intervalRaw >= 1000 ? intervalRaw : 5000;
  stopReceivedMonitor();
  state.metrics.configuredIntervalMs = interval;
  state.metrics.lastConfiguredAt = Date.now();
  updateReceivedMonitorDiagnostics();
  const tick = () => {
    pollReceivedMessages({ silentError: true }).catch((err) => {
      console.warn("[recv] непредвиденная ошибка опроса:", err);
    });
  };
  if (options.immediate !== false) {
    tick();
  }
  state.timer = setInterval(tick, interval);
  if (!state.metricsTimer) {
    state.metricsTimer = setInterval(() => {
      updateReceivedMonitorDiagnostics();
    }, 1000);
  }
}

/* Таблица каналов */
let channels = [];
let channelsMocked = false;

// Восстанавливаем список каналов из кеша браузера, если устройство временно недоступно
function restoreChannelsFromStorage() {
  const raw = storage.get(CHANNELS_CACHE_STORAGE_KEY);
  if (!raw) return false;
  try {
    const parsed = JSON.parse(raw);
    if (!Array.isArray(parsed) || !parsed.length) return false;
    const normalized = parsed.map(normalizeChannelEntry).filter(Boolean);
    if (!normalized.length) return false;
    channels = normalized;
    channelsMocked = false;
    return true;
  } catch (err) {
    console.warn("[channels] не удалось восстановить кеш:", err);
    storage.remove(CHANNELS_CACHE_STORAGE_KEY);
    return false;
  }
}

// Сохраняем актуальный список каналов в кеш браузера
function persistChannelsToStorage(list) {
  if (channelsMocked) return;
  try {
    storage.set(CHANNELS_CACHE_STORAGE_KEY, JSON.stringify(list));
  } catch (err) {
    console.warn("[channels] не удалось сохранить кеш:", err);
  }
}

// Приводим объект канала к ожидаемой структуре, отбрасывая некорректные данные
function normalizeChannelEntry(entry) {
  if (!entry || typeof entry !== "object") return null;
  const ch = Number(entry.ch);
  const tx = Number(entry.tx);
  const rx = Number(entry.rx);
  if (!Number.isFinite(ch) || !Number.isFinite(tx)) return null;
  const normalizedRx = Number.isFinite(rx) ? rx : tx;
  const rssi = Number(entry.rssi);
  const snr = Number(entry.snr);
  return {
    ch,
    tx,
    rx: normalizedRx,
    rssi: Number.isFinite(rssi) ? rssi : 0,
    snr: Number.isFinite(snr) ? snr : 0,
    st: typeof entry.st === "string" ? entry.st : "",
    scan: typeof entry.scan === "string" ? entry.scan : "",
    scanState: typeof entry.scanState === "string" ? entry.scanState : null,
  };
}
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
    updateChannelTestsUi();
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

  updateChannelTestsUi();
  updateChannelInfoHighlight();
}

// Записываем значение в элемент, если он существует
function setChannelInfoText(el, text) {
  if (!el) return;
  const value = text == null ? "" : String(text);
  el.textContent = value;
  el.title = value && value !== "—" ? value : "";
}

/* Тесты вкладки Channels/Ping */
function setChannelTestsStatus(text, type, channel) {
  const tests = UI.state.channelTests;
  if (!tests) return;
  tests.message = text || "";
  tests.messageType = type || null;
  if (typeof channel === "number") tests.messageChannel = channel;
  else if (UI.state.infoChannel != null) tests.messageChannel = UI.state.infoChannel;
  else tests.messageChannel = null;
  applyChannelTestsStatus();
}
function applyChannelTestsStatus() {
  const statusEl = UI.els.channelInfoTestsStatus || $("#channelInfoTestsStatus");
  if (!statusEl) return;
  const tests = UI.state.channelTests;
  const infoChannel = UI.state.infoChannel;
  const message = tests ? tests.message : "";
  const messageChannel = tests ? tests.messageChannel : null;
  const type = tests ? tests.messageType : null;
  if (!message || messageChannel == null || infoChannel == null || messageChannel !== infoChannel) {
    statusEl.textContent = "";
    statusEl.hidden = true;
    statusEl.removeAttribute("data-state");
    return;
  }
  statusEl.textContent = message;
  statusEl.hidden = false;
  if (type) statusEl.setAttribute("data-state", type);
  else statusEl.removeAttribute("data-state");
}
function findCrPreset(value) {
  const num = Number(value);
  return CHANNEL_CR_PRESETS.find((preset) => Number(preset.value) === num) || null;
}
function formatCrLabel(value) {
  const preset = findCrPreset(value);
  if (!preset) return String(value);
  return preset.label + " (" + preset.value + ")";
}
function updateChannelTestsUi() {
  const tests = UI.state.channelTests;
  const infoChannel = UI.state.infoChannel;
  const hasChannel = infoChannel != null;
  const running = tests && (tests.stability.running || tests.cr.running);
  const stabilityBtn = UI.els.channelInfoStabilityBtn || $("#channelInfoStabilityTest");
  const crBtn = UI.els.channelInfoCrBtn || $("#channelInfoCrTest");
  if (stabilityBtn) {
    stabilityBtn.disabled = !hasChannel || running;
  }
  if (crBtn) {
    crBtn.disabled = !hasChannel || running;
  }
  applyChannelTestsStatus();

  const summary = UI.els.channelInfoStabilitySummary || $("#channelInfoStabilitySummary");
  const successEl = UI.els.channelInfoStabilitySuccess || $("#channelInfoStabilitySuccess");
  const percentEl = UI.els.channelInfoStabilityPercent || $("#channelInfoStabilityPercent");
  const latencyAvgEl = UI.els.channelInfoStabilityLatencyAvg || $("#channelInfoStabilityLatencyAvg");
  const latencyJitterEl = UI.els.channelInfoStabilityLatencyJitter || $("#channelInfoStabilityLatencyJitter");
  const cardsWrap = UI.els.channelInfoStabilityCards || $("#channelInfoStabilityCards");
  const chartWrap = UI.els.channelInfoStabilityWrap || $("#channelInfoStabilityResult");
  const issuesWrap = UI.els.channelInfoStabilityIssues || $("#channelInfoStabilityIssues");
  const historyWrap = UI.els.channelInfoStabilityHistory || $("#channelInfoStabilityHistory");
  const crWrap = UI.els.channelInfoCrResult || $("#channelInfoCrResult");

  let stabilityStats = null;
  const canShowStability = tests && hasChannel && tests.stability.channel === infoChannel && tests.stability.points.length;
  if (canShowStability) {
    if (!tests.stability.stats) {
      tests.stability.stats = computeStabilityStats(tests.stability.points, {
        planned: tests.stability.total,
        startedAt: tests.stability.startedAt,
        finishedAt: tests.stability.finishedAt,
      });
    }
    stabilityStats = tests.stability.stats;
  }
  if (!stabilityStats) {
    if (summary) summary.hidden = true;
    if (cardsWrap) cardsWrap.hidden = true;
    if (issuesWrap) issuesWrap.hidden = true;
    if (chartWrap) chartWrap.hidden = true;
  } else {
    const attempts = stabilityStats.attempts;
    const successCount = stabilityStats.success;
    const percentText = formatPercentValue(stabilityStats.percent);
    if (successEl) successEl.textContent = successCount + "/" + attempts + " (" + percentText + ")";
    if (percentEl) percentEl.textContent = percentText;
    if (latencyAvgEl) latencyAvgEl.textContent = formatLatencyMetric(stabilityStats.metrics.latency, "avg");
    if (latencyJitterEl) latencyJitterEl.textContent = formatLatencyMetric(stabilityStats.metrics.latency, "std");
    if (summary) summary.hidden = false;
    renderChannelStabilityCards(stabilityStats);
    renderChannelStabilityIssues(stabilityStats);
    if (chartWrap) {
      chartWrap.hidden = false;
      renderChannelStabilityChart(tests.stability.points, { stats: stabilityStats });
    }
  }

  renderChannelStabilityHistory(infoChannel);
  if (historyWrap) {
    historyWrap.hidden = infoChannel == null;
  }

  if (!tests || !hasChannel || tests.cr.channel !== infoChannel || !tests.cr.results.length) {
    if (crWrap) crWrap.hidden = true;
  } else {
    if (crWrap) crWrap.hidden = false;
    renderChannelCrResults(tests.cr.results);
  }
}
function renderChannelCrResults(results) {
  const tbody = UI.els.channelInfoCrTableBody || $("#channelInfoCrTableBody");
  if (!tbody) return;
  while (tbody.firstChild) tbody.removeChild(tbody.firstChild);
  for (let i = 0; i < results.length; i++) {
    const item = results[i];
    const tr = document.createElement("tr");
    const crCell = document.createElement("td");
    crCell.textContent = formatCrLabel(item.value);
    tr.appendChild(crCell);
    const pingCell = document.createElement("td");
    const summary = item.summary ? String(item.summary) : item.success ? "ОК" : "Нет ответа";
    pingCell.textContent = summary;
    pingCell.classList.add(item.success ? "ok" : "fail");
    tr.appendChild(pingCell);
    const rssiCell = document.createElement("td");
    rssiCell.textContent = Number.isFinite(item.rssi) ? String(item.rssi) : "—";
    tr.appendChild(rssiCell);
    const snrCell = document.createElement("td");
    snrCell.textContent = Number.isFinite(item.snr) ? item.snr.toFixed(1) : "—";
    tr.appendChild(snrCell);
    tbody.appendChild(tr);
  }
}
function renderChannelStabilityChart(points, opts) {
  const canvas = UI.els.channelInfoStabilityChart || $("#channelInfoStabilityChart");
  if (!canvas) return;
  const ctx = canvas.getContext("2d");
  if (!ctx) return;
  const width = canvas.clientWidth || canvas.width;
  const height = canvas.clientHeight || canvas.height;
  if (!width || !height) return;
  const dpr = window.devicePixelRatio || 1;
  const pixelWidth = Math.max(1, Math.round(width * dpr));
  const pixelHeight = Math.max(1, Math.round(height * dpr));
  if (canvas.width !== pixelWidth || canvas.height !== pixelHeight) {
    canvas.width = pixelWidth;
    canvas.height = pixelHeight;
  }
  ctx.setTransform(1, 0, 0, 1, 0, 0);
  ctx.clearRect(0, 0, canvas.width, canvas.height);
  ctx.scale(dpr, dpr);

  const options = opts || {};
  const currentHover = options.hover || canvas._stabilityHover || null;
  const stats = options.stats || null;

  const displayWidth = width;
  const displayHeight = height;
  const paddingLeft = 46;
  const paddingRight = 16;
  const paddingTop = 18;
  const paddingBottom = 36;
  const chartWidth = Math.max(10, displayWidth - paddingLeft - paddingRight);
  const chartHeight = Math.max(10, displayHeight - paddingTop - paddingBottom);
  const left = paddingLeft;
  const top = paddingTop;
  const right = left + chartWidth;
  const bottom = top + chartHeight;
  const styles = getComputedStyle(document.documentElement);
  const themeLight = document.documentElement.classList.contains("light");
  const rssiColor = (styles.getPropertyValue("--stability-rssi") || "#f97316").trim() || "#f97316";
  const snrColor = (styles.getPropertyValue("--stability-snr") || "#38bdf8").trim() || "#38bdf8";
  const dangerColor = (styles.getPropertyValue("--danger") || "#ef4444").trim() || "#ef4444";
  const backgroundFill = themeLight ? "rgba(148,163,184,0.14)" : "rgba(15,23,42,0.32)";
  const gridColor = themeLight ? "rgba(15,23,42,0.12)" : "rgba(255,255,255,0.12)";
  const axisColor = themeLight ? "rgba(15,23,42,0.6)" : "rgba(255,255,255,0.65)";

  ctx.fillStyle = backgroundFill;
  ctx.fillRect(0, 0, displayWidth, displayHeight);

  const values = [];
  const datasetValues = { rssi: [], snr: [] };
  if (Array.isArray(points)) {
    for (let i = 0; i < points.length; i++) {
      const entry = points[i];
      if (Number.isFinite(entry.rssi)) {
        values.push(entry.rssi);
        datasetValues.rssi.push(entry.rssi);
      }
      if (Number.isFinite(entry.snr)) {
        values.push(entry.snr);
        datasetValues.snr.push(entry.snr);
      }
    }
  }
  if (!values.length) {
    ctx.fillStyle = axisColor;
    ctx.font = "12px sans-serif";
    ctx.textAlign = "center";
    ctx.textBaseline = "middle";
    ctx.fillText("Нет данных для построения графика", displayWidth / 2, displayHeight / 2);
    canvas._stabilityChart = { points: [], coords: [], stats: stats || null };
    canvas._stabilityHover = null;
    ensureStabilityChartEvents(canvas);
    return;
  }

  let min = Math.min.apply(Math, values);
  let max = Math.max.apply(Math, values);
  if (!Number.isFinite(min) || !Number.isFinite(max)) {
    min = -10;
    max = 10;
  }
  if (min === max) {
    min -= 1;
    max += 1;
  }
  const span = max - min;
  const padding = span * 0.1;
  min -= padding;
  max += padding;
  const mapX = (index) => {
    if (!points || points.length <= 1) return left + chartWidth / 2;
    return left + (chartWidth * index) / (points.length - 1);
  };
  const mapY = (value) => {
    const clamped = value <= min ? min : value >= max ? max : value;
    const ratio = (clamped - min) / (max - min);
    return bottom - ratio * chartHeight;
  };

  ctx.strokeStyle = gridColor;
  ctx.lineWidth = 1;
  ctx.setLineDash([]);
  const horizontalSteps = 4;
  for (let i = 0; i <= horizontalSteps; i++) {
    const y = top + (chartHeight * i) / horizontalSteps;
    ctx.beginPath();
    ctx.moveTo(left, y);
    ctx.lineTo(right, y);
    ctx.stroke();
  }
  const verticalSteps = points && points.length > 1 ? Math.min(points.length - 1, 5) : 0;
  for (let i = 0; i <= verticalSteps; i++) {
    const ratio = verticalSteps ? i / verticalSteps : 0;
    const x = left + chartWidth * ratio;
    ctx.beginPath();
    ctx.moveTo(x, top);
    ctx.lineTo(x, bottom);
    ctx.stroke();
  }
  if (min < 0 && max > 0) {
    ctx.setLineDash([4, 4]);
    ctx.beginPath();
    ctx.moveTo(left, mapY(0));
    ctx.lineTo(right, mapY(0));
    ctx.stroke();
    ctx.setLineDash([]);
  }

  const drawBand = (key, color) => {
    const valuesForKey = datasetValues[key];
    if (!valuesForKey.length) return;
    const minVal = Math.min.apply(Math, valuesForKey);
    const maxVal = Math.max.apply(Math, valuesForKey);
    const yTop = mapY(maxVal);
    const yBottom = mapY(minVal);
    ctx.save();
    ctx.globalAlpha = 0.12;
    ctx.fillStyle = color;
    ctx.fillRect(left, Math.min(yTop, yBottom), chartWidth, Math.max(1, Math.abs(yBottom - yTop)));
    ctx.restore();
    const metric = stats && stats.metrics ? stats.metrics[key] : null;
    if (metric && Number.isFinite(metric.avg)) {
      const avgY = mapY(metric.avg);
      ctx.save();
      ctx.globalAlpha = 0.7;
      ctx.setLineDash([4, 4]);
      ctx.strokeStyle = color;
      ctx.beginPath();
      ctx.moveTo(left, avgY);
      ctx.lineTo(right, avgY);
      ctx.stroke();
      ctx.restore();
    }
  };
  drawBand("rssi", rssiColor);
  drawBand("snr", snrColor);

  ctx.strokeStyle = axisColor;
  ctx.beginPath();
  ctx.moveTo(left, top);
  ctx.lineTo(left, bottom);
  ctx.lineTo(right, bottom);
  ctx.stroke();

  ctx.fillStyle = axisColor;
  ctx.font = "11px sans-serif";
  ctx.textAlign = "right";
  ctx.textBaseline = "middle";
  ctx.fillText(formatNumber(max, 1) || max.toFixed(1), left - 6, top);
  ctx.fillText(formatNumber(min, 1) || min.toFixed(1), left - 6, bottom);
  ctx.textAlign = "center";
  ctx.textBaseline = "top";
  ctx.fillText("1", left, bottom + 8);
  ctx.fillText(points && points.length ? String(points.length) : "0", right, bottom + 8);

  const coords = [];
  const drawDataset = (key, color) => {
    ctx.strokeStyle = color;
    ctx.lineWidth = 2.2;
    ctx.lineJoin = "round";
    ctx.lineCap = "round";
    ctx.beginPath();
    let started = false;
    for (let i = 0; i < points.length; i++) {
      const value = Number(points[i][key]);
      if (!Number.isFinite(value)) continue;
      const x = mapX(i);
      const y = mapY(value);
      coords.push({ dataset: key, index: i, x, y, point: points[i] });
      if (!started) {
        ctx.moveTo(x, y);
        started = true;
      } else {
        ctx.lineTo(x, y);
      }
    }
    if (started) ctx.stroke();
    ctx.fillStyle = color;
    for (let i = 0; i < points.length; i++) {
      const value = Number(points[i][key]);
      if (!Number.isFinite(value)) continue;
      const x = mapX(i);
      const y = mapY(value);
      const hovered = currentHover && currentHover.dataset === key && currentHover.index === i;
      ctx.beginPath();
      ctx.arc(x, y, hovered ? 4.4 : 3, 0, Math.PI * 2);
      ctx.fill();
      if (hovered) {
        ctx.save();
        ctx.strokeStyle = themeLight ? "rgba(15,23,42,0.65)" : "rgba(255,255,255,0.75)";
        ctx.lineWidth = 1.2;
        ctx.stroke();
        ctx.restore();
      }
    }
  };
  if (points && points.length) {
    drawDataset("rssi", rssiColor);
    drawDataset("snr", snrColor);
  }

  if (currentHover && points && points.length > currentHover.index) {
    const hoverX = mapX(currentHover.index);
    ctx.save();
    ctx.setLineDash([2, 6]);
    ctx.strokeStyle = themeLight ? "rgba(30,41,59,0.22)" : "rgba(248,250,252,0.25)";
    ctx.beginPath();
    ctx.moveTo(hoverX, top);
    ctx.lineTo(hoverX, bottom + 18);
    ctx.stroke();
    ctx.restore();
  }

  ctx.fillStyle = dangerColor;
  for (let i = 0; i < points.length; i++) {
    if (points[i].success) continue;
    const x = mapX(i);
    const markerY = bottom + 14;
    ctx.beginPath();
    ctx.arc(x, markerY, 3, 0, Math.PI * 2);
    ctx.fill();
  }

  canvas._stabilityChart = {
    points: Array.isArray(points) ? points.slice() : [],
    coords,
    stats: stats || null,
  };
  canvas._stabilityHover = currentHover || null;
  ensureStabilityChartEvents(canvas);
}

function ensureStabilityChartEvents(canvas) {
  if (!canvas || canvas._stabilityEventsBound) return;
  const handler = onStabilityChartPointer;
  canvas.addEventListener("pointermove", handler);
  canvas.addEventListener("pointerleave", handler);
  canvas.addEventListener("pointercancel", handler);
  canvas._stabilityEventsBound = true;
}

function onStabilityChartPointer(event) {
  const canvas = UI.els.channelInfoStabilityChart || $("#channelInfoStabilityChart");
  const tooltip = UI.els.channelInfoStabilityTooltip || $("#channelInfoStabilityTooltip");
  if (!canvas || !tooltip) return;
  const chart = canvas._stabilityChart;
  if (!chart || !Array.isArray(chart.coords) || !chart.coords.length) {
    hideStabilityTooltip(tooltip);
    updateStabilityChartHover(canvas, null);
    return;
  }
  if (!event || event.type === "pointerleave" || event.type === "pointercancel") {
    hideStabilityTooltip(tooltip);
    updateStabilityChartHover(canvas, null);
    return;
  }
  const rect = canvas.getBoundingClientRect();
  const parent = canvas.parentElement || canvas;
  const parentRect = parent.getBoundingClientRect();
  const x = event.clientX - rect.left;
  const y = event.clientY - rect.top;
  let nearest = null;
  for (let i = 0; i < chart.coords.length; i++) {
    const coord = chart.coords[i];
    if (!coord) continue;
    const dx = coord.x - x;
    const dy = coord.y - y;
    const dist = Math.sqrt(dx * dx + dy * dy);
    if (dist <= CHANNEL_STABILITY_TOOLTIP_RADIUS && (!nearest || dist < nearest.dist)) {
      nearest = { dist, coord };
    }
  }
  if (!nearest) {
    hideStabilityTooltip(tooltip);
    updateStabilityChartHover(canvas, null);
    return;
  }
  updateStabilityChartHover(canvas, { dataset: nearest.coord.dataset, index: nearest.coord.index });
  const localX = event.clientX - parentRect.left;
  const localY = event.clientY - parentRect.top;
  showStabilityTooltip(canvas, tooltip, nearest.coord, localX, localY);
}

function updateStabilityChartHover(canvas, hover) {
  if (!canvas) return;
  const prev = canvas._stabilityHover;
  if (prev && hover && prev.dataset === hover.dataset && prev.index === hover.index) return;
  if (!prev && !hover) return;
  canvas._stabilityHover = hover || null;
  const chart = canvas._stabilityChart;
  if (chart && Array.isArray(chart.points)) {
    renderChannelStabilityChart(chart.points, { stats: chart.stats, hover: canvas._stabilityHover });
  }
}

function showStabilityTooltip(canvas, tooltip, coord, localX, localY) {
  if (!tooltip || !coord || !coord.point) return;
  const point = coord.point;
  const packetIndex = typeof point.index === "number" ? point.index : coord.index + 1;
  const statusText = point.success ? "успех" : "ошибка";
  const reasonKey = point.state || point.error || (point.success ? "signal" : "unknown");
  const reasonText = describeFailureReason(reasonKey);
  const rows = [
    { label: "RSSI", value: formatMetricValue(point.rssi, "дБм", 1) },
    { label: "SNR", value: formatMetricValue(point.snr, "дБ", 1) },
    { label: "Задержка", value: formatMetricValue(point.latency, "мс", point.latency >= 100 ? 0 : 1) },
    { label: "Состояние", value: reasonText }
  ];
  const grid = rows.map((row) => `<div><span>${row.label}</span><strong>${escapeHtml(row.value || "—")}</strong></div>`).join("");
  const parts = [];
  parts.push(`<div class="channel-info-test-tooltip-title">Пакет ${packetIndex} • ${statusText}${point.success ? "" : " (" + reasonText + ")"}</div>`);
  parts.push(`<div class="channel-info-test-tooltip-grid">${grid}</div>`);
  if (point.raw) {
    parts.push(`<div class="channel-info-test-tooltip-raw">${escapeHtml(truncateText(point.raw, 160))}</div>`);
  }
  tooltip.innerHTML = parts.join("");
  tooltip.hidden = false;
  tooltip.style.opacity = "0";
  const parent = canvas.parentElement || canvas;
  const offset = { x: localX + 12, y: localY + 12 };
  tooltip.style.left = offset.x + "px";
  tooltip.style.top = offset.y + "px";
  tooltip.style.maxWidth = Math.max(180, parent.clientWidth - 16) + "px";
  const bounds = tooltip.getBoundingClientRect();
  let left = offset.x;
  let top = offset.y;
  if (left + bounds.width > parent.clientWidth) left = Math.max(0, parent.clientWidth - bounds.width - 8);
  if (top + bounds.height > parent.clientHeight) top = Math.max(0, parent.clientHeight - bounds.height - 8);
  tooltip.style.left = left + "px";
  tooltip.style.top = top + "px";
  tooltip.style.opacity = "1";
}

function hideStabilityTooltip(tooltip) {
  if (!tooltip) return;
  tooltip.hidden = true;
  tooltip.textContent = "";
}

function formatMetricValue(value, unit, digits) {
  if (!Number.isFinite(value)) return null;
  const formatted = formatNumber(value, typeof digits === "number" ? digits : 1);
  if (!formatted) return null;
  return unit ? formatted + " " + unit : formatted;
}

function escapeHtml(value) {
  if (value == null) return "";
  return String(value).replace(/[&<>"']/g, (char) => {
    switch (char) {
      case "&": return "&amp;";
      case "<": return "&lt;";
      case ">": return "&gt;";
      case '"': return "&quot;";
      case "'": return "&#39;";
      default: return char;
    }
  });
}

function truncateText(text, maxLength) {
  const value = text == null ? "" : String(text);
  if (!maxLength || value.length <= maxLength) return value;
  return value.slice(0, Math.max(0, maxLength - 1)) + "…";
}

function formatNumber(value, fractionDigits) {
  if (!Number.isFinite(value)) return null;
  const digits = typeof fractionDigits === "number" ? Math.max(0, fractionDigits) : 1;
  return value.toLocaleString("ru-RU", { minimumFractionDigits: 0, maximumFractionDigits: digits });
}

function formatPercentValue(value) {
  if (!Number.isFinite(value)) return "0%";
  const digits = Math.abs(Math.round(value) - value) < 0.05 ? 0 : 1;
  const formatted = formatNumber(value, digits);
  return formatted ? formatted.replace(/\s+/g, "") + "%" : "0%";
}

function formatLatencyMetric(metric, key) {
  if (!metric || !Number.isFinite(metric[key])) return "—";
  const digits = metric[key] >= 100 ? 0 : 1;
  const formatted = formatNumber(metric[key], digits);
  return formatted ? formatted + " мс" : "—";
}

function formatRange(metric, unit, digits) {
  if (!metric || !Number.isFinite(metric.min) || !Number.isFinite(metric.max)) return "—";
  const minText = formatNumber(metric.min, typeof digits === "number" ? digits : 1);
  const maxText = formatNumber(metric.max, typeof digits === "number" ? digits : 1);
  if (!minText || !maxText) return "—";
  return minText + "…" + maxText + (unit ? " " + unit : "");
}

function formatRssiMeta(metric) {
  if (!metric) return "—";
  const parts = [];
  if (Number.isFinite(metric.avg)) parts.push("среднее " + (formatNumber(metric.avg, 1) || metric.avg));
  if (Number.isFinite(metric.median)) parts.push("медиана " + (formatNumber(metric.median, 1) || metric.median));
  if (Number.isFinite(metric.std) && metric.std > 0) parts.push("σ " + (formatNumber(metric.std, 1) || metric.std));
  return parts.length ? parts.map((part) => part + " дБм").join(", ") : "—";
}

function formatSnrMeta(metric) {
  if (!metric) return "—";
  const parts = [];
  if (Number.isFinite(metric.avg)) parts.push("среднее " + (formatNumber(metric.avg, 1) || metric.avg));
  if (Number.isFinite(metric.median)) parts.push("медиана " + (formatNumber(metric.median, 1) || metric.median));
  if (Number.isFinite(metric.std) && metric.std > 0) parts.push("σ " + (formatNumber(metric.std, 1) || metric.std));
  return parts.length ? parts.map((part) => part + " дБ").join(", ") : "—";
}

function formatLatencyMeta(metric) {
  if (!metric) return "—";
  const parts = [];
  if (Number.isFinite(metric.median)) {
    const digits = metric.median >= 100 ? 0 : 1;
    parts.push("медиана " + (formatNumber(metric.median, digits) || metric.median) + " мс");
  }
  if (Number.isFinite(metric.p95)) {
    const digits = metric.p95 >= 100 ? 0 : 1;
    parts.push("95% " + (formatNumber(metric.p95, digits) || metric.p95) + " мс");
  }
  if (Number.isFinite(metric.std) && metric.std > 0) {
    parts.push("σ " + (formatNumber(metric.std, 1) || metric.std) + " мс");
  }
  return parts.length ? parts.join(", ") : "—";
}

function renderChannelStabilityCards(stats) {
  const wrap = UI.els.channelInfoStabilityCards || $("#channelInfoStabilityCards");
  if (!wrap) return;
  const rssiRangeEl = UI.els.channelInfoStabilityRssiRange || $("#channelInfoStabilityRssiRange");
  const rssiMetaEl = UI.els.channelInfoStabilityRssiMeta || $("#channelInfoStabilityRssiMeta");
  const snrRangeEl = UI.els.channelInfoStabilitySnrRange || $("#channelInfoStabilitySnrRange");
  const snrMetaEl = UI.els.channelInfoStabilitySnrMeta || $("#channelInfoStabilitySnrMeta");
  const latencyRangeEl = UI.els.channelInfoStabilityLatencyRange || $("#channelInfoStabilityLatencyRange");
  const latencyMetaEl = UI.els.channelInfoStabilityLatencyMeta || $("#channelInfoStabilityLatencyMeta");
  const metrics = stats ? stats.metrics : null;
  const hasMetrics = metrics && (metrics.rssi || metrics.snr || metrics.latency);
  if (!hasMetrics) {
    wrap.hidden = true;
    if (rssiRangeEl) rssiRangeEl.textContent = "—";
    if (rssiMetaEl) rssiMetaEl.textContent = "—";
    if (snrRangeEl) snrRangeEl.textContent = "—";
    if (snrMetaEl) snrMetaEl.textContent = "—";
    if (latencyRangeEl) latencyRangeEl.textContent = "—";
    if (latencyMetaEl) latencyMetaEl.textContent = "—";
    return;
  }
  wrap.hidden = false;
  if (rssiRangeEl) rssiRangeEl.textContent = formatRange(metrics.rssi, "дБм");
  if (rssiMetaEl) rssiMetaEl.textContent = formatRssiMeta(metrics.rssi);
  if (snrRangeEl) snrRangeEl.textContent = formatRange(metrics.snr, "дБ");
  if (snrMetaEl) snrMetaEl.textContent = formatSnrMeta(metrics.snr);
  if (latencyRangeEl) {
    const digits = metrics.latency && metrics.latency.max >= 100 ? 0 : 1;
    latencyRangeEl.textContent = formatRange(metrics.latency, "мс", digits);
  }
  if (latencyMetaEl) latencyMetaEl.textContent = formatLatencyMeta(metrics.latency);
}

function renderChannelStabilityIssues(stats) {
  const issues = UI.els.channelInfoStabilityIssues || $("#channelInfoStabilityIssues");
  if (!issues) return;
  const failures = stats && Array.isArray(stats.failureReasons) ? stats.failureReasons : [];
  if (!failures.length) {
    issues.hidden = true;
    issues.textContent = "";
    return;
  }
  const parts = failures.slice(0, 3).map((item) => {
    const label = item.label || describeFailureReason(item.key);
    return label + " ×" + item.count;
  });
  issues.textContent = "Проблемы: " + parts.join(", ");
  issues.hidden = false;
}

function describeFailureReason(key) {
  if (!key) return "Неопознано";
  const map = {
    "signal": "Ответ",
    "no-response": "Нет ответа",
    "crc-error": "CRC ошибка",
    "transport": "Ошибка запроса",
    "busy": "Канал занят",
    "timeout": "Тайм-аут",
    "unknown": "Неопознано"
  };
  const normalized = String(key).toLowerCase();
  if (map[normalized]) return map[normalized];
  if (normalized.startsWith("err")) return "Ошибка";
  return normalized.replace(/[-_]+/g, " ").replace(/\b\w/g, (ch) => ch.toUpperCase());
}

function normalizeFailureKey(point) {
  if (!point) return "unknown";
  if (point.error) return String(point.error);
  if (point.state) return String(point.state);
  return "unknown";
}

function summarizeFailureReasons(points) {
  const counts = new Map();
  if (Array.isArray(points)) {
    for (let i = 0; i < points.length; i++) {
      const point = points[i];
      if (!point || point.success) continue;
      const key = normalizeFailureKey(point);
      counts.set(key, (counts.get(key) || 0) + 1);
    }
  }
  const list = Array.from(counts.entries()).map(([key, count]) => ({
    key,
    count,
    label: describeFailureReason(key)
  })).sort((a, b) => {
    if (b.count !== a.count) return b.count - a.count;
    return a.label.localeCompare(b.label);
  });
  return { list, top: list.length ? list[0].key : null };
}

function computeStabilityStats(points, opts) {
  const options = opts || {};
  const attempts = Array.isArray(points) ? points.length : 0;
  const success = Array.isArray(points) ? points.reduce((total, item) => total + (item && item.success ? 1 : 0), 0) : 0;
  const percent = attempts ? (success / attempts) * 100 : 0;
  const metrics = {
    rssi: collectStabilityMetric(points, "rssi"),
    snr: collectStabilityMetric(points, "snr"),
    latency: collectStabilityMetric(points, "latency")
  };
  const failures = summarizeFailureReasons(points);
  let durationMs = null;
  if (Number.isFinite(options.startedAt) && Number.isFinite(options.finishedAt)) {
    durationMs = Math.max(0, options.finishedAt - options.startedAt);
  }
  return {
    attempts,
    planned: Number.isFinite(options.planned) ? options.planned : attempts,
    success,
    percent,
    metrics,
    failureReasons: failures.list,
    dominantFailure: failures.top,
    durationMs,
    intervalMs: CHANNEL_STABILITY_INTERVAL_MS,
  };
}

function collectStabilityMetric(points, key) {
  if (!Array.isArray(points) || !points.length) return null;
  const values = [];
  const indexes = [];
  for (let i = 0; i < points.length; i++) {
    const point = points[i];
    if (!point) continue;
    const value = Number(point[key]);
    if (!Number.isFinite(value)) continue;
    values.push(value);
    indexes.push(typeof point.index === "number" ? point.index : i + 1);
  }
  if (!values.length) return null;
  const sorted = values.slice().sort((a, b) => a - b);
  const count = values.length;
  const sum = values.reduce((total, value) => total + value, 0);
  const avg = sum / count;
  const median = count % 2 ? sorted[(count - 1) / 2] : (sorted[count / 2 - 1] + sorted[count / 2]) / 2;
  let variance = 0;
  for (let i = 0; i < values.length; i++) {
    const diff = values[i] - avg;
    variance += diff * diff;
  }
  variance /= count;
  const std = Math.sqrt(variance);
  const minValue = Math.min.apply(Math, values);
  const maxValue = Math.max.apply(Math, values);
  const minIndex = indexes[values.indexOf(minValue)];
  const maxIndex = indexes[values.indexOf(maxValue)];
  const p95 = calculatePercentile(sorted, 0.95);
  return {
    count,
    min: minValue,
    max: maxValue,
    avg,
    median,
    std,
    p95,
    minIndex,
    maxIndex,
  };
}

function calculatePercentile(values, fraction) {
  if (!Array.isArray(values) || !values.length) return NaN;
  if (fraction <= 0) return values[0];
  if (fraction >= 1) return values[values.length - 1];
  const pos = (values.length - 1) * fraction;
  const base = Math.floor(pos);
  const rest = pos - base;
  if (base + 1 < values.length) {
    return values[base] + rest * (values[base + 1] - values[base]);
  }
  return values[base];
}

function ensureStabilityHistory() {
  const tests = UI.state.channelTests;
  if (!tests) return { loaded: true, map: new Map() };
  if (!tests.stabilityHistory) {
    tests.stabilityHistory = { loaded: false, map: new Map() };
  }
  if (!tests.stabilityHistory.loaded) {
    tests.stabilityHistory.map = loadStabilityHistoryFromStorage();
    tests.stabilityHistory.loaded = true;
  }
  return tests.stabilityHistory;
}

function loadStabilityHistoryFromStorage() {
  const raw = storage.get(CHANNEL_STABILITY_HISTORY_KEY);
  const map = new Map();
  if (!raw) return map;
  try {
    const parsed = JSON.parse(raw);
    if (!parsed || parsed.version !== CHANNEL_STABILITY_HISTORY_VERSION || typeof parsed.channels !== "object") {
      return map;
    }
    const channels = parsed.channels;
    Object.keys(channels).forEach((key) => {
      const list = Array.isArray(channels[key]) ? channels[key] : [];
      const normalized = list.map(normalizeHistoryRecord).filter(Boolean);
      if (normalized.length) {
        normalized.sort((a, b) => b.ts - a.ts);
        map.set(key, normalized);
      }
    });
  } catch (err) {
    console.warn("[stability-history] не удалось прочитать историю:", err);
  }
  return map;
}

function normalizeHistoryRecord(record) {
  if (!record || typeof record !== "object") return null;
  const ts = Number(record.ts);
  const channel = Number(record.channel);
  const attempts = Number(record.attempts);
  const success = Number(record.success);
  const percent = Number(record.percent);
  const durationMs = record.durationMs != null ? Number(record.durationMs) : null;
  const intervalMs = record.intervalMs != null ? Number(record.intervalMs) : null;
  const metrics = record.metrics && typeof record.metrics === "object" ? {
    rssi: normalizeMetric(record.metrics.rssi),
    snr: normalizeMetric(record.metrics.snr),
    latency: normalizeMetric(record.metrics.latency),
  } : {};
  const failures = Array.isArray(record.failures) ? record.failures.map(normalizeFailureItem).filter(Boolean) : [];
  const points = Array.isArray(record.points) ? record.points.map(normalizeHistoryPoint).filter(Boolean) : [];
  return {
    ts: Number.isFinite(ts) ? ts : Date.now(),
    channel: Number.isFinite(channel) ? channel : null,
    attempts: Number.isFinite(attempts) ? attempts : points.length,
    success: Number.isFinite(success) ? success : 0,
    percent: Number.isFinite(percent) ? percent : 0,
    durationMs: Number.isFinite(durationMs) ? durationMs : null,
    intervalMs: Number.isFinite(intervalMs) ? intervalMs : CHANNEL_STABILITY_INTERVAL_MS,
    metrics,
    failures,
    dominantFailure: typeof record.dominantFailure === "string" ? record.dominantFailure : null,
    points,
  };
}

function normalizeMetric(metric) {
  if (!metric || typeof metric !== "object") return null;
  const out = {};
  if (Number.isFinite(metric.count)) out.count = Number(metric.count);
  if (Number.isFinite(metric.min)) out.min = Number(metric.min);
  if (Number.isFinite(metric.max)) out.max = Number(metric.max);
  if (Number.isFinite(metric.avg)) out.avg = Number(metric.avg);
  if (Number.isFinite(metric.median)) out.median = Number(metric.median);
  if (Number.isFinite(metric.std)) out.std = Number(metric.std);
  if (Number.isFinite(metric.p95)) out.p95 = Number(metric.p95);
  if (Number.isFinite(metric.minIndex)) out.minIndex = Number(metric.minIndex);
  if (Number.isFinite(metric.maxIndex)) out.maxIndex = Number(metric.maxIndex);
  return out;
}

function normalizeFailureItem(item) {
  if (!item || typeof item !== "object") return null;
  const key = item.key != null ? String(item.key) : null;
  const count = Number(item.count);
  if (!key || !Number.isFinite(count)) return null;
  return { key, count, label: item.label ? String(item.label) : describeFailureReason(key) };
}

function normalizeHistoryPoint(point) {
  if (!point || typeof point !== "object") return null;
  return {
    index: Number.isFinite(point.index) ? Number(point.index) : null,
    success: !!point.success,
    rssi: Number.isFinite(point.rssi) ? Number(point.rssi) : null,
    snr: Number.isFinite(point.snr) ? Number(point.snr) : null,
    latency: Number.isFinite(point.latency) ? Number(point.latency) : null,
    state: point.state != null ? String(point.state) : null,
    error: point.error != null ? String(point.error) : null,
    raw: point.raw != null ? String(point.raw) : null,
  };
}

function saveStabilityHistory(map) {
  const payload = { version: CHANNEL_STABILITY_HISTORY_VERSION, channels: {} };
  if (map && typeof map.forEach === "function") {
    map.forEach((records, key) => {
      if (Array.isArray(records) && records.length) {
        payload.channels[key] = records;
      }
    });
  }
  try {
    storage.set(CHANNEL_STABILITY_HISTORY_KEY, JSON.stringify(payload));
  } catch (err) {
    console.warn("[stability-history] не удалось сохранить историю:", err);
  }
}

function getChannelStabilityHistory(channel) {
  if (channel == null) return [];
  const history = ensureStabilityHistory();
  const key = String(channel);
  const list = history.map.get(key);
  return Array.isArray(list) ? list : [];
}

function persistChannelStabilityHistory(channel, stability) {
  if (channel == null || !stability) return;
  const history = ensureStabilityHistory();
  const record = prepareStabilityHistoryRecord(channel, stability);
  if (!record) return;
  const key = String(channel);
  const list = history.map.get(key) || [];
  list.unshift(record);
  while (list.length > CHANNEL_STABILITY_HISTORY_LIMIT) list.pop();
  history.map.set(key, list);
  saveStabilityHistory(history.map);
  renderChannelStabilityHistory(channel);
}

function serializeMetric(metric) {
  if (!metric) return null;
  const out = {};
  if (Number.isFinite(metric.count)) out.count = Number(metric.count);
  if (Number.isFinite(metric.min)) out.min = Math.round(metric.min * 100) / 100;
  if (Number.isFinite(metric.max)) out.max = Math.round(metric.max * 100) / 100;
  if (Number.isFinite(metric.avg)) out.avg = Math.round(metric.avg * 100) / 100;
  if (Number.isFinite(metric.median)) out.median = Math.round(metric.median * 100) / 100;
  if (Number.isFinite(metric.std)) out.std = Math.round(metric.std * 100) / 100;
  if (Number.isFinite(metric.p95)) out.p95 = Math.round(metric.p95 * 100) / 100;
  if (Number.isFinite(metric.minIndex)) out.minIndex = Number(metric.minIndex);
  if (Number.isFinite(metric.maxIndex)) out.maxIndex = Number(metric.maxIndex);
  return out;
}

function prepareStabilityHistoryRecord(channel, stability) {
  if (!stability || !Array.isArray(stability.points) || !stability.points.length) return null;
  const stats = stability.stats || computeStabilityStats(stability.points, {
    planned: stability.total,
    startedAt: stability.startedAt,
    finishedAt: stability.finishedAt,
  });
  if (!stats) return null;
  const ts = Number.isFinite(stability.finishedAt) ? stability.finishedAt : Date.now();
  const points = stability.points.map((point) => ({
    index: point.index != null ? point.index : null,
    success: !!point.success,
    rssi: Number.isFinite(point.rssi) ? Number(point.rssi) : null,
    snr: Number.isFinite(point.snr) ? Number(point.snr) : null,
    latency: Number.isFinite(point.latency) ? Number(point.latency) : null,
    state: point.state || null,
    error: point.error || null,
    raw: point.raw ? truncateText(String(point.raw), 200) : null,
  }));
  return {
    ts,
    channel: Number(channel),
    attempts: stats.attempts,
    success: stats.success,
    percent: Math.round(stats.percent * 10) / 10,
    durationMs: stats.durationMs != null ? Math.round(stats.durationMs) : null,
    intervalMs: CHANNEL_STABILITY_INTERVAL_MS,
    metrics: {
      rssi: stats.metrics.rssi ? serializeMetric(stats.metrics.rssi) : null,
      snr: stats.metrics.snr ? serializeMetric(stats.metrics.snr) : null,
      latency: stats.metrics.latency ? serializeMetric(stats.metrics.latency) : null,
    },
    failures: stats.failureReasons ? stats.failureReasons.map((item) => ({ key: item.key, count: item.count, label: item.label })) : [],
    dominantFailure: stats.dominantFailure || null,
    points,
  };
}

function renderChannelStabilityHistory(channel) {
  const wrap = UI.els.channelInfoStabilityHistory || $("#channelInfoStabilityHistory");
  const list = UI.els.channelInfoStabilityHistoryList || $("#channelInfoStabilityHistoryList");
  const empty = UI.els.channelInfoStabilityHistoryEmpty || $("#channelInfoStabilityHistoryEmpty");
  if (!wrap || !list) return;
  list.innerHTML = "";
  if (channel == null) {
    if (empty) empty.hidden = false;
    return;
  }
  const history = getChannelStabilityHistory(channel);
  if (!history.length) {
    if (empty) empty.hidden = false;
    return;
  }
  const fragment = document.createDocumentFragment();
  const limit = Math.min(history.length, 10);
  for (let i = 0; i < limit; i++) {
    const record = history[i];
    const previous = history[i + 1] || null;
    fragment.appendChild(createStabilityHistoryItem(record, previous));
  }
  list.appendChild(fragment);
  if (empty) empty.hidden = true;
}

function createStabilityHistoryItem(record, previous) {
  const li = document.createElement("li");
  li.className = "channel-info-test-history-item";
  const header = document.createElement("div");
  header.className = "channel-info-test-history-line";
  const time = document.createElement("span");
  time.className = "channel-info-test-history-time";
  time.textContent = formatHistoryTimestamp(record.ts);
  header.appendChild(time);
  const success = document.createElement("span");
  success.className = "channel-info-test-history-success";
  const percentText = formatPercentValue(record.percent);
  success.textContent = `${record.success}/${record.attempts} (${percentText})`;
  header.appendChild(success);
  if (previous && Number.isFinite(previous.percent) && Number.isFinite(record.percent)) {
    const diffValue = record.percent - previous.percent;
    if (Math.abs(diffValue) > 0.05) {
      const diff = document.createElement("span");
      diff.className = "channel-info-test-history-diff" + (diffValue > 0 ? " up" : " down");
      const diffText = formatNumber(Math.abs(diffValue), 1) || Math.abs(diffValue).toFixed(1);
      diff.textContent = (diffValue > 0 ? "↑ " : "↓ ") + diffText + "%";
      header.appendChild(diff);
    }
  }
  li.appendChild(header);

  const metricsParts = [];
  if (record.metrics && record.metrics.latency && Number.isFinite(record.metrics.latency.avg)) {
    const digits = record.metrics.latency.avg >= 100 ? 0 : 1;
    const avgLatency = formatNumber(record.metrics.latency.avg, digits) || record.metrics.latency.avg;
    metricsParts.push(`Задержка ${avgLatency} мс`);
  }
  if (record.metrics && record.metrics.latency && Number.isFinite(record.metrics.latency.std) && record.metrics.latency.std > 0) {
    const stdLatency = formatNumber(record.metrics.latency.std, 1) || record.metrics.latency.std;
    metricsParts.push(`σ ${stdLatency} мс`);
  }
  if (record.metrics && record.metrics.rssi && Number.isFinite(record.metrics.rssi.min) && Number.isFinite(record.metrics.rssi.max)) {
    metricsParts.push(`RSSI ${formatNumber(record.metrics.rssi.min, 1)}…${formatNumber(record.metrics.rssi.max, 1)} дБм`);
  }
  if (record.metrics && record.metrics.snr && Number.isFinite(record.metrics.snr.min) && Number.isFinite(record.metrics.snr.max)) {
    metricsParts.push(`SNR ${formatNumber(record.metrics.snr.min, 1)}…${formatNumber(record.metrics.snr.max, 1)} дБ`);
  }
  if (Number.isFinite(record.durationMs)) {
    const duration = formatDurationMs(record.durationMs);
    if (duration) metricsParts.push(`Время ${duration}`);
  }
  if (metricsParts.length) {
    const metricsLine = document.createElement("div");
    metricsLine.className = "channel-info-test-history-metrics small muted";
    metricsLine.textContent = metricsParts.join(" · ");
    li.appendChild(metricsLine);
  }
  if (record.failures && record.failures.length) {
    const issues = document.createElement("div");
    issues.className = "channel-info-test-history-issues small";
    const parts = record.failures.slice(0, 3).map((item) => `${item.label || describeFailureReason(item.key)} ×${item.count}`);
    issues.textContent = "Ошибки: " + parts.join(", ");
    li.appendChild(issues);
  }
  return li;
}

function formatHistoryTimestamp(ts) {
  const date = ts ? new Date(ts) : new Date();
  const pad = (value) => String(value).padStart(2, "0");
  return `${pad(date.getDate())}.${pad(date.getMonth() + 1)} ${pad(date.getHours())}:${pad(date.getMinutes())}`;
}

function formatDurationMs(ms) {
  if (!Number.isFinite(ms)) return null;
  if (ms >= 1000) {
    const seconds = ms / 1000;
    const digits = seconds >= 10 ? 1 : 2;
    return (formatNumber(seconds, digits) || seconds.toFixed(digits)) + " с";
  }
  return (formatNumber(ms, 0) || Math.round(ms).toString()) + " мс";
}

function buildStabilityHistoryCsv(channel, records) {
  const header = [
    "type","timestamp","channel","attempts","success","percent","duration_ms","interval_ms",
    "rssi_min","rssi_max","rssi_avg","snr_min","snr_max","snr_avg",
    "latency_min_ms","latency_max_ms","latency_avg_ms","latency_median_ms","latency_std_ms","latency_p95_ms",
    "dominant_failure","failures","point_index","point_success","point_rssi","point_snr","point_latency_ms","point_state","point_error","point_raw"
  ];
  const rows = [header];
  records.forEach((record) => {
    const timestamp = new Date(record.ts).toISOString();
    const failures = record.failures && record.failures.length ? record.failures.map((item) => `${item.key}:${item.count}`).join("|") : "";
    rows.push([
      "summary",
      timestamp,
      record.channel != null ? record.channel : "",
      record.attempts,
      record.success,
      record.percent,
      record.durationMs != null ? record.durationMs : "",
      record.intervalMs != null ? record.intervalMs : "",
      metricField(record.metrics.rssi, "min"),
      metricField(record.metrics.rssi, "max"),
      metricField(record.metrics.rssi, "avg"),
      metricField(record.metrics.snr, "min"),
      metricField(record.metrics.snr, "max"),
      metricField(record.metrics.snr, "avg"),
      metricField(record.metrics.latency, "min"),
      metricField(record.metrics.latency, "max"),
      metricField(record.metrics.latency, "avg"),
      metricField(record.metrics.latency, "median"),
      metricField(record.metrics.latency, "std"),
      metricField(record.metrics.latency, "p95"),
      record.dominantFailure || "",
      failures,
      "","","","","","","",""
    ]);
    if (Array.isArray(record.points)) {
      record.points.forEach((point) => {
        rows.push([
          "point",
          timestamp,
          record.channel != null ? record.channel : "",
          record.attempts,
          record.success,
          record.percent,
          record.durationMs != null ? record.durationMs : "",
          record.intervalMs != null ? record.intervalMs : "",
          "","","","","","","","","","","","",
          record.dominantFailure || "",
          failures,
          point.index != null ? point.index : "",
          point.success ? "1" : "0",
          Number.isFinite(point.rssi) ? point.rssi : "",
          Number.isFinite(point.snr) ? point.snr : "",
          Number.isFinite(point.latency) ? point.latency : "",
          point.state || "",
          point.error || "",
          point.raw ? point.raw.replace(/\r?\n/g, " ") : "",
        ]);
      });
    }
  });
  return rows.map((row) => row.map(csvEscape).join(",")).join("\n");
}

function metricField(metric, key) {
  if (!metric || !Number.isFinite(metric[key])) return "";
  return String(metric[key]);
}

function buildStabilityHistoryJson(channel, records) {
  const payload = {
    channel,
    exportedAt: new Date().toISOString(),
    records,
  };
  return JSON.stringify(payload, null, 2);
}

function buildStabilityExportName(channel, ext) {
  const now = new Date();
  const pad = (value) => String(value).padStart(2, "0");
  return `stability_ch${channel}_${now.getFullYear()}${pad(now.getMonth() + 1)}${pad(now.getDate())}_${pad(now.getHours())}${pad(now.getMinutes())}.${ext}`;
}

function csvEscape(value) {
  if (value == null) return "";
  const str = String(value);
  if (/[",\n]/.test(str)) return '"' + str.replace(/"/g, '""') + '"';
  return str;
}

function downloadTextFile(filename, content, mime) {
  const blob = new Blob([content], { type: mime || "text/plain" });
  const url = URL.createObjectURL(blob);
  const link = document.createElement("a");
  link.href = url;
  link.download = filename;
  document.body.appendChild(link);
  link.click();
  document.body.removeChild(link);
  URL.revokeObjectURL(url);
}

function onChannelStabilityExportCsv(event) {
  if (event) event.preventDefault();
  const channel = UI.state.infoChannel;
  if (channel == null) {
    note("Выберите канал для экспорта истории.");
    return;
  }
  const history = getChannelStabilityHistory(channel);
  if (!history.length) {
    note("История Stability пуста.");
    return;
  }
  const csv = buildStabilityHistoryCsv(channel, history);
  downloadTextFile(buildStabilityExportName(channel, "csv"), csv, "text/csv");
  note("История Stability экспортирована в CSV.");
}

function onChannelStabilityExportJson(event) {
  if (event) event.preventDefault();
  const channel = UI.state.infoChannel;
  if (channel == null) {
    note("Выберите канал для экспорта истории.");
    return;
  }
  const history = getChannelStabilityHistory(channel);
  if (!history.length) {
    note("История Stability пуста.");
    return;
  }
  const json = buildStabilityHistoryJson(channel, history);
  downloadTextFile(buildStabilityExportName(channel, "json"), json, "application/json");
  note("История Stability экспортирована в JSON.");
}

function onChannelStabilityHistoryClear(event) {
  if (event) event.preventDefault();
  const channel = UI.state.infoChannel;
  if (channel == null) {
    note("Нет выбранного канала.");
    return;
  }
  const cleared = clearChannelStabilityHistory(channel);
  renderChannelStabilityHistory(channel);
  note(cleared ? "История Stability очищена." : "История уже пуста.");
}

function clearChannelStabilityHistory(channel) {
  const history = ensureStabilityHistory();
  const key = String(channel);
  if (!history.map.has(key)) return false;
  history.map.delete(key);
  saveStabilityHistory(history.map);
  return true;
}
async function onChannelStabilityTest(event) {
  if (event) event.preventDefault();
  const tests = UI.state.channelTests;
  if (!tests || tests.stability.running || tests.cr.running) return;
  const channel = UI.state.infoChannel;
  if (channel == null) {
    note("Выберите канал в таблице");
    return;
  }
  if (UI.state.channel !== channel) {
    setChannelTestsStatus("Stability test: установите канал кнопкой «Установить текущим каналом».", "warn", channel);
    note("Сначала сделайте канал активным.");
    return;
  }
  const btn = UI.els.channelInfoStabilityBtn || $("#channelInfoStabilityTest");
  if (btn) btn.setAttribute("aria-busy", "true");
  tests.stability.running = true;
  tests.stability.channel = channel;
  tests.stability.total = CHANNEL_STABILITY_ATTEMPTS;
  tests.stability.success = 0;
  tests.stability.points = [];
  tests.stability.stats = null;
  tests.stability.startedAt = Date.now();
  tests.stability.finishedAt = null;
  setChannelTestsStatus("Stability test: запуск…", "info", channel);
  updateChannelTestsUi();
  let cancelled = false;
  try {
    for (let i = 0; i < CHANNEL_STABILITY_ATTEMPTS; i++) {
      if (UI.state.infoChannel !== channel) {
        cancelled = true;
        break;
      }
      setChannelTestsStatus(`Stability test: пакет ${i + 1} из ${CHANNEL_STABILITY_ATTEMPTS}`, "info", channel);
      applyChannelTestsStatus();
      const startedTick = typeof performance !== "undefined" && performance && typeof performance.now === "function"
        ? performance.now()
        : Date.now();
      const res = await sendCommand("PI", undefined, { silent: true, timeoutMs: 5000, debugLabel: `PI stability #${i + 1}` });
      const finishedTick = typeof performance !== "undefined" && performance && typeof performance.now === "function"
        ? performance.now()
        : Date.now();
      const latency = Math.max(0, Math.round(finishedTick - startedTick));
      let success = false;
      let rssi = null;
      let snr = null;
      let state = null;
      let raw = null;
      let error = null;
      if (res != null) {
        raw = String(res).trim();
        const metrics = extractPingMetrics(raw);
        rssi = metrics.rssi;
        snr = metrics.snr;
        state = detectScanState(raw);
        success = state === "signal" || (!state && raw.length > 0);
      } else {
        error = "transport";
      }
      if (success) tests.stability.success += 1;
      tests.stability.points.push({ index: i + 1, success, rssi, snr, latency, state: state || null, raw, error });
      tests.stability.stats = computeStabilityStats(tests.stability.points, {
        planned: tests.stability.total,
        startedAt: tests.stability.startedAt,
        finishedAt: null,
      });
      updateChannelTestsUi();
      await uiYield();
      if (i < CHANNEL_STABILITY_ATTEMPTS - 1) {
        await new Promise((resolve) => setTimeout(resolve, CHANNEL_STABILITY_INTERVAL_MS));
      }
    }
  } finally {
    tests.stability.running = false;
    if (btn) btn.removeAttribute("aria-busy");
    updateChannelTestsUi();
  }
  tests.stability.finishedAt = Date.now();
  tests.stability.stats = computeStabilityStats(tests.stability.points, {
    planned: tests.stability.total,
    startedAt: tests.stability.startedAt,
    finishedAt: tests.stability.finishedAt,
  });
  if (cancelled) {
    setChannelTestsStatus("Stability test остановлен: выбран другой канал.", "warn", channel);
    return;
  }
  const attempts = tests.stability.points.length;
  const success = tests.stability.success;
  const percent = attempts ? (success / attempts) * 100 : 0;
  const rounded = attempts ? Math.round(percent * 10) / 10 : 0;
  const formattedPercent = attempts ? (Number.isInteger(rounded) ? String(rounded) : rounded.toFixed(1)) : "0";
  setChannelTestsStatus(`Stability test завершён: ${success}/${attempts} (${formattedPercent}% успешных пакетов)`, attempts ? "success" : "warn", channel);
  persistChannelStabilityHistory(channel, tests.stability);
  updateChannelTestsUi();
}
async function onChannelCrTest(event) {
  if (event) event.preventDefault();
  const tests = UI.state.channelTests;
  if (!tests || tests.cr.running || tests.stability.running) return;
  const channel = UI.state.infoChannel;
  if (channel == null) {
    note("Выберите канал в таблице");
    return;
  }
  if (UI.state.channel !== channel) {
    setChannelTestsStatus("CR test: установите канал кнопкой «Установить текущим каналом».", "warn", channel);
    note("Сначала сделайте канал активным.");
    return;
  }
  const btn = UI.els.channelInfoCrBtn || $("#channelInfoCrTest");
  if (btn) btn.setAttribute("aria-busy", "true");
  tests.cr.running = true;
  tests.cr.channel = channel;
  tests.cr.results = [];
  tests.cr.previous = null;
  setChannelTestsStatus("CR test: считываем текущий CR…", "info", channel);
  updateChannelTestsUi();
  let previousCr = null;
  try {
    const current = await deviceFetch("CR", {}, 2000);
    if (current.ok) {
      const token = extractNumericToken(current.text);
      if (token != null) {
        const parsed = Number(token);
        if (Number.isFinite(parsed)) previousCr = parsed;
      }
    }
  } catch (err) {
    console.warn("[channels] CR test: fetch current", err);
  }
  tests.cr.previous = previousCr;
  let cancelled = false;
  let restoreState = "unknown";
  let failed = false;
  try {
    for (let i = 0; i < CHANNEL_CR_PRESETS.length; i++) {
      const preset = CHANNEL_CR_PRESETS[i];
      if (UI.state.infoChannel !== channel) {
        cancelled = true;
        break;
      }
      setChannelTestsStatus(`CR test: CR ${preset.label}`, "info", channel);
      applyChannelTestsStatus();
      const response = await sendCommand("CR", { v: String(preset.value) }, { silent: true, timeoutMs: 3000, debugLabel: `CR test set ${preset.value}` });
      const setOk = response != null && /OK/i.test(response);
      let success = false;
      let summary = setOk ? "" : "Ошибка установки";
      let rssi = null;
      let snr = null;
      if (setOk) {
        await new Promise((resolve) => setTimeout(resolve, 120));
        const pingRes = await sendCommand("PI", undefined, { silent: true, timeoutMs: 5000, debugLabel: `PI CR test ${preset.value}` });
        if (pingRes != null) {
          const metrics = extractPingMetrics(pingRes);
          rssi = metrics.rssi;
          snr = metrics.snr;
          const state = detectScanState(pingRes);
          const trimmed = String(pingRes).trim();
          success = state === "signal" || (!state && trimmed.length > 0);
          summary = summarizeResponse(pingRes, success ? "ОК" : trimmed || "Нет ответа");
        } else {
          summary = "Нет ответа";
        }
      }
      tests.cr.results.push({
        value: preset.value,
        success: setOk && success,
        summary: summary,
        rssi: rssi,
        snr: snr,
      });
      updateChannelTestsUi();
      await uiYield();
    }
    if (previousCr != null) {
      try {
        setChannelTestsStatus("CR test: восстанавливаем исходный CR…", "info", channel);
        const restore = await sendCommand("CR", { v: String(previousCr) }, { silent: true, timeoutMs: 3000, debugLabel: "CR test restore" });
        restoreState = restore != null && /OK/i.test(restore) ? "ok" : "fail";
        if (restoreState === "ok") {
          const crSelect = $("#CR");
          if (crSelect) crSelect.value = String(previousCr);
        }
      } catch (err) {
        console.warn("[channels] CR test: restore", err);
        restoreState = "fail";
      }
    } else restoreState = "skip";
  } catch (err) {
    console.warn("[channels] CR test: выполнение завершилось с ошибкой", err);
    const reason = err && err.message ? err.message : String(err);
    setChannelTestsStatus(`CR test: ошибка выполнения: ${reason}`, "warn", channel);
    failed = true;
  } finally {
    tests.cr.running = false;
    if (btn) btn.removeAttribute("aria-busy");
    updateChannelTestsUi();
  }
  if (failed) return;
  if (cancelled) {
    setChannelTestsStatus("CR test остановлен: выбран другой канал.", "warn", channel);
    return;
  }
  if (restoreState === "ok") {
    setChannelTestsStatus("CR test завершён: исходный CR восстановлен.", "success", channel);
  } else if (restoreState === "skip") {
    setChannelTestsStatus("CR test завершён, но исходный CR неизвестен.", "warn", channel);
  } else {
    setChannelTestsStatus("CR test завершён, но восстановить исходный CR не удалось.", "warn", channel);
  }
  updateChannelTestsUi();
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
  if (channels.length) return;
  channels = [
    { ch: 1, tx: 868.1, rx: 868.1, rssi: -92, snr: 8.5, st: "idle", scan: "", scanState: null },
    { ch: 2, tx: 868.3, rx: 868.3, rssi: -97, snr: 7.1, st: "listen", scan: "", scanState: null },
    { ch: 3, tx: 868.5, rx: 868.5, rssi: -88, snr: 10.2, st: "tx", scan: "", scanState: null },
  ];
  channelsMocked = true;
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
        channelsMocked = false;
        persistChannelsToStorage(channels);
      } else if (!channels.length) {
        if (!restoreChannelsFromStorage()) mockChannels();
      }
    } else if (!channels.length) {
      if (!restoreChannelsFromStorage()) mockChannels();
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
    if (!channels.length && !restoreChannelsFromStorage()) mockChannels();
    const errText = e != null ? String(e) : "unknown";
    logErrorEvent("refreshChannels", errText);
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
  return out.map(normalizeChannelEntry).filter(Boolean);
}
function applyPingResult(text) {
  if (UI.state.channel == null) return;
  const entry = channels.find((c) => c.ch === UI.state.channel);
  if (!entry) return;
  applyPingToEntry(entry, text);
  renderChannels();
  persistChannelsToStorage(channels);
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
  if (changed) {
    renderChannels();
    persistChannelsToStorage(channels);
  }
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
// Извлекаем RSSI и SNR из текста ответа PI
function extractPingMetrics(text) {
  const raw = text != null ? String(text).trim() : "";
  const matchNumber = /-?\d+(?:\.\d+)?/;
  const rssiMatch = raw.match(/RSSI\s*(-?\d+(?:\.\d+)?)/i);
  const snrMatch = raw.match(/SNR\s*(-?\d+(?:\.\d+)?)/i);
  return {
    rssi: rssiMatch && matchNumber.test(rssiMatch[1]) ? Number(rssiMatch[1]) : null,
    snr: snrMatch && matchNumber.test(snrMatch[1]) ? Number(snrMatch[1]) : null,
  };
}
function applyPingToEntry(entry, text) {
  if (!entry) return null;
  const raw = (text || "").trim();
  const metrics = extractPingMetrics(raw);
  if (metrics.rssi != null) entry.rssi = metrics.rssi;
  if (metrics.snr != null) entry.snr = metrics.snr;
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
    const errText = e != null ? String(e) : "unknown";
    logErrorEvent("Search", errText);
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
function parseRxBoostedGainResponse(text) {
  if (!text) return null;
  const trimmed = String(text).trim();
  if (trimmed.length === 0) return null;
  const normalized = trimmed.toLowerCase();
  if (normalized.indexOf("err") >= 0) return null;
  if (trimmed === "1") return true;
  if (trimmed === "0") return false;
  const match = trimmed.match(/(?:^|[^0-9])([01])(?:[^0-9]|$)/);
  if (match) return match[1] === "1";
  if (normalized.indexOf("on") >= 0 || normalized.indexOf("включ") >= 0) return true;
  if (normalized.indexOf("off") >= 0 || normalized.indexOf("выключ") >= 0) return false;
  return null;
}
function updateRxBoostedGainUi() {
  const state = UI.state.rxBoostedGain;
  const input = UI.els.rxBoostedGain;
  if (input) {
    if (state === null) {
      input.indeterminate = true;
    } else {
      input.indeterminate = false;
      input.checked = state;
    }
  }
  const hint = UI.els.rxBoostedGainHint;
  if (hint) {
    if (state === null) {
      hint.textContent = "Состояние не загружено.";
    } else {
      hint.textContent = state ? "Повышенное усиление включено." : "Повышенное усиление выключено.";
    }
  }
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
function parseLightPackResponse(text) {
  if (!text) return null;
  const trimmed = String(text).trim();
  if (!trimmed) return null;
  const token = extractNumericToken(trimmed);
  if (token === "1") return true;
  if (token === "0") return false;
  const low = trimmed.toLowerCase();
  if (low.indexOf("light:1") >= 0) return true;
  if (low.indexOf("light:0") >= 0) return false;
  if (low.indexOf("on") >= 0 || low.indexOf("включ") >= 0) return true;
  if (low.indexOf("off") >= 0 || low.indexOf("выключ") >= 0) return false;
  return null;
}
function updateLightPackUi() {
  const input = UI.els.lightPack;
  const hint = UI.els.lightPackHint;
  const wrap = UI.els.lightPackControl;
  const state = UI.state.lightPack;
  const busy = UI.state.lightPackBusy;
  if (wrap) {
    wrap.classList.toggle("waiting", busy);
    if (busy) wrap.setAttribute("aria-busy", "true");
    else wrap.removeAttribute("aria-busy");
  }
  if (input) {
    input.disabled = busy;
    if (state === null) {
      input.indeterminate = true;
    } else {
      input.indeterminate = false;
      input.checked = state;
    }
  }
  if (hint) {
    if (state === true) {
      hint.textContent = "Light pack активен: текст передаётся без служебного префикса.";
    } else if (state === false) {
      hint.textContent = "Light pack выключен: используется стандартный префикс для пакетов.";
    } else {
      hint.textContent = "Состояние Light pack ещё не получено.";
    }
  }
}
async function refreshLightPackState() {
  const res = await deviceFetch("LIGHT", {}, 2000);
  if (res.ok) {
    const state = parseLightPackResponse(res.text);
    if (state !== null) {
      UI.state.lightPack = state;
      storage.set("set.LIGHTPACK", state ? "1" : "0");
      updateLightPackUi();
      return state;
    }
  }
  UI.state.lightPack = null;
  updateLightPackUi();
  return null;
}
async function setLightPack(value) {
  if (UI.state.lightPackBusy) return UI.state.lightPack;
  UI.state.lightPackBusy = true;
  updateLightPackUi();
  try {
    const response = await sendCommand("LIGHT", { v: value ? "1" : "0" });
    if (typeof response === "string") {
      const parsed = parseLightPackResponse(response);
      if (parsed !== null) {
        UI.state.lightPack = parsed;
        storage.set("set.LIGHTPACK", parsed ? "1" : "0");
        updateLightPackUi();
        return parsed;
      }
    }
    return await refreshLightPackState();
  } finally {
    UI.state.lightPackBusy = false;
    updateLightPackUi();
  }
}
async function onLightPackToggle(event) {
  if (!UI.els.lightPack) return;
  if (UI.state.lightPackBusy) {
    if (event && typeof event.preventDefault === "function") event.preventDefault();
    updateLightPackUi();
    return;
  }
  await setLightPack(UI.els.lightPack.checked);
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
async function refreshRxBoostedGain() {
  const res = await deviceFetch("RXBG", {}, 2000);
  if (res.ok) {
    const state = parseRxBoostedGainResponse(res.text);
    if (state !== null) {
      UI.state.rxBoostedGain = state;
      storage.set("set.RXBG", state ? "1" : "0");
      updateRxBoostedGainUi();
      return state;
    }
  }
  UI.state.rxBoostedGain = null;
  updateRxBoostedGainUi();
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
const SETTINGS_KEYS = ["BANK","BF","CH","CR","PW","RXBG","SF","PAUSE","ACKT","ACKR","TESTRXMMSG"];
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
      if (key === "RXBG") {
        UI.state.rxBoostedGain = el.checked;
        updateRxBoostedGainUi();
      }
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
  updateRxBoostedGainUi();
  if (UI.els.lightPack) {
    const stored = storage.get("set.LIGHTPACK");
    if (stored === "1" || stored === "0") {
      UI.state.lightPack = stored === "1";
    } else {
      UI.state.lightPack = null;
    }
    updateLightPackUi();
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
    if (key === "RXBG") {
      UI.state.rxBoostedGain = (v === "1");
      updateRxBoostedGainUi();
    }
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
  await refreshLightPackState();
  await refreshRxBoostedGain();
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
      if (key === "RXBG") {
        const enabled = obj[key] === "1" || obj[key] === 1 || obj[key] === true;
        el.checked = enabled;
        if (typeof el.indeterminate === "boolean") el.indeterminate = false;
        UI.state.rxBoostedGain = enabled;
        storage.set("set.RXBG", enabled ? "1" : "0");
        updateRxBoostedGainUi();
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
    const rxbgRes = await deviceFetch("RXBG", {}, 2000);
    if (rxbgRes.ok) {
      const state = parseRxBoostedGainResponse(rxbgRes.text);
      if (state !== null) {
        UI.state.rxBoostedGain = state;
        if (UI.els.rxBoostedGain) {
          UI.els.rxBoostedGain.checked = state;
          UI.els.rxBoostedGain.indeterminate = false;
        }
        storage.set("set.RXBG", state ? "1" : "0");
        updateRxBoostedGainUi();
      }
    }
  } catch (err) {
    console.warn("[settings] RXBG", err);
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
  const peerBtn = UI.els.keyGenPeerBtn || $("#btnKeyGenPeer");
  if (!data || typeof data !== "object") {
    if (stateEl) stateEl.textContent = "—";
    if (idEl) idEl.textContent = "";
    if (pubEl) pubEl.textContent = "";
    if (peerEl) peerEl.textContent = "";
    if (backupEl) backupEl.textContent = "";
    if (peerBtn) peerBtn.disabled = true;
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
    if (peerBtn) {
      // Активируем кнопку KEYGEN PEER только когда известен удалённый ключ
      const hasPeer = typeof data.peer === "string" && data.peer.trim().length > 0;
      peerBtn.disabled = !hasPeer;
    }
  }
  if (messageEl) messageEl.textContent = UI.key.lastMessage || "";
}

async function refreshKeyState(options) {
  const opts = options || {};
  if (!opts.silent) status("→ KEYSTATE");
  debugLog("KEYSTATE → запрос состояния");
  const res = await deviceFetch("KEYSTATE", {}, 4000);
  if (res.ok) {
    const rawText = res.text != null ? String(res.text) : "";
    debugLog("KEYSTATE ← " + rawText);
    let data = null;
    let parseFailed = false;
    if (rawText.trim()) {
      try {
        data = JSON.parse(rawText);
      } catch (err) {
        // Всегда оставляем комментарии на русском
        console.warn("[key] не удалось разобрать JSON KEYSTATE:", err);
        data = parseJsonLenient(rawText);
        if (data) {
          console.warn("[key] JSON KEYSTATE очищен и разобран повторно");
        } else {
          data = parseKeyStateFallback(rawText);
          if (!data) parseFailed = true;
          else {
            // Резервный разбор сработал, предупреждаем в консоли
            console.warn("[key] использован резервный парсер KEYSTATE");
          }
        }
      }
    }
    if (data && data.error) {
      if (!opts.silent) note("KEYSTATE: " + data.error);
      return;
    }
    if (data) {
      UI.key.state = data;
      UI.key.lastMessage = "";
      renderKeyState(data);
      if (!opts.silent) status("✓ KEYSTATE");
      return;
    }
    const plain = rawText.trim();
    if (plain) {
      const normalized = plain.toUpperCase();
      UI.key.state = null;
      if (normalized === "UNKNOWN") {
        UI.key.lastMessage = "Команда KEYSTATE не поддерживается текущей прошивкой";
        note("KEYSTATE: команда недоступна на устройстве");
      } else {
        UI.key.lastMessage = plain;
        note("KEYSTATE: получен ответ без JSON — " + plain);
      }
      renderKeyState(null);
      if (!opts.silent) status(parseFailed ? "✗ KEYSTATE" : "✓ KEYSTATE");
      return;
    }
    if (parseFailed) note("Не удалось разобрать состояние ключа");
  } else if (!opts.silent) {
    status("✗ KEYSTATE");
    note("Ошибка KEYSTATE: " + res.error);
  }
  if (!res.ok) debugLog("KEYSTATE ✗ " + res.error);
}

// Аккуратно очищаем нестандартный JSON прежде чем отдавать его в JSON.parse
function parseJsonLenient(rawText) {
  const raw = rawText != null ? String(rawText).trim() : "";
  if (!raw) return null;
  let sanitized = raw.replace(/^\uFEFF/, "");
  sanitized = sanitized
    .replace(/\/\*[\s\S]*?\*\//g, "") // убираем блочные комментарии
    .replace(/(^|[^:])\/\/.*$/gm, "$1"); // убираем построчные комментарии без трогания URL
  sanitized = sanitized.replace(/,\s*([}\]])/g, "$1"); // отрезаем висячие запятые
  sanitized = sanitized.replace(/([\{,]\s*)([A-Za-z_][A-Za-z0-9_-]*)\s*:/g, (match, prefix, key) => {
    // Аккуратно добавляем кавычки к необрамлённым идентификаторам ключей
    return `${prefix}"${key}":`;
  });
  sanitized = sanitized.replace(/([\{,]\s*)'([^'"\n]*)'\s*:/g, "$1\"$2\":"); // нормализуем ключи в кавычках
  sanitized = sanitized.replace(/:\s*'([^'"\n]*)'/g, ': "$1"'); // заменяем одиночные кавычки в значениях
  if (!sanitized.trim()) return null;
  try {
    return JSON.parse(sanitized);
  } catch (err) {
    console.warn("[key] повторная попытка JSON.parse не удалась:", err);
    return null;
  }
}

// Пытаемся восстановить объект состояния ключа из нестрогого JSON или пары ключ=значение
function parseKeyStateFallback(rawText) {
  const raw = rawText != null ? String(rawText).trim() : "";
  if (!raw) return null;
  // Убираем фигурные скобки, если они есть, чтобы облегчить дальнейший разбор
  const inner = raw.replace(/^[{\[]|[}\]]$/g, "");
  const result = {};
  let hasData = false;
  inner
    .split(/[,;\n]+/)
    .map((part) => part.trim())
    .filter(Boolean)
    .forEach((part) => {
      const match = part.match(/^"?([A-Za-z0-9_]+)"?\s*[:=]\s*(.+)$/);
      if (!match) return;
      const key = match[1];
      let value = match[2].trim();
      if (!value) return;
      // Удаляем кавычки вокруг значения, если они присутствуют
      value = value.replace(/^"(.+)"$/, "$1").replace(/^'(.+)'$/, "$1");
      if (!value) return;
      // Приводим булевы значения к типу boolean
      if (/^(true|false)$/i.test(value)) {
        result[key] = /^true$/i.test(value);
      } else if (/^\d+$/.test(value)) {
        // Числовые значения оставляем строками, чтобы не менять текущую логику
        result[key] = value;
      } else {
        result[key] = value;
      }
      hasData = true;
    });
  if (!hasData) return null;
  // Нормализуем известные поля
  if (result.type) result.type = String(result.type).toLowerCase();
  if (result.id) result.id = String(result.id).toUpperCase();
  if (result.public) result.public = String(result.public).toUpperCase();
  if (result.peer) result.peer = String(result.peer).toUpperCase();
  if (typeof result.hasBackup === "string") {
    const backupStr = result.hasBackup.toLowerCase();
    result.hasBackup = /^(1|true|yes|да|on)$/i.test(backupStr);
  }
  return result;
}

// Пытаемся извлечь полезную информацию из текстового ответа KEYGEN без JSON
function parseKeygenPlainResponse(text) {
  const raw = text != null ? String(text).trim() : "";
  if (!raw) return null;
  const lower = raw.toLowerCase();
  if (/(err|fail|ошиб|нет)/.test(lower)) return null; // текст похож на ошибку
  const resultState = {};
  let hasState = false;
  const idMatch = raw.match(/\bID\s*[:=]?\s*([0-9a-f]{4,})/i);
  if (idMatch) {
    resultState.id = idMatch[1].toUpperCase();
    hasState = true;
  }
  const pubMatch = raw.match(/\bPUB(?:LIC)?\s*[:=]?\s*([0-9a-f]{8,})/i);
  if (pubMatch) {
    resultState.public = pubMatch[1].toUpperCase();
    hasState = true;
  }
  const peerMatch = raw.match(/\bPEER\s*[:=]?\s*([0-9a-f]{8,})/i);
  if (peerMatch) {
    resultState.peer = peerMatch[1].toUpperCase();
    hasState = true;
  }
  const backupMatch = raw.match(/\b(BACKUP|RESERVE)\s*[:=]?\s*(YES|ДА|ON|TRUE|1|NO|НЕТ|OFF|FALSE|0)/i);
  if (backupMatch) {
    const flag = backupMatch[2].toLowerCase();
    resultState.hasBackup = /yes|да|on|true|1/.test(flag);
    hasState = true;
  }
  if (/external|внеш/i.test(lower)) {
    resultState.type = "external";
    hasState = true;
  } else if (/local|локал/i.test(lower)) {
    resultState.type = "local";
    hasState = true;
  }
  const looksSuccess = hasState
    || /keygen[^\n]*\b(ok|done|success|готов|сгенер|обновл)/i.test(raw)
    || /^ok\b/i.test(raw)
    || /ключ[^\n]*\b(готов|создан|обновлен|обновлён)/i.test(lower);
  if (!looksSuccess) return null;
  const messageParts = [];
  if (resultState.id) messageParts.push("ID " + resultState.id);
  if (resultState.public) messageParts.push("PUB " + resultState.public);
  const friendly = messageParts.length ? "Ключ обновлён: " + messageParts.join(", ") : "Ключ обновлён";
  return {
    state: hasState ? resultState : null,
    message: friendly,
    note: "KEYGEN: " + friendly,
    raw,
  };
}

// Универсальный обработчик команд, которые обновляют ключи устройства
async function handleKeyGenerationCommand(options) {
  const command = options.command;
  const label = options.label || command;
  const params = options.params || {};
  const requestLog = options.requestLog || "запрос";
  const successMessage = options.successMessage || "Ключ обновлён";
  const emptyResponseMessage = options.emptyResponseMessage || "устройство вернуло пустой ответ";
  const timeoutMs = options.timeoutMs || 6000;
  const refreshAfterPlain = options.refreshAfterPlain !== false;

  status(`→ ${label}`);
  debugLog(`${label} → ${requestLog}`);
  const res = await deviceFetch(command, params, timeoutMs);
  if (!res.ok) {
    debugLog(`${label} ✗ ${res.error}`);
    status(`✗ ${label}`);
    note(`Ошибка ${label}: ${res.error}`);
    return false;
  }

  const rawText = res.text != null ? String(res.text) : "";
  debugLog(`${label} ← ${rawText}`);
  const trimmed = rawText.trim();
  if (!trimmed) {
    status(`✗ ${label}`);
    note(`${label}: ${emptyResponseMessage}`);
    return false;
  }

  let data = null;
  try {
    data = JSON.parse(trimmed);
  } catch (err) {
    console.warn(`[key] не удалось разобрать ответ ${label} как JSON:`, err);
  }
  if (data && data.error) {
    note(`${label}: ${data.error}`);
    status(`✗ ${label}`);
    return false;
  }
  if (data) {
    UI.key.state = data;
    UI.key.lastMessage = successMessage;
    renderKeyState(data);
    debugLog(`${label} ✓ ключ обновлён на устройстве`);
    status(`✓ ${label}`);
    return true;
  }

  const plainInfo = parseKeygenPlainResponse(trimmed);
  if (plainInfo) {
    UI.key.lastMessage = plainInfo.message;
    if (plainInfo.state) {
      UI.key.state = plainInfo.state;
      renderKeyState(plainInfo.state);
    } else {
      UI.key.state = null;
      renderKeyState(null);
    }
    note(`${label}: ${plainInfo.message}`);
    status(`✓ ${label}`);
    debugLog(`${label} ✓ ${plainInfo.message}`);
    if (refreshAfterPlain) {
      await refreshKeyState({ silent: true }).catch((err) => {
        console.warn(`[key] не удалось обновить состояние после ${label}:`, err);
      });
    }
    return true;
  }

  const normalized = trimmed.toLowerCase();
  if (/(err|fail|ошиб|нет)/.test(normalized)) {
    status(`✗ ${label}`);
    note(`${label}: ${trimmed}`);
    debugLog(`${label} ✗ ${trimmed}`);
    return false;
  }

  UI.key.lastMessage = successMessage;
  renderKeyState(UI.key.state);
  note(`${label}: ${trimmed}`);
  status(`✓ ${label}`);
  debugLog(`${label} ✓ ${trimmed}`);
  if (refreshAfterPlain) {
    await refreshKeyState({ silent: true }).catch((err) => {
      console.warn(`[key] не удалось обновить состояние после ${label}:`, err);
    });
  }
  return true;
}

async function requestKeyGen() {
  await handleKeyGenerationCommand({
    command: "KEYGEN",
    label: "KEYGEN",
    requestLog: "запрос генерации",
    successMessage: "Сгенерирован новый локальный ключ",
  });
}

async function requestKeyGenPeer() {
  await handleKeyGenerationCommand({
    command: "KEYGEN PEER",
    label: "KEYGEN PEER",
    requestLog: "запрос повторного применения удалённого ключа",
    successMessage: "Обновлён симметричный ключ по сохранённому удалённому ключу",
  });
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
// Обновление визуального статуса последних IRQ событий под чатом
function renderChatIrqStatus() {
  const wrap = UI.els && UI.els.chatIrqStatus ? UI.els.chatIrqStatus : null;
  if (!wrap) return;
  const messageEl = UI.els.chatIrqMessage || wrap.querySelector(".chat-irq-message");
  const metaEl = UI.els.chatIrqMeta || wrap.querySelector(".chat-irq-meta");
  const state = UI.state && UI.state.irqStatus ? UI.state.irqStatus : { message: "", uptimeMs: null, timestamp: null };
  if (!messageEl) return;
  if (!state.message) {
    messageEl.textContent = "IRQ: событий нет";
    if (metaEl) {
      metaEl.hidden = true;
      metaEl.textContent = "";
    }
    wrap.classList.remove("active");
    return;
  }
  messageEl.textContent = state.message;
  if (metaEl) {
    const parts = [];
    if (Number.isFinite(state.uptimeMs)) parts.push(formatDeviceUptime(state.uptimeMs));
    if (Number.isFinite(state.timestamp)) parts.push(new Date(state.timestamp).toLocaleTimeString());
    if (parts.length) {
      metaEl.hidden = false;
      metaEl.textContent = parts.join(" · ");
    } else {
      metaEl.hidden = true;
      metaEl.textContent = "";
    }
  }
  wrap.classList.add("active");
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

// Добавление записей устройства в область Debug
function appendDeviceLogEntries(entries, opts) {
  const log = UI.els.debugLog;
  if (!log) return;
  const list = Array.isArray(entries) ? entries : [];
  const options = opts || {};
  const state = getDeviceLogState();
  const replace = options.replace === true;
  if (replace) {
    const nodes = log.querySelectorAll('.debug-line[data-origin="device"]');
    nodes.forEach((node) => node.remove());
    state.known.clear();
    state.lastId = 0;
  }
  if (list.length === 0) {
    if (replace) state.initialized = true;
    return;
  }
  const fragment = document.createDocumentFragment();
  let appended = 0;
  for (const item of list) {
    if (!item) continue;
    const text = item.text != null ? String(item.text) : "";
    if (!text) continue;
    const idValue = Number(item.id);
    const id = Number.isFinite(idValue) && idValue > 0 ? idValue : null;
    if (id != null) {
      if (state.known.has(id)) continue;
      state.known.add(id);
      if (id > state.lastId) state.lastId = id;
    }
    const uptimeValue = Number(item.uptime != null ? item.uptime : item.uptimeMs);
    const uptime = Number.isFinite(uptimeValue) && uptimeValue >= 0 ? uptimeValue : null;
    debugLog(text, {
      origin: "device",
      uptimeMs: uptime,
      id: id,
      fragment,
    });
    appended += 1;
  }
  if (appended > 0) {
    log.appendChild(fragment);
    log.scrollTop = log.scrollHeight;
  }
  if (replace) state.initialized = true;
}

// Загрузка последних сообщений журнала с устройства
async function hydrateDeviceLog(options) {
  const opts = options || {};
  const state = getDeviceLogState();
  if (state.loading) return;
  if (state.initialized && !opts.force) return;
  state.loading = true;
  try {
    const res = await fetchDeviceLogHistory(opts.limit || 150, opts.timeoutMs || 4000);
    if (!res.ok) {
      if (!opts.silent) {
        const errText = res.error != null && res.error !== "" ? String(res.error) : "unknown";
        logErrorEvent("LOGS", errText);
      }
    } else {
      appendDeviceLogEntries(res.logs, { replace: true });
      if (state.queue.length) {
        const pending = state.queue.splice(0, state.queue.length);
        appendDeviceLogEntries(pending);
      }
    }
  } catch (err) {
    if (!opts.silent) {
      const errText = err != null ? String(err) : "unknown";
      logErrorEvent("LOGS", errText);
    }
  } finally {
    state.loading = false;
  }
}
function setDebugLineMessage(line, message, baseMessage) {
  if (!line) return;
  const payload = message != null ? String(message) : "";
  if (baseMessage != null) {
    line.dataset.baseMessage = String(baseMessage);
  } else if (!line.dataset.baseMessage) {
    line.dataset.baseMessage = payload;
  }
  line.dataset.message = payload;
  const stamp = line.dataset.stamp || "";
  line.textContent = stamp + payload;
}

function debugLog(text, opts) {
  const options = opts || {};
  const log = UI.els.debugLog;
  if (!log) return null;
  const line = document.createElement("div");
  const type = classifyDebugMessage(text);
  line.className = "debug-line debug-line--" + type;
  if (options.origin) line.dataset.origin = options.origin;
  if (options.id != null) line.dataset.id = String(options.id);
  if (options.uptimeMs != null && Number.isFinite(options.uptimeMs)) {
    line.dataset.uptime = String(options.uptimeMs);
  }
  const stamp = options.timestamp != null
    ? new Date(options.timestamp).toLocaleTimeString()
    : (options.uptimeMs != null && Number.isFinite(options.uptimeMs)
        ? formatDeviceUptime(options.uptimeMs)
        : new Date().toLocaleTimeString());
  line.dataset.stamp = "[" + stamp + "] ";
  setDebugLineMessage(line, text);
  if (options.origin === "device") {
    const rawText = text != null ? String(text) : "";
    const lowered = rawText.toLowerCase();
    if (rawText && (lowered.includes("irq=") || /\birq(?:[-_\s]|$)/i.test(rawText)) && (lowered.includes("radiosx1262") || lowered.includes("sx1262"))) {
      if (!UI.state.irqStatus || typeof UI.state.irqStatus !== "object") {
        UI.state.irqStatus = { message: "", uptimeMs: null, timestamp: null };
      }
      UI.state.irqStatus.message = rawText;
      UI.state.irqStatus.uptimeMs = Number.isFinite(options.uptimeMs) ? Number(options.uptimeMs) : null;
      UI.state.irqStatus.timestamp = Date.now();
      renderChatIrqStatus();
    }
  }
  const target = options.fragment && typeof options.fragment.appendChild === "function" ? options.fragment : log;
  target.appendChild(line);
  if (!options.fragment) {
    log.scrollTop = log.scrollHeight;
  }
  return line;
}

// Интервал подавления одинаковых ошибок, чтобы не засорять журнал повторяющимися строками
const ERROR_LOG_REPEAT_WINDOW_MS = 15000;
const ERROR_LOG_REPEAT_MAX_COUNT = 99;
const errorLogRepeatState = new Map();

function logRepeatedMessage(key, message, opts) {
  const now = Date.now();
  const text = message != null ? String(message) : "";
  const entry = errorLogRepeatState.get(key);
  const lineConnected = entry && entry.line && typeof entry.line.isConnected === "boolean"
    ? entry.line.isConnected
    : true;
  if (!entry || entry.text !== text || !lineConnected || (now - entry.lastTimestamp) > ERROR_LOG_REPEAT_WINDOW_MS) {
    const line = debugLog(text, opts);
    errorLogRepeatState.set(key, { text, line, count: 1, lastTimestamp: now });
    if (line) {
      line.dataset.repeatKey = key;
      line.dataset.repeatCount = "1";
      setDebugLineMessage(line, text, text);
    }
    return line;
  }
  entry.lastTimestamp = now;
  entry.count = Math.min(entry.count + 1, ERROR_LOG_REPEAT_MAX_COUNT);
  if (entry.line) {
    const base = entry.text;
    const suffix = entry.count > 1 ? " · повтор ×" + entry.count : "";
    setDebugLineMessage(entry.line, base + suffix, base);
    entry.line.dataset.repeatCount = String(entry.count);
  }
  return entry.line;
}

// Агрегированная запись об ошибке с префиксом ERR <label>
function logErrorEvent(label, detail, opts) {
  const cleanLabel = label != null ? String(label).trim() : "";
  const prefix = cleanLabel ? "ERR " + cleanLabel : "ERR";
  const detailText = detail != null ? String(detail) : "";
  const message = detailText ? prefix + ": " + detailText : prefix;
  return logRepeatedMessage("error:" + prefix, message, opts);
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
    probe().catch(() => {});
    const monitor = getReceivedMonitorState();
    if (monitor && monitor.known) monitor.known = new Set();
    await pollReceivedMessages({ silentError: true });
    openReceivedPushChannel();
  } catch (err) {
    console.warn("[endpoint] resync error", err);
  }
}
)~~~";

// web/libs/sha256.js
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

// web/libs/mgrs.js
const char MGRS_JS[] PROGMEM = R"~~~(
/* Поддержка сетки MGRS (100 км) для вкладки Pointing.
 * Используется упрощённая адаптация алгоритмов библиотеки proj4js/mgrs (MIT).
 */
(function (global) {
  // Константы UTM/MGRS (адаптированы из proj4js/mgrs)
  const NUM_100K_SETS = 6;
  const SET_ORIGIN_COLUMN_LETTERS = "AJSAJS";
  const SET_ORIGIN_ROW_LETTERS = "AFAFAF";
  const A = 65;
  const I = 73;
  const O = 79;
  const V = 86;
  const Z = 90;
  const ECC_SQUARED = 0.00669438;
  const SCALE_FACTOR = 0.9996;
  const SEMI_MAJOR_AXIS = 6378137;
  const EASTING_OFFSET = 500000;
  const NORTHING_OFFSET = 10000000;
  const UTM_ZONE_WIDTH = 6;
  const HALF_UTM_ZONE_WIDTH = UTM_ZONE_WIDTH / 2;

  // Нормализация ввода — принимаем только обозначения зоны и 100‑км квадрата
  function normalizeInput(value) {
    if (!value) return null;
    const trimmed = String(value).toUpperCase().replace(/[^0-9A-Z]/g, "");
    const match = trimmed.match(/^(\d{1,2})([C-HJ-NP-X])([A-HJ-NP-Z]{2})$/);
    if (!match) return null;
    const zone = parseInt(match[1], 10);
    if (!Number.isFinite(zone) || zone < 1 || zone > 60) return null;
    const e100k = match[3][0];
    const n100k = match[3][1];
    if (e100k === "I" || e100k === "O" || n100k === "I" || n100k === "O") return null;
    return {
      zone,
      band: match[2],
      e100k,
      n100k,
      text: String(zone) + match[2] + e100k + n100k,
    };
  }

  function degToRad(deg) {
    return (deg * (Math.PI / 180));
  }

  function radToDeg(rad) {
    return (180 * (rad / Math.PI));
  }

  function get100kSetForZone(i) {
    let setParm = i % NUM_100K_SETS;
    if (setParm === 0) {
      setParm = NUM_100K_SETS;
    }
    return setParm;
  }

  function getEastingFromChar(e, set) {
    let curCol = SET_ORIGIN_COLUMN_LETTERS.charCodeAt(set - 1);
    let eastingValue = 100000;
    let rewindMarker = false;

    while (curCol !== e.charCodeAt(0)) {
      curCol++;
      if (curCol === I) curCol++;
      if (curCol === O) curCol++;
      if (curCol > Z) {
        if (rewindMarker) {
          throw new Error("Bad character: " + e);
        }
        curCol = A;
        rewindMarker = true;
      }
      eastingValue += 100000;
    }
    return eastingValue;
  }

  function getNorthingFromChar(n, set) {
    if (n > "V") {
      throw new TypeError("MGRSPoint given invalid Northing " + n);
    }
    let curRow = SET_ORIGIN_ROW_LETTERS.charCodeAt(set - 1);
    let northingValue = 0;
    let rewindMarker = false;

    while (curRow !== n.charCodeAt(0)) {
      curRow++;
      if (curRow === I) curRow++;
      if (curRow === O) curRow++;
      if (curRow > V) {
        if (rewindMarker) {
          throw new Error("Bad character: " + n);
        }
        curRow = A;
        rewindMarker = true;
      }
      northingValue += 100000;
    }
    return northingValue;
  }

  function getMinNorthing(zoneLetter) {
    switch (zoneLetter) {
      case "C": return 1100000;
      case "D": return 2000000;
      case "E": return 2800000;
      case "F": return 3700000;
      case "G": return 4600000;
      case "H": return 5500000;
      case "J": return 6400000;
      case "K": return 7300000;
      case "L": return 8200000;
      case "M": return 9100000;
      case "N": return 0;
      case "P": return 800000;
      case "Q": return 1700000;
      case "R": return 2600000;
      case "S": return 3500000;
      case "T": return 4400000;
      case "U": return 5300000;
      case "V": return 6200000;
      case "W": return 7000000;
      case "X": return 7900000;
      default:
        throw new TypeError("Invalid zone letter: " + zoneLetter);
    }
  }

  function decodeMgrs(mgrsString) {
    if (!mgrsString) throw new TypeError("MGRSPoint converting from empty value");
    let value = mgrsString.replace(/ /g, "");
    let i = 0;
    let zoneBuffer = "";
    const length = value.length;

    while (!(/[A-Z]/).test(value.charAt(i))) {
      if (i >= 2) throw new Error("MGRSPoint bad conversion from: " + value);
      zoneBuffer += value.charAt(i);
      i++;
    }

    const zoneNumber = parseInt(zoneBuffer, 10);
    if (i === 0 || i + 3 > length) throw new Error("MGRSPoint bad conversion from " + value);

    const zoneLetter = value.charAt(i++);
    if (zoneLetter <= "A" || zoneLetter === "B" || zoneLetter === "Y" || zoneLetter >= "Z" || zoneLetter === "I" || zoneLetter === "O") {
      throw new Error("MGRSPoint zone letter " + zoneLetter + " not handled: " + value);
    }

    const hunK = value.substring(i, i += 2);
    const set = get100kSetForZone(zoneNumber);
    const east100k = getEastingFromChar(hunK.charAt(0), set);
    let north100k = getNorthingFromChar(hunK.charAt(1), set);
    while (north100k < getMinNorthing(zoneLetter)) {
      north100k += 2000000;
    }

    const remainder = length - i;
    if (remainder % 2 !== 0) {
      throw new Error("MGRSPoint has to have an even number of digits: " + value);
    }

    const sep = remainder / 2;
    let accuracyBonus;
    let sepEasting = 0;
    let sepNorthing = 0;
    if (sep > 0) {
      accuracyBonus = 100000 / Math.pow(10, sep);
      const eastingString = value.substring(i, i + sep);
      const northingString = value.substring(i + sep);
      sepEasting = parseFloat(eastingString) * accuracyBonus;
      sepNorthing = parseFloat(northingString) * accuracyBonus;
    }

    return {
      easting: east100k + sepEasting,
      northing: north100k + sepNorthing,
      zoneLetter,
      zoneNumber,
      accuracy: accuracyBonus,
    };
  }

  function UTMtoLL(utm) {
    const { northing: UTMNorthing, easting: UTMEasting, zoneLetter, zoneNumber } = utm;
    if (zoneNumber < 0 || zoneNumber > 60) return null;

    const a = SEMI_MAJOR_AXIS;
    const e1 = (1 - Math.sqrt(1 - ECC_SQUARED)) / (1 + Math.sqrt(1 - ECC_SQUARED));
    const x = UTMEasting - EASTING_OFFSET;
    let y = UTMNorthing;
    if (zoneLetter < "N") {
      y -= NORTHING_OFFSET;
    }

    const longOrigin = (zoneNumber - 1) * UTM_ZONE_WIDTH - 180 + HALF_UTM_ZONE_WIDTH;
    const eccPrimeSquared = ECC_SQUARED / (1 - ECC_SQUARED);
    const M = y / SCALE_FACTOR;
    const mu = M / (a * (1 - ECC_SQUARED / 4 - 3 * ECC_SQUARED * ECC_SQUARED / 64 - 5 * ECC_SQUARED * ECC_SQUARED * ECC_SQUARED / 256));

    const phi1Rad = mu + (3 * e1 / 2 - 27 * Math.pow(e1, 3) / 32) * Math.sin(2 * mu)
      + (21 * e1 * e1 / 16 - 55 * Math.pow(e1, 4) / 32) * Math.sin(4 * mu)
      + (151 * Math.pow(e1, 3) / 96) * Math.sin(6 * mu);

    const N1 = a / Math.sqrt(1 - ECC_SQUARED * Math.sin(phi1Rad) * Math.sin(phi1Rad));
    const T1 = Math.tan(phi1Rad) * Math.tan(phi1Rad);
    const C1 = eccPrimeSquared * Math.cos(phi1Rad) * Math.cos(phi1Rad);
    const R1 = a * (1 - ECC_SQUARED) / Math.pow(1 - ECC_SQUARED * Math.sin(phi1Rad) * Math.sin(phi1Rad), 1.5);
    const D = x / (N1 * SCALE_FACTOR);

    let lat = phi1Rad - (N1 * Math.tan(phi1Rad) / R1) * (D * D / 2 - (5 + 3 * T1 + 10 * C1 - 4 * C1 * C1 - 9 * eccPrimeSquared) * Math.pow(D, 4) / 24
      + (61 + 90 * T1 + 298 * C1 + 45 * T1 * T1 - 252 * eccPrimeSquared - 3 * C1 * C1) * Math.pow(D, 6) / 720);
    lat = radToDeg(lat);

    let lon = (D - (1 + 2 * T1 + C1) * Math.pow(D, 3) / 6
      + (5 - 2 * C1 + 28 * T1 - 3 * C1 * C1 + 8 * eccPrimeSquared + 24 * T1 * T1) * Math.pow(D, 5) / 120) / Math.cos(phi1Rad);
    lon = longOrigin + radToDeg(lon);

    if (typeof utm.accuracy === "number") {
      const topRight = UTMtoLL({
        northing: utm.northing + utm.accuracy,
        easting: utm.easting + utm.accuracy,
        zoneLetter: utm.zoneLetter,
        zoneNumber: utm.zoneNumber,
      });
      if (!topRight) return null;
      return {
        top: topRight.lat,
        right: topRight.lon,
        bottom: lat,
        left: lon,
      };
    }

    return { lat, lon };
  }

  function toLatLon100k(value) {
    const normalized = normalizeInput(value);
    if (!normalized) return null;
    let utm;
    try {
      utm = decodeMgrs(normalized.text);
    } catch (err) {
      return null;
    }
    if (!utm) return null;
    const point = UTMtoLL(utm);
    if (!point || typeof point.lat !== "number" || typeof point.lon !== "number") return null;
    return {
      lat: point.lat,
      lon: point.lon,
      zone: normalized.zone,
      band: normalized.band,
      square: normalized.e100k + normalized.n100k,
      text: normalized.text,
    };
  }

  global.satMgrs = {
    normalize100k(value) {
      return normalizeInput(value);
    },
    toLatLon100k(value) {
      return toLatLon100k(value);
    },
  };
})(typeof window !== "undefined" ? window : this);
)~~~";

// web/libs/freq-info.csv
const char FREQ_INFO_CSV[] PROGMEM = R"~~~(
Channel,RX (MHz),TX (MHz),System,Band Plan,Purpose,Frequency (MHz),Bandwidth,Satellite Name,Satellite Position,Comments,Modulation,Usage
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
162,269.95,310.95,DOD 25 kHz,DOD 25K,Tactical communications (DoD),269.95,25KHz,UFO-7,23.3 West,PSK mode,PSK (Phase Shift Keying),Цифровые данные
)~~~";

// web/libs/geostat_tle.js
const char GEOSTAT_TLE_JS[] PROGMEM = R"~~~(
// Геостационарные спутники из REF/tle.txt (фильтр по наклонению и среднему движению)
// Файл сгенерирован скриптом tools/generate_geostat_tle.py
(function(global){
  const tle = [
  {
    "name": "OPS 0757 (TACSAT)",
    "line1": "1 03691U 69013A   22083.65958150  .00000048  00000-0  00000-0 0  9994",
    "line2": "2 03691   1.1382 293.9008 0015240 291.3913  61.4549  1.00438695 98582"
  },
  {
    "name": "MARISAT 1",
    "line1": "1 08697U 76017A   22083.47307565 -.00000130  00000-0  00000+0 0  9997",
    "line2": "2 08697   9.4298 308.0145 0013559 153.4087  74.2575  0.99209735166755"
  },
  {
    "name": "MARISAT 2",
    "line1": "1 09478U 76101A   22083.62934434 -.00000044  00000-0  00000-0 0  9992",
    "line2": "2 09478   9.0521 316.5884 0102530 322.3755  14.7293  0.97590233108272"
  },
  {
    "name": "MARISAT 3",
    "line1": "1 08882U 76053A   22083.56744873 -.00000176  00000-0  00000-0 0  9991",
    "line2": "2 08882  11.2718 313.1676 0122641 298.0496 255.9990  0.95899900106330"
  },
  {
    "name": "NATO 2A",
    "line1": "1 04353U 70021A   22083.64038487 -.00000128  00000-0  00000-0 0  9992",
    "line2": "2 04353   4.2612 290.9626 0002180 168.6866 212.3113  1.00267890110275"
  },
  {
    "name": "NATO 2B",
    "line1": "1 04902U 71009A   22083.63725102 -.00000099  00000-0  00000-0 0  9992",
    "line2": "2 04902   5.0908 283.5153 0005489 157.8279 224.9355  1.00271107103149"
  },
  {
    "name": "NATO 3A",
    "line1": "1 08808U 76035A   22083.46130427 -.00000286  00000-0  00000-0 0  9994",
    "line2": "2 08808   8.4965 308.7229 0020554 290.3834  54.9182  0.99845006731836"
  },
  {
    "name": "NATO 3B",
    "line1": "1 09785U 77005A   22083.63283691 -.00000083  00000-0  00000-0 0  9993",
    "line2": "2 09785  11.4011 320.3640 0044318  19.2903 157.1103  0.95268846101942"
  },
  {
    "name": "NATO 3C",
    "line1": "1 11115U 78106A   22083.83504979 -.00000122  00000-0  00000-0 0  9997",
    "line2": "2 11115  11.9703 326.1982 0002744   8.3861 169.5246  0.98483958639999"
  },
  {
    "name": "NATO 3D",
    "line1": "1 15391U 84115A   22083.57501007 -.00000213  00000-0  00000-0 0  9995",
    "line2": "2 15391  14.4503   4.1984 0105106  93.9303 113.1587  0.94955421133208"
  },
  {
    "name": "NATO 4A",
    "line1": "1 21047U 91001A   22079.96093859 -.00000110  00000-0  00000-0 0  9997",
    "line2": "2 21047  13.2273   3.8778 0003442 104.1814  82.5534  0.98283824112891"
  },
  {
    "name": "NATO 4B",
    "line1": "1 22921U 93076A   22083.54193237 -.00000082  00000-0  00000-0 0  9991",
    "line2": "2 22921  12.6647   9.8930 0007136  40.0437 175.9930  0.98517569103387"
  },
  {
    "name": "PAKSAT 1 (ANATOLIA 1)",
    "line1": "1 23779U 96006A   22083.88896361 -.00000217  00000-0  00000-0 0  9994",
    "line2": "2 23779   7.6454  57.8114 0002698  63.4929 312.8733  0.99591844 95460"
  },
  {
    "name": "Intelsat 22",
    "line1": "1 38098U 12011A   22083.84507990 -.00000069  00000-0  00000-0 0  9996",
    "line2": "2 38098   0.0095 202.8642 0002802 166.4992 189.2476  1.00271312 36325"
  },
  {
    "name": "COMSATBW-1",
    "line1": "1 35943U 09054B   22083.62803991  .00000015  00000-0  00000-0 0  9991",
    "line2": "2 35943   0.0778  97.5731 0002619 299.1306  74.4441  1.00272077 45816"
  },
  {
    "name": "COMSATBW-2",
    "line1": "1 36582U 10021B   22083.83339265  .00000078  00000-0  00000-0 0  9997",
    "line2": "2 36582   0.0689  94.9949 0002313 287.6127 112.8497  1.00269927 43460"
  },
  {
    "name": "MUOS 1",
    "line1": "1 38093U 12009A   22083.88251799 -.00000136  00000-0  00000-0 0  9990",
    "line2": "2 38093   2.8436  29.7253 0058771 358.0649  12.2220  1.00273314 36952"
  },
  {
    "name": "MUOS 2",
    "line1": "1 39206U 13036A   22083.86742925  .00000056  00000-0  00000-0 0  9992",
    "line2": "2 39206   2.6398  20.1948 0051732 181.9264 115.4432  1.00274455 31690"
  },
  {
    "name": "MUOS 3",
    "line1": "1 40374U 15002A   22083.82754904 -.00000138  00000-0  00000-0 0  9993",
    "line2": "2 40374   2.5401   9.3896 0051626 176.6498 278.6304  1.00270448 26091"
  },
  {
    "name": "MUOS 4",
    "line1": "1 40887U 15044A   22083.77202144 -.00000102  00000-0  00000-0 0  9997",
    "line2": "2 40887   2.5558   4.4365 0058377 359.6597 171.0748  1.00271454 24064"
  },
  {
    "name": "MUOS 5",
    "line1": "1 41622U 16041A   22083.64648823 -.00000149  00000-0  00000-0 0  9998",
    "line2": "2 41622   5.8106 301.5749 0194214 233.4703 134.7522  1.00272046 21592"
  },
  {
    "name": "SKYNET 1",
    "line1": "1 04250U 69101A   22083.63582503 -.00000088  00000-0  00000-0 0  9992",
    "line2": "2 04250   3.5324 285.8664 0020042 283.1259  95.0723  1.00269381119026"
  },
  {
    "name": "SKYNET 2B",
    "line1": "1 07547U 74094A   22083.69284299  .00000136  00000-0  00000-0 0  9991",
    "line2": "2 07547   7.4218 297.1398 0004685 136.2730  26.0000  1.00346827109268"
  },
  {
    "name": "SKYNET 4A",
    "line1": "1 20401U 90001A   22083.24170772 -.00000123  00000-0  00000-0 0  9999",
    "line2": "2 20401  13.0926 358.8230 0010581 299.0424 256.3706  0.98996909 31466"
  },
  {
    "name": "SKYNET 4B",
    "line1": "1 19687U 88109A   22083.16914223 -.00000182  00000-0  00000-0 0  9992",
    "line2": "2 19687  14.5382 348.6986 0002828 355.2116 179.1812  0.99670240102754"
  },
  {
    "name": "SKYNET 4C",
    "line1": "1 20776U 90079A   22083.85574941  .00000120  00000-0  00000-0 0  9998",
    "line2": "2 20776  13.8305   0.6961 0003214   6.5851 156.5296  1.00276681115395"
  },
  {
    "name": "SKYNET 4D",
    "line1": "1 25134U 98002A   22083.04389699 -.00000116  00000-0  00000-0 0  9996",
    "line2": "2 25134  11.7925  19.6868 0005859  74.3332 115.8913  0.99154503 88043"
  },
  {
    "name": "SKYNET 4E",
    "line1": "1 25639U 99009B   22083.04404500 -.00000037  00000-0  00000-0 0  9995",
    "line2": "2 25639  11.2057  17.3794 0002711 341.0872 197.6767  1.00274641 35882"
  },
  {
    "name": "SKYNET 4F",
    "line1": "1 26695U 01005B   22083.88129693  .00000014  00000-0  00000-0 0  9990",
    "line2": "2 26695  10.5746  26.6582 0002491  88.9089 277.3986  0.99144722 77295"
  },
  {
    "name": "SKYNET 5A",
    "line1": "1 30794U 07007B   22083.84981118 -.00000282  00000-0  00000-0 0  9995",
    "line2": "2 30794   1.9953  88.6685 0003037 278.1802 216.5723  1.00272930 55206"
  },
  {
    "name": "SKYNET 5B",
    "line1": "1 32294U 07056B   22083.83583476  .00000132  00000-0  00000-0 0  9993",
    "line2": "2 32294   1.3141  89.7509 0003993 270.3454 148.1481  1.00271905 52705"
  },
  {
    "name": "SKYNET 5C",
    "line1": "1 33055U 08030A   22083.89899368 -.00000151  00000-0  00000+0 0  9999",
    "line2": "2 33055   0.0668 350.1710 0003226  36.5084 101.4286  1.00270840 50407"
  },
  {
    "name": "SKYNET 5D",
    "line1": "1 39034U 12075A   22083.82063293  .00000089  00000-0  00000-0 0  9999",
    "line2": "2 39034   0.0717 357.5966 0003665   8.6768 164.1607  1.00274730 33670"
  },
  {
    "name": "LEASAT 1",
    "line1": "1 15384U 84113C   22083.17027608 -.00000092  00000-0  00000+0 0  9995",
    "line2": "2 15384  12.8671 347.0443 0025770 266.0748 276.6447  0.98543784108785"
  },
  {
    "name": "LEASAT 2",
    "line1": "1 15236U 84093C   22083.54775705 -.00000179  00000-0  00000-0 0  9994",
    "line2": "2 15236  13.7755 325.2443 0026491 237.3353 326.8578  0.97457073112991"
  },
  {
    "name": "LEASAT 3",
    "line1": "1 15643U 85028C   22083.63618006 -.00000067  00000-0  00000-0 0  9991",
    "line2": "2 15643  14.4583 321.4886 0071400 264.4074  77.3375  0.96961777121623"
  },
  {
    "name": "LEASAT 4",
    "line1": "1 15995U 85076D   22083.46021365 -.00000160  00000-0  00000-0 0  9995",
    "line2": "2 15995  12.0922 338.6941 0005529 353.9521 337.7999  0.97800082103016"
  },
  {
    "name": "LEASAT 5",
    "line1": "1 20410U 90002B   22081.96155833  .00000057  00000-0  00000-0 0  9993",
    "line2": "2 20410  11.5213 359.8403 0008531  43.8387 153.7856  0.99595150103017"
  },
  {
    "name": "OPS 6391 (FLTSATCOM 1)",
    "line1": "1 10669U 78016A   22083.89681323 -.00000253  00000-0  00000+0 0  9999",
    "line2": "2 10669  10.7840 307.4329 0005858 189.5039 339.3514  0.99006091739341"
  },
  {
    "name": "OPS 6392 (FLTSATCOM 2)",
    "line1": "1 11353U 79038A   22083.89951699 -.00000222  00000-0  00000+0 0  9997",
    "line2": "2 11353  11.0943 315.3100 0018429  51.8721 123.7212  0.98545981155147"
  },
  {
    "name": "OPS 6393 (FLTSATCOM 3)",
    "line1": "1 11669U 80004A   22083.59794965 -.00000192  00000-0  00000-0 0  9997",
    "line2": "2 11669   9.3771 316.2822 0023746 196.5515 156.1181  1.00163822154354"
  },
  {
    "name": "OPS 6394 (FLTSATCOM 4)",
    "line1": "1 12046U 80087A   22083.30279953 -.00000055  00000-0  00000+0 0  9999",
    "line2": "2 12046  11.2730 317.1441 0005578  92.6517 267.3529  0.98951031114865"
  },
  {
    "name": "FLTSATCOM 5",
    "line1": "1 12635U 81073A   22083.37050207 -.00000190  00000-0  00000-0 0  9996",
    "line2": "2 12635  16.9567 324.6144 0012661 116.2838  55.5501  0.98607169117828"
  },
  {
    "name": "FLTSATCOM 7 (USA 20)",
    "line1": "1 17181U 86096A   22083.52396388 -.00000140  00000-0  00000-0 0  9992",
    "line2": "2 17181  13.9635 350.6088 0038278 300.0063 269.1146  0.98930428136672"
  },
  {
    "name": "FLTSATCOM 8 (USA 46)",
    "line1": "1 20253U 89077A   22083.91045723 -.00000085  00000-0  00000-0 0  9993",
    "line2": "2 20253  12.8842   0.7986 0004573  67.3524 153.8242  1.00272162245114"
  },
  {
    "name": "UFO 2 (USA 95)",
    "line1": "1 22787U 93056A   22083.08524037  .00000122  00000-0  00000-0 0  9998",
    "line2": "2 22787  11.4471  11.6071 0004132 354.0666 235.2715  1.00269492103048"
  },
  {
    "name": "UFO 3 (USA 104)",
    "line1": "1 23132U 94035A   22083.04049549 -.00000060  00000-0  00000-0 0  9999",
    "line2": "2 23132  11.7410  14.5454 0003243  21.8090 155.2523  1.00287320116196"
  },
  {
    "name": "UFO 4 (USA 108)",
    "line1": "1 23467U 95003A   22083.69331020 -.00000029  00000-0  00000-0 0  9990",
    "line2": "2 23467   9.8492  18.1762 0003818  28.4715 197.3434  1.00271732 99459"
  },
  {
    "name": "UFO 5 (USA 111)",
    "line1": "1 23589U 95027A   22083.62733389 -.00000152  00000-0  00000-0 0  9995",
    "line2": "2 23589  11.0859  17.6113 0006694  14.9718 204.9186  0.98752629 97731"
  },
  {
    "name": "UFO 6 (USA 114)",
    "line1": "1 23696U 95057A   22082.92688021  .00000036  00000-0  00000-0 0  9991",
    "line2": "2 23696  10.3153  21.3738 0005744  78.7418 116.9097  0.98762227 96506"
  },
  {
    "name": "UFO 7 (USA 127)",
    "line1": "1 23967U 96042A   22083.28725021 -.00000289  00000-0  00000-0 0  9990",
    "line2": "2 23967   9.7308  23.9422 0009386  53.5434 165.4027  1.00218142 93981"
  },
  {
    "name": "UFO 8 (USA 138)",
    "line1": "1 25258U 98016A   22083.69444823 -.00000032  00000-0  00000-0 0  9997",
    "line2": "2 25258   9.3363  30.2335 0003143 347.7192 225.8201  1.00269013 88354"
  },
  {
    "name": "UFO 9 (USA 140)",
    "line1": "1 25501U 98058A   22082.83954490 -.00000010  00000-0  00000-0 0  9991",
    "line2": "2 25501   9.6105  27.5268 0014854 338.5973 184.9526  0.98175309 84645"
  },
  {
    "name": "UFO 10 (USA 146)",
    "line1": "1 25967U 99063A   22083.86805484 -.00000196  00000-0  00000-0 0  9991",
    "line2": "2 25967   8.6097  33.7111 0003153   0.5555  77.5493  1.00267739 81851"
  },
  {
    "name": "UFO 11 (USA 174)",
    "line1": "1 28117U 03057A   22083.01785374 -.00000099  00000-0  00000-0 0  9993",
    "line2": "2 28117   6.8670  37.9130 0005318 354.5212 230.0634  1.00271147 66946"
  }
  ];
  if (!global.SAT_TLE_DATA) {
    global.SAT_TLE_DATA = tle;
  } else if (Array.isArray(global.SAT_TLE_DATA)) {
    global.SAT_TLE_DATA = global.SAT_TLE_DATA.concat(tle);
  } else {
    global.SAT_TLE_DATA = tle;
  }
})(typeof globalThis !== "undefined" ? globalThis : (typeof window !== "undefined" ? window : this));
)~~~";

