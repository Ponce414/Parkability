# Architecture Decision Records

Lightweight ADRs go here. One file per decision, named `NNNN-short-title.md`,
using the template below.

## Template

```markdown
# ADR-NNNN: title

- Status: accepted | proposed | superseded by NNNN
- Date: YYYY-MM-DD

## Context
What forced the decision.

## Decision
What we picked.

## Alternatives considered
Briefly, with a one-line "why not".

## Consequences
What this commits us to, and what it forecloses.
```

## Decisions already made (not yet written up — TODO)

These are documented inline in code/docs but a dedicated ADR each would be
useful before grading:

- ADR-0001: ESP-NOW for sensor↔leader (vs. WiFi mesh, BLE mesh)
- ADR-0002: Bully algorithm for leader election (vs. Raft)
- ADR-0003: SQLite for backend persistence (vs. Postgres, flat file)
- ADR-0004: Lamport clocks for event ordering (vs. vector clocks)
- ADR-0005: Eventual consistency between lot and backend (vs. linearizability)
- ADR-0006: MQTT primary, REST fallback for leader uplink (vs. one or other)
- ADR-0007: Hierarchical string spot ids (vs. packed bitfield)
