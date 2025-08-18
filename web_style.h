// Стили веб-интерфейса
#pragma once
// Добавлены стили блока чата, меток времени и уменьшена кнопка отправки
const char WEB_STYLE_CSS[] PROGMEM = R"rawliteral(
:root{--bg-color:#222;--fg-color:#eee;--panel-bg:#333;--panel-bg-open:#2a2a2a;--primary:#3f51b5;--primary-hover:#5c6bc0;--base-font:16px;--tx-color:#00ccff;--rx-color:#ffb74d;--sys-color:#66bb6a}
body.light{--bg-color:#f5f5f5;--fg-color:#222;--panel-bg:#eee;--panel-bg-open:#e0e0e0;--primary:#3f51b5;--primary-hover:#5c6bc0;--tx-color:#01579b;--rx-color:#e65100;--sys-color:#2e7d32}
body{background-color:var(--bg-color);color:var(--fg-color);font-family:Arial,sans-serif;font-size:var(--base-font);margin:0;padding:10px}
h2{margin-top:0}
#statusBar{display:flex;align-items:center;margin-bottom:8px}
.indicator{width:12px;height:12px;border-radius:50%;display:inline-block;margin-right:4px}
.indicator.local{background-color:var(--sys-color)}
.indicator.remote{background-color:var(--rx-color)}
.indicator.tx{background-color:var(--tx-color)}
.indicator.rx{background-color:var(--rx-color)}
.indicator.blink{animation:blink 1s linear infinite}
@keyframes blink{0%,50%{opacity:1}50%,100%{opacity:0}}
#chatPanel{height:40vh;display:flex;flex-direction:column;margin-bottom:10px}
#chat{flex:1;width:100%;background-color:var(--panel-bg);border:1px solid #555;padding:10px;box-sizing:border-box;overflow-y:auto;display:flex;flex-direction:column;justify-content:flex-end}
#inputRow{display:flex;margin-top:6px;flex-wrap:wrap;gap:6px}
#msg{flex:1;padding:8px;border:1px solid #555;background-color:var(--panel-bg);color:var(--fg-color)}
button{padding:8px 12px;border:none;color:#fff;cursor:pointer;border-radius:3px}
.btn-primary{background-color:var(--primary)}
.btn-primary:hover{background-color:var(--primary-hover)}
.btn-secondary{background-color:#607d8b}
.btn-secondary:hover{background-color:#78909c}
.btn-danger{background-color:#e53935}
.btn-danger:hover{background-color:#ef5350}
#sendBtn{padding:4px 8px}
select,input[type=number],input[type=text]{padding:4px;background-color:var(--panel-bg);color:var(--fg-color);border:1px solid #555;border-radius:3px}
label{margin-right:4px}
details{margin-bottom:8px}
details summary{cursor:pointer;padding:6px;background-color:var(--panel-bg);border:1px solid #555;border-radius:4px;list-style:none}
details[open] summary{background-color:var(--panel-bg-open)}
.panel-content{padding:6px;border:1px solid #555;border-top:none;background-color:var(--panel-bg)}
#pingHistory{font-family:monospace;font-size:.9em;white-space:pre}
.row{display:flex;flex-wrap:wrap;align-items:center;margin-bottom:6px}
.row>*{margin-right:6px;margin-bottom:6px}
.tx{color:var(--tx-color)}
.rx{color:var(--rx-color)}
.sys{color:var(--sys-color)}
.msg-line{display:flex;align-items:flex-start}
.msg-time{font-family:monospace;margin-right:4px}
.msg-tag{margin-right:4px;font-weight:700}
.msg-text{flex:1}
canvas.graph{background-color:#111;border:1px solid #555}
@media(max-width:600px){#inputRow{flex-direction:column}#msg{width:100%}}
)rawliteral";

