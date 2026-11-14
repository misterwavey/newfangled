# Newfangled v0.8 desktop simulator

Run `make simulator-dev`, then open http://localhost:8765. Source changes rebuild
and reload the page; no unzip step is needed. `make simulator` also produces an
offline build/simulator/index.html with embedded WebAssembly.

Hardware and simulator use the same C engine, UI and font. All eight outputs have
independent ratios, gate/triangle/ramps/random shapes, width, phase, level and
offset. On an output, click to browse parameters, turn to select one, click to
edit its value, and click again to leave value editing. Hold encoder / Back (or
Escape) performs the hardware 500 ms hold to return to output browsing.
Output identity uses the large top row, with smaller VIEW/SELECT/EDIT labels. Transport uses a play triangle or stop
square, right-aligned on output pages and left-aligned on the BPM page. The lower row shows both parameter name and value in large text. Offset
is OFFS; waveform names are shortened to GATE/TRI/RISE/FALL/RAND.
See [module controls and limits](firmware.md). Click Play to advance
virtual time and Start to run the module. These are independent controls.

The output monitors sample virtual time at 1 ms during playback. Button gestures
advance virtual time internally; monitor samples can span those gestures. The OUT1
change ledger records actual C output codes/timestamps, retaining the latest 2048
changes. Long analog runs overwrite old ledger entries. Phase reset performs the
hardware encoder+Start gesture. External clock and Run patch-bay controls now drive the shared synchronization
engine. Hold Home in VIEW to select SRC EXT, matching PPQN and Run mode; see
[synchronization](synchronization.md). CV1/CV2 sliders now drive per-parameter routes configured through CV SETUP.
Use CV MON for raw and normalized input readings; see [CV routing](cv.md).

Sessions record actions and a core hash for deterministic replay. The v0.8 session identifier intentionally rejects earlier navigation sessions
because output navigation now includes rhythm parameters. The simulator does not model oscillator
accuracy, SPI execution latency, DAC analog behavior or EEPROM.

`make test-simulator` checks native C/WebAssembly state, all eight output values,
framebuffers and output-change traces across 896 actions; independent timing
oracles check baseline transport and trace bounds. Frontend tests use a minimal
DOM/Canvas adapter and do not assert browser layout. `make test-firmware` checks portable UI, clock, CV and engine requirements. These
tests do not emulate SPI or measure hardware latency.

Eight presets use the shared C journal in simulated flash. Browser local storage
retains saved presets when available. Power cycle / reset session restores the
last saved slot stopped; it does not erase presets. Export/import preset JSON
works with [hardware backup tools](presets.md). Session files include their initial
library; replay resets to that library before executing recorded actions.

Each output monitor shows its Euclidean pattern, current step, probability and
LOOP status. Filled dots are the underlying Euclidean hits; probability can mute
those hits, which is visible in the waveform trace. The simulator and hardware
share the PRNG sequence, so phase reset and LOOP have the same behavior.

SWING and RATCH appear after LOOP. Each scope shows swing and the effective /
requested ratchet count. Non-gate shapes show one effective cycle; ratchets only
affect gates. Fast swing cycles may have different effective pulse counts in
the long and short steps. See [groove controls](groove.md).

RANDOM outputs offer SLW, SCL, ROOT and OCTS after RATCH. Monitors show the
target note when quantization is enabled and playback is active; during slew
the current voltage lies between notes. C0 is the simulator’s relative zero-volt
reference, not a claim about the oscillator receiving the CV. Scopes use the
nominal 0–5 V output model, not measured hardware calibration. See [pitch](pitch.md).

CLEAR OUT follows OCTS, or turn one step left from RATIO. Click, choose YES and
click to silence/reset only that output. NO or Back cancels. Saved presets are
unchanged until SAVE; see [clear one output](firmware.md#clear-one-output--v08).
