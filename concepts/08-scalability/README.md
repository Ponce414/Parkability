# 08 — Scalability

How the architecture scales (and where it doesn't).

## What we did

**Hierarchical zones to fit ESP-NOW's peer limit.** ESP-NOW caps at ~20
encrypted peers per node. With one backup leader per zone, that leaves
~19 sensors per zone. Bigger lots are partitioned into multiple zones,
each with its own pair of leaders, fanning into the backend independently.

**Backend is a single-node bottleneck — by choice.** Per-zone independent
upload means the backend sees fan-in of N zones. SQLite on one machine
handles ~10k writes/sec which is plenty for class-scale (each spot changes
state at most every few minutes). For a real deployment we'd swap SQLite
for Postgres and run multiple FastAPI workers behind a queue.

**WebSocket fan-out scales linearly with clients.** No room for a million
spectators, but for the few-tens-of-tabs in our demo the in-memory broadcast
loop is fine.

## Key code

- [`firmware/leader-node/src/espnow_handler.c`](../../firmware/leader-node/src/espnow_handler.c) — `MAX_ESPNOW_PEERS` constant + comment explaining the zone-size cap
- [`backend/state.py`](../../backend/state.py) — single-node in-memory cache; would shard by lot for real scale
- [`backend/db.py`](../../backend/db.py) — SQLite, fine for class-scale, NOT for production

## Tradeoff we made

We sized for the rubric, not for production. The architecture *can* scale
horizontally (zones are independent) but several pieces (SQLite, single
WebSocket server, in-memory state cache) would have to be replaced for a
real deployment of hundreds of zones. We documented the rip-and-replace
boundaries here so a reader sees the exits clearly.
