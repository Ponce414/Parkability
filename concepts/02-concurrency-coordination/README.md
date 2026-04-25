# 02 — Concurrency & Coordination

Where multiple things happen at once and we have to coordinate them.

## What we did

Two distinct concurrency problems:

**On the leader (single-radio coordination).** ESP-NOW and WiFi share the
ESP32-C3's one radio. We can't run both concurrently; we must time-slice.
The radio scheduler is the coordination point — every sender checks
`radio_scheduler_current_mode()` before transmitting, and the scheduler
itself is the single source of truth for which mode the radio is in.

**On the backend (async fan-out).** A single MQTT message arrival fans out
to potentially many WebSocket clients. We use FastAPI's asyncio event loop
with paho-mqtt running in its own thread; the bridge is
`asyncio.run_coroutine_threadsafe`. The WebSocket manager guards its client
set with an `asyncio.Lock`.

## Key code

- [`firmware/leader-node/src/radio_scheduler.c`](../../firmware/leader-node/src/radio_scheduler.c) — single-radio time-sharing (stub; team picks final policy)
- [`backend/ingest_mqtt.py`](../../backend/ingest_mqtt.py) — MQTT thread → asyncio bridge via `run_coroutine_threadsafe`
- [`backend/ws.py`](../../backend/ws.py) — async broadcast guarded by an `asyncio.Lock`
- [`firmware/shared/lamport.c`](../../firmware/shared/lamport.c) — FreeRTOS mutex around the Lamport clock

## Tradeoff we made

Naive time-slicing (current stub) is predictable but always pays a switching
cost. Inactivity-based scheduling has lower latency for the busy mode but
needs careful tuning to avoid starving the other. Both are documented in
`radio_scheduler.c` for the team to evaluate.
