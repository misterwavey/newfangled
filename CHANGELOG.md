# Changelog

## v0.3 — CV input bench verification pending

- Independent CV1/CV2 assignment for all six parameters on all eight outputs.
- Signed attenuation/inversion and range-scaled bias; base knob values remain intact.
- Input smoothing, ratio/shape hysteresis and phase-preserving CV ratio changes.
- CV SETUP, raw/normalized monitors, input swap, and volatile 0 V/+5 V calibration.
- Nonblocking ADC8/9 sampling; physical jack order, polarity and scaling need a bench pass.
- Matching simulator controls and 745-action native/WASM parity including CV routes.
- Existing clock/Run behavior retained; routes default OFF. No EEPROM operations added.

## v0.2 — core external-input behavior bench-confirmed

- Internal/external clock source and eight PPQN choices from 1 to 48.
- Home-hold synchronization settings; retains the user's internal BPM.
- Timestamp-based interval averaging and bounded forward phase correction.
- External clock loss parks all outputs at zero; no internal fallback.
- Run jack OFF, gated playback permission, or rising-edge reset.
- 1 MHz TIM2 timestamp counter, EXTI edge FIFO and overflow warning.
- Matching browser clock/Run inputs, armed-state indication and input diagnostics.
- Fractional-tempo, wraparound, loss/reacquisition, Run/reset, UI and native/WASM tests.

Owner bench-confirmed external acquisition at 1 PPQN, source/OUT1 rising-edge
alignment, clock-loss stopping/reacquisition, Run GATE, and Run RESET. The tested
image hash and scope are recorded in `docs/validation.json`. Other PPQN settings
have software test coverage; physical checks and calibrated jitter remain pending.
No EEPROM operations added.

## v0.1

- Eight outputs, independent ratios, five waveforms, width, phase, level and offset.
- Start/Stop, phase reset, readable two-line parameter UI and transport icons.
- Standalone source build and offline/live browser simulator.
- Output ratio/waveform behavior, Stop-to-zero and reset bench-confirmed by owner.
