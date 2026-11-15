# v0.11: waveform, rhythm and modulation expansion

This release implements the five additions discussed after the stock-207 comparison.
It is an independent implementation, not an emulation of stock parameter encodings
or a claim of stock timing parity. Build with `make newfangled`; the browser uses
exactly the same C application (`make simulator-dev`).

## Pitch holds across rests

With SHAPE RAND and SCL enabled, an empty Euclidean step or probability skip holds
that output's last accepted pitch. An existing glide finishes, then holds. Before
the first accepted pitch, the output is zero. Manual Start/reset, individual output
clear/copy reset, and an independent beat-loop restart clear the held pitch.
Stop, lost external clock and mute still force the physical output to zero.
Muting continues to advance the internal pitch/glide, so unmute rejoins it.
Unquantized random and other waveforms still return zero during rests.

Use a separate gate output to articulate an oscillator/envelope: holding pitch
only controls voltage, not whether a voice sounds. Pitch voltages remain nominal;
physical CV and output calibration require measurement.

## Waveforms and WIDTH

SHAPE adds SINE and ENV after RAND. SINE uses an independently calculated integer
sine approximation, not a copied waveform table. Against an ideal cosine-start
wave it differs by fewer than six DAC codes at 120 BPM/50% width in software
checks; the DAC, update rate and analog circuitry impose additional limits.
ENV has a brief linear attack and a curved release; WIDTH sets its release length.
It is a repeating clocked envelope, not a Run-input-triggered ADSR.

WIDTH now skews TRI and SINE: 50% is symmetric, smaller widths peak earlier,
larger widths peak later. Gate width retains its existing meaning. Rise, fall and
random retain their existing behaviour; SLW remains specific to random.

## Logic between outputs

After OCTS, find BEATS, LOGIC and WITH. LOGIC offers OFF, AND, OR and XOR;
WITH chooses output 1–8. Nonzero raw voltage means true. A true result is a full
5 V nominal gate, false is zero. Select OFF to restore the original waveform.

All eight raw output values are sampled before applying any logic or mute. Thus
muting the partner does not remove it from the internal logic signal, two-way
connections do not recurse, and output order does not change the result. Self
selection is permitted: AND/OR reproduce that output's boolean state and XOR
produces zero. Logic does not feed the pitch/glide state. Stop overrides logic.

## More CV destinations

CV SETUP / DEST now includes HITS, ROT, PROB and SWING after the original six
fields. Each of the 80 assignments has independent OFF/CV1/CV2, DEP and BIAS.
Existing additive modulation, filtering, discrete-value hysteresis and base-value
editing apply. HITS clamps to STEPS, ROT to STEPS minus one, PROB to 0–100 and
SWING to 0–50. CV never overwrites the saved base values. Invalid CV falls back to
base settings. The ACTUAL CV screen and normal parameter views show effective
values; entering EDIT shows the base value.

These modulation changes preserve running phase and the random generator, rather
than restarting an output on each CV sample. Membership/probability is reevaluated
against the current step's existing chance draw; no extra random draw occurs just
because a CV value changes. Swing changes take effect on the current timing span.
Other rhythm/logic/pitch controls are still manual; this is not all-parameter CV.

## Ratios and independent beat loops

There are 30 ratios spanning /512 to X48, including X2/3, X3/2, X4/3, X3/4,
X6/5, X12/5 and X24/5. Original ratio IDs and their initial menu positions are
unchanged for old patches. Additional choices follow X16:

`/32 /64 /128 /256 /512 X2/3 X3/2 X4/3 X3/4 X6 X12 X24 X32 X48 X5 X6/5 X7 X12/5 X24/5`

BEATS selects OFF or 1–128 master beats. When enabled, that output restarts its
waveform, Euclidean position, random/chance generators, swing parity and held
pitch at that interval, regardless of output ratio or STEPS. Each output has its
own interval. BEATS takes precedence over LOOP; with BEATS OFF the existing LOOP
switch repeats random/chance sequences at the Euclidean pattern length as before.
A short beat loop may intentionally restart a slow output before it completes a
cycle. Beat counting follows internal or locked external clock advancement.

The 1 kHz engine remains unchanged. At X48 / 300 BPM a cycle is about 4.17 ms;
waveforms have very few samples there. Ratchets are capped to retain at least one
sample high and low, so high multipliers cannot also deliver eight distinct
ratchets. This extends musical timing choices, not analog bandwidth or proven
jitter performance.

## Presets and compatibility

All new settings participate in Save/Load, output Copy/Clear and browser JSON.
JSON exports use version 6; older JSON versions import with beat loops/logic OFF
and the four new CV routes OFF. Old ratio and waveform IDs are unchanged.

Firmware reads existing schema 1–5 records in 256-byte-record banks without
writing on boot. New saves use schema 6: a 334-byte payload in a 384-byte record.
The bank container advances to version 2, with nine records in each 4 KiB bank;
eight preset slots and the 8 KiB flash reservation remain unchanged. The first
Save migrates all occupied slots into the inactive bank before committing it.
Power-cut tests cover every byte boundary during this migration and normal
append/compaction. The old bank remains intact until replacement commits.

Use v0.11 tools for new JSON and DFU imports. Export a JSON backup before a firmware
downgrade: older firmware cannot read container version 2 and may restore an older
bank or defaults. It refuses saves when it detects that newer committed container.
Browser action sessions use a new v0.11 identifier; export patches rather than
replaying older navigation sessions.

## Validation

`make test` passes native firmware tests, native/WebAssembly state and pixel parity,
browser UI/replay tests, JSON/DFU tooling tests and ARM artifact checks.
`tests/test_expansion.c` checks pitch hold/glide, sine against a mathematical
oracle, all ratio/shape bounds, fractional timing, independent beat resets, all
56 distinct output pairings for each logic truth table, CV continuity and new
preset fields. Address/undefined-behaviour sanitizers pass for the expansion and
storage suites. Storage tests inject interruption at every byte boundary during
append, compaction and migration from a complete eight-slot legacy bank.

The release image has not been flashed or bench-tested. Test physical pitch hold,
SINE/ENV and skew, logic gates, CV modulation and beat-loop alignment before
relying on it live. Scope timing measurements remain outstanding.
