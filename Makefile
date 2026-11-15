CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Werror -pedantic
.PHONY: all newfangled newfangled-inputs test clean
all: newfangled
newfangled:
	python3 tools/build.py
newfangled-inputs:
	python3 tools/build.py --inputs
build:
	mkdir -p build
build/test-ui: tests/test_ui.c firmware/ui.c firmware/font.c firmware/voice.c firmware/sync.c firmware/cv.c firmware/settings.c firmware/store.c firmware/ui.h firmware/sync.h firmware/cv.h firmware/settings.h firmware/store.h firmware/preset_format.h firmware/font.h firmware/voice.h include/pnw_board.h | build
	$(CC) $(CFLAGS) -Iinclude -Ifirmware tests/test_ui.c firmware/ui.c firmware/font.c firmware/voice.c firmware/sync.c firmware/cv.c firmware/settings.c firmware/store.c -o $@
build/test-clock: tests/test_clock.c firmware/clock.c firmware/clock.h firmware/gate_bus.c firmware/gate_bus.h | build
	$(CC) $(CFLAGS) -Ifirmware tests/test_clock.c firmware/clock.c firmware/gate_bus.c -o $@
build/test-engine: tests/test_engine.c tests/oled_stub.c firmware/voice.c firmware/app.c firmware/sync.c firmware/cv.c firmware/settings.c firmware/store.c firmware/ui.c firmware/clock.c firmware/font.c firmware/voice.h firmware/app.h firmware/sync.h firmware/cv.h firmware/settings.h firmware/store.h firmware/preset_format.h firmware/ui.h firmware/clock.h firmware/font.h | build
	$(CC) $(CFLAGS) -Iinclude -Ifirmware tests/test_engine.c tests/oled_stub.c firmware/voice.c firmware/app.c firmware/sync.c firmware/cv.c firmware/settings.c firmware/store.c firmware/ui.c firmware/clock.c firmware/font.c -o $@
test-firmware: build/test-ui build/test-clock build/test-engine
	./build/test-ui
	./build/test-clock
	./build/test-engine
clean:
	rm -rf build

.PHONY: simulator simulator-dev test-firmware test-simulator
simulator:
	python3 tools/build_simulator.py
simulator-dev:
	python3 tools/serve_simulator.py
test-simulator: simulator
	python3 -m unittest discover -s tests -p test_simulator.py
	node tests/simulator_browser.mjs
	python3 -m unittest discover -s tests -p test_simulator_dev.py
test: test-firmware test-simulator

build/test-sync: tests/test_sync.c tests/oled_stub.c firmware/voice.c firmware/app.c firmware/sync.c firmware/cv.c firmware/settings.c firmware/store.c firmware/ui.c firmware/clock.c firmware/font.c firmware/voice.h firmware/app.h firmware/sync.h firmware/cv.h firmware/settings.h firmware/store.h firmware/preset_format.h firmware/ui.h firmware/clock.h firmware/font.h | build
	$(CC) $(CFLAGS) -Iinclude -Ifirmware tests/test_sync.c tests/oled_stub.c firmware/voice.c firmware/app.c firmware/sync.c firmware/cv.c firmware/settings.c firmware/store.c firmware/ui.c firmware/clock.c firmware/font.c -o $@
.PHONY: test-sync
test-sync: build/test-sync
	./build/test-sync
test-firmware: test-sync

.PHONY: test-build
test-build: newfangled
	python3 -m unittest discover -s tests -p test_build.py
test: test-build

build/test-cv: tests/test_cv.c tests/oled_stub.c firmware/voice.c firmware/app.c firmware/sync.c firmware/cv.c firmware/settings.c firmware/store.c firmware/ui.c firmware/clock.c firmware/font.c firmware/voice.h firmware/app.h firmware/sync.h firmware/cv.h firmware/settings.h firmware/store.h firmware/preset_format.h firmware/ui.h firmware/clock.h firmware/font.h | build
	$(CC) $(CFLAGS) -Iinclude -Ifirmware tests/test_cv.c tests/oled_stub.c firmware/voice.c firmware/app.c firmware/sync.c firmware/cv.c firmware/settings.c firmware/store.c firmware/ui.c firmware/clock.c firmware/font.c -o $@
.PHONY: test-cv
test-cv: build/test-cv
	./build/test-cv
test-firmware: test-cv

build/test-store: tests/test_store.c firmware/store.c firmware/settings.c firmware/store.h firmware/settings.h firmware/preset_format.h firmware/ui.h firmware/cv.h firmware/voice.h firmware/sync.h | build
	$(CC) $(CFLAGS) -Iinclude -Ifirmware tests/test_store.c firmware/store.c firmware/settings.c -o $@
.PHONY: test-store
test-store: build/test-store
	./build/test-store
test-firmware: test-store

.PHONY: test-presets
test-presets:
	python3 -m unittest discover -s tests -p test_presets.py
test: test-presets

build/test-rhythm: tests/test_rhythm.c tests/oled_stub.c firmware/voice.c firmware/app.c firmware/sync.c firmware/cv.c firmware/ui.c firmware/clock.c firmware/font.c firmware/voice.h firmware/app.h firmware/ui.h | build
	$(CC) $(CFLAGS) -Iinclude -Ifirmware tests/test_rhythm.c tests/oled_stub.c firmware/voice.c firmware/app.c firmware/sync.c firmware/cv.c firmware/ui.c firmware/clock.c firmware/font.c -o $@
.PHONY: test-rhythm
test-rhythm: build/test-rhythm
	./build/test-rhythm
test-firmware: test-rhythm

build/test-groove: tests/test_groove.c tests/oled_stub.c firmware/voice.c firmware/app.c firmware/sync.c firmware/cv.c firmware/ui.c firmware/clock.c firmware/font.c firmware/voice.h firmware/app.h firmware/ui.h | build
	$(CC) $(CFLAGS) -Iinclude -Ifirmware tests/test_groove.c tests/oled_stub.c firmware/voice.c firmware/app.c firmware/sync.c firmware/cv.c firmware/ui.c firmware/clock.c firmware/font.c -o $@
.PHONY: test-groove
test-groove: build/test-groove
	./build/test-groove
test-firmware: test-groove

build/test-pitch: tests/test_pitch.c tests/oled_stub.c firmware/voice.c firmware/app.c firmware/sync.c firmware/cv.c firmware/ui.c firmware/clock.c firmware/font.c firmware/voice.h firmware/app.h firmware/ui.h | build
	$(CC) $(CFLAGS) -Iinclude -Ifirmware tests/test_pitch.c tests/oled_stub.c firmware/voice.c firmware/app.c firmware/sync.c firmware/cv.c firmware/ui.c firmware/clock.c firmware/font.c -o $@
.PHONY: test-pitch
test-pitch: build/test-pitch
	./build/test-pitch
test-firmware: test-pitch

build/test-clear: tests/test_clear.c tests/oled_stub.c firmware/voice.c firmware/app.c firmware/sync.c firmware/cv.c firmware/ui.c firmware/clock.c firmware/font.c firmware/voice.h firmware/app.h firmware/ui.h | build
	$(CC) $(CFLAGS) -Iinclude -Ifirmware tests/test_clear.c tests/oled_stub.c firmware/voice.c firmware/app.c firmware/sync.c firmware/cv.c firmware/ui.c firmware/clock.c firmware/font.c -o $@
.PHONY: test-clear
test-clear: build/test-clear
	./build/test-clear
test-firmware: test-clear

build/test-copy: tests/test_copy.c tests/oled_stub.c firmware/voice.c firmware/app.c firmware/sync.c firmware/cv.c firmware/ui.c firmware/clock.c firmware/font.c firmware/voice.h firmware/app.h firmware/ui.h | build
	$(CC) $(CFLAGS) -Iinclude -Ifirmware tests/test_copy.c tests/oled_stub.c firmware/voice.c firmware/app.c firmware/sync.c firmware/cv.c firmware/ui.c firmware/clock.c firmware/font.c -o $@
.PHONY: test-copy
test-copy: build/test-copy
	./build/test-copy
test-firmware: test-copy

build/test-mute: tests/test_mute.c tests/oled_stub.c firmware/voice.c firmware/app.c firmware/sync.c firmware/cv.c firmware/ui.c firmware/clock.c firmware/font.c firmware/voice.h firmware/app.h firmware/ui.h | build
	$(CC) $(CFLAGS) -Iinclude -Ifirmware tests/test_mute.c tests/oled_stub.c firmware/voice.c firmware/app.c firmware/sync.c firmware/cv.c firmware/ui.c firmware/clock.c firmware/font.c -o $@
.PHONY: test-mute
test-mute: build/test-mute
	./build/test-mute
test-firmware: test-mute

build/test-expansion: tests/test_expansion.c tests/oled_stub.c firmware/voice.c firmware/app.c firmware/sync.c firmware/cv.c firmware/settings.c firmware/ui.c firmware/clock.c firmware/font.c firmware/voice.h firmware/app.h firmware/ui.h firmware/settings.h | build
	$(CC) $(CFLAGS) -Iinclude -Ifirmware tests/test_expansion.c tests/oled_stub.c firmware/voice.c firmware/app.c firmware/sync.c firmware/cv.c firmware/settings.c firmware/ui.c firmware/clock.c firmware/font.c -lm -o $@
.PHONY: test-expansion
test-expansion: build/test-expansion
	./build/test-expansion
test-firmware: test-expansion

.PHONY: bia bia-simulator bia-simulator-dev test-bia
bia:
	python3 tools/build.py --bia
bia-simulator:
	python3 tools/build_bia_simulator.py
bia-simulator-dev:
	python3 tools/serve_simulator.py --bia --port 8766
build/test-bia: tests/test_bia.c tests/oled_stub.c firmware/bia/engine.c firmware/bia/ui.c firmware/bia/settings.c firmware/bia/bia.h firmware/bia/settings.h firmware/font.c firmware/cv.c firmware/sync.c firmware/store.c firmware/store.h firmware/preset_format.h | build
	$(CC) $(CFLAGS) -DBIA_BUILD -Iinclude -Ifirmware tests/test_bia.c tests/oled_stub.c firmware/bia/engine.c firmware/bia/ui.c firmware/bia/settings.c firmware/font.c firmware/cv.c firmware/sync.c firmware/store.c -o $@
test-bia: build/test-bia
	./build/test-bia
.PHONY: test-bia-simulator
test-bia-simulator: bia bia-simulator
	python3 -m unittest discover -s tests -p test_bia_simulator.py
	node tests/bia_browser.mjs
	python3 -m unittest discover -s tests -p test_bia_tools.py
test-bia: test-bia-simulator

build/test-bia-layout: tests/test_bia_layout.c firmware/bia/ui.c firmware/bia/engine.c firmware/bia/bia.h firmware/font.c firmware/cv.c firmware/sync.c | build
	$(CC) $(CFLAGS) -Iinclude -Ifirmware tests/test_bia_layout.c firmware/bia/ui.c firmware/bia/engine.c firmware/font.c firmware/cv.c firmware/sync.c -o $@
.PHONY: test-bia-layout
test-bia-layout: build/test-bia-layout
	./build/test-bia-layout
test-bia: test-bia-layout

build/test-bia-v2: tests/test_bia_v2.c tests/oled_stub.c firmware/bia/engine.c firmware/bia/ui.c firmware/bia/settings.c firmware/bia/bia.h firmware/font.c firmware/cv.c firmware/sync.c | build
	$(CC) $(CFLAGS) -DBIA_BUILD -Iinclude -Ifirmware tests/test_bia_v2.c tests/oled_stub.c firmware/bia/engine.c firmware/bia/ui.c firmware/bia/settings.c firmware/font.c firmware/cv.c firmware/sync.c -o $@
.PHONY: test-bia-v2
test-bia-v2: build/test-bia-v2
	./build/test-bia-v2
test-bia: test-bia-v2

# Shared control decoder dependency for portable UI consumers.
build/test-ui build/test-clock build/test-engine build/test-sync build/test-cv build/test-store build/test-rhythm build/test-groove build/test-pitch build/test-clear build/test-copy build/test-mute build/test-expansion build/test-bia build/test-bia-layout build/test-bia-v2: firmware/controls.h

build/test-controls: tests/test_controls.c tests/oled_stub.c firmware/controls.h firmware/ui.c firmware/bia/ui.c firmware/bia/engine.c firmware/font.c firmware/voice.c firmware/sync.c firmware/cv.c firmware/settings.c firmware/store.c | build
	$(CC) $(CFLAGS) -Iinclude -Ifirmware tests/test_controls.c tests/oled_stub.c firmware/ui.c firmware/bia/ui.c firmware/bia/engine.c firmware/font.c firmware/voice.c firmware/sync.c firmware/cv.c firmware/settings.c firmware/store.c -o $@
.PHONY: test-controls
test-controls: build/test-controls
	./build/test-controls
test-firmware test-bia: test-controls

.PHONY: test-eeprom
test-eeprom:
	python3 -m unittest discover -s tests -p test_eeprom.py
test-firmware test-bia: test-eeprom
