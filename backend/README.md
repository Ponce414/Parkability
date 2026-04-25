# Backend

FastAPI + paho-mqtt + sqlite3. Real, working, runnable.

## Run

```
make backend-install
make db-init
make backend-run
```

That starts uvicorn on `0.0.0.0:8000`. Sanity check:

```
curl localhost:8000/health
curl localhost:8000/lot/lotA/state
```

WebSocket endpoint: `ws://localhost:8000/ws`.

## Configuration

Environment variables (all optional):

| Var | Default | Notes |
|---|---|---|
| `PARKING_DB_PATH` | `parking.db` | SQLite file location |
| `MQTT_HOST` | `localhost` | If unreachable, MQTT ingestion is silently skipped |
| `MQTT_PORT` | `1883` | |

## Modules

- [main.py](main.py) — FastAPI app, WebSocket endpoint, lifespan setup
- [db.py](db.py) — SQLite layer, parameterized queries, `connect()` context manager
- [state.py](state.py) — in-memory state + write-through to DB, Lamport-based ordering
- [ws.py](ws.py) — WebSocket manager (connect, disconnect, broadcast)
- [ingest_mqtt.py](ingest_mqtt.py) — MQTT subscriber, runs in paho's thread, bridges via `run_coroutine_threadsafe`
- [ingest_rest.py](ingest_rest.py) — REST endpoints `/ingest/event` and `/ingest/leader` for leaders that prefer HTTP
- [schema.sql](schema.sql) — DB schema. `events` is append-only and used by experiments.

## Why both MQTT and REST?

The leader firmware has a compile-time switch (`UPLINK_PROTOCOL_MQTT` vs
`UPLINK_PROTOCOL_REST`). Both backends are always running; deployment picks
which one the leaders point at. See `protocols/leader-backend-api.md`.
