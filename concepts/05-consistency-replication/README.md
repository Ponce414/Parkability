# 05 — Consistency & Replication

What consistency model we chose and why.

## What we did

**Eventual consistency between physical lot and backend state.** A car
arrives → sensor detects → debounces → sends → leader buffers → uploads to
backend → backend persists → WebSocket fan-out to apps. Each hop adds
latency. We do not attempt linearizability — there is no global lock; the
backend is the convergence point, not a coordinator.

**Append-only event log as ground truth.** The `events` table records every
sensor update with both the sender's wall time and the backend's receive
wall time. This lets us reconstruct propagation delays and detect re-ordering
in analysis.

**Last-writer-wins on the live snapshot, ordered by Lamport timestamp.**
`apply_sensor_update` rejects strictly older Lamport values. Equal Lamport
values fall back to wall-time comparison for determinism. Either way, the
event is *still logged* in `events` even when not applied to the snapshot.

## Key code

- [`backend/schema.sql`](../../backend/schema.sql) — `events` table is the propagation log
- [`backend/state.py`](../../backend/state.py) — `apply_sensor_update` enforces eventual convergence rules
- [`backend/db.py`](../../backend/db.py) — `record_event` always logs; `upsert_spot` is idempotent

## Tradeoff we made

We explicitly rejected linearizability. With ESP-NOW retries, single-radio
time-slicing, and WiFi handoffs, the cost in latency would be huge for a
problem (parking) where ~1s staleness is fine. Eventual consistency was
the right call and the events log lets us *measure* the staleness rather
than just hoping it's small.
