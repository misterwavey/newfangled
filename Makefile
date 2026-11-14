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
build/test-ui: tests/test_ui.c firmware/ui.c firmware/font.c firmware/voice.c firmware/ui.h firmware/font.h firmware/voice.h include/pnw_board.h | build
	$(CC) $(CFLAGS) -Iinclude -Ifirmware tests/test_ui.c firmware/ui.c firmware/font.c firmware/voice.c -o $@
build/test-clock: tests/test_clock.c firmware/clock.c firmware/clock.h firmware/gate_bus.c firmware/gate_bus.h | build
	$(CC) $(CFLAGS) -Ifirmware tests/test_clock.c firmware/clock.c firmware/gate_bus.c -o $@
build/test-engine: tests/test_engine.c tests/oled_stub.c firmware/voice.c firmware/app.c firmware/ui.c firmware/clock.c firmware/font.c firmware/voice.h firmware/app.h firmware/ui.h firmware/clock.h firmware/font.h | build
	$(CC) $(CFLAGS) -Iinclude -Ifirmware tests/test_engine.c tests/oled_stub.c firmware/voice.c firmware/app.c firmware/ui.c firmware/clock.c firmware/font.c -o $@
test: build/test-ui build/test-clock build/test-engine
	./build/test-ui
	./build/test-clock
	./build/test-engine
clean:
	rm -rf build
