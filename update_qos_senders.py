#!/usr/bin/env python3
"""Автогенерация таблицы отправляющих функций и их приоритетов QoS."""
import re
from pathlib import Path

root = Path(__file__).resolve().parent
qos_re = re.compile(r"//\s*QOS:\s*([^\s]+)\s+(.*)")

# Определение имени функции по коду перед комментарием
fetch_re = re.compile(r"fetch\('/([^'?]+)")
sendparam_re = re.compile(r"sendParam\('([^']+)")
function_re_js = re.compile(r"function\s+([A-Za-z0-9_]+)\s*\(")
function_re_cpp = re.compile(r"([A-Za-z_][A-Za-z0-9_:]*)\s*\(")
ons_re = re.compile(r"on\('([^']+)'")

def detect_name(code: str) -> str:
    if (m := fetch_re.search(code)):
        return m.group(1)
    if (m := sendparam_re.search(code)):
        return m.group(1)
    if (m := function_re_js.search(code)):
        return m.group(1)
    if (m := ons_re.search(code)):
        return m.group(1)
    if (m := function_re_cpp.search(code)):
        return m.group(1)
    return 'неизвестно'

def main() -> None:
    entries = []
    for path in root.rglob('*'):
        if path.suffix in {'.cpp', '.h', '.ino'} or path.name == 'web_script.h':
            rel = path.relative_to(root)
            with path.open(encoding='utf-8') as f:
                for line in f:
                    if 'QOS:' not in line:
                        continue
                    m = qos_re.search(line)
                    if not m:
                        continue
                    prio, desc = m.groups()
                    name = detect_name(line.split('//')[0])
                    entries.append((str(rel), name, desc.strip(), prio))
    entries.sort()
    out = root / 'QOS_SENDERS.md'
    with out.open('w', encoding='utf-8') as f:
        f.write('# Функции отправки и приоритеты\n\n')
        f.write('Документ генерируется автоматически скриптом `update_qos_senders.py`.\n\n')
        f.write('| Файл | Функция | Описание | Приоритет |\n')
        f.write('|------|---------|----------|-----------|\n')
        for file, func, desc, prio in entries:
            f.write(f'| `{file}` | `{func}` | {desc} | {prio} |\n')

if __name__ == '__main__':
    main()
