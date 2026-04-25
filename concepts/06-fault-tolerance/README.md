# 06 — Fault Tolerance

How the system survives node failures.

## What we did

**Bully algorithm for leader election.** Two leaders per zone — one active,
one backup. If the active one stops sending heartbeats, the backup detects
the timeout and starts an election. Highest-priority node (by node_id from
MAC) wins.

**Heartbeats for failure detection.** Every leader sends a heartbeat each
`HEARTBEAT_INTERVAL_MS`. Missing heartbeats for `HEARTBEAT_TIMEOUT_MS`
(default 3.5x interval) triggers `bully_on_heartbeat_timeout`.

**Idempotent sensor registration and state writes.** A sensor coming back
online after a leader failover doesn't need to know anything special —
`MSG_REGISTER` is idempotent by `spot_id`, and `upsert_spot` in the backend
is too. Replays of the same event are safe.

**Backend tolerates absent transports.** If MQTT is unreachable on startup,
ingestion just logs a warning and the REST endpoint still works.

## Key code

- [`firmware/leader-node/src/bully.c`](../../firmware/leader-node/src/bully.c) — election state machine (skeleton — team to implement)
- [`firmware/leader-node/src/heartbeat.c`](../../firmware/leader-node/src/heartbeat.c) — periodic heartbeat + timeout detection
- [`backend/db.py`](../../backend/db.py) — `upsert_spot` idempotent under replay
- [`backend/ingest_mqtt.py`](../../backend/ingest_mqtt.py) — backend boots even if MQTT is down

## Tradeoff we made

Bully election is O(N²) messages in the worst case but N ≤ 3 in our
deployment, so it's effectively free. We picked it over Raft for that
reason and because the rubric calls it out by name.
