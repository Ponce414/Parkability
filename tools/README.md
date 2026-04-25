# tools/

Helper scripts for development and bring-up. Currently empty — likely
candidates as the project progresses:

- `replay_events.py` — feed `events` rows from one SQLite file into a
  running backend's `/ingest/event` endpoint, for replaying captured runs
- `mock_leader.py` — pretend to be a leader, publish synthetic
  `MSG_SENSOR_UPDATE` payloads via MQTT or REST, so the app can be
  developed without flashed hardware
- `pair_leader.py` — interactive helper that reads a leader's MAC over
  serial and writes it into a sensor's `config.h` (`LEADER_MAC`)
- `flash_one.sh` — wrap `pio run --target upload` with the right port
  selection on macOS/Linux
