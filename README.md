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
| [`firmware/leader-node/`](firmware/leader-node/) | Per-zone leader firmware (skeleton — see "left for the team") |
| [`firmware/shared/`](firmware/shared/) | Wire formats, Lamport clock, spot-id encoding |
| [`backend/`](backend/) | FastAPI + paho-mqtt + sqlite3 |
| [`app/`](app/) | Vite + React, no extra libs |
| [`protocols/`](protocols/) | Wire-format docs (ESP-NOW, MQTT/REST, WebSocket) |
| [`concepts/`](concepts/) | One README per rubric concept, with code pointers |
| [`docs/`](docs/) | Architecture, hardware decisions, database, ADRs |

## What is real vs. skeleton

**Real, runnable code:**
- `app/` — builds and runs
- `backend/` — starts and serves
- `firmware/sensor-node/` — full I2C + ESP-NOW polling loop
- `firmware/shared/` — Lamport, spot id, message structs

**Skeleton with team-implementation specs in comments:**
- `firmware/leader-node/src/bully.{c,h}` — election state machine (per rubric, this is a learning exercise)
- `firmware/leader-node/src/radio_scheduler.c` — naive time-slicing stub; team picks final policy
- `firmware/leader-node/src/heartbeat.c` — send side real, receive-side timeout detection stubbed
- `firmware/leader-node/src/aggregator.c` — interface real, batching policy TODO

## What is explicitly left for the team

These are intentionally not finished — they're the meat of the project work.
Each is documented inline at the file referenced.

| Item | Where the spec lives |
|---|---|
| Bully algorithm implementation | [`firmware/leader-node/src/bully.h`](firmware/leader-node/src/bully.h) — full state-transition table in header comments |
| Radio scheduler strategy choice | [`firmware/leader-node/src/radio_scheduler.c`](firmware/leader-node/src/radio_scheduler.c) — three options listed |
| Heartbeat timeout detection per-leader | [`firmware/leader-node/src/heartbeat.c`](firmware/leader-node/src/heartbeat.c) |
| VL53L0X threshold calibration | [`firmware/sensor-node/src/config.h`](firmware/sensor-node/src/config.h) (`OCCUPANCY_THRESHOLD_MM`) |
| Leader MAC pairing procedure | [`firmware/sensor-node/src/config.h`](firmware/sensor-node/src/config.h) (`LEADER_MAC`) |
| Aggregator batching policy | [`firmware/leader-node/src/aggregator.c`](firmware/leader-node/src/aggregator.c) — three options listed |

## Rubric coverage

Eight READMEs in [`concepts/`](concepts/), each with code pointers:

1. [Network communication](concepts/01-network-communication/README.md)
2. [Concurrency & coordination](concepts/02-concurrency-coordination/README.md)
3. [Peer-to-peer](concepts/03-peer-to-peer/README.md)
4. [Naming & service discovery](concepts/04-naming-service-discovery/README.md)
5. [Consistency & replication](concepts/05-consistency-replication/README.md)
6. [Fault tolerance](concepts/06-fault-tolerance/README.md)
7. [Logical time](concepts/07-logical-time/README.md)
8. [Scalability](concepts/08-scalability/README.md)

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

- Leader uplink: MQTT vs REST (both supported; pick per deployment)
- VL53L0X distance threshold (calibrate at bring-up)
- Spot ID encoding: string (current) vs packed bitfield
