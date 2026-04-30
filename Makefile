LEADER_PORT ?= /dev/cu.usbmodem1101
SENSOR_PORT ?= /dev/cu.usbmodem101
SENSOR_SPOT ?= spot1
SENSOR_BUILD_DIR ?= build-c3-$(SENSOR_SPOT)

.PHONY: sensor-build sensor-flash sensor-c3-build sensor-c3-flash leader-build leader-flash leader-c5-build leader-c5-flash backend-run backend-install app-run app-install db-init help

help:
	@echo "Firmware: sensor-build, sensor-flash, leader-build, leader-flash"
	@echo "IDF/C5:   leader-c5-build, leader-c5-flash, sensor-c3-build, sensor-c3-flash"
	@echo "Backend:  backend-install, db-init, backend-run"
	@echo "App:      app-install, app-run"

sensor-build:
	cd firmware/sensor-node && pio run

sensor-flash:
	cd firmware/sensor-node && pio run --target upload

leader-build:
	cd firmware/leader-node && pio run

leader-flash:
	cd firmware/leader-node && pio run --target upload

sensor-c3-build:
	cd firmware/sensor-node && idf.py -B $(SENSOR_BUILD_DIR) -DIDF_TARGET=esp32c3 -DSENSOR_SPOT=$(SENSOR_SPOT) build

sensor-c3-flash:
	cd firmware/sensor-node && idf.py -B $(SENSOR_BUILD_DIR) -DIDF_TARGET=esp32c3 -DSENSOR_SPOT=$(SENSOR_SPOT) -p $(SENSOR_PORT) flash

leader-c5-build:
	cd firmware/leader-node && idf.py -B build-c5 -DIDF_TARGET=esp32c5 -DSDKCONFIG_DEFAULTS=sdkconfig.defaults build

leader-c5-flash:
	cd firmware/leader-node && idf.py -B build-c5 -DIDF_TARGET=esp32c5 -DSDKCONFIG_DEFAULTS=sdkconfig.defaults -p $(LEADER_PORT) flash

backend-install:
	cd backend && pip install -r requirements.txt

db-init:
	cd backend && sqlite3 parking.db < schema.sql

backend-run:
	cd backend && uvicorn main:app --reload --host 0.0.0.0 --port 8000

app-install:
	cd app && npm install

app-run:
	cd app && npm run dev
