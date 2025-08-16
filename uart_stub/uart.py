"""UART communication stub module for Radxa Zero.

This module provides a tiny wrapper around :mod:`pyserial` to demonstrate
how UART communication might be wired on a Radxa Zero board.  When a serial
device is unavailable, the interface falls back to simple console prints so
the rest of the application can be exercised without actual hardware.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Optional
import argparse

try:
    import serial  # type: ignore
except Exception:  # pragma: no cover - serial may be absent
    serial = None


@dataclass
class UARTInterface:
    """Simple UART interface with send and receive stubs.

    Parameters are tuned for the Radxa Zero where the default UART device is
    typically ``/dev/ttyS2`` running at ``115200`` baud.
    """

    port: str = "/dev/ttyS2"
    baudrate: int = 115200
    timeout: float = 1.0
    _serial: Optional["serial.Serial"] = None

    def __post_init__(self) -> None:
        if serial is not None:
            try:
                self._serial = serial.Serial(self.port, self.baudrate, timeout=self.timeout)
            except Exception:
                self._serial = None
        else:
            self._serial = None

    def send(self, data: bytes) -> None:
        """Send data over UART or print a placeholder."""
        if self._serial is not None:
            self._serial.write(data)
        else:
            print(f"[UART TX] {data!r}")

    def receive(self) -> bytes:
        """Receive data from UART or return placeholder bytes."""
        if self._serial is not None:
            return self._serial.read(self._serial.in_waiting or 1)
        print("[UART RX] placeholder, returning empty bytes")
        return b""


def main() -> None:
    parser = argparse.ArgumentParser(
        description="UART communication stub for Radxa Zero"
    )
    parser.add_argument(
        "--port",
        default="/dev/ttyS2",
        help="UART device path (default: /dev/ttyS2)",
    )
    parser.add_argument(
        "--baudrate",
        type=int,
        default=115200,
        help="Serial baud rate (default: 115200)",
    )
    parser.add_argument(
        "--message",
        default="Hello, UART!",
        help="Message to send over UART",
    )
    args = parser.parse_args()

    uart = UARTInterface(port=args.port, baudrate=args.baudrate)
    uart.send(args.message.encode())
    received = uart.receive()
    if received:
        print(f"[UART RX] {received!r}")


if __name__ == "__main__":
    main()
