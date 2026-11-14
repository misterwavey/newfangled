# External clock and Run — v0.2

Newfangled now supports explicit internal/external source selection, matching
pulses per quarter note (PPQN), and OFF/GATE/RESET modes for the Run jack.
Settings remain in RAM: boot defaults are **INT, 24 PPQN, Run OFF, stopped**.
No EEPROM or stock preset storage is accessed.

## Settings and operation

From the BPM page in VIEW mode, hold the encoder for 500 ms to open SYNC settings.
Turn to choose SRC, PPQN, or RUN. Click to edit, turn to change, and click to finish.
Hold to return to the BPM page. Holding while editing first exits editing; open
SYNC settings from the normal BPM VIEW page.

- **SRC INT:** original internal clock; click BPM to change its tempo.
- **SRC EXT:** take tempo from Clk. The internal BPM setting is retained for
  returning to INT. Short clicks on the external BPM display do not edit tempo.
- **PPQN:** 1, 2, 4, 8, 12, 16, 24, or 48. Match the incoming source; changing this
  setting in INT does not reset playback. Changing it in EXT reacquires the clock.
- **RUN OFF:** ignore the Run jack.
- **RUN GATE:** Start arms playback; a high Run gate then permits playback and a
  low gate stops all outputs at zero. Start can disarm playback even with Run high.
  Raising Run restarts internal cycles or waits for the next clock edge in EXT.
- **RUN RESET:** a rising Run edge resets the armed transport. INT restarts on the
  next synthesis tick. EXT parks at zero and restarts on the next valid clock edge.
  Holding Run high does not repeatedly reset. Reset never starts disarmed playback.

Start toggles the armed state. The play triangle reflects actual playback, so
an armed module waiting for clock or Run permission shows a stop square. The BPM
footer reports EXT WAIT, EXT LOCK, EXT LOST, EXT ARMED, or RUN LOW. In EXT the
EXT ARMED confirms Start is armed while waiting for an edge; Run GATE low reports
RUN LOW. The browser Start button reads DISARM during this wait. The
large BPM value is the rounded measured tempo; interpolation retains fractional
tempo. Hold encoder plus Start remains the phase-reset gesture.

## Acquisition, tracking and loss

Two valid incoming edges are needed to measure the initial period. If already
locked when armed, playback begins on the next valid edge. That edge establishes
beat zero; pulses alone contain no independent bar-position information.

The estimator averages up to eight pulse intervals. A rational accumulator
advances the existing 60000-unit-per-beat engine without rounding external tempo
to an integer BPM. Pulse timing supplies a bounded phase correction: forward
advance changes by at most approximately 25% per tick while correcting drift;
waveforms never run backwards. Tempo changes settle over several input pulses.
All ratios, including /3 and /16, retain their relative cycles across beats.
Acquisition accepts a small margin around the nominal 10–300 BPM range. Very short
intervals are rejected as glitches without moving the last accepted timestamp;
very long intervals reacquire. PPQN must still match the actual source.

A missing clock stops all eight outputs at zero, without switching to INT. The
loss threshold is twice the estimated pulse interval plus 1 ms, with a 100 ms
minimum, checked on the next 1 ms tick. Consequently, loss takes roughly one
second at 120 BPM/1 PPQN, approximately 100 ms at 24 PPQN, and up to twelve seconds
at 10 BPM/1 PPQN. Clock return reacquires; armed playback then restarts its cycles.

## Hardware path and limits

Rev 62 routes Clk through an inverting input to PA10, and Run through an inverting
input to PA8. Positive clock edges at the jack therefore use falling PA10 edges.
Run uses both PA8 edges, normalized so true means jack high.

TIM2 runs freely at 1 MHz, with a 32-bit counter that wraps about every 71 minutes.
EXTI4–15 records timestamps and normalized event kinds in a 15-event usable FIFO.
The edge ISR has higher priority than synthesis and performs no SPI or waveform
work. SysTick consumes events and updates outputs at 1 kHz. A pending Run event
is processed before a coincident clock event. FIFO overflow latches TIMING FAULT.

These are **software interrupt timestamps**, not hardware input-capture timestamps.
Interrupt latency and short foreground critical sections still affect the recorded
edge time. Analog output changes remain quantized to 1 ms. Use clean Run pulses
at least 1 ms wide; clock-edge detection is independent of foreground display
polling. High-resolution timestamps do not establish a hardware jitter guarantee.

The register layout and IRQ number were checked against ST's
[STM32F042 device definitions](https://github.com/STMicroelectronics/cmsis-device-f0/blob/master/Include/stm32f042x6.h).
The [ALM manual](https://assets.busycircuits.com/docs/alm017-manual.pdf) provides the
physical jack names and PPQN terminology. No vendor source or manual assets are
included in this repository.

## Simulator

`make simulator-dev` uses the same source and Run-mode settings. Set the module
SRC to EXT and match its PPQN to the patch-bay clock generator. Enable External
clock, click module Start, then click Play to advance virtual time. Run gate drives
the Run jack in either GATE or RESET mode. The generator interpolates edge times
within each millisecond to avoid turning PPQN rounding into artificial tempo error.
CV assignment remains unimplemented. The simulator does not model ISR latency or
analog circuitry. v0.1 action sessions are rejected because Home-hold navigation
has changed.

## Bench acceptance and confirmed results

On 2026-11-14 the owner confirmed external lock at **1 PPQN**, source and OUT1
rising-edge alignment on the scope, unplug/reconnect behavior, Run GATE, and
Run RESET. An initial apparent inversion was resolved by comparing rising edges;
the source and output had different pulse widths. The tested image hash is in
[validation.json](validation.json). Other PPQN settings have software test coverage
but are not recorded as physically validated. Calibrated timing/jitter and an
exhaustive input/tempo matrix remain pending.

The following procedure documents the functional checks and further measurements:

Keep the known-good v0.1 image available. Build/flash from this standalone repo;
USB only during flashing, then disconnect USB and apply normal Eurorack power.

1. Verify INT Start/Stop, waveform edits and reset still behave as in v0.1.
2. Set EXT and 1 PPQN. Feed a known 120 BPM clock to Clk, arm playback, and
   compare OUT1 x1 gate against the incoming clock on the scope's other channel.
   It should acquire after two edges; OUT1 should follow the source at 500 ms/beat.
3. Check x2, /2 and a divided ramp. Vary source tempo, then test a matching higher
   PPQN setting. Use the actual PPQN emitted by the source, not its BPM number.
4. Remove the clock: verify all outputs go to zero within the documented timeout.
   Reconnect: verify acquisition and cycle restart. No silent internal fallback.
5. Check Run GATE low/high, Start disarm while Run high, and Run RESET with both
   a short trigger and a sustained gate, under INT and EXT.
6. Rotate through settings and outputs while clocked. Watch for TIMING FAULT and
   compare scope periods. Hardware latency and jitter measurements remain pending.

If Clk/Run appear inactive, `make newfangled-inputs` builds the zero-output monitor:
its clock count digit advances on captured Clk edges, and Run shows normalized
jack level. This isolates pin/interrupt behavior from synchronization settings.
