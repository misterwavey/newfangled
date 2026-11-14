# Pam’s Newfangled Workout

Independent v0.2 firmware for **ALM017 Pamela’s NEW Workout, PCB Rev 62**
(STM32F042K6T6). Eight outputs with clock ratios, gate/triangle/ramps/random,
width, phase, level and offset, plus external clock and Run/reset inputs. Boots
stopped; Stop returns all outputs to zero.

This repository contains only the Newfangled firmware, its source dependencies,
build/DFU tools, the offline browser simulator, documentation and focused tests. It contains no stock firmware,
recovered application, reverse-engineering artifacts, downloaded vendor manuals,
prebuilt binaries. No stock file is needed to build.

## Build

Requirements: Python 3, Make, Clang with the `arm-none-eabi` target, and LLVM LLD.
The build can use `ld.lld` or Rust’s bundled `rust-lld`. No Python packages,
network downloads, proprietary SDK or vendor firmware are required.

```sh
make test-firmware
make newfangled
```

Output: `build/newfangled/pnw.bin`, with ELF, map and build metadata alongside.
The build checks the 32 KiB flash and 6 KiB RAM target limits, including 1 KiB
stack headroom. Set `PNW_CC` and `PNW_LD` to override compiler/linker paths.
`CC` selects the host compiler used by the tests.

On macOS, installed Apple Clang can build this ARM target; Rust’s installed LLD
is detected automatically. On Linux, install Clang and LLD through your package
manager, or supply their paths. `dfu-util` is only needed for flashing.

## Browser simulator

```sh
make simulator-dev
```

Open http://127.0.0.1:8765. The server watches source changes, rebuilds the shared
C engine/UI to WebAssembly, and reloads the page while preserving compatible
sessions. No unzipping, flashing or hardware connection is involved.

Use `make simulator` to create `build/simulator/index.html`, a self-contained
HTML file with embedded WebAssembly, CSS and JavaScript that also works offline.
Click **Play** to advance virtual time, then **Start** to run the module.
The simulator uses the same firmware engine, UI and font; it does not emulate
MCU execution time, physical DAC behavior or jitter. See [simulator guide](docs/simulator.md).

Simulator builds need LLVM Clang with a `wasm32` backend plus `wasm-ld` or
Rust’s bundled `rust-lld`. Apple Clang usually lacks that backend; on macOS,
Homebrew LLVM is detected at `/usr/local/opt/llvm` or `/opt/homebrew/opt/llvm`.
Override paths with `PNW_WASM_CC` and `PNW_WASM_LD` if needed.

For the full test suite, also install Node.js:

```sh
make test
```

The suite compares native C and WebAssembly output values, pixels and traces,
checks controls/replay using a minimal DOM adapter, and tests development-server
routes/rebuild behavior. `make test-firmware` needs only the firmware host-test
requirements; it does not build WebAssembly or invoke Node.

## Flash

Put the module into STM32 DFU mode. Disconnect Eurorack power and expanders;
connect USB only. Inspect the connected device first:

```sh
python3 tools/flash.py
```

Then write the image, replacing `YOUR_SERIAL` with the reported serial:

```sh
python3 tools/flash.py --write --serial YOUR_SERIAL --image build/newfangled/pnw.bin
```

Disconnect USB before reconnecting normal Eurorack power. This replaces application
flash; the tool does not write option bytes. Keep a known-working restore image.
The firmware does not use EEPROM, presets, CV assignments or expanders. External
clock and Run/reset inputs are supported; see [synchronization](docs/synchronization.md).

## Controls

- Turn to browse BPM and OUT1–OUT8.
- On BPM: click to edit tempo, turn to adjust, click to finish.
- On an output: click to browse parameters, turn to select one, click to edit its
  value, turn to adjust, click to return to parameter selection.
- Hold the encoder for half a second to return directly to output browsing.
- Hold the encoder on the BPM VIEW page to open SRC, PPQN and RUN settings.
- Start arms/disarms playback. Hold the encoder and press Start to reset phase.
- EXT waits for the incoming clock; Run GATE also requires the Run jack high.
  A lost external clock stops outputs and never silently falls back to INT.

Output identity and parameter name/value use two large-text rows. Transport uses
a play triangle or stop square. See [firmware details](docs/firmware.md) for
defaults, abbreviations, timing limitations and the zero-output input diagnostic.

## Source layout

- `firmware/main.c`, `app.c`: application and transport integration.
- `firmware/voice.c`, `clock.c`, `sync.c`: waveforms, rational phase timing and external sync.
- `firmware/ui.c`, `font.c`: controls, display and original bitmap font.
- `firmware/board.c`, `gate_bus.c`, `protocol.c`: peripheral and SPI/DAC handling.
- `firmware/startup.c`, `runtime.c`, `divmod.S`, `stm32f042.ld`: MCU runtime/linker.
- `tools/build.py`, `flash.py`: build and explicitly requested DFU writes.
- `simulator/`: browser interface, WebAssembly bridge and output monitors.
- `tools/build_simulator.py`, `serve_simulator.py`: offline build and live development.
- `tests/`: portable UI/engine/clock, native/WASM parity, browser-control and server tests.

The tests do not measure real interrupt timing, analog output accuracy or jitter.
The user bench-confirmed v0.1 output ratios/waveforms, Stop-to-zero and reset.
For v0.2 the owner also confirmed external clock acquisition at 1 PPQN,
source/OUT1 rising-edge alignment, clock loss/reacquisition, Run GATE and Run RESET.
See the [validation record](docs/validation.json) and
[remaining bench checks](docs/synchronization.md#bench-acceptance-and-confirmed-results).
Other PPQN settings have software coverage; calibrated hardware timing remains unmeasured.

## License and provenance

MIT; see [LICENSE](LICENSE). Firmware and font source are independently authored.
Board wiring was informed by reverse engineering, and the authors had prior
exposure to the stock implementation. This is not a clean-room provenance claim
or a guarantee of legal clearance. No ALM application code, binary, font or manual
is included. This is an unofficial project and is not affiliated with ALM.
