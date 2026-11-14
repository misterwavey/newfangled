# CV routing — v0.3

Every output parameter has an independent CV assignment: **48 routes** covering
Ratio, Shape, Width, Phase, Level and Offset on all eight outputs. A route selects
OFF, CV1, or CV2, with signed depth and bias. All routes boot **OFF**, so v0.2
output and synchronization behavior remain the defaults. No EEPROM is accessed.

## Controls

From an output's parameter-selection view, turn to **CV SETUP** after Offset;
click to open it. You can also turn anticlockwise twice from Ratio to reach it.
Turn to select a control, click to edit, turn to change, then click to finish:

- **DEST:** the parameter whose assignment you are editing.
- **SRC:** OFF, CV1, or CV2.
- **DEP:** -100% through +100%. Negative values invert modulation; zero removes
  the input contribution while retaining the selected route's bias.
- **BIAS:** -100% through +100% of the parameter's range, added to its base value.
- **ACTUAL:** read-only view of the currently applied parameter value.

Hold the encoder for half a second to return directly to output browsing. Every
parameter keeps its own route; selecting another DEST does not overwrite others.
OFF ignores both depth and bias and returns to the knob value.

The normal parameter page shows the effective value and a CV1/CV2 indicator when
routed. Clicking to edit shows **BASE**, so turning changes the underlying knob
setting rather than writing over a live modulated value. Hold/back behavior and
Start/Stop are unchanged.

**CV MON** is available beside CV SETUP, and in the Home settings menu. Turn or
click to select CV1/CV2; the large line shows normalized percent and the small
`R` number is the raw ADC count. A dash means no valid sample; CAL? means the
stored calibration span is too small. Monitoring refreshes at 20 Hz.

## Modulation model

The effective value is the base knob value, plus the normalized input multiplied
by depth and the parameter's range, plus the range-scaled bias; the result is
clipped to the parameter limits.

CV1 is normalized from 0 to +100%; CV2 from -100% to +100%. For example:

- LEVEL base 0, SRC CV1, DEP +100%, BIAS 0: 0 to +5 V input spans level 0–100%.
- LEVEL base 50%, SRC CV2, DEP +50%, BIAS 0: bipolar input spans level 0–100%.
- LEVEL base 100%, SRC CV1, DEP -100%, BIAS 0: rising input decreases output level.
- RATIO base x1, SRC CV1, DEP +50%: input moves through the upper ratio choices.

The CV offset is called BIAS to distinguish it from the output's own OFFSET
parameter. Ratios and shapes are quantized choices, with 0.1-bin hysteresis
around the half-bin threshold to prevent boundary chatter. CV-controlled ratio
changes preserve fractional cycle position, quantized to 1/1024 cycle; they do
not retrigger the cycle on every input change. Shape selection changes immediately
at the current cycle position. Unrouted manual ratio edits still restart that
channel, as in v0.2.

Inputs have a roughly 8 ms first-order smoothing filter. Parameters are updated
at 250 Hz, plus immediate control-setting updates; waveform/DAC updates remain
at 1 kHz. This is modulation-rate control, not audio-rate FM. Phase modulation
acts on gate/triangle/ramp position; stepped-random values still change at the
channel's cycle boundary. Missing ADC samples or invalid calibration bypass the
route to its base value, including ignoring its bias. Stopping overrides all CV
and output offsets: every output goes to zero.

## Jack mapping and calibration — bench verification needed

The Rev 62 pin facts are ADC8/PB0 and ADC9/PB1. **Their physical jack order,
polarity and voltage scaling have not been bench-verified for this driver.**
Do not treat a nominal percent display as a calibrated voltage measurement.
Defaults assume ADC8=CV1 and ADC9=CV2; CV SWAP can reverse the mapping. Both
increasing and decreasing ADC response are supported by endpoint calibration.

Calibrate with playback stopped and routes OFF. Hold the encoder on BPM VIEW
to open SET. The first three entries remain SRC, PPQN and RUN. CV entries follow:

1. **CV MON:** patch a stable voltage into CV1 and observe which raw input changes.
   If it appears under CV2, set **SWAP ON**, then check again. Turn in the monitor
   to compare the two inputs; hold to leave.
2. Apply a known **0 V** to CV1. Select **C1 0V**, wait at least 100 ms for the
   signal/monitor to settle, then click to capture the raw value.
3. Apply a known **+5 V** to CV1. Select **C1 +5**, wait, and click to capture.
4. Repeat **C2 0V** and **C2 +5** for CV2. Negative CV2 values are extrapolated
   around the measured zero; a negative reference is not needed for calibration.
5. Verify CV1 reads approximately 0–100% across 0 to +5 V. Verify CV2 reads zero
   at 0 V and follows negative/positive input. Then enable a route and test it.

Capture uses the latest raw reading; the displayed calibration number is the
stored endpoint, not a continuously changing input. A span below 64 ADC counts
is rejected to avoid extreme gain/noise amplification. ADC readings must be valid
before a capture will take effect. Use a held gate/DC source, not a running clock,
for the reference levels. Calibration and all routes are RAM-only and reset on
power cycling; saving settings is a later milestone.

Nominal starting endpoints are CV1 zero=0/+5=4095, CV2 zero=2048/+5=4095.
In the simulator, these match the idealized sliders. Swapping inputs or changing
calibration also affects simulation, allowing inverted/raw hardware responses to
be tested. Nominal volts beside the sliders describe the idealized source, not a
claim about an uncalibrated physical module.

## Driver and validation

The board service alternates ADC8/9 single conversions, caching each channel at
500 Hz. It checks completion on each SysTick and never waits for a conversion in
the synthesis path. A conversion stalled for over 20 ticks invalidates cached
samples rather than driving modulation from stale data. Initial ADC calibration
and readiness checks still happen during board startup.

Register flags were checked against the official
[STM32F042 device definitions](https://github.com/STMicroelectronics/cmsis-device-f0/blob/master/Include/stm32f042x6.h).
No vendor application source, calibration data or EEPROM layout is used.

`make test` includes signed gain/bias, calibration direction and input swap,
invalid-input bypass, clipping, hysteresis, phase-preserving ratio changes,
all-route parameter bounds and Stop-to-zero requirements. Native C and WASM are
compared across the actual CV UI gestures, input changes, all 48 effective values,
route settings, framebuffers and output traces. The browser interaction tests
exercise the existing sliders against the shared engine. Software tests do not
establish physical ADC mapping, volts, modulation latency or hardware jitter.
