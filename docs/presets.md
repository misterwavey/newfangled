# Presets — v0.10

Eight slots store the complete patch: tempo, clock source/PPQN, Run mode, eight
output configurations and rhythm/probability/loop/swing/ratchet and random pitch/slew settings, all 48 CV routes, input swap and both calibration endpoint
pairs. Calibration belongs to the saved patch: loading an older slot restores
its calibration too. Current transport, phases, random state and menu position
are not stored. No automatic saves occur.

## Controls

1. Stop/disarm playback using Start. A low Run gate or missing EXT clock alone
   is insufficient: playback must be disarmed.
2. On BPM VIEW, hold the encoder for 500 ms to open SET.
3. Turn one step counterclockwise from SRC to **PRESETS**, then click.
4. **SLOT:** click, turn to 1–8, click to finish selecting the slot.
5. Turn to **SAVE** and click to overwrite that slot with the current patch.
   Wait for **SAVED**. **STOP FIRST**, **I/O ERROR**, **BAD DATA** and **NEWER DATA**
   mean the operation did not succeed.
6. **LOAD** recalls the selected slot. **EMPTY** leaves current settings intact.
7. Hold the encoder to return directly to BPM VIEW.

Power-on restores the most recently **saved** slot, always stopped. Loading a
slot does not change that boot choice; save it to make it the boot patch.
With no valid saves, factory defaults apply. An interrupted save recovers either
the previous valid patch or the completed new patch. Wait for completion before
removing power. This software recovery does not replace hardware brownout tests.

Saving performs flash programming/erase operations, so Save and Load require
playback stopped. External acquisition is restarted after an operation, avoiding
stale captured clock events. Re-arm Start after leaving the menu. Flash has finite
write endurance: explicit saves avoid wearing it on every encoder movement.

## Storage and updates

The STM32's first 24 KiB hold the application. The final 8 KiB,
**0x08006000–0x08007fff**, belong exclusively to Newfangled presets.
The linker and flash tool enforce the application boundary. Normal Newfangled
DFU application updates leave those pages alone. Flashing stock firmware, a
larger image or performing a mass erase can overwrite Newfangled presets.
Export a backup before changing firmware families.

**The external EEPROM is neither read nor written.** Stock presets/calibration
there are preserved. No option bytes or ROM bootloader locations are written.
The flash programming primitive executes from SRAM with interrupts masked while
each page/halfword operation is busy; startup copies its code and literals from
the application image. The primitive is range-gated by its callers and does no
mass erase. ST specifies 1 KiB pages for STM32F042x6 in its
[flash definitions](https://github.com/STMicroelectronics/stm32f0xx-hal-driver/blob/master/Inc/stm32f0xx_hal_flash_ex.h)
and implements the halfword programming sequence in its
[STM32F0 flash driver](https://github.com/STMicroelectronics/stm32f0xx-hal-driver/blob/master/Src/stm32f0xx_hal_flash.c).

Two 4 KiB banks alternate. Each has a versioned header and up to 14 append-only
256-byte records. A record contains slot/sequence, an explicit 230-byte
little-endian settings payload (v0.4/v0.5/v0.6/v0.7–v0.9 use 208/232/212/228 bytes), CRC32 and a final commit halfword. Uncommitted,
corrupt or semantically invalid records are ignored. When full, the inactive bank
is erased, seeded with the latest eight slots and sealed last. Its header includes
a CRC over the checkpoint records, so even a host transfer that writes the footer
early cannot activate an incomplete checkpoint. The previous bank remains intact
until the next replacement. Sequence/generation comparisons support wraparound.
Unsupported committed schemas block saving rather than erasing newer data.

CRC32 detects accidental corruption; it is not an authentication mechanism.
A damaged checkpoint can invalidate its bank and fall back to the older bank,
losing more recent saves. JSON backups remain advisable.

## Portable JSON backups

Browser **Export presets** downloads a readable JSON library; **Import presets**
merges its slots into the simulated library and restarts the session stopped.
Unsaved edits are excluded. Browser storage retains presets across reloads when
available; a downloaded JSON file is the portable backup. Session recordings
include their initial preset library for deterministic replay. The simulator uses
the actual C journal with an in-memory flash model; it does not emulate flash
latency, endurance or electrical power loss.

The same JSON can move between simulator and hardware. Put the module into DFU
mode, disconnect Eurorack power/expanders and connect USB only. From the repository:

```sh
# Read only; creates a new file, refusing to overwrite an existing backup.
python3 tools/presets.py --serial FFFFFFFEFFFF --export my-presets.json

# Read-only import preview, then explicit import.
python3 tools/presets.py --serial FFFFFFFEFFFF --import my-presets.json
python3 tools/presets.py --serial FFFFFFFEFFFF --import my-presets.json --write
```

The tool requires the storage-enabled Newfangled identity in the installed
application. It refuses stock/older firmware. Import validates the entire file,
merges slots, backs up the previous library to `build/backups/`, writes only the
inactive 4 KiB bank, then verifies the entire 8 KiB area including the unchanged
old bank. The backup's `boot_slot` becomes the new boot patch. It exports only
validated settings, never residual code or arbitrary bytes from the region.
Disconnect USB before reconnecting Eurorack power.

## Validation

Software checks cover the codec, eight slots, repeated bank rotations, schema
protection, stopped-only operations, every byte interruption boundary in an append
and a bank replacement, host/C/browser format compatibility, native/WASM parity,
boot restoration, session replay and DFU tool range/identity/readback behavior.
The built ARM image checks SRAM placement of the flash primitive, identity,
vectors, application bounds and absence of EEPROM functions.

Physical flash Save/Load, cold power-on, interrupted-save recovery and preservation
across an application update remain **unbench-tested** in v0.4. First bench pass:
save distinct tempi in slots 1 and 8, change settings without saving, power-cycle,
load both, confirm stopped outputs, then export/import and check again. Keep the
known-working restore image and export presets before testing interrupted power.
CV voltage calibration still awaits a reliable reference; presets do not change
that requirement.

## v0.4 compatibility

v0.5 reads both record schemas: v0.4 schema 1 / 208 bytes and v0.5 schema 2 /
232 bytes. Legacy patches gain STEPS=1, HITS=1, ROT=0, PROB=100%, LOOP=OFF.
They sound as before. Loading does not write a migration; explicitly saving
writes schema 2. The bank container and flash reservation are unchanged.
The extra 24 payload bytes pack each output's rhythm into three bytes:
STEPS−1 bits 0–4, HITS bits 5–10, ROT bits 11–15, PROB bits 16–22, LOOP bit 23.

v0.5 introduced JSON version 2, adding `rhythm` to each output. Version 1 JSON
imports remain supported and receive the same defaults. Use current tools:
v0.4 firmware/tools cannot interpret schema 2 records and block saves when they
encounter them. Export with v0.5 before downgrading; older firmware may restore
an older compatible save rather than the most recent v0.5 patch.

## v0.6 format and compatibility

New records use schema 3 with a 212-byte payload; JSON exports use version 3.
Readers still accept schemas 1/2 and JSON versions 1/2. Old patches gain SWING=0
and RATCH=1. Existing rhythm settings and CV calibration/routes are preserved.
The 8 KiB reservation, two banks, record size and commit/recovery scheme are
unchanged. Only explicitly saved/imported slots gain the new schema; loading
older slots never writes automatically.

Bytes 0–63 retain the previous global/output settings. Bytes 64–171 pack 48 CV
routes into 18 bits apiece: source bits 0–1, signed depth byte bits 2–9, signed
bias byte bits 10–17, in a continuous little-endian bitstream. Bytes 172–211 hold
five bytes per output: the previous three rhythm bytes, swing 0–50, ratchet 1–8.
This fits all features without shrinking the application or preset slot count.
Use current tools for import/export. Older firmware/tools cannot read schema 3;
export a backup before downgrading, which may restore older settings or defaults.

## v0.7 format and compatibility

Schema 4 uses a 228-byte payload, retaining v0.6 bytes 0–211 unchanged. Each
output adds one little-endian 16-bit word at 212 + output×2: scale index bits
0–2, root 0–11 bits 3–6, octave range minus one bits 7–8, slew in 20 ms units
0–100 bits 9–15. JSON version 4 adds each output’s `pitch` object with `scale`,
`root`, `octaves` and `slew_ms`; the last must be a multiple of 20.

Schemas/JSON versions 1–3 remain readable. Older patches default to scale OFF,
root C, one octave and slew zero, preserving their original output. All previous
rhythm, groove, CV routing and calibration settings remain intact. Loading never
writes a migration; explicit Save/Import writes the current schema. The two-bank
journal, eight slots, flash reservation and external EEPROM policy are unchanged.
Use v0.7 tools for export/import before downgrading: older firmware cannot read
schema 4 and may restore an older patch or defaults.

## v0.10 format and compatibility

Schema 5 uses 230 bytes: v0.7–v0.9 bytes 0–227 unchanged, followed by the muted
output mask at byte 228 (bit 0 OUT1 through bit 7 OUT8) and a zero reserved byte
at 229. JSON version 5 adds a boolean `muted` field to every output. Numeric or
string substitutes for boolean values are rejected. Schemas/JSON versions 1–4
remain readable and gain muted=false, preserving their previous output behavior.

Mute flags participate in Save/Load and Copy. Clear resets only the selected
output's mute flag along with its settings. Runtime phase/random/slew state is
still not persisted. Loading alone never writes a migration; explicit Save or
Import writes the current format. Eight slots, bank layout, 8 KiB reservation,
CRC/commit recovery and the external EEPROM policy are unchanged. Use current
tools before downgrading; old firmware cannot interpret schema 5 records.
