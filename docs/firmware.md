# Pam’s Newfangled Workout v0.11

See [v0.11 expansion](expansion.md) for current waveform, CV, logic, loop and preset changes.

Eight independently configured unipolar outputs on ALM017 Rev 62. Boots stopped;
Stop parks every output at zero. This is an early, independently authored application
using the existing board SDK. No stock binary is needed to build it.

## Features

- Global tempo: 10–300 BPM; Start/Stop and phase reset.
- Internal/external clock source, 1–48 PPQN options, Run gate/reset input modes.
  See [synchronization settings and behavior](synchronization.md).
- Ratios per output: 30 choices, ÷512 through ×48, including fractional factors.
- Shapes: gate, triangle, rising ramp, falling ramp, stepped random, sine and envelope.
- Gate width: 1–99%; phase: 0–99%; level and positive offset: 0–100%.
- Nominal voltage range: 0–5 V; level plus offset clips at full scale.
- Independent CV1/CV2 routing, signed depth and bias for ten parameters per output;
  raw input monitor, mapping swap and 0 V/+5 V calibration (persisted with an explicit preset SAVE). See [CV guide](cv.md).
- Portable C engine, UI and manually authored font shared with the included browser simulator.

Defaults: OUT1 ×1 gate; OUT2 ×2 gate; OUT3 ×4 gate; OUT4 ÷2 gate;
OUT5 ÷4 gate; OUT6 ÷8 gate; OUT7 ×1 triangle; OUT8 ×1 rising ramp.
Gates start at 50% width, all levels at 100%, phases/offsets at zero.
Random is deterministic per output and changes once per cycle. Phase shifts apply
to gate/triangle/ramp position; random changes stay on the channel cycle boundary.

## Controls

Turn clockwise to browse Home → OUT1 → … → OUT8 → Home.
Encoder and button levels are sampled at 1 kHz in SysTick, independently of OLED
rendering. A timestamped 31-sample backlog preserves click/turn ordering while
the foreground is busy. Both applications use the shared detent decoder in
`firmware/controls.h`: complete paths generate one turn, bounce retraces cancel,
and incomplete paths re-anchor at the resting detent. Queue overflow reports a
timing fault. Startup and stopped preset operations discard their stale samples.
This replaces foreground-only input polling; verify the first notch after a
click in both directions on hardware after flashing.

On Home in INT mode, press the encoder to enter/leave tempo editing; turn to change BPM.
On an output, click the encoder to open parameter browsing; turn to choose Ratio,
Shape, Width, Phase, Level or Offset. Click to edit the selected value; turn to
change it. Click again to return to parameter browsing. Hold the encoder for
500 ms from either mode to return directly to output browsing. A hold leaves
changes applied and its release does not generate an extra click. Short encoder
clicks take effect on release so they are distinct from holds.
The top row shows OUT1–OUT8 in large text, plus VIEW/SELECT/EDIT mode. A filled
play triangle or pause symbol indicates transport, right-aligned on output pages
and left-aligned on the BPM page.
The lower row shows the parameter name and value together in large text, such as
RAT X2 or WIDTH 50%. Offset is abbreviated OFFS; waveform names are GATE, TRI,
RISE, FALL, RAND, SINE and ENV to fit the display. VIEW means turn to change outputs; SELECT
means turn to choose a parameter; EDIT means turn to change its value. Start arms/disarms transport in any page.
Hold the encoder and press Start to reset phase without changing transport;
this also leaves editing. Hold the encoder on BPM VIEW to open SET settings. Output parameter browsing
also includes CV SETUP and CV MON; see [CV controls and calibration](cv.md). Ratio changes restart only the affected channel;
tempo changes preserve phase. Start and Stop reset all channel phases.

## Build and flash

```sh
make newfangled
make test
python3 tools/flash.py --write --serial FFFFFFFEFFFF --image build/newfangled/pnw.bin
```

Flash with USB only and Eurorack power/expanders disconnected. Disconnect USB,
then reconnect normal Eurorack power to test. The build allows 32 KiB of application flash and checks the 6 KiB RAM limit,
reserving 1 KiB for stack. Presets use this firmware’s dedicated SPI EEPROM
partition; the other application’s library is preserved. See [presets](presets.md).
Eight full-patch presets use external SPI EEPROM; see [Save/Load and backups](presets.md).
MIDI and expander output are not implemented. Unsaved CV assignments/calibration are RAM-only.

## Timing and hardware acceptance

The interrupt advances the engine at 1 kHz. In INT, each channel adds BPM × multiplier
into an integer phase with period 60000 × divisor. This avoids accumulating
rounded beat periods. Edges and analog steps are quantized to 1 ms; maximum
nominal cycle rate is 240 Hz. EXT uses rational advances with bounded phase correction;
see [synchronization](synchronization.md). Gate width is clamped to at least one tick high and one tick
low at fast ratios. At these rates some width values therefore sound identical.
This is a modulation engine, not a high-resolution audio oscillator.

The board driver queues a complete eight-channel frame, sends all eight DAC
values under one SPI ownership interval, then latches them together. OLED data
releases the bus every 16 bytes. If an interrupt queues a sample during a
foreground DAC transfer, the sender drains that pending frame immediately after
unlocking, before returning to display rendering. This prevents an idle-bus queue
from waiting until the following tick. A scheduled sample still waiting at the next scheduled tick increments a counter
and latches TIMING FAULT on screen; it does not replay stale samples. Immediate
parameter/transport updates may intentionally coalesce without raising the warning.
An unchanged foreground refresh does not reset a scheduled sample's deadline.
The warning records this queue deadline condition, not all possible interrupt jitter.
Foreground display rendering runs with interrupts enabled. Control changes use a
short interrupt-masked synchronization section. Physical latency and jitter still
need measuring under the eight-output load.

The earlier single-output build was observed at approximately 501.2 ms period
and 250.8 ms high time at 120 BPM on the user's scope; probe scaling was unresolved.
This establishes neither calibrated tempo accuracy nor a jitter bound. Start/PB4
was measured LOW released/HIGH pressed; the driver normalizes button inputs.
The owner bench-confirmed v0.1 ratios and waveforms on all eight outputs,
Stop-to-zero and phase reset. v0.2 external acquisition at 1 PPQN, clock/output
rising-edge alignment, loss/reacquisition, Run GATE and Run RESET are also confirmed.
See the [validation scope](validation.json) and
[remaining checks](synchronization.md#bench-acceptance-and-confirmed-results).
Calibrated timing/jitter measurements remain pending.

## Physical input troubleshooting

```sh
make newfangled-inputs
python3 tools/flash.py --write --serial FFFFFFFEFFFF --image build/newfangled-inputs/pnw.bin
```

The separate diagnostic holds all outputs zero and shows physical PB4/PB5 levels,
independent of transport/debounce. The clock count digit advances on captured input edges, and Run reports normalized
jack level. It accesses no EEPROM. Return to the application by flashing build/newfangled/pnw.bin.

## Provenance

The font is manually authored and waveforms are generated by arithmetic. No
recovered application source or stock binary is linked. The existing SDK board
mapping was informed by reverse engineering; this is not a clean-room claim.

## Rhythms and probability — v0.5

Output parameter selection adds STEPS, HITS, ROT, PROB and LOOP after CV MON.
These filter complete waveform cycles; rests force zero even with offset set, except quantized random holds its last accepted pitch.
See [rhythms and repeatable random patterns](rhythms.md) for controls, timing,
reset/edit behavior and examples. CV routing still targets the six waveform
parameters. Presets store the new rhythm settings, with v0.4 load compatibility.

## Swing and ratchets — v0.6

SWING (0–50%) and RATCH (1–8) follow LOOP in each output menu. Swing stretches
the first cycle of each pair and shortens the next; ratchets subdivide accepted
GATE steps. See [controls, timing limits and examples](groove.md). Presets/JSON
retain both settings and can load v0.4/v0.5 data with neutral defaults.

## Musical RANDOM output — v0.7

SLW (0–2000 ms), SCL, ROOT and OCTS follow RATCH. They apply to RANDOM
outputs; gates/ratchets and other shapes retain their timing. Scale quantization
is off by default. Pitch uses nominal 1 V/oct conversion, pending physical
output calibration. See [random pitch and slew](pitch.md) for scales, control
ordering, glide lifecycle, nominal tuning limits and bench checks.

## Clear one output — v0.8

From an OUT's VIEW screen, click to select RATIO, then turn one step left to
**CLEAR OUT** (the last item, following COPY TO after OCTS). Click to open **CLEAR? NO**, turn
right to **YES**, and click to clear. Choosing NO leaves the output unchanged;
holding the encoder cancels and returns directly to VIEW. Confirmation always
starts at NO. In the simulator, Back/Escape performs the hold.

An empty output is silent: X1 GATE, WIDTH=50, PHASE=0, LEVEL=0, OFFS=0. Its six
CV routes are OFF with default depth/bias; Euclidean, probability, loop, swing,
ratchet and pitch/slew settings return to neutral defaults. Its phase, random
sequence and in-progress slew reset too. It remains silent under incoming CV.
Set LEVEL back to 100 to enable a plain X1 gate, then configure it as desired.

Clear can be used while playback runs. It does not restart the master clock or
other outputs and does not alter tempo, source, PPQN, Run mode, calibration or
saved preset slots. Clearing is an unsaved patch edit: explicitly SAVE afterward
if it should survive power-on. Loading a saved patch can restore the previous
output. The mute state is reset to OFF; LEVEL stays zero. See [mute output](#mute-output--v010) for current preset behavior.

Native tests cover cancellation, confirmation, each of eight output positions,
clearing while running, independent phase/random state, zero output under CV and
re-enabling an already-cleared output. Browser tests cover the real gestures,
session replay and explicit Save/power-cycle behavior. Physical v0.8 clearing
has not yet been bench-tested.

## Copy output — v0.9

From the **source** OUT's VIEW screen, click to select RATIO, turn left twice to
**COPY TO**, then click. Turn to select **TO OUT1–OUT8**, excluding the source
itself. Click to open **COPY? NO**, turn right to **YES**, then click to overwrite
the selected destination. The confirmation screen shows the destination in its
small second line while the large OUT number remains the source. NO cancels;
holding the encoder cancels from either stage and returns to VIEW. Simulator
Back/Escape performs the hold. Every confirmation starts at NO.

Copied settings include ratio, waveform, width, phase, level/offset, all six CV
routes, Euclidean steps/hits/rotation, probability/loop, swing/ratchets and
pitch/slew. These are the base settings and assignments, not a frozen snapshot
of CV-modulated values. Global calibration, tempo, source, PPQN, Run mode and
transport are unchanged, as are the source and six other outputs.

Copy works during playback and restarts only the destination's phase, rhythm and
slew. Even copying identical settings restarts it. The destination retains its own
per-output random seed: COPY TO copies configuration, not the source's current
phase, random-voltage sequence or sounding pitch. For random outputs, identical
settings can therefore produce different notes/probability decisions. Copying an
empty output silences the destination and removes its previous CV routes.

Copy returns to the source's VIEW screen. It is an unsaved patch edit: SAVE if
you want it retained after power cycling. Preset slots and format are unchanged
until an explicit Save; the current schema/JSON version stores mute along with the other output settings.

Native tests cover destination selection, self-exclusion, NO/YES/hold, all 56
ordered pairs, every configuration field and route, running source/other-output
isolation, repeated copies and copying an empty output. Browser tests cover the
complete UI, session replay and Save/power-cycle behavior. Physical v0.9 copying
has not yet been bench-tested.

## Mute output — v0.10

From an OUT's VIEW screen, click to select RATIO, then turn left **three** steps
to **MUTE**. Click to toggle **MUTE ON/OFF** immediately. No separate edit mode
or confirmation is needed. Turn to select another parameter, or hold to return
to VIEW. The parameter order after OCTS is MUTE, COPY TO, CLEAR OUT, preserving
the one-step-left Clear and two-step-left Copy shortcuts.

VIEW shows **MUTED** while a muted output is selected. An **M** beside the global
play/stop icon remains visible during parameter selection/editing, CV monitoring
and Copy/Clear confirmation. Simulator scope headings also show MUTED. The
transport icon continues to show playback state: muting is not Stop.

Mute forces the final output voltage to zero for every waveform, regardless of
LEVEL, OFFS, CV routing, pitch or slew. All latent processing continues: phase,
Euclidean position, swing, random/probability decisions and slew stay in time.
Unmuting immediately returns to the current value; it can join a gate or glide
partway through. It does not restart that output or affect the other seven.
All eight outputs may be muted while transport continues running. Start/Stop or
reset retains mute flags and otherwise follows its normal lifecycle.

SAVE retains the per-output flags; loading a preset restores them stopped. Old
presets default to all outputs unmuted. Copy inherits the source's mute flag;
Clear resets that flag OFF but leaves the output silent at LEVEL=0. No automatic
save occurs. Even if all outputs are muted, Save/Load still require playback
fully stopped/disarmed. See [preset format](presets.md#v010-format-and-compatibility).

Software tests compare muted and unmuted reference engines for phase, random,
probability, swing, slew and external-clock continuity, including live unmute.
They cover all waveforms/outputs, CV/OFFS masking, all-muted playback, OLED/scope
indicators, Copy/Clear interactions, old-format migration and saved mute flags.
Physical v0.10 mute/unmute behavior has not yet been bench-tested.
