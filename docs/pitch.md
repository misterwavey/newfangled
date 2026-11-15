# Random pitch and slew — v0.11

RANDOM outputs can now produce notes and glide between them. After RATCH, each
output’s parameter menu adds:

| Control | Values | Meaning |
| --- | --- | --- |
| SLW | 0–2000 MS, 20 ms increments | Linear glide duration between random targets |
| SCL | OFF, CHROM, MAJOR, MINOR, DORIAN, MAJPNT, MINPNT | Pitch scale |
| ROOT | C, C#, D, D#, E, F, F#, G, G#, A, A#, B | Lowest note / scale tonic |
| OCTS | 1–4 | Pitch span above ROOT |

Click to edit, turn to change, click to finish; hold to return directly to output
browsing. Defaults are SLW=0 / SCL=OFF / ROOT=C / OCTS=1. These preserve previous
behavior. Controls apply only to RANDOM; other shapes, gates and ratchets ignore
them. They currently have no dedicated CV destinations.

## Pitch selection

With SCL OFF, RANDOM retains its previous nominal 0–5 V modulation range and
ROOT/OCTS have no effect. Enabling a scale maps the random value across OCTS,
then selects the nearest allowed note, with exact ties going to the lower note.
ROOT transposes the entire range; note selection includes both its bottom tonic
and the tonic OCTS octaves above it.

LEVEL and OFFS act **before** note selection, so they change the spread/position
of notes without subsequently detuning them. Their existing CV routes can thus
alter the note sequence. Pitch is quantized before slew. Between notes, an active
glide intentionally passes through voltages that are not scale notes.

Scale degrees in semitones relative to ROOT:

- CHROM: all twelve notes.
- MAJOR: 0, 2, 4, 5, 7, 9, 11.
- MINOR (natural minor): 0, 2, 3, 5, 7, 8, 10.
- DORIAN: 0, 2, 3, 5, 7, 9, 10.
- MAJPNT: 0, 2, 4, 7, 9.
- MINPNT: 0, 3, 5, 7, 10.

The nominal conversion is **1 V/octave**, treating a full-scale 10-bit code of
1023 as 5 V. C at the bottom octave is the relative zero-volt reference. ROOT C#
starts one semitone above it; ROOT B starts eleven semitones above it. Four octaves
above B remain below 5 V. Simulator target labels use C0 for this zero reference;
your receiving oscillator determines the actual sounding octave and tuning.

This is a nominal model, **not output calibration**. Hardware voltage accuracy
has not been established. The 10-bit grid alone contributes up to about 3 cents
of ideal rounding error; offset, gain, analog nonlinearity and oscillator tuning
can add error. Software scale tests do not establish physical pitch accuracy.

## Slew and lifecycle

SLW=0 changes voltage immediately at each random step. SLW>0 makes a linear glide
from the current voltage to the new target over that many milliseconds. Both
upward and downward transitions take the selected time, regardless of interval.
If another target arrives first, the glide restarts from its current position.
This also works with SCL OFF for smooth, unquantized random modulation.

The first accepted step after Start/reset is immediate. Stop, clock loss, Run
closure force zero with no slew tail. Euclidean/probability rests hold quantized random pitch and let its glide finish; unquantized random still returns zero and its next accepted target is immediate. Use another output’s gate
pattern to articulate an oscillator: a pitch output falling to zero alone does
not silence the oscillator.

LOOP repeats the random targets and probability decisions. With slew spanning
several steps, the initial loop can have a settling transient; subsequent glides
remain continuous. Reset restores the same starting target/sequence. Pitch/slew
edits update the target without rerolling the pattern and clear any in-progress
glide, so the new setting takes effect immediately. Foreground drawing, exporting
or refreshing UI does not advance glide time; synthesis advances it once per ms.

## Quick patch

Choose OUT1 RANDOM, ratio X1, LEVEL=100, OFFS=0, STEPS=8, HITS=8, PROB=100,
LOOP=ON. Set SCL=MINPNT, ROOT=C, OCTS=2, SLW=0 for a repeating nominal pitch
sequence. Try SLW=100 MS for glide. Patch another output at the matching ratio/
pattern to the voice’s envelope or gate input. SAVE retains all settings.

Old v0.4–v0.6 presets load with pitch/slew disabled. See [preset migration](presets.md#v07-format-and-compatibility).

## Validation

Independent note-list tests compare every input code across every supported
scale, root and octave range to a nearest-note oracle. Tests cover endpoints,
monotonicity, LEVEL/OFFS ordering, nominal DAC conversion, non-RANDOM independence,
linear up/down glide timing, retargeting, maximum duration, rests, Stop/reset and
foreground refreshes that must not advance time. Native/WASM parity, browser
controls/session replay, preset power cycling, JSON validation and all previous
preset schema migrations exercise the complete path. Storage interruption tests
continue to apply.

No v0.7 image has been bench-flashed. First verify RANDOM / SCL OFF / SLW=0 still
behaves as before. Then test SCL=CHROM / ROOT=C / OCTS=1 / SLW=0: notes should
occupy nominal 0–1 V, with approximately 83.33 mV per semitone. Compare several
stable outputs against a reliable voltage reference and a tuned oscillator.
Test ROOT and octave changes, then glide and Stop-to-zero. A measured output
calibration pass is required before relying on tuning; none is silently invented
from the existing input calibration or stock EEPROM data.

## v0.11 rest behaviour

Quantized random now holds the last accepted pitch across rests, letting a glide finish. Stop still forces zero. Unquantized random retains zero-on-rest. See [pitch hold](expansion.md#pitch-holds-across-rests).
