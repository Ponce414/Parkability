.PHONY: sensor-build sensor-flash leader-build leader-flash backend-run backend-install app-run app-install db-init help

help:
	@echo "Firmware: sensor-build, sensor-flash, leader-build, leader-flash"
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
