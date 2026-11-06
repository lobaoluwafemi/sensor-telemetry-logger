# Measuring detection latency

This procedure measures the time from a temperature crossing the threshold to
the alert pin responding. It gives the software-polling baseline to compare with
the FPGA hardware detector.

**Status: not yet measured.** The figures under "Predicted" come from the design,
not from a scope. Fill in the results table after running this on real hardware.

## Equipment

- Arduino Uno running this firmware
- Two-channel oscilloscope
- Function generator (the TMP36 itself changes far too slowly to give a sharp edge)

## Setup

1. Disconnect the TMP36 from A0.
2. Set the function generator to a 1 Hz square wave, 500 mV low and 1000 mV high.
   **Check this on the scope before connecting it.** The input must stay between
   0 V and 5 V. These levels read as 0.0 degC and 50.0 degC, either side of the
   30.0 degC threshold.
3. Connect the generator output to A0 and its ground to the Uno's GND.
4. Connect scope CH1 to A0 and CH2 to D8 (alert), with both probe grounds on GND.
5. Trigger on the rising edge of CH1.

## Measurements

### Detection latency: A0 rising edge to D8 rising edge

The edge arrives at a random point in the 10 ms sample period, so the delay
varies from edge to edge. Use the scope's persistence or averaging mode, or
record at least 20 edges, and note the minimum, maximum and mean.

### Sample period: D9 rising edge to next D9 rising edge

Move CH2 to D9. This confirms the actual sample rate.

### Interrupt execution time: width of the D9 high pulse

This is the CPU time spent on each sample, including conversion to temperature
and detection.

## Predicted

With 100 Hz sampling and `DEBOUNCE_SAMPLES = 3`:

| Contribution | Time |
| --- | --- |
| Waiting for the next sample | 0 to 10 ms |
| Two more samples for debounce | 20 ms |
| ADC conversion (13 cycles at 125 kHz) | about 0.1 ms |
| **Predicted detection latency** | **about 20.1 to 30.1 ms** |

Debounce dominates. With `DEBOUNCE_SAMPLES = 1` the prediction falls to about
0.1 to 10.1 ms, at the cost of no longer rejecting single-sample noise. Measuring
both shows the trade-off.

## Results

| Quantity | Min | Mean | Max | Samples |
| --- | --- | --- | --- | --- |
| Detection latency, debounce = 3 | | | | |
| Detection latency, debounce = 1 | | | | |
| Sample period | | | | |
| Interrupt execution time | | | | |

Scope and probes used:

Date measured:
