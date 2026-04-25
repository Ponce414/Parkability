# Database

## Why SQLite

- **Zero config.** Single file, no daemon, no auth. Right call for a class project.
- **Relational fits the data.** Lots have zones, zones have spots, spots
  have events. Foreign keys keep the schema honest.
- **Sufficient for class scale.** SQLite handles ~10k writes/sec on a laptop.
  Our peak is maybe a few writes per second.
- **Same file is the experiment log.** No separate logging pipeline; just
  query the `events` table.

The cost is single-writer concurrency, but the backend serializes writes
through one Python process anyway, so we don't feel it.

## Schema

See [`backend/schema.sql`](../backend/schema.sql).

| Table | Purpose | Mutability |
|---|---|---|
| `lots` | top-level groupings | rarely changes |
| `zones` | a leader's coverage area; stores current leader MAC | updated on every leader change |
| `spots` | current state per parking spot | upserted on every accepted update |
| `events` | append-only log of every received event, with both sender's wall_ts and backend's received_wall_ts | INSERT only — never UPDATE/DELETE |

The `events` table is the heart of the experiment story: it captures
ordering (`lamport_ts`), propagation latency (`received_wall_ts - wall_ts`),
and recoverability (you can rebuild the snapshot from it).

## What lives in memory only

- Connected WebSocket clients (`backend/ws.py`)
- Aggregated in-memory snapshot used for fast reads (`backend/state.py`) —
  this is a *cache* of `spots`, reloaded from the DB on backend startup
- Lamport clocks on each device

## Important nuance

The database is internal to the backend. The system's eventual-consistency
property is between the **physical lot** and **persisted backend state** —
not between replicas of the database. There is no replication; SQLite is
a single source of truth. See [`concepts/05-consistency-replication`](../concepts/05-consistency-replication/README.md)
for the full story.
