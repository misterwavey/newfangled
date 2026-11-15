# Pam’s Newfangled Workout

Two independent firmware builds for **ALM017 Pamela’s NEW Workout, PCB Rev 62**
(STM32F042K6T6), each with its own browser simulator and eight-slot preset library.

| Build | Purpose | Guide |
| --- | --- | --- |
| **Newfangled v0.11** | Eight independently configurable clock, rhythm and modulation outputs | [Newfangled guide](docs/firmware.md) |
| **BIA controller v0.2** | A coordinated trigger-and-CV sequencer for Noise Engineering’s Basimilus Iteritas Alter | [BIA guide](docs/bia.md) |

Both images run on **Pam’s hardware**. The BIA build controls an external BIA
through patch cables; it does not replace BIA’s own firmware. Install one image
at a time. Switching between the current builds preserves both preset libraries.

The repository includes both applications, their shared source dependencies,
build/DFU tools, offline browser simulators, documentation and focused tests.
No stock firmware, recovered application, vendor assets or prebuilt binaries
are included. No stock file is needed to build.

## Choose a build

### Newfangled

Eight outputs with clock ratios, gate/triangle/ramps/random/sine/envelope shapes,
waveform skew, width, phase, level and offset. Features include external clock
and Run inputs, 80 CV routes, Euclidean rhythms, probability, repeatable random
patterns, swing, ratchets, held random pitch with scales/slew, AND/OR/XOR logic,
independent beat loops, per-output mute/copy/clear and eight full-patch presets.
Ratios span /512–X48, including fractional factors.

Start with the [controls and timing guide](docs/firmware.md) and
[v0.11 feature details](docs/expansion.md). Further guides cover
[external synchronization](docs/synchronization.md), [CV](docs/cv.md),
[rhythms](docs/rhythms.md), [swing/ratchets](docs/groove.md) and [pitch/slew](docs/pitch.md).

### BIA controller

A 16-step sequencer selects among eight editable seven-CV sound scenes, keeping
trigger probability and ratchets coordinated with the whole voice. It provides
CV-before-trigger lead time, scene audition, swing, external clock/Run,
Fold/Harm envelopes, step accents, per-scene variation limits and eight presets.
Momentary **FILL**, **ACCENT** and **VAR** effects add live performance control;
FILL has six discrete levels, 0–5.

| Pam output | BIA input |
| --- | --- |
| OUT1 | Trig |
| OUT2 | Pitch |
| OUT3 | Decay |
| OUT4 | Harm |
| OUT5 | Fold |
| OUT6 | Morph |
| OUT7 | Spread |
| OUT8 | Attack |

Scene names such as KICK, SNARE and HAT are editable starting recipes, not
independent drum voices. BIA remains one shared voice, and its knob/switch
positions affect every scene. See the [BIA patching, motion and performance guide](docs/bia.md).

## Build

Run commands from the repository root. Requirements: Python 3, Make, Clang with
the `arm-none-eabi` target, and LLVM LLD. The build can use `ld.lld` or Rust’s
bundled `rust-lld`. No Python packages, network downloads, proprietary SDK or
vendor firmware are required.

```sh
# General Newfangled firmware
make newfangled

# Dedicated BIA controller firmware
make bia
```

| Build | Flash image | Other artifacts |
| --- | --- | --- |
| Newfangled | `build/newfangled/pnw.bin` | ELF, map and source/build manifest alongside |
| BIA | `build/bia/pnw.bin` | ELF, map and source/build manifest alongside |

Both builds allow **32 KiB of application flash**. Presets live in external
EEPROM. The linker checks the 6 KiB RAM limit, reserving at least 1 KiB for stack.
Set `PNW_CC` and `PNW_LD` to override compiler/linker paths; `CC` selects the host
test compiler. On macOS, Apple Clang can build the ARM target and Rust’s installed
LLD is detected automatically. On Linux, install Clang and LLD or supply their paths.

## Browser simulators

Each workbench runs its application's real C engine, OLED renderer and font,
compiled to WebAssembly. It provides virtual controls, output monitors and
local preset/session persistence. Neither simulates MCU execution timing,
physical DAC accuracy or jitter; the BIA workbench does **not** emulate BIA audio.

| Workbench | Development command | Open | Offline build command | Offline page |
| --- | --- | --- | --- | --- |
| Newfangled | `make simulator-dev` | http://127.0.0.1:8765 | `make simulator` | `build/simulator/index.html` |
| BIA | `make bia-simulator-dev` | http://127.0.0.1:8766 | `make bia-simulator` | `build/bia-simulator/index.html` |

Development servers watch source changes, rebuild and reload compatible sessions.
Offline builds are self-contained HTML files with embedded WebAssembly and
JavaScript. No unzipping, hardware connection or flashing is required. Click
**Play** to advance virtual time, then **Start** to run the module.

Simulator builds need LLVM Clang with a `wasm32` backend plus `wasm-ld` or Rust’s
bundled `rust-lld`. Apple Clang usually lacks that backend; Homebrew LLVM is
detected at `/usr/local/opt/llvm` or `/opt/homebrew/opt/llvm`. Override paths with
`PNW_WASM_CC` and `PNW_WASM_LD`. See the [Newfangled simulator guide](docs/simulator.md)
and [BIA workbench guide](docs/bia.md#build-and-simulator).

## Flash either firmware

Install `dfu-util`. Put Pam’s module into STM32 DFU mode, disconnect Eurorack
power and expanders, and connect **USB only**. Inspect the device:

```sh
python3 tools/flash.py
```

Build and flash **one** of the following, replacing `YOUR_SERIAL` with the
reported device serial.

Newfangled:

```sh
make newfangled
python3 tools/flash.py --write --serial YOUR_SERIAL --image build/newfangled/pnw.bin
```

BIA controller:

```sh
make bia
python3 tools/flash.py --write --serial YOUR_SERIAL --image build/bia/pnw.bin
```

Disconnect USB before reconnecting normal Eurorack power. The tool replaces
application flash without writing option bytes. Keep a known-working restore image.

## Presets and switching builds

Each firmware has **eight independent preset slots**, stored in separate regions
of the Rev 62 SPI EEPROM:

| Application | EEPROM region |
| --- | --- |
| Newfangled | `0x0000–0x1fff` |
| BIA | `0x2000–0x3fff` |

Save/Load require stopped, disarmed transport. Power-on restores that
application's last saved slot, stopped. Loading an empty slot leaves the current
patch intact. There are no automatic saves on knob changes. BIA's ERASE MEM
command clears only BIA's library.

**First Save replaces stock EEPROM data in that application's partition.**
Current EEPROM-aware Newfangled and BIA builds preserve each other's libraries;
returning to stock firmware can overwrite them. Neither build interprets stock
presets or factory calibration.

Old custom presets in **internal flash are not automatically migrated**. Before
upgrading an older image, export with `tools/presets.py` (add `--bia` for BIA).
That tool is now legacy-only: ROM DFU cannot access external EEPROM, and hardware
EEPROM JSON import/export is not implemented. Browser JSON export/import still
backs up simulated libraries; it does not synchronize with hardware. Read the
[storage, migration and backup limitations](docs/presets.md) before upgrading.

## Hardware controls

Both builds use the rotary encoder, its Program push switch and Start/Stop.
The display uses **▶** while active and **❚❚** while stopped. Encoder capture is
shared and continues during display updates.

### Newfangled

- Turn to browse BPM and OUT1–OUT8. On BPM, click to edit tempo and click again to finish.
- On an output, click to browse parameters; turn to choose one, click to edit,
  turn to adjust, then click to return to parameter selection.
- Hold the encoder for 500 ms to return directly to browsing. Hold it on BPM VIEW
  to open clock source, PPQN, Run, CV/calibration and PRESETS settings.
- Output parameters also include CV setup/monitor, rhythm, swing, ratchets,
  pitch/slew, beat loops, logic, mute, copy and clear.
- Start arms/disarms playback. Hold the encoder first and press Start to reset phase.
  Missing external clock or a low Run gate does not disarm the module for saving.

See the [complete Newfangled controls](docs/firmware.md).

### BIA controller

- Turn to browse Home, STEP1–16 and the eight scenes. Click to browse/edit fields;
  hold the encoder for 500 ms to leave directly.
- On Home, click for performance effect/strength selection. Hold for global
  settings, including BPM, clock source, PPQN, Run and PRESETS.
- While stopped, select a scene's AUDITION field and click to hear that recipe.
- Tap Start to toggle transport **on release**. During playback, hold Start for
  300 ms to apply the selected momentary effect; release to return to the base pattern.
- While Start is held, turn the encoder to adjust strength and click it to cycle
  FILL → ACCENT → VAR. Hold the encoder first, then press Start, to reset phase.

See the [complete BIA controls and suggested first motion patch](docs/bia.md#hardware-controls).

## Tests and hardware validation

For both full suites, install Node.js in addition to the build/simulator tools:

```sh
make test test-bia
```

`make test` checks general Newfangled; `make test-bia` checks the BIA controller.
The suites include portable engine/UI tests, native C/WebAssembly comparisons,
OLED pixels, browser controls and preset formats. Shared tests cover encoder
click/turn ordering and both EEPROM journals on one simulated device, including
partition preservation and interrupted page writes. `make test-firmware` runs
Newfangled's host tests without building WebAssembly or invoking Node.

The owner has reported working Newfangled outputs, ratios/waveforms, Stop-to-zero,
reset, external clock/Run and ratchets, and working BIA sequencing/fills. The
click-then-rotate fix is hardware-confirmed. These are bench observations, not
complete validation of every current feature or image. **The new EEPROM storage
and preservation across firmware switching still need physical verification.**
Tests do not measure real interrupt latency, analog accuracy, DAC settling or
jitter. Output voltage/pitch calibration and CV-input calibration checks remain
pending. See [Newfangled validation](docs/validation.json),
[sync bench checks](docs/synchronization.md#bench-acceptance-and-confirmed-results),
[CV calibration](docs/cv.md#jack-mapping-and-calibration--bench-verification-needed)
and [BIA validation](docs/bia.md#validation-status).

## Source layout

| Area | Newfangled | BIA controller |
| --- | --- | --- |
| Application, engine and UI | `firmware/main.c`, `app.c`, `voice.c`, `ui.c` | `firmware/bia/` |
| Settings codec | `firmware/settings.c` | `firmware/bia/settings.c` |
| Browser workbench | `simulator/` | `simulator/bia/` |
| Simulator builder | `tools/build_simulator.py` | `tools/build_bia_simulator.py` |

Shared code includes `firmware/board.c`, `gate_bus.c`, `protocol.c`, `controls.h`,
`font.c`, `sync.c`, `cv.c`, `store.c`, `eeprom_store.c` and the MCU runtime/linker.
`tools/build.py` builds either image; `tools/flash.py` installs the chosen image;
`tools/serve_simulator.py` serves either workbench. `tests/` contains application
and shared regressions. `tools/presets.py` and `firmware/flash_store.c` are legacy
internal-flash utilities/source, not the current preset backend.

## License and provenance

MIT; see [LICENSE](LICENSE). Both applications and the font are independently
authored. Board wiring was informed by reverse engineering, and the authors had
prior exposure to the stock implementation. This is not a clean-room provenance
claim or a guarantee of legal clearance. No ALM application code, binary, font
or manual is included. This is an unofficial project, not affiliated with ALM
or Noise Engineering.
