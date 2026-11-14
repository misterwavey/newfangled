# Pam’s Newfangled Workout v0.4

Eight independently configured unipolar outputs on ALM017 Rev 62. Boots stopped;
Stop parks every output at zero. This is an early, independently authored application
using the existing board SDK. No stock binary is needed to build it.

## Features

- Global tempo: 10–300 BPM; Start/Stop and phase reset.
- Internal/external clock source, 1–48 PPQN options, Run gate/reset input modes.
  See [synchronization settings and behavior](synchronization.md).
- Ratios per output: ÷16, ÷8, ÷4, ÷3, ÷2, ×1, ×2, ×3, ×4, ×8, ×16.
- Shapes: gate, triangle, rising ramp, falling ramp, stepped random.
- Gate width: 1–99%; phase: 0–99%; level and positive offset: 0–100%.
- Nominal voltage range: 0–5 V; level plus offset clips at full scale.
- Independent CV1/CV2 routing, signed depth and bias for each output parameter;
  raw input monitor, mapping swap and 0 V/+5 V calibration (persisted with an explicit preset SAVE). See [CV guide](cv.md).
- Portable C engine, UI and manually authored font shared with the included browser simulator.

Defaults: OUT1 ×1 gate; OUT2 ×2 gate; OUT3 ×4 gate; OUT4 ÷2 gate;
OUT5 ÷4 gate; OUT6 ÷8 gate; OUT7 ×1 triangle; OUT8 ×1 rising ramp.
Gates start at 50% width, all levels at 100%, phases/offsets at zero.
Random is deterministic per output and changes once per cycle. Phase shifts apply
to gate/triangle/ramp position; random changes stay on the channel cycle boundary.

## Controls

Turn clockwise to browse Home → OUT1 → … → OUT8 → Home.
On Home in INT mode, press the encoder to enter/leave tempo editing; turn to change BPM.
On an output, click the encoder to open parameter browsing; turn to choose Ratio,
Shape, Width, Phase, Level or Offset. Click to edit the selected value; turn to
change it. Click again to return to parameter browsing. Hold the encoder for
500 ms from either mode to return directly to output browsing. A hold leaves
changes applied and its release does not generate an extra click. Short encoder
clicks take effect on release so they are distinct from holds.
The top row shows OUT1–OUT8 in large text, plus VIEW/SELECT/EDIT mode. A filled
play triangle or stop square indicates transport, right-aligned on output pages
and left-aligned on the BPM page.
The lower row shows the parameter name and value together in large text, such as
RATIO X2 or WIDTH 50%. Offset is abbreviated OFFS; waveform names are GATE, TRI,
RISE, FALL and RAND to fit the display. VIEW means turn to change outputs; SELECT
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
then reconnect normal Eurorack power to test. The build reserves 24 KiB application flash and 8 KiB preset flash, and checks the
6 KiB RAM limit, reserving 1 KiB for stack. No EEPROM operations are linked.
Eight full-patch presets use internal flash; see [Save/Load and backups](presets.md).
MIDI and expander output are not implemented. Unsaved CV assignments/calibration are RAM-only.

## Timing and hardware acceptance

The interrupt advances the engine at 1 kHz. In INT, each channel adds BPM × multiplier
into an integer phase with period 60000 × divisor. This avoids accumulating
rounded beat periods. Edges and analog steps are quantized to 1 ms; maximum
nominal cycle rate is 80 Hz. EXT uses rational advances with bounded phase correction;
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
