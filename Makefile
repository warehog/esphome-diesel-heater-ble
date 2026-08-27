PYTHON ?= python3
VENV ?= .venv
CONFIG ?= esphome.yaml
NODE ?= heater-adapter-ble
DIST_DIR ?= dist
HOST_TEST_BUILD ?= /tmp/diesel-heater-host-tests
LOCAL_TEST_CONFIG ?= tests/esphome-local.yaml
MINIMAL_TEST_CONFIG ?= tests/esphome-minimal.yaml
IDF_CACHE_ROOT ?= $(shell $(PYTHON) -c 'from pathlib import Path; print(Path.home() / ".cache" / "esphome" / "idf")')

# Keep ESP-IDF's revision probe from inheriting an unrelated parent Git repository.
export GIT_CEILING_DIRECTORIES := $(IDF_CACHE_ROOT)

VENV_BIN := $(VENV)/bin
VENV_PYTHON := $(VENV_BIN)/python
ESPHOME := $(VENV_BIN)/esphome
INSTALL_STAMP := $(VENV)/.esphome-installed
BUILD_DIR := .esphome/build/$(NODE)/build
FACTORY_IMAGE := $(BUILD_DIR)/firmware.factory.bin
OTA_IMAGE := $(BUILD_DIR)/firmware.ota.bin

.DEFAULT_GOAL := help

.PHONY: help venv install bootstrap config clean compile cpp artifact verify-artifacts firmware verify flash versions \
        host-configure host-build host-test test-config-local test-compile-local \
        test-config-minimal test-compile-minimal test

help:
	@echo "ESPHome diesel-heater build targets:"
	@echo "  make bootstrap             Create .venv and install the pinned ESPHome"
	@echo "  make config                Validate CONFIG=$(CONFIG)"
	@echo "  make clean                 Remove generated ESPHome build files"
	@echo "  make compile               Generate and compile the ESP32 C++ firmware"
	@echo "  make artifact              Copy flashable images into $(DIST_DIR)/"
	@echo "  make verify-artifacts      Validate image metadata and merged factory segments"
	@echo "  make firmware              Bootstrap, validate, compile, and collect images"
	@echo "  make verify                Revalidate, rebuild, check artifacts and Git whitespace"
	@echo "  make flash PORT=/dev/ttyUSB0  Flash explicitly selected serial device"
	@echo "  make host-test             Configure, build, and run native protocol smoke tests"
	@echo "  make test                  Run host tests and both local ESPHome configurations"
	@echo ""
	@echo "Override CONFIG, NODE, VENV, or DIST_DIR on the make command line."

$(VENV_PYTHON):
	$(PYTHON) -m venv $(VENV)

venv: $(VENV_PYTHON)

$(INSTALL_STAMP): requirements.txt | $(VENV_PYTHON)
	$(VENV_PYTHON) -m pip install -r requirements.txt
	touch $@

install: $(INSTALL_STAMP)

bootstrap: install versions

versions: $(INSTALL_STAMP)
	$(VENV_PYTHON) --version
	$(ESPHOME) version

config: $(INSTALL_STAMP)
	$(ESPHOME) config $(CONFIG)

clean: $(INSTALL_STAMP)
	$(ESPHOME) clean $(CONFIG)

compile: $(INSTALL_STAMP)
	$(ESPHOME) compile $(CONFIG)

cpp: compile

artifact: compile
	test -f $(FACTORY_IMAGE)
	test -f $(OTA_IMAGE)
	mkdir -p $(DIST_DIR)
	cp $(FACTORY_IMAGE) $(DIST_DIR)/$(NODE).factory.bin
	cp $(OTA_IMAGE) $(DIST_DIR)/$(NODE).ota.bin
	cd $(DIST_DIR) && sha256sum $(NODE).factory.bin $(NODE).ota.bin > SHA256SUMS
	cat $(DIST_DIR)/SHA256SUMS

verify-artifacts: artifact
	$(VENV_PYTHON) -m esptool image-info $(DIST_DIR)/$(NODE).ota.bin >/dev/null
	@factory="$(DIST_DIR)/$(NODE).factory.bin"; build="$(BUILD_DIR)"; \
	for spec in "4096 bootloader/bootloader.bin" "32768 partition_table/partition-table.bin" \
	            "36864 ota_data_initial.bin" "65536 firmware.ota.bin"; do \
	  set -- $$spec; offset=$$1; source=$$2; size=$$(wc -c < "$$build/$$source"); \
	  dd if="$$factory" bs=1 skip="$$offset" count="$$size" status=none | cmp "$$build/$$source" -; \
	done

firmware: bootstrap config artifact

verify: config verify-artifacts
	git diff --check
	git status --short

flash: compile
	@test -n "$(PORT)" || { echo "PORT is required, for example PORT=/dev/ttyUSB0"; exit 2; }
	$(ESPHOME) upload $(CONFIG) --device $(PORT)

host-configure:
	cmake -S tests -B $(HOST_TEST_BUILD)

host-build: host-configure
	cmake --build $(HOST_TEST_BUILD) --parallel

host-test: host-build
	ctest --test-dir $(HOST_TEST_BUILD) --output-on-failure

test-config-local: $(INSTALL_STAMP)
	$(ESPHOME) config $(LOCAL_TEST_CONFIG)

test-compile-local: $(INSTALL_STAMP)
	$(ESPHOME) compile $(LOCAL_TEST_CONFIG)

test-config-minimal: $(INSTALL_STAMP)
	$(ESPHOME) config $(MINIMAL_TEST_CONFIG)

test-compile-minimal: $(INSTALL_STAMP)
	$(ESPHOME) compile $(MINIMAL_TEST_CONFIG)

test: host-test test-config-local test-compile-local test-config-minimal test-compile-minimal
