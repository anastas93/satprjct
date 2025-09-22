#!/usr/bin/env python3
"""Генерация постоянного набора TLE для веб-интерфейса."""
from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, List

# Ограничения для отбора геостационарных аппаратов
MAX_INCLINATION_DEG = 20.0
MIN_MEAN_MOTION = 0.9
MAX_MEAN_MOTION = 1.1


@dataclass
class TleEntry:
    """Структура для хранения одной записи TLE."""

    name: str
    line1: str
    line2: str

    @property
    def inclination(self) -> float:
        """Извлекает наклонение орбиты из второй строки."""

        return float(self.line2[8:16])

    @property
    def mean_motion(self) -> float:
        """Возвращает среднее движение (оборотов в сутки)."""

        return float(self.line2[52:63])


def load_tle(path: Path) -> List[TleEntry]:
    """Читает файл TLE и разбивает его на отдельные записи."""

    raw_lines = [line.strip() for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]
    if len(raw_lines) % 3 != 0:
        raise ValueError(f"Файл {path} содержит неполные записи TLE")

    entries: List[TleEntry] = []
    for idx in range(0, len(raw_lines), 3):
        name, line1, line2 = raw_lines[idx : idx + 3]
        entries.append(TleEntry(name=name, line1=line1, line2=line2))
    return entries


def filter_geostat(entries: Iterable[TleEntry]) -> List[TleEntry]:
    """Отбирает спутники с параметрами, характерными для геостационарных орбит."""

    filtered: List[TleEntry] = []
    for entry in entries:
        try:
            inc = entry.inclination
            mean_motion = entry.mean_motion
        except ValueError:
            # Пропускаем некорректные строки
            continue

        if inc > MAX_INCLINATION_DEG:
            continue
        if not (MIN_MEAN_MOTION <= mean_motion <= MAX_MEAN_MOTION):
            continue
        filtered.append(entry)
    return filtered


def render_js(entries: Iterable[TleEntry]) -> str:
    """Формирует содержимое файла web/libs/geostat_tle.js."""

    entries = list(entries)
    lines = [
        "// Геостационарные спутники из REF/tle.txt (фильтр по наклонению и среднему движению)",
        "// Файл сгенерирован скриптом tools/generate_geostat_tle.py",
        "(function(global){",
        "  const tle = [",
    ]

    for idx, entry in enumerate(entries):
        lines.append("  {")
        lines.append(f"    \"name\": {json.dumps(entry.name, ensure_ascii=False)},")
        lines.append(f"    \"line1\": {json.dumps(entry.line1, ensure_ascii=False)},")
        lines.append(f"    \"line2\": {json.dumps(entry.line2, ensure_ascii=False)}")
        lines.append("  }" + ("," if idx != len(entries) - 1 else ""))

    lines.extend([
        "  ];",
        "  if (!global.SAT_TLE_DATA) {",
        "    global.SAT_TLE_DATA = tle;",
        "  } else if (Array.isArray(global.SAT_TLE_DATA)) {",
        "    global.SAT_TLE_DATA = global.SAT_TLE_DATA.concat(tle);",
        "  } else {",
        "    global.SAT_TLE_DATA = tle;",
        "  }",
        "})(typeof globalThis !== \"undefined\" ? globalThis : (typeof window !== \"undefined\" ? window : this));",
        "",
    ])
    return "\n".join(lines)


def main() -> None:
    """Читает REF/tle.txt и обновляет web/libs/geostat_tle.js."""

    repo_root = Path(__file__).resolve().parent.parent
    source = repo_root / "REF" / "tle.txt"
    target = repo_root / "web" / "libs" / "geostat_tle.js"

    entries = load_tle(source)
    geostat_entries = filter_geostat(entries)
    js_content = render_js(geostat_entries)
    target.write_text(js_content, encoding="utf-8")
    print(f"Сохранено {len(geostat_entries)} записей TLE в {target}")


if __name__ == "__main__":
    main()
