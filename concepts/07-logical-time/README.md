# 07 — Logical Time

How we order events without trusting wall clocks.

## What we did

**Lamport clocks on every node.** Each sensor and leader maintains a local
counter. On a local event (e.g., sending a sensor update), the counter
increments. On receiving any stamped message, the counter becomes
`max(local, received) + 1`.

**Stamp every wire message with `lamport_ts`.** It's part of `MessageHeader`,
so every ESP-NOW frame and every JSON payload to the backend carries one.

**Persist `lamport_ts` per event.** The `events` table has an indexed
`lamport_ts` column. Backend uses it to reject older-than-snapshot updates;
analysis uses it to detect re-ordering between wall-time and logical-time
orderings.

**Wall time is logged but not trusted for ordering.** Sensors only have
`xTaskGetTickCount` (ms since boot) — useful for relative timing on one
node, useless for cross-node ordering since boots are unsynchronized.

## Key code

- [`firmware/shared/lamport.c`](../../firmware/shared/lamport.c) — thread-safe Lamport clock implementation
- [`firmware/shared/messages.h`](../../firmware/shared/messages.h) — `MessageHeader.lamport_ts`
- [`backend/state.py`](../../backend/state.py) — Lamport-based update rejection in `apply_sensor_update`
- [`backend/schema.sql`](../../backend/schema.sql) — `events.lamport_ts` indexed column

## Tradeoff we made

Lamport clocks give us a partial order, not a total one — two unrelated
events can have the same timestamp. We resolve ties by wall time for
display purposes, with a clear note that this is heuristic. For our
"detect a state change" use case, partial ordering is enough.
