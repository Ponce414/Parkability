#!/usr/bin/env python3
"""
bully_timing.py — concept 06 (fault tolerance).

Measures recovery time after a leader failure: from the last sensor event
attributed to leader L1 to the first event attributed to leader L2 in the
same zone. The events table records `leader_mac` per event, so leader changes
are detectable purely from backend data.

Run:
    python analysis/bully_timing.py
    python analysis/bully_timing.py --zone zone1
"""

import argparse
import sqlite3
import statistics
import sys


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--db", default="backend/parking.db")
    ap.add_argument("--zone", default=None,
                    help="restrict to a single zone (matches spot_id prefix)")
    args = ap.parse_args()

    con = sqlite3.connect(args.db)
    cur = con.cursor()

    sql = ("SELECT spot_id, received_wall_ts, leader_mac "
           "FROM events WHERE leader_mac IS NOT NULL ORDER BY received_wall_ts")
    rows = cur.execute(sql).fetchall()

    if args.zone:
        prefix = "/" + args.zone + "/"
        rows = [r for r in rows if prefix in r[0]]

    if len(rows) < 2:
        print("not enough events with leader_mac populated to detect failovers")
        sys.exit(0)

    # Group by zone (derived from spot_id "lot/zone/spot").
    failovers_ms = []
    last_per_leader = {}  # zone -> (leader_mac, last_recv_ts)
    for spot_id, recv, leader in rows:
        parts = spot_id.split("/")
        zone = "/".join(parts[:2]) if len(parts) >= 2 else spot_id
        prev = last_per_leader.get(zone)
        if prev is not None and prev[0] != leader:
            failovers_ms.append(recv - prev[1])
        last_per_leader[zone] = (leader, recv)

    if not failovers_ms:
        print(f"no failovers observed across {len(rows)} events")
        print("Trigger one by powering off the current leader and re-running.")
        sys.exit(0)

    failovers_ms.sort()
    print(f"failovers detected: {len(failovers_ms)}")
    print(f"recovery time ms:   min={failovers_ms[0]}  median={failovers_ms[len(failovers_ms)//2]}  max={failovers_ms[-1]}")
    if len(failovers_ms) >= 3:
        print(f"recovery time mean: {statistics.mean(failovers_ms):.1f} ms")
    print()
    print("Recovery time = (last event from old leader) -> (first event from new).")
    print("Bound by HEARTBEAT_TIMEOUT_MS + BULLY_T_OK_MS in leader-node config.h.")


if __name__ == "__main__":
    main()
