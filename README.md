# Embedded Sensor Telemetry Logger

[![CI](https://github.com/lobaoluwafemi/sensor-telemetry-logger/actions/workflows/ci.yml/badge.svg)](https://github.com/lobaoluwafemi/sensor-telemetry-logger/actions/workflows/ci.yml)

Bare-metal C firmware for the ATmega328P (Arduino Uno). It samples a TMP36
temperature sensor at a fixed 100 Hz, detects over-temperature and sensor faults,
and streams checksummed, timestamped telemetry over UART to a Python logger.

This is the software-polling half of a comparison with an FPGA hardware detector:
it sets the detection-latency baseline that the hardware version is measured against.

## Architecture

```text
 Timer1 (100 Hz)
    | hardware trigger, no software jitter
    v
   ADC ---> ADC interrupt: convert ---> detect ---> D8 alert pin
                                |
                                v
                          sample queue
                                |
                                v
                      main loop: format line ---> UART ring buffer ---> UART interrupt
                                                                            |
                                                                            v USB serial
                                                  host/telemetry_logger.py ---> CSV
```

- **Detection runs in the ADC interrupt**, so serial traffic can't delay the alert pin.
- **All logic in `lib/` is free of hardware access**, so the code that runs on the
  chip is the same code the unit tests run on a PC.
- **Everything uses integer maths**, because the ATmega328P has no FPU.

## Detection

| Flag | Meaning |
| --- | --- |
| `alert` | At or above 30.0 degC; clears at or below 29.0 degC (hysteresis) |
| `fault_range` | ADC reading outside the TMP36's plausible range |
| `fault_stuck` | ADC reading unchanged for 60 s |

Each change of alert or range-fault state needs 3 consecutive samples, which
rejects single-sample noise. Any flag drives the alert pin, so a failed sensor
fails safe. Settings are in [`src/config.h`](src/config.h).

## Telemetry format

```text
$T,<seq>,<ms>,<raw>,<deci_c>,<flags>*<checksum>
$T,42,420,700,251,1*78
```

The checksum is the XOR of the bytes between `$` and `*`. The logger uses it to
reject corrupt lines, and uses the sequence number to detect dropped samples and
board resets. The C and Python tests assert the same example line, so a format
change on one side fails the other side's tests.

## Hardware

| Uno pin | Connection |
| --- | --- |
| A0 | TMP36 output (middle pin) |
| 5V | TMP36 power |
| GND | TMP36 ground |
| D8 | Alert output, high on alert or fault (LED via a resistor, or scope probe) |
| D9 | Timing probe, high while the ADC interrupt runs |

Check the TMP36 pinout against its datasheet before powering it: reversed, it heats
up quickly.

## Getting started

Install [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html), then:

```bash
pio test -e native
```

```bash
pio run -e uno --target upload
```

Then log the telemetry, replacing `COM3` with your board's port:

```bash
pip install -r host/requirements.txt
```

```bash
python host/telemetry_logger.py --port COM3 --out run.csv
```

Run the host tests with:

```bash
pytest host/tests
```

## Continuous integration

GitHub Actions runs on every push. It runs the C unit tests on the host, builds the
firmware for the Uno and saves the `.hex` file, and runs the Python tests.

## Latency measurement

[`docs/measurement.md`](docs/measurement.md) has the oscilloscope procedure, the
predicted figures and the results table.
