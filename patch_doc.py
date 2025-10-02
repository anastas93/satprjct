from pathlib import Path

path = Path(r"src/tools/generate_web_content.py")
text = path.read_text(encoding="utf-8")
old_doc = "\"\"\"��������� PROGMEM-������ ������ ��� ��� �����.\"\"\""
new_doc = "\"\"\"Формирует PROGMEM-массив байтов для содержимого файла.\"\"\""
if old_doc not in text:
    raise SystemExit("Не найден старый docstring")
text = text.replace(old_doc, new_doc, 1)
path.write_text(text, encoding="utf-8")
