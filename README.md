# Pam’s Newfangled Workout

Independent v0.1 firmware for **ALM017 Pamela’s NEW Workout, PCB Rev 62**
(STM32F042K6T6). Eight outputs with clock ratios, gate/triangle/ramps/random,
width, phase, level and offset. Boots stopped; Stop returns all outputs to zero.

This repository contains only the Newfangled firmware, its source dependencies,
build/DFU tools, documentation and focused tests. It contains no stock firmware,
recovered application, reverse-engineering artifacts, downloaded vendor manuals,
prebuilt binaries or desktop simulator. No stock file is needed to build.

## Build

Requirements: Python 3, Make, Clang with the `arm-none-eabi` target, and LLVM LLD.
The build can use `ld.lld` or Rust’s bundled `rust-lld`. No Python packages,
network downloads, proprietary SDK or vendor firmware are required.

```sh
make test
make newfangled
```

Output: `build/newfangled/pnw.bin`, with ELF, map and build metadata alongside.
The build checks the 32 KiB flash and 6 KiB RAM target limits, including 1 KiB
stack headroom. Set `PNW_CC` and `PNW_LD` to override compiler/linker paths.
`CC` selects the host compiler used by the tests.

On macOS, installed Apple Clang can build this ARM target; Rust’s installed LLD
is detected automatically. On Linux, install Clang and LLD through your package
manager, or supply their paths. `dfu-util` is only needed for flashing.

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
The firmware does not use EEPROM, presets, external clock/CV assignments or expanders.

## Controls

- Turn to browse BPM and OUT1–OUT8.
- On BPM: click to edit tempo, turn to adjust, click to finish.
- On an output: click to browse parameters, turn to select one, click to edit its
  value, turn to adjust, click to return to parameter selection.
- Hold the encoder for half a second to return directly to output browsing.
- Start toggles playback. Hold the encoder and press Start to reset phase.

Output identity and parameter name/value use two large-text rows. Transport uses
a play triangle or stop square. See [firmware details](docs/firmware.md) for
defaults, abbreviations, timing limitations and the zero-output input diagnostic.

## Source layout

- `firmware/main.c`, `app.c`: application and transport integration.
- `firmware/voice.c`, `clock.c`: waveforms and rational phase timing.
- `firmware/ui.c`, `font.c`: controls, display and original bitmap font.
- `firmware/board.c`, `gate_bus.c`, `protocol.c`: peripheral and SPI/DAC handling.
- `firmware/startup.c`, `runtime.c`, `divmod.S`, `stm32f042.ld`: MCU runtime/linker.
- `tools/build.py`, `flash.py`: build and explicitly requested DFU writes.
- `tests/`: portable UI, engine and clock requirements.

The larger development workspace also has simulator and compiled-ARM interruption
tests; those research/development tools are intentionally outside this firmware-only
repository. The portable tests here do not emulate peripherals or measure real jitter.

## License and provenance

MIT; see [LICENSE](LICENSE). Firmware and font source are independently authored.
Board wiring was informed by reverse engineering, and the authors had prior
exposure to the stock implementation. This is not a clean-room provenance claim
or a guarantee of legal clearance. No ALM application code, binary, font or manual
is included. This is an unofficial project and is not affiliated with ALM.
