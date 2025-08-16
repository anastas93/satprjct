"""Простой watchdog для Radxa Zero.

Отправляет периодические ping-сообщения по второму UART,
что позволяет микроконтроллеру контролировать наличие хоста.
"""

from __future__ import annotations

import argparse
import time

from .uart import UARTInterface


def watchdog_loop(port: str, baudrate: int, interval: float = 1.0) -> None:
    """Циклично отправляет ping-сообщения."""
    uart = UARTInterface(port=port, baudrate=baudrate)
    while True:
        uart.send(b"[WDOG] ping\n")
        time.sleep(interval)


def main() -> None:
    parser = argparse.ArgumentParser(description="Watchdog для Radxa Zero")
    parser.add_argument("--port", default="/dev/ttyS2", help="Путь к UART устройству")
    parser.add_argument("--baudrate", type=int, default=115200, help="Скорость UART")
    parser.add_argument("--interval", type=float, default=1.0, help="Интервал ping, сек")
    args = parser.parse_args()
    watchdog_loop(args.port, args.baudrate, args.interval)


if __name__ == "__main__":
    main()
