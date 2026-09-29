# Convenience wrapper around CMake.
#   make            build ./build/leet
#   make install    install to /usr/local (PREFIX=... to change)
#   make verify     run every reference solution through the judge
#   make clean

PREFIX ?= /usr/local
BUILD  ?= build
JOBS   ?= $(shell nproc 2>/dev/null || echo 2)

.PHONY: all build install uninstall verify check clean run

all: build

build:
	@cmake -S . -B $(BUILD) -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$(PREFIX) >/dev/null
	@cmake --build $(BUILD) -j $(JOBS)
	@echo ""
	@echo "  Built $(BUILD)/leet   ->  try:  ./$(BUILD)/leet"
	@echo ""

install: build
	@cmake --install $(BUILD)
	@echo "  Installed to $(PREFIX)/bin/leet"

uninstall:
	rm -f $(PREFIX)/bin/leet
	rm -rf $(PREFIX)/share/leet

check: build
	./$(BUILD)/leet dev check

verify: build
	./$(BUILD)/leet dev verify --jobs 2

run: build
	./$(BUILD)/leet

clean:
	rm -rf $(BUILD)
