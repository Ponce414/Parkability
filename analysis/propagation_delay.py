#!/usr/bin/env python3
"""
propagation_delay.py — concept 05 (consistency / replication).

Measures how far behind physical reality the backend is on average. For each
sensor event we log the sensor-side wall_ts (ms since sensor boot) and the
backend-side received_wall_ts (ms epoch). The difference is the end-to-end
propagation delay: sensor read -> ESP-NOW -> leader buffer -> WiFi window
-> MQTT/REST -> backend ingest.

We can't compare wall_ts to received_wall_ts directly because they're on
different clocks. Instead, per spot, we look at the *change* in delay across
consecutive events: if the gap between two backend-side receive times is
much larger than the gap between the corresponding sensor-side wall_ts,
that excess is propagation jitter.

Run:
    python analysis/propagation_delay.py
    python analysis/propagation_delay.py --db backend/parking.db
"""

import argparse
import sqlite3
import statistics
import sys


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--db", default="backend/parking.db")
    ap.add_argument("--limit", type=int, default=10000,
                    help="max events to scan")
    args = ap.parse_args()

    con = sqlite3.connect(args.db)
    cur = con.cursor()
    rows = cur.execute(
        "SELECT spot_id, wall_ts, received_wall_ts "
        "FROM events ORDER BY spot_id, event_id LIMIT ?",
        (args.limit,),
    ).fetchall()

    if not rows:
        print("no events in db — run the system first to collect data")
        sys.exit(0)

    # Group consecutive deltas by spot.
    deltas_ms = []
    prev_per_spot = {}
    for spot_id, wall, recv in rows:
        prev = prev_per_spot.get(spot_id)
        if prev is not None:
            d_sensor = wall - prev[0]
            d_backend = recv - prev[1]
            jitter = d_backend - d_sensor
            if 0 <= jitter < 60_000:  # discard clock-resync outliers
                deltas_ms.append(jitter)
        prev_per_spot[spot_id] = (wall, recv)

    if not deltas_ms:
        print(f"scanned {len(rows)} events but no usable consecutive pairs")
        sys.exit(0)

    deltas_ms.sort()
    n = len(deltas_ms)
    p50 = deltas_ms[n // 2]
    p95 = deltas_ms[int(n * 0.95)]
    p99 = deltas_ms[int(n * 0.99)]
    mean = statistics.mean(deltas_ms)

    print(f"events scanned:        {len(rows)}")
    print(f"jitter samples:        {n}")
    print(f"propagation jitter ms: mean={mean:.1f}  p50={p50}  p95={p95}  p99={p99}")
    print()
    print("Interpretation: this is the *additional* delay between successive")
    print("events for the same spot, beyond the time the sensor itself spent")
    print("between readings. p99 sets a lower bound on staleness in the UI.")


if __name__ == "__main__":
    main()
