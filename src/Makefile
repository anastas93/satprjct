# Автоматическая сборка прошивки и обновление веб-ресурсов
# Комментарии на русском по требованию

ARDUINO_CLI ?= arduino-cli
FQBN ?= esp32:esp32:esp32dev
BUILD_DIR ?= build

WEB_HEADER := web/web_content.h
WEB_SOURCES := \
web/index.html \
web/style.css \
web/script.js \
web/libs/sha256.js \
web/libs/mgrs.js \
web/libs/freq-info.csv \
web/libs/geostat_tle.js

.PHONY: all firmware web-header clean

all: firmware

web-header: $(WEB_HEADER)

$(WEB_HEADER): $(WEB_SOURCES) tools/generate_web_content.py
	@python3 tools/generate_web_content.py

firmware: $(WEB_HEADER)
	$(ARDUINO_CLI) compile --fqbn $(FQBN) --build-path $(BUILD_DIR) serial_radio_control.ino

clean:
	rm -rf $(BUILD_DIR)
