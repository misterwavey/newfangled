# Newfangled BIA controller v0.2

A separate firmware for Pamela’s NEW Workout ALM017 Rev62 that controls a Noise
Engineering **Basimilus Iteritas Alter** over CV and triggers. It does not replace
BIA’s own firmware. The general Newfangled image remains a separate build.

## Build and simulator

From this repository:

```sh
make bia
make test-bia
make bia-simulator-dev
```

Open <http://127.0.0.1:8766>. The server watches source changes and rebuilds the
workbench. No zip files or installation are involved. A reload preserves the
working patch and presets, with transport stopped. `make bia-simulator` produces
an offline `build/bia-simulator/index.html` containing the C WebAssembly engine,
OLED renderer and browser controls. There is no BIA audio emulation: the graphs
show outgoing CV and triggers. The actual BIA must be patched to hear results.

The firmware image is `build/bia/pnw.bin`; its ELF, map and build/source manifest
are alongside it. Requirements are the same Clang/LLD/Python/Make toolchain as
Newfangled. Tests additionally use Node. No stock firmware or vendor assets are
needed. Source is in `firmware/bia/`, browser files in `simulator/bia/`. The build
shares the independently developed board driver, font, sync estimator, CV filter,
EEPROM adapter and transactional preset journal with Newfangled.

## Patch the module

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

Leave BIA’s S/L/M and B/A/T switches at **Skin / Bass** initially. Start with its
seven parameter knobs low; choose a useful low base pitch using BIA’s Pitch knob.
Use scene auditioning to adjust the CV recipes to your module and taste. Its
knobs are offsets, not attenuators: moving them changes every recalled scene.
The fixed scene names KICK/SNARE/HAT/BASS/TOM/CLANG/WOOD/NOISE are editing labels
and draft starting points, not measured factory patches or guaranteed sounds.
Other switch settings can be used, but change the sound of all scenes.

BIA’s trigger is a rising-edge input, approximately 3 V threshold; OUT1 provides
nominal 0/5 V pulses. Parameter CV is nominal 0–5 V. Pitch is nominal 1 V/octave,
with 0–60 semitones available above the knob position. The Rev62 DAC is 10-bit;
physical voltage/pitch calibration remains unverified. Reference:
[manufacturer’s BIA manual](https://noiseengineering.us/manuals/basimilus-iteritas-alter/).

## The sequencer

There are 16 sixteenth-note steps: one loop is four master beats. Each step has:

- HIT ON/OFF, allowing explicit rests;
- scene 1–8;
- probability 0–100%;
- requested ratchets 1–8;
- accent 0–100%;
- FILL eligibility, independently of whether the normal HIT is enabled.

One accepted event captures all seven scene CV values together. Probability is
shared across that whole event. Ratchets retain the captured scene, accent and random variation. Fold/Harm
envelopes can retrigger on each strike or continue through the ratchet group. Disabled/failed steps produce no trigger and retain the previous
CV, including pitch; an existing envelope continues its decay. There are no independent random sequences competing for a
single sound. The fixed seed restarts on Start/reset, making a repeatable sequence
for identical settings and inputs; it does not automatically reset each loop.

The default pattern alternates kick, snare and hat-labelled scenes on eight of
sixteen steps. It is a demonstration, not a conventional drum kit with independent
voices. BIA is still a shared voice: retriggers and CV changes can affect a ringing
sound. Stop cancels all queued strikes and immediately zeros all outputs. It does
not mute BIA’s audio output or promise to silence its remaining decay.

SWING 0–50% alternates longer/shorter sixteenth-note steps while retaining the
pair’s total duration. Internal tempo is 10–300 BPM. External clock uses the same
1/2/4/8/12/16/24/48 PPQN estimator as Newfangled; Start arms it, lock starts playback,
loss cancels pending events and zeros outputs, and valid returning pulses reacquire.
RUN offers OFF, GATE and RESET. GATE requires manual arming plus a high Run input;
RESET restarts the pattern. Hold Program and press Start for manual phase reset.

## CV-before-trigger scheduling

DELAY is the desired CV lead time, 1–20 ms (default 3 ms). At an accepted step the
firmware changes the seven CV outputs with OUT1 low. A later tick raises OUT1.
PULSE is 1–10 ms (default 3 ms). The trigger schedule includes a low interval
between ratchets. At short step durations it clamps lead/pulse width and reduces
the effective ratchet count to preserve at least one sampled millisecond low/high.
The browser status shows the effective count. Requested settings are retained.

The minimum lead is one later DAC frame; physical transfer timing depends on the
board driver meeting its deadlines. This is a configurable settling allowance,
not an assertion that every BIA requires 3 ms. All strikes are delayed relative to
the musical step by that allowance. Synthesis is at 1 kHz; this is not an audio-rate
modulator or a measured low-jitter clock. Changing tempo/clock can shorten a current
step; reaching the next step cancels any unfinished old ratchets.

## Scene motion, accents and variation

Each scene has a **Fold envelope** and a **Harm envelope**: signed initial offsets
from -100 to +100% of the nominal 5 V output range, with independent 5–500 ms
linear decay times in 5 ms increments. The initial values are presented during
CV lead time; decay starts at the first trigger. They return to the captured
scene value, including accent and variation, with output clipped to 0–5 V.
RETRIG ON restarts both envelopes on every ratchet; OFF lets them continue.
Edits take effect at the next accepted event. Pitch is never enveloped.

Each step's ACC amount scales three signed scene mappings: FACC (Fold), HACC
(Harm) and DACC (Decay). A mapping of +20% and step accent of 50% adds nominal
0.5 V to that lane; negative mappings subtract. These are timbre/decay accents,
not an audio VCA or a guaranteed increase in perceived loudness.

VAR 0–100% perturbs the six timbre voltages once per accepted event, never pitch.
Each scene now sets its own maximum deviation for Decay, Harm, Fold, Morph,
Spread and Attack: DVAR/HVAR/FVAR/MVAR/SVAR/AVAR. These are percentages of the
nominal 5 V range, scaled by VAR. For example, FVAR 20% with VAR 50% permits
roughly +/-0.5 V. Zero locks that lane. New patches lock Attack and use smaller
Decay/Morph/Spread ranges to protect the basic voice. All ratchets retain the
same random draw. Values are clipped to the output range.

VARCV can be OFF, CV1 or CV2. The selected normalized input adds up to +/-100
percentage points to base VAR, clipped to 0–100. CV1 is unipolar and CV2 bipolar
under the current default mapping. An invalid/missing sample falls back to base
VAR. SWAP and the C1/C2 0 V/+5 V capture controls allow the same endpoint setup as
Newfangled; this build has its own saved calibration. No input voltage is inferred
from browser sliders on physical hardware. CV is sampled for variation when an
event starts, not used to keep rerolling a held scene.

## Hardware controls

Encoder/button capture and the detent decoder are shared with Newfangled:
1 kHz sampling continues during OLED work, with timestamped foreground delivery.
Contact bounce/incomplete paths re-anchor at the resting detent, so the next
complete notch does not inherit a partial count from a push. See the
[input handling notes](firmware.md) and `make test-controls` regression.

Home shows BPM, transport/lock status and the current step. Turn in browse mode:
Home -> STEP1..16 -> KICK..NOISE scene pages -> Home.

**Performance controls while playing:**

| Gesture | Action |
| --- | --- |
| Tap Start/Stop | Toggle transport when released |
| Hold Start/Stop for 300 ms | Apply the selected momentary effect |
| Turn encoder while held | Adjust that effect's strength |
| Click encoder while held | Cycle FILL -> ACCENT -> VAR |
| Release Start/Stop | Remove the effect and keep playing |
| Hold encoder first, then tap Start | Reset phase; consume both releases |

Stop, reset and external-clock loss cancel active effects and queued strikes.
Holding Start while stopped does not activate an effect. There is no double-tap
transport shortcut. Click Program on Home to open the performance page: turn to
choose an effect, click to edit its remembered strength, and hold to exit.

FILL overlays the base pattern without resetting phase. Only steps marked FILL
ON are eligible; the default mask selects steps 14 and 16, protecting the main
kick/snare positions. FILL uses levels **0–5**, one encoder click per level:

| Level | Effect on the default fill mask |
| --- | --- |
| 0 | No fill |
| 1 | Add step 14 |
| 2 | Add steps 14 and 16 |
| 3 | Both, with two strikes each |
| 4 | Both, with three strikes each |
| 5 | Both, with four strikes each |

With a custom mask, level 1 adds the first half (rounded up) of eligible disabled
steps, and level 2 adds all of them. Levels 3–5 apply two/three/four strikes to
eligible events, subject to the normal timing limits. Existing higher ratchet requests remain higher. ACCENT and VAR retain 0–100%
controls because they scale voltage contributions rather than select fill patterns.
Existing saved fill percentages map to their corresponding level without erasing presets.
Probability still applies. Release cancels remaining fill strikes; already
triggered sound and held CV cannot be undone. The underlying pattern and its
probability random sequence are preserved.

ACCENT temporarily adds its strength to the current programmed accent, capped
at 100%, using that scene's FACC/HACC/DACC mappings. VAR temporarily increases
variation using the scene's per-lane limits. Release returns these contributions
to their base amounts without rerolling the held event. A zero mapping/limit
stays locked. None of these effects changes pitch.

On a step, click Program to browse HIT, scene, PROB, RATCH, ACC and FILL. Click to
edit, turn, click to finish. Hold Program for 500 ms to exit directly to browsing.

On a scene, click Program to browse AUDITION, PIT, DEC, HARM, FOLD, MORPH, SPRD,
ATK, FENV, F MS, HENV, H MS, RETRIG, FACC/HACC/DACC and the six VAR limits.
PIT edits semitones; CV values, offsets and mappings edit percentages. Select
AUDITION and click again while stopped to strike once with the same motion and
scheduler. Start ends audition and zeros outputs; tap again to run. Audition
again after edits to hear them.

Hold Program on Home for global settings: BPM, VAR, DELAY, PULSE, SWING, SRC,
PPQN, RUN, VARCV, SWAP, C1/C2 calibration captures, PRESETS and ERASE MEM.
Hold again to leave. Preset controls have SLOT, SAVE and LOAD. Save/Load require
transport stopped/disarmed and audition ended. Power-on restores the last saved
slot, stopped; empty Load leaves the working patch untouched. Effect selection
and strengths are saved; an active momentary effect is never saved.

For a first motion patch, try a kick scene with FENV +20%, F MS 50, HENV +10%,
H MS 100, RETRIG ON, and AVAR 0%. Use a step ACC of 50% and scene mappings
FACC +10%, HACC +10%, DACC +5%. Then play and hold Start for a fill; click the
encoder while held to try the other effects. These are starting points for tuning
against your BIA knob positions, not measured sound recipes.

The browser exposes the same hardware buttons and OLED plus direct editors for
all 16 steps and 8 scenes. Clicking a scene name auditions it. Pitch editor values
are semitones; the other scene columns are volts. Their conversion is nominal.
The two CV sliders and clock/Run controls are virtual inputs, not connections to
the real module. Browser data uses `newfangled-bia-presets-v1` storage, separate
from general Newfangled. Export JSON for a portable backup.

## Storage and switching firmware

The BIA controller now stores its eight slots at **EEPROM 0x2000–0x3fff**.
Newfangled uses 0x0000–0x1fff. Each has two journal banks and its own last-saved
boot slot. Switching between the current EEPROM-aware builds preserves both.
The first explicit Save replaces stock data in that application's partition;
boot does not format storage. ERASE MEM requires stopped/disarmed transport and
clears only BIA's library, leaving Newfangled intact.

Before upgrading an older BIA internal-flash build, export its library using
`python3 tools/presets.py --bia --serial FFFFFFFEFFFF --export bia-before-eeprom.json`.
Old internal-flash presets are not automatically migrated, and the legacy DFU
tool cannot access EEPROM after upgrading. See [storage and migration limits](presets.md).

Build and flash with USB only, Eurorack power and expanders disconnected:

```sh
make bia
python3 tools/flash.py --write --serial FFFFFFFEFFFF --image build/bia/pnw.bin
```

The BIA settings payload remains version 2 and accepts v0.1 payloads/JSON.
Old patches get zero envelopes/accents and the original variation limits.
New saves retain effect selection/strengths, never an active momentary effect.
Hardware EEPROM JSON import/export is not implemented; browser JSON still
backs up simulated presets.

## Validation status

`make test-bia` covers event timing, all tempo/swing/lead/width/ratchet combinations
in its test matrix, shared probability and held rests, bounded/pitch-safe
variation, all external PPQN settings, loss/reacquisition, Run gate/reset,
audition, eight slots, interrupted save/compaction boundaries, foreign-format
write refusal, v0.1 migration and mixed-bank compaction, signed envelopes and
accents, protected variation, reversible fills, button ordering/hold/cancellation, native/WASM state/output/pixel parity, browser controls and JSON,
DFU identity/range guards and ARM image layout.

The simulator demonstrates controller logic, not MCU interrupt latency, DAC
settling, BIA’s input sampling or its sound. BIA v0.1 was reported working on the owner’s module. The **v0.2 additions have
not been bench-tested**; automated checks do not establish their sound or physical timing. First physical checks should measure OUT1
against a changing scene CV on a second scope channel, confirm the chosen lead,
then tune scenes using a consistent BIA knob/switch setup. Verify external clock
and Stop before relying on it in a performance.
