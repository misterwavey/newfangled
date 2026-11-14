# Euclidean rhythms and random patterns — v0.5

Every output can filter its waveform cycles into a rhythm. The ratio sets the
step rate: at 120 BPM, X1 advances two steps per second, X2 four, /2 one.
Internal and external clock sources use the same pattern engine.

## Controls

On an OUT page, click to select parameters. After CV SETUP and CV MON come:

| Parameter | Range | Meaning |
| --- | --- | --- |
| STEPS | 1–32 | Pattern length in output cycles |
| HITS | 0–STEPS | Number of evenly distributed active steps |
| ROT | 0–STEPS−1 | Rotate the rhythm right by this many steps |
| PROB | 0–100% | Chance of playing each Euclidean hit |
| LOOP | OFF / ON | Repeat random decisions and random-wave values each pattern |

Click a parameter to edit, turn to change it, click to finish. Hold the encoder
for 500 ms to leave directly. Reducing STEPS clamps HITS and ROT to valid bounds.
Defaults are 1 step, 1 hit, zero rotation, 100% probability and LOOP OFF, preserving
v0.4 behavior. New rhythm controls do not yet have CV destinations.

STEPS=8, HITS=3, ROT=0 gives `X..X..X.`. ROT=1 gives `.X..X..X`.
The first step is index zero; positive rotation delays the first Euclidean hit.
HITS=0 or PROB=0 produces silence. HITS=STEPS, PROB=100 passes every cycle.

An accepted step plays the selected waveform for that cycle with the existing
width, phase, level and offset controls. A rest or failed probability decision
forces **zero volts**, including when offset is nonzero. This applies equally
to gates, triangle, ramps and random voltage. Width retains its existing meaning
for gates. Phase shifts the waveform within the cycle; it does not move the
Euclidean step boundary. Nonzero phase can therefore produce discontinuities
when a shaped wave meets a rest.

## Probability and repeatability

PROB is sampled once per step and held for the entire waveform cycle; there is
no extra random decision on each display update or synthesis tick. Each output
has its own fixed seed. Waveform randomness and probability have separate PRNG
states, so changing shape does not change the probability sequence.

With LOOP OFF, decisions and random voltages evolve continuously. Start and phase
reset reproduce the sequence from its beginning. LOOP ON additionally restarts
both random streams at the start of every STEPS-length pattern: the same hits
pass/fail and the same random-wave values recur each time. A loop can have fewer
actual hits than HITS, including none when probability removes them all.

For a repeating random CV sequence, choose RANDOM, STEPS=8, HITS=8, PROB=100 and
LOOP ON. Choose PROB=70 to make a repeatable sparse pattern. Set LOOP OFF for an
evolving sequence. A one-step locked RANDOM pattern repeats one voltage.
Seeds are deterministic per output; seed editing and a reroll control are not
implemented in this version. Reset repeats a pattern rather than rerolling it.

Manual Start/Stop, encoder+Start reset, external restart/reset and a preset load
restart pattern positions and random sequences with the transport. Editing a
rhythm parameter restarts that output's cycle/pattern immediately, so changes are
not bar-quantized. Other outputs keep their positions. Manual ratio changes also
restart that output's pattern. CV ratio modulation retains the current step and
retimes its fractional phase. Tempo changes keep pattern position.

## Presets and simulator

SAVE stores every output's five rhythm settings. Loading older v0.4 patches adds
pass-through defaults. See [preset compatibility and JSON backup](presets.md#v04-compatibility).
The simulator uses the same C engine and deterministic random streams, shows
pattern dots/current step below each scope and persists settings through presets.
Session exports include the starting preset library; older navigation recordings
are rejected by the v0.5 core identifier.

## Validation

Native tests cover all STEPS/HITS/ROT combinations up to 32, hit counts, balanced
spacing and rotation; gates and every other waveform honor rests including
nonzero offset. Tests check 0/100% probability, a broad distribution bound at 37%,
repeatable probability/random-voltage loops, channel independence, resets,
manual/CV ratio behavior and independent output editing. Native/WASM parity and
browser tests exercise rhythm navigation, saving/rebooting, JSON round trips and
legacy preset migration. Existing clock, sync, CV and storage tests still apply.

No v0.5 image has been bench-flashed. Suggested check: OUT1 GATE at 120 BPM, X1,
STEPS=8, HITS=3, ROT=0, PROB=100. Expect pulses at steps 1, 4 and 7, repeating every
four seconds. ROT=1 moves these to steps 2, 5 and 8. Try PROB=50 / LOOP ON, confirm
the same sparse pattern repeats, then reset and compare. Check RANDOM with all
hits and LOOP ON, Stop-to-zero and saved-patch power-on. Measure hardware timing
separately; the simulator does not establish physical jitter or ADC calibration.
