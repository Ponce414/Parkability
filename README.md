# Parking Lot Sensor System

Distributed parking lot occupancy system. Each spot has a VL53L0X ToF sensor on
an ESP32-C3 that reports occupancy over ESP-NOW to a zone leader; the leader
aggregates and uplinks to a Python backend over MQTT (or REST as fallback);
the backend pushes live updates to a React app over WebSocket.

CSULB CECS 327 final project.

## Quickstart

```
1. make backend-install
2. make db-init
3. make backend-run        # terminal 1
4. make app-install
5. make app-run            # terminal 2, open http://localhost:5173
6. make sensor-flash       # with a C3 connected
7. (leader flashing + pairing is done once hardware is ready)
```

The app will load and show "Connected" with no spots — that's expected until
sensors are flashed and paired.

## Layout

| Path | What |
|---|---|
| [`firmware/sensor-node/`](firmware/sensor-node/) | Per-spot sensor firmware (ESP-IDF, real and runnable) |
| [`firmware/leader-node/`](firmware/leader-node/) | Per-zone leader firmware (real and runnable) |
| [`firmware/shared/`](firmware/shared/) | Wire formats, Lamport clock, spot-id encoding |
| [`backend/`](backend/) | FastAPI + paho-mqtt + sqlite3 |
| [`app/`](app/) | Vite + React, no extra libs |
| [`analysis/`](analysis/) | Python scripts that read `backend/parking.db` to produce concept-rubric numbers |
| [`protocols/`](protocols/) | Wire-format docs (ESP-NOW, MQTT/REST, WebSocket) |
| [`concepts/`](concepts/) | One README per rubric concept, with code pointers |
| [`docs/`](docs/) | Architecture, hardware decisions, database, ADRs, bring-up procedure |

## Status

Everything builds and runs. See [`docs/bringup.md`](docs/bringup.md) for the
end-to-end sync-up procedure.

| Component | State |
|---|---|
| `app/` | Real |
| `backend/` | Real |
| `firmware/sensor-node/` | Real — VL53L0X poll, debounce, ESP-NOW send, leader re-pairing on COORDINATOR |
| `firmware/leader-node/` | Real — bully election, per-peer heartbeat tracking, inactivity-based radio scheduler, ring-buffer aggregator with RegisterAck |
| `firmware/shared/` | Real |
| `analysis/` | Four runnable scripts; need a populated `parking.db` to produce numbers |

## Per-deployment configuration

Two files need editing per zone:

| File | What to set |
|---|---|
| [`firmware/leader-node/src/config.h`](firmware/leader-node/src/config.h) | `ZONE_LOT`, `ZONE_ID`, `WIFI_SSID`/`WIFI_PASSWORD`, broker/backend host, transport choice |
| [`firmware/sensor-node/src/config.h`](firmware/sensor-node/src/config.h) | `LEADER_MAC` (MAC of the elected leader), `SPOT_ID`, `OCCUPANCY_THRESHOLD_MM` calibration |

## Rubric coverage

Seven concepts targeted for the presentation, each backed by a runnable
analysis script in [`analysis/`](analysis/):

| # | Concept | Code pointer | Measurement |
|---|---|---|---|
| 01 | [Network communication](concepts/01-network-communication/README.md) | ESP-NOW + MQTT/REST + WebSocket | end-to-end latency in app |
| 02 | [Concurrency & coordination](concepts/02-concurrency-coordination/README.md) | `radio_scheduler.c`, FreeRTOS tasks | `analysis/radio_scheduler_stats.py` |
| 04 | [Naming & service discovery](concepts/04-naming-service-discovery/README.md) | `spot_id.c`, broadcast COORDINATOR + Register/Ack | spot tiles use hierarchical names |
| 05 | [Consistency & replication](concepts/05-consistency-replication/README.md) | eventual-consistency model | `analysis/propagation_delay.py` |
| 06 | [Fault tolerance](concepts/06-fault-tolerance/README.md) | `bully.c`, `heartbeat.c` | `analysis/bully_timing.py` |
| 07 | [Logical time](concepts/07-logical-time/README.md) | `lamport.c`, `events.lamport_ts` column | `analysis/lamport_vs_wall.py` |
| 08 | [Scalability](concepts/08-scalability/README.md) | `MAX_PEER_TABLE`, ESP-NOW peer limit | leader log `peer table full` |

Concept 03 (peer-to-peer) is implicitly covered by the intra-zone ESP-NOW
mesh in concept 01 — kept in the repo at
[`concepts/03-peer-to-peer/README.md`](concepts/03-peer-to-peer/README.md)
for completeness, not a presentation focus.

## Prerequisites

| Tool | Used for | Install |
|---|---|---|
| Python 3.11+ | backend | system Python or pyenv |
| Node 18+ / npm | app | nodejs.org |
| sqlite3 CLI | `make db-init` | usually preinstalled |
| PlatformIO | firmware build/flash | `pip install platformio` |
| (optional) Mosquitto | MQTT broker | brew/apt; backend boots fine without it |

## Decisions already made (don't relitigate)

- Hierarchical names `lot/zone/spot`
- Lamport timestamps for event ordering
- Eventual consistency between physical lot and backend (linearizability rejected)
- Bully algorithm for leader election
- SQLite for backend persistence

## Decisions still deferred

- Leader uplink: MQTT vs REST (both supported; pick per deployment in `config.h`)
- VL53L0X distance threshold (calibrate at bring-up)
- Spot ID encoding: string (current) vs packed bitfield

## Bring-up

See [`docs/bringup.md`](docs/bringup.md) for the step-by-step procedure
covering backend → app → leader → sensor with verification checks at every
step.
