#!/usr/bin/env python3
"""
lamport_vs_wall.py — concept 07 (logical time).

Demonstrates *why* we use Lamport timestamps. Wall clocks across nodes are
unsynchronized (no NTP on the sensor nodes), so two events with wall_ts A < B
can correspond to causal order B happened-before A. Lamport timestamps
encode causality regardless of clock skew.

We scan the events table and count event pairs where:
  - wall_ts orders them (a, b)
  - lamport_ts orders them (b, a)
That count is a lower bound on the number of causality violations the
backend would log if it trusted wall_ts alone.

Run:
    python analysis/lamport_vs_wall.py
"""

import argparse
import sqlite3
import sys


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--db", default="backend/parking.db")
    ap.add_argument("--limit", type=int, default=5000)
    args = ap.parse_args()

    con = sqlite3.connect(args.db)
    cur = con.cursor()
    rows = cur.execute(
        "SELECT event_id, spot_id, wall_ts, lamport_ts "
        "FROM events ORDER BY event_id LIMIT ?",
        (args.limit,),
    ).fetchall()

    if len(rows) < 2:
        print("need at least 2 events to compare orderings — none found")
        sys.exit(0)

    inversions = 0
    same_lamport = 0
    pairs = 0
    n = len(rows)
    for i in range(n):
        for j in range(i + 1, n):
            _, _, w_i, l_i = rows[i]
            _, _, w_j, l_j = rows[j]
            pairs += 1
            if w_i < w_j and l_j < l_i:
                inversions += 1
            elif w_j < w_i and l_i < l_j:
                inversions += 1
            elif l_i == l_j and w_i != w_j:
                same_lamport += 1

    print(f"events scanned:                 {n}")
    print(f"pairs compared:                 {pairs}")
    print(f"wall/lamport order disagreements: {inversions}")
    print(f"concurrent-by-lamport pairs:    {same_lamport}")
    print()
    if inversions == 0:
        print("No disagreements observed in this sample. Scale up the data set,")
        print("or generate concurrent activity from multiple sensors, to surface")
        print("inversions. (With one bursty sensor and a quiet backend, wall and")
        print("Lamport often agree.)")
    else:
        print(f"Wall-clock ordering would have mis-ordered {inversions} pair(s).")
        print("Lamport ordering preserves causality in those cases.")


if __name__ == "__main__":
    main()
