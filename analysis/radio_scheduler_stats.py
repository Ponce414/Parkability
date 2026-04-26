#!/usr/bin/env python3
"""
radio_scheduler_stats.py — concept 02 (concurrency / coordination).

The leader's radio scheduler arbitrates the single C3 radio between ESP-NOW
and WiFi (see firmware/leader-node/src/radio_scheduler.c). It exposes
RadioStats counters via radio_scheduler_get_stats(): espnow_windows,
wifi_windows, mode_switches, espnow_dropped_due_to_mode, wifi_dropped_due_to_mode.

To get those stats off the device, either:
  1. Snapshot via UART log lines (the scheduler logs each mode switch), then
     paste the resulting JSON / counters into a file and feed it here.
  2. Add a /stats endpoint to wifi_uplink and have the backend log it. (TODO.)

For now this script accepts a JSON file with the format:
    {"espnow_windows":N, "wifi_windows":N, "mode_switches":N,
     "espnow_dropped":N, "wifi_dropped":N, "duration_s":N}

Run:
    python analysis/radio_scheduler_stats.py --stats analysis/runs/scheduler.json
"""

import argparse
import json
import sys


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--stats", required=True,
                    help="JSON dump of RadioStats with duration_s field")
    args = ap.parse_args()

    try:
        with open(args.stats) as f:
            s = json.load(f)
    except FileNotFoundError:
        print(f"file not found: {args.stats}")
        print("Capture from the leader's UART log and write a JSON snapshot:")
        print('  {"espnow_windows":42,"wifi_windows":18,"mode_switches":60,')
        print('   "espnow_dropped":3,"wifi_dropped":1,"duration_s":120}')
        sys.exit(1)

    duration = s.get("duration_s", 1)
    espnow = s.get("espnow_windows", 0)
    wifi = s.get("wifi_windows", 0)
    switches = s.get("mode_switches", 0)
    espnow_drop = s.get("espnow_dropped", 0)
    wifi_drop = s.get("wifi_dropped", 0)

    total = espnow + wifi
    if total == 0:
        print("no windows recorded")
        sys.exit(0)

    print(f"duration:                {duration} s")
    print(f"ESP-NOW windows:         {espnow} ({100 * espnow / total:.1f}%)")
    print(f"WiFi windows:            {wifi} ({100 * wifi / total:.1f}%)")
    print(f"mode switches:           {switches}  ({switches / max(duration,1):.2f}/s)")
    print(f"ESP-NOW sends dropped:   {espnow_drop}  (radio in WiFi mode)")
    print(f"WiFi sends dropped:      {wifi_drop}  (radio in ESP-NOW mode)")
    print()
    print("Interpretation: mode_switches/s should be roughly 1 / dwell_avg.")
    print("If espnow_dropped is large, the WiFi window is too long or too frequent")
    print("for sensor-update latency requirements. Tune ESPNOW_MAX_DWELL_MS /")
    print("WIFI_MAX_DWELL_MS in firmware/leader-node/src/config.h.")


if __name__ == "__main__":
    main()
