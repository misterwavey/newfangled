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
