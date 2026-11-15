# Presets — v0.11

See [v0.11 expansion](expansion.md) for current waveform, CV, logic, loop and preset changes.

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

Saving and loading require stopped/disarmed playback. EEPROM writes use the
shared SPI bus and take milliseconds; external clock acquisition is restarted
after an operation. No automatic saves occur on knob changes.

## EEPROM storage and updates

The application now has the full **32 KiB MCU flash**. Presets use the Rev62's
64 KiB SPI EEPROM, independently partitioned:

| Application | EEPROM addresses | Library |
| --- | --- | --- |
| Newfangled | 0x0000–0x1fff | Eight slots, two 4 KiB banks |
| BIA controller | 0x2000–0x3fff | Eight slots, two 4 KiB banks |
| Unallocated | 0x4000–0xffff | Not accessed by either library |

Each firmware reads/writes only its own partition. Switching between these two
EEPROM-aware images preserves both libraries and their independent boot slots.
Normal DFU application flashing does not write EEPROM. BIA's ERASE MEM command
clears **only BIA's eight slots**; it preserves Newfangled.

Stock presets/calibration in reclaimed EEPROM pages are deliberately replaced.
No stock layout, factory calibration or preset migration is used. First boot
reads existing data without formatting it; with no valid custom library it uses
defaults. **The first explicit Save initializes that application's partition.**
That is the point at which stock data there is overwritten. Returning to stock
firmware does not restore stock presets; stock firmware may also overwrite ours.
Use only the EEPROM-aware custom builds when preserving both custom libraries.

The existing transactional journal is retained: CRC-protected 384-byte records,
334-byte payloads, sequence numbers and a final commit marker. Each bank has its
own header, checkpoint CRC and final seal. Logical bank clearing writes FF to
the inactive EEPROM bank; the active bank remains untouched until replacement
is committed. Records and bank metadata occupy separate 128-byte pages, so an
interrupted write cannot share a page with an earlier record. The driver splits
writes at page boundaries, waits for completion and the adapter verifies every
write by reading it back. Unsupported committed schemas still block saving.
CRC is corruption detection, not authentication. Physical brownout behavior
still needs bench testing.

## Upgrading from internal-flash presets

**Existing internal-flash custom presets are not automatically copied to EEPROM.**
Before installing an EEPROM build, use the old firmware's DFU export:

```sh
python3 tools/presets.py --serial FFFFFFFEFFFF --export pnw-before-eeprom.json
# If the installed firmware is BIA instead:
python3 tools/presets.py --bia --serial FFFFFFFEFFFF --export bia-before-eeprom.json
```

Retain that JSON and the old firmware if you need its presets. Current images
may use the former flash preset area as code; do not rely on its contents after
updating. The legacy tool can import the JSON only while the corresponding old
internal-flash firmware is installed. It does not transfer EEPROM presets.
Recreate/save required patches in the new build; a hardware EEPROM JSON transfer
interface is not implemented yet.

## Portable JSON and USB limitations

Browser Export/Import still saves and restores simulated libraries, using the
same settings payload and JSON schemas as before. Browser persistence is local
to the simulator; it does not synchronize with the module.

STM32 ROM USB DFU accesses MCU memory, **not the external SPI EEPROM**. The existing
`tools/presets.py` is now a legacy internal-flash tool and explicitly refuses new
EEPROM identities before accessing the old flash area. Do not use a raw DFU
upload of that area as an EEPROM backup. Hardware EEPROM export/import is a
separate follow-up; for now use the module's eight Save/Load slots per firmware.

## Validation

`make test-eeprom` runs both real C journals and the real partition adapter
against one shared 64 KiB EEPROM model. It covers eight-slot independence,
alternating firmware saves, bank replacements, boot-slot restoration, own-library
clear, range rejection, and interrupted writes that corrupt the entire target
page. It also checks that the legacy USB tool refuses EEPROM images and that
only EEPROM-aware application images can use all 32 KiB of MCU flash.

Existing codec, interrupted-journal, browser/native/WASM and UI tests remain.
ARM artifact checks prove matching EEPROM identities and linked EEPROM drivers,
with no internal-flash programming routine. **EEPROM persistence is not yet
bench-validated.** First check Save/Load and cold restart in each build, then
switch builds twice and confirm that both libraries survive. Test interrupted
power only after ordinary persistence works.

## Historical internal-flash format notes

The following version notes describe the earlier internal-flash releases. Their
settings payload compatibility still applies; flash reservation and EEPROM
preservation statements below refer to those historical builds only.

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

## v0.11 storage migration

See [expansion compatibility](expansion.md#presets-and-compatibility). New payload bytes 0–229 preserve schema 5, bytes 230–301 contain four additional packed 18-bit CV routes per output, and bytes 302–333 contain each output’s little-endian 16-bit beat interval, logic selector and zero-based partner. New bank container version 2 uses 384-byte records (CRC at 376, commit at 382). Firmware reads old 256-byte banks and migrates occupied slots transactionally on first Save.
