# Newfangled v0.2 desktop simulator

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
[synchronization](synchronization.md). CV assignments remain unimplemented.

Sessions record actions and a core hash for deterministic replay. The v0.2 session identifier intentionally rejects earlier navigation sessions
because click/hold behavior has changed. The simulator does not model oscillator
accuracy, SPI execution latency, DAC analog behavior or EEPROM.

`make test-simulator` checks native C/WebAssembly state, all eight output values,
framebuffers and output-change traces across 617 actions; independent timing
oracles check baseline transport and trace bounds. Frontend tests use a minimal
DOM/Canvas adapter and do not assert browser layout. `make test-firmware` checks portable UI, clock and engine requirements. These
tests do not emulate SPI or measure hardware latency.
