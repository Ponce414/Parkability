# analysis/

Experiment scripts and writeups for the rubric-coverage claims in
[`concepts/`](../concepts/).

All experiments read from the `events` table in `backend/parking.db`
(see [`docs/database.md`](../docs/database.md)). That table records
`lamport_ts`, sender `wall_ts`, and backend `received_wall_ts` on every
sensor update, which is enough to support the analyses below.

## Planned experiments

| Experiment | Concept | What to measure |
|---|---|---|
| `propagation_delay.py` | 05 consistency | Distribution of `received_wall_ts - wall_ts` per hop; how far behind physical reality is the backend at p50 / p99? |
| `lamport_vs_wall.py` | 07 logical time | Count pairs of events where wall-clock order disagrees with Lamport order, to demonstrate why Lamport is needed |
| `radio_scheduler_stats.py` | 02 concurrency | Plot `RadioStats` from `radio_scheduler_get_stats()` over time; compare naive time-slicing vs inactivity-based |
| `bully_timing.py` | 06 fault tolerance | Time from induced leader failure to new coordinator announcement, across N trials |
| `peer_limit_stress.py` | 08 scalability | Saturate ESP-NOW peer table; confirm drop-off matches documented ~20-peer cap |

## Running

These are all TODO. A stub workflow:

```bash
pip install matplotlib pandas
python analysis/propagation_delay.py --db backend/parking.db --out analysis/out/propagation.png
```

## Reproducibility

Capture every experiment run by copying `backend/parking.db` to
`analysis/runs/<date>-<label>.db` *before* running the analysis, so the raw
data is preserved even if the running system keeps generating events.
