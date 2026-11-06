import csv
import io
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import telemetry_logger as tl  # noqa: E402

# Also asserted by test/test_telemetry/test_main.c, which pins the wire format
# between the C firmware and this parser.
GOLDEN_LINE = "$T,42,420,700,251,1*78\n"


def make_line(seq, ms=None, raw=700, deci_c=250, flags=0):
    ms = seq * 10 if ms is None else ms
    body = f"T,{seq},{ms},{raw},{deci_c},{flags}"
    return f"${body}*{tl.checksum(body):02X}\n"


def frame(seq, deci_c=250, flags=0):
    return tl.Frame(seq, seq * 10, 700, deci_c, flags)


def rows(out):
    out.seek(0)
    return list(csv.DictReader(out))


def test_parses_golden_line_from_firmware():
    assert tl.parse_line(GOLDEN_LINE) == tl.Frame(42, 420, 700, 251, 1)


def test_negative_temperature():
    assert tl.parse_line(make_line(1, deci_c=-447)).temp_c == pytest.approx(-44.7)


@pytest.mark.parametrize(
    "line",
    [
        "",
        "garbage",
        "$T,42,420,700,251,1*79",  # wrong checksum
        "$T,43,420,700,251,1*78",  # corrupted field
        "$T,42,420,700,251*78",  # missing field
        "T,42,420,700,251,1*78",  # missing start
        "2,420,700,251,1*78",  # partial line after a reset
        "$T,42,420,700,251,1*7",  # truncated checksum
    ],
)
def test_rejects_bad_lines(line):
    with pytest.raises(tl.ParseError):
        tl.parse_line(line)


def test_monitor_quiet_for_normal_stream():
    monitor = tl.AnomalyMonitor()
    assert monitor.check(frame(0)) == []
    assert monitor.check(frame(1)) == []


def test_monitor_flags_sequence_gap():
    monitor = tl.AnomalyMonitor()
    monitor.check(frame(5))
    assert monitor.check(frame(8)) == ["seq_gap"]


def test_monitor_flags_device_reset():
    monitor = tl.AnomalyMonitor()
    monitor.check(frame(100))
    assert monitor.check(frame(0)) == ["device_reset"]


def test_monitor_reports_flag_edges_once():
    monitor = tl.AnomalyMonitor()
    monitor.check(frame(0))
    assert monitor.check(frame(1, flags=tl.FLAG_ALERT)) == ["alert_on"]
    assert monitor.check(frame(2, flags=tl.FLAG_ALERT)) == []
    assert monitor.check(frame(3)) == ["alert_off"]


def test_monitor_reports_flag_already_set_on_first_frame():
    assert tl.AnomalyMonitor().check(frame(0, flags=tl.FLAG_FAULT_RANGE)) == ["fault_range_on"]


def test_monitor_flags_spike_at_threshold_only():
    monitor = tl.AnomalyMonitor(spike_deci_c=50)
    monitor.check(frame(0, deci_c=250))
    assert monitor.check(frame(1, deci_c=299)) == []
    assert monitor.check(frame(2, deci_c=349)) == ["temp_spike"]


def test_monitor_ignores_jump_when_sensor_is_faulted():
    monitor = tl.AnomalyMonitor(spike_deci_c=50)
    monitor.check(frame(0, deci_c=250))
    assert monitor.check(frame(1, deci_c=-450, flags=tl.FLAG_FAULT_RANGE)) == ["fault_range_on"]


def test_log_stream_writes_csv_and_skips_noise(capsys):
    lines = [
        "# sensor-telemetry-logger boot\n",
        "\n",
        "2,420,700,251,1*78\n",  # partial first line, as after a board reset
        make_line(0, deci_c=251),
        make_line(1, deci_c=305, flags=tl.FLAG_ALERT),
    ]
    out = io.StringIO()

    stats = tl.log_stream(lines, out)

    assert stats["frames"] == 2
    assert stats["bad_lines"] == 1
    assert stats["alert_on"] == 1

    result = rows(out)
    assert [row["seq"] for row in result] == ["0", "1"]
    assert result[0]["temp_c"] == "25.1"
    assert result[1]["alert"] == "1"
    assert "alert_on" in result[1]["anomalies"]
    assert "[seq 1] alert_on" in capsys.readouterr().out


def test_replay_end_to_end(tmp_path, capsys):
    capture = tmp_path / "capture.log"
    capture.write_text(make_line(0) + make_line(5), encoding="ascii")
    out_path = tmp_path / "run.csv"

    assert tl.main(["--replay", str(capture), "--out", str(out_path)]) == 0

    with out_path.open(encoding="utf-8") as out:
        assert [row["anomalies"] for row in csv.DictReader(out)] == ["", "seq_gap"]
    assert "seq_gap=1" in capsys.readouterr().out
