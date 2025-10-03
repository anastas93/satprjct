import re
from pathlib import Path

# Считываем текущее значение тайм-аута из исходника
SOURCE = Path('src/main.cpp').read_text(encoding='utf-8')
match = re.search(r"kSerialLineTimeoutMs\s*=\s*(\d+)", SOURCE)
if not match:
    raise SystemExit("Не удалось найти константу kSerialLineTimeoutMs")
TIMEOUT = int(match.group(1))

# Простая модель последовательного ввода символов с логикой из loop()
class SerialBufferModel:
    def __init__(self):
        self.buffer = ""
        self.overflow = False
        self.last_byte_at = 0
        self.now = 0
        self.completed = []

    def advance(self, delta_ms):
        self.now += delta_ms
        if self.buffer and self.last_byte_at and (self.now - self.last_byte_at) > TIMEOUT:
            # Полностью повторяем поведение loop(): сбрасываем буфер и флаги
            self.buffer = ""
            self.overflow = False
            self.last_byte_at = 0

    def feed(self, ch: str):
        # Воспроизводим ключевые ветви обработки символов из loop()
        if ch == '\r':
            return
        if ch != '\n':
            if not self.overflow:
                if len(self.buffer) < 1024:
                    self.buffer += ch
                else:
                    self.overflow = True
            self.last_byte_at = self.now
            return
        # Пришёл перевод строки: проверяем переполнение и фиксацию команды
        line = self.buffer
        overflow = self.overflow
        self.buffer = ""
        self.overflow = False
        if overflow:
            self.completed.append({"type": "overflow"})
            return
        line = line.strip()
        if not line:
            return
        self.completed.append({"type": "command", "value": line})


def check_manual_input_pause():
    model = SerialBufferModel()
    model.feed('B')
    model.advance(TIMEOUT // 2)
    model.feed('F')
    model.advance((TIMEOUT // 2) + 200)  # суммарная пауза > 1 c, но < тайм-аута
    model.feed(' ')
    model.feed('4')
    model.feed('\n')
    assert model.completed == [{"type": "command", "value": "BF 4"}], model.completed


def check_binary_stream_continuous():
    model = SerialBufferModel()
    for _ in range(512):
        model.feed('X')  # произвольный байт
        model.advance(10)  # имитируем плотный поток < тайм-аута
    assert model.buffer, "Буфер не должен обнуляться в непрерывном потоке"
    model.advance(TIMEOUT + 1)
    assert model.buffer == "", "Буфер должен очищаться после паузы больше тайм-аута"


def main():
    check_manual_input_pause()
    check_binary_stream_continuous()
    print(f"Тесты симуляции тайм-аута пройдены. Тайм-аут = {TIMEOUT} мс")


if __name__ == "__main__":
    main()
