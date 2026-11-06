#!/usr/bin/env python3
"""Log telemetry from the sensor firmware to CSV and flag anomalies.

Live capture:   python telemetry_logger.py --port COM3 --out run.csv
Replay a file:  python telemetry_logger.py --replay capture.log --out run.csv
"""

from __future__ import annotations

import argparse
import csv
import re
import sys
from collections import Counter
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Iterable, Iterator, TextIO

FLAG_ALERT = 0x01
FLAG_FAULT_RANGE = 0x02
FLAG_FAULT_STUCK = 0x04

# Must match the flags in lib/detector/src/detector.h.
FLAG_NAMES = {
    FLAG_ALERT: "alert",
    FLAG_FAULT_RANGE: "fault_range",
    FLAG_FAULT_STUCK: "fault_stuck",
}

DEFAULT_SPIKE_DECI_C = 50  # 5.0 degC between samples is far faster than a TMP36 can follow

CSV_HEADER = [
    "host_time",
    "seq",
    "device_ms",
    "raw",
    "temp_c",
    "alert",
    "fault_range",
    "fault_stuck",
    "anomalies",
]

_LINE_RE = re.compile(r"^\$(T,(\d+),(\d+),(\d+),(-?\d+),(\d+))\*([0-9A-F]{2})$")


class ParseError(ValueError):
    """A line was not a valid telemetry frame."""


@dataclass(frozen=True)
class Frame:
    seq: int
    device_ms: int
    raw: int
    deci_c: int
    flags: int

    @property
    def temp_c(self) -> float:
        return self.deci_c / 10


def checksum(body: str) -> int:
    """XOR of the bytes between '$' and '*'."""
    result = 0
    for byte in body.encode("ascii"):
        result ^= byte
    return result


def parse_line(line: str) -> Frame:
    """Parse one telemetry line, raising ParseError if malformed or corrupt."""
    match = _LINE_RE.match(line.strip())
    if match is None:
        raise ParseError(f"malformed line: {line.strip()!r}")

    body, seq, device_ms, raw, deci_c, flags, received = match.groups()
    if checksum(body) != int(received, 16):
        raise ParseError(f"checksum mismatch: {line.strip()!r}")

    return Frame(int(seq), int(device_ms), int(raw), int(deci_c), int(flags))


class AnomalyMonitor:
    """Compares each frame with the previous one and names anything unusual."""

    def __init__(self, spike_deci_c: int = DEFAULT_SPIKE_DECI_C) -> None:
        self._spike_deci_c = spike_deci_c
        self._previous: Frame | None = None

    def check(self, frame: Frame) -> list[str]:
        anomalies: list[str] = []
        previous = self._previous
        previous_flags = previous.flags if previous else 0

        if previous is not None:
            if frame.seq <= previous.seq:
                anomalies.append("device_reset")
            else:
                if frame.seq != previous.seq + 1:
                    anomalies.append("seq_gap")
                # A fault makes the temperature meaningless, so don't call it a spike.
                faults = FLAG_FAULT_RANGE | FLAG_FAULT_STUCK
                if (
                    abs(frame.deci_c - previous.deci_c) >= self._spike_deci_c
                    and not (frame.flags | previous.flags) & faults
                ):
                    anomalies.append("temp_spike")

        for bit, name in FLAG_NAMES.items():
            if frame.flags & bit and not previous_flags & bit:
                anomalies.append(f"{name}_on")
            elif previous_flags & bit and not frame.flags & bit:
                anomalies.append(f"{name}_off")

        self._previous = frame
        return anomalies


def log_stream(lines: Iterable[str], out: TextIO, spike_deci_c: int = DEFAULT_SPIKE_DECI_C) -> Counter:
    """Write valid frames from lines to out as CSV and return event counts."""
    writer = csv.writer(out, lineterminator="\n")
    writer.writerow(CSV_HEADER)

    monitor = AnomalyMonitor(spike_deci_c)
    stats: Counter = Counter()

    for line in lines:
        text = line.strip()
        if not text or text.startswith("#"):
            continue

        try:
            frame = parse_line(text)
        except ParseError as error:
            stats["bad_lines"] += 1
            print(f"warning: {error}", file=sys.stderr)
            continue

        anomalies = monitor.check(frame)
        stats["frames"] += 1
        stats.update(anomalies)

        writer.writerow(
            [
                datetime.now(timezone.utc).isoformat(timespec="milliseconds"),
                frame.seq,
                frame.device_ms,
                frame.raw,
                f"{frame.temp_c:.1f}",
                int(bool(frame.flags & FLAG_ALERT)),
                int(bool(frame.flags & FLAG_FAULT_RANGE)),
                int(bool(frame.flags & FLAG_FAULT_STUCK)),
                ";".join(anomalies),
            ]
        )
        out.flush()  # keeps the data if the run ends abruptly

        for anomaly in anomalies:
            print(f"[seq {frame.seq}] {anomaly} ({frame.temp_c:.1f} degC)")

    return stats


def serial_lines(port: str, baud: int) -> Iterator[str]:
    """Yield decoded lines from a serial port until interrupted."""
    import serial  # imported here so replay and tests work without pyserial

    with serial.Serial(port, baud, timeout=1) as link:
        while True:
            raw = link.readline()
            if raw:
                yield raw.decode("ascii", errors="replace")


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--port", help="serial port, for example COM3 or /dev/ttyACM0")
    source.add_argument("--replay", type=Path, help="read a saved capture instead of a port")
    parser.add_argument("--baud", type=int, default=115200, help="serial baud rate (default: 115200)")
    parser.add_argument("--out", type=Path, required=True, help="CSV file to write")
    parser.add_argument(
        "--spike",
        type=float,
        default=DEFAULT_SPIKE_DECI_C / 10,
        help="temperature jump between samples to flag, in degC (default: %(default)s)",
    )
    return parser.parse_args(argv)


def run(lines: Iterable[str], out_path: Path, spike_deci_c: int) -> Counter:
    stats: Counter = Counter()
    with out_path.open("w", newline="", encoding="utf-8") as out:
        try:
            stats = log_stream(lines, out, spike_deci_c)
        except KeyboardInterrupt:
            print("\nstopped")
    return stats


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    spike_deci_c = round(args.spike * 10)

    if args.replay:
        with args.replay.open(encoding="ascii", errors="replace") as capture:
            stats = run(capture, args.out, spike_deci_c)
    else:
        stats = run(serial_lines(args.port, args.baud), args.out, spike_deci_c)

    if stats:
        print("summary: " + ", ".join(f"{key}={value}" for key, value in sorted(stats.items())))
    print(f"wrote {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
