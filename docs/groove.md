# Swing and gate ratchets — v0.6

Each output gets **SWING** and **RATCH** after LOOP in its parameter menu.
Click to edit, turn to adjust, click to finish; hold to exit directly.
Defaults SWING=0 / RATCH=1 preserve the previous timing and waveforms.
These controls are per output, persist in presets and currently have no CV routes.

## Swing

SWING ranges from 0 to 50%, expressed as the **delay of alternate steps relative
to one straight step**. It is not the percentage occupied by the long step.
For a straight step duration T, the first step lasts T×(1+SWING/100), the second
T×(1−SWING/100). Their combined length remains 2T, so the output keeps its average
ratio to the master clock.

At 120 BPM / X1, T=500 ms. SWING=33 gives intervals 665/335 ms, and SWING=50 gives
750/250 ms. Step starts are 0, 665, 1000, 1665, 2000 ms at 33% swing, subject to
the existing 1 ms sampling resolution. Even when the first step is a Euclidean
rest, the second step retains its delayed time.

Swing affects all waveforms: gates, triangle and ramps stretch with their step;
random voltage stays held for that step. Euclidean rotation and probability
still operate once per step. Alternation continues across pattern boundaries,
so an odd-length pattern takes two loops to repeat its complete timing shape.
A locked probability/random-value pattern still repeats its values each loop.
Internal and external clock sources share this scheduling.

## Ratchets

RATCH requests 1–8 evenly spaced pulses inside each accepted **GATE** step.
Probability is decided once for the entire step. A rest or failed probability
check mutes all its pulses. Ratcheting does not change the output ratio, advance
the Euclidean pattern faster or reroll probability per pulse.

At 120 BPM / X1 / SWING=0 / RATCH=4, rising edges are 0, 125, 250, 375 ms inside
each 500 ms step. Swing stretches/squeezes the pulse spacing along with its step.
WIDTH applies within each pulse; PHASE shifts the pulse train within the step.
LEVEL/OFFS retain their existing meaning. Use OFFS=0 for a zero-volt gate baseline.
Triangle, ramps and random voltage ignore RATCH and retain one waveform per step.

The engine updates outputs at 1 kHz. To retain at least one sampled high and low
per pulse at steady rates, the effective count is capped at
`floor(step duration / 2 ms)`, with a minimum of one. WIDTH is constrained as in
the previous gate engine to retain sampled highs/lows. At 300 BPM / X16 /
SWING=50 / RATCH=8, the long step can fit eight pulses, while the short step is
limited to three. The simulator shows **effective/requested** count per output.
The effective count follows tempo/clock correction and can differ between steps;
actual hardware pulse timing still needs measurement. Saved RATCH remains the
requested value, so slowing playback restores the available subdivisions.

## Lifecycle and presets

Start/Stop, transport reset and external restart reset swing to the long first
step and restart the rhythm/random sequence. Editing SWING or RATCH also restarts
that output's pattern immediately; other outputs retain their positions. These
edits are not quantized to a beat or bar. Tempo changes preserve position, and
CV ratio modulation preserves the fractional position within the current long
or short step. Manual ratio changes restart the output.

SAVE includes both settings. v0.4/v0.5 presets and JSON backups load with neutral
groove defaults. The packed v0.6 format retains all eight slots and leaves stock
Current presets use partitioned EEPROM; see [preset compatibility](presets.md#v06-format-and-compatibility).

## Validation

Tests check swing pair alignment across all ratios, several tempi and swing
amounts, exact steady-clock edge examples, gate pulse counts at extreme WIDTH,
fast-rate caps, non-gate independence, per-step probability and reset/Stop behavior.
External clock tests check swing plus ratchet edges and loss-to-zero. Native/WASM
parity, browser controls/session replay, preset power cycling and v0.4/v0.5
migration cover the complete path. Existing storage power-cut checks still apply.

No v0.6 image has been bench-flashed. Suggested check: OUT1 GATE, 120 BPM, X1,
STEPS=1, HITS=1, PROB=100, PHASE=0, OFFS=0. Try SWING=33 / RATCH=1 and verify
alternating 665/335 ms intervals. Set SWING=0 / RATCH=4 and verify four pulses
per 500 ms step. Then combine both, Stop/reset, save and power-cycle. The
simulator verifies software behavior, not physical jitter or analog pulse quality.
