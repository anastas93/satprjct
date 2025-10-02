#!/usr/bin/env python3
"""Генерация web/web_content.h из исходников веб-интерфейса."""
from __future__ import annotations

import argparse
from pathlib import Path
from typing import Iterable, Tuple

# Константные ресурсы, которые нужно упаковать в PROGMEM.
# Порядок важен: HTML должен идти первым для удобства чтения.
RESOURCES: Tuple[Tuple[str, str], ...] = (
    ("web/index.html", "INDEX_HTML"),
    ("web/style.css", "STYLE_CSS"),
    ("web/script.js", "SCRIPT_JS"),
    ("web/libs/sha256.js", "SHA256_JS"),
    ("web/libs/mgrs.js", "MGRS_JS"),
    ("web/libs/freq-info.csv", "FREQ_INFO_CSV"),
    ("web/libs/geostat_tle.js", "GEOSTAT_TLE_JS"),
)

HEADER_PROLOGUE = """#pragma once
#include <pgmspace.h>
// Встроенные ресурсы веб-интерфейса.
// Файл сгенерирован автоматически скриптом tools/generate_web_content.py.
"""


def _normalise_content(text: str) -> str:
    """Приводит контент к формату LF и убирает лишние отступы."""

    text = text.replace("\r\n", "\n").replace("\r", "\n")
    if text.startswith("\ufeff"):
        text = text[1:]
    if text.startswith("\n"):
        text = text[1:]
    # Сохраняем завершающую пустую строку только одну (если была)
    if text.endswith("\n"):
        while text.endswith("\n\n"):
            text = text[:-1]
        text = text[:-1]
    return text


def _choose_delimiter(content: str) -> str:
    """Подбирает разделитель для сырой строки так, чтобы он не конфликтовал с данными."""

    delimiter = "~~~"
    while f"){delimiter}\"" in content:
        delimiter += "~"
    return delimiter


def _render_constant(const_name: str, content: bytes, source_path: Path) -> str:
    """Формирует PROGMEM-массив байтов для содержимого файла."""

    relative = source_path.as_posix()
    if not content:
        body = "  "
    else:
        lines = []
        for idx in range(0, len(content), 16):
            chunk = ", ".join(f"0x{byte:02x}" for byte in content[idx:idx + 16])
            lines.append(f"  {chunk}")
        body = ",\n".join(lines)
    return (
        f"// {relative}\n"
        f"const uint8_t {const_name}[] PROGMEM = {{\n"
        f"{body}\n"
        f"}};\n"
    )


def generate_header(repo_root: Path, output: Path) -> None:
    """Формирует заголовочный файл web_content.h."""

    parts = [HEADER_PROLOGUE.strip() + "\n\n"]
    for relative_path, const_name in RESOURCES:
        source = repo_root / relative_path
        if not source.is_file():
            raise FileNotFoundError(f"Не найден исходный файл {relative_path}")
        content = _normalise_content(source.read_text(encoding="utf-8"))
        content_bytes = content.encode("utf-8")
        parts.append(_render_constant(const_name, content_bytes, Path(relative_path)))
        parts.append("\n")
    output.write_text("".join(parts), encoding="utf-8")


def main(argv: Iterable[str] | None = None) -> None:
    """Точка входа для CLI."""

    parser = argparse.ArgumentParser(description="Генерация web/web_content.h из исходников веб-интерфейса")
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("web/web_content.h"),
        help="Путь к заголовку (по умолчанию web/web_content.h)",
    )
    args = parser.parse_args(list(argv) if argv is not None else None)

    repo_root = Path(__file__).resolve().parent.parent
    generate_header(repo_root, args.output.resolve())
    print(f"Обновлён {args.output}")


if __name__ == "__main__":
    main()
