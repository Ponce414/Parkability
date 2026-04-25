"""In-memory view of current lot state plus write-through to SQLite.

Reads hit the in-memory dict for speed; writes go to both memory and DB.
The DB is the durable source of truth — on startup we reload from it.

Eventual consistency note: this module is the convergence point. Sensors
update at the physical layer with some delay; once updates arrive here the
backend state converges. See concepts/05-consistency-replication/.
"""
from __future__ import annotations

from typing import Optional
from threading import RLock

import db


class LotState:
    def __init__(self) -> None:
        self._lock = RLock()
        # spot_id -> dict(state, lamport_ts, wall_ts, zone_id, raw_distance_mm)
        self._spots: dict[str, dict] = {}
        # zone_id -> leader_mac
        self._zone_leaders: dict[str, Optional[str]] = {}

    def reload_from_db(self, lot_id: str) -> None:
        snap = db.get_lot_snapshot(lot_id)
        with self._lock:
            self._spots.clear()
            self._zone_leaders.clear()
            for zone in snap["zones"]:
                self._zone_leaders[zone["zone_id"]] = zone["leader_mac"]
                for s in zone["spots"]:
                    self._spots[s["spot_id"]] = {
                        "zone_id": zone["zone_id"],
                        "state": s["current_state"],
                        "lamport_ts": s["last_lamport_ts"],
                        "wall_ts": s["last_update_wall_ts"],
                        "raw_distance_mm": None,
                    }

    def apply_sensor_update(
        self,
        spot_id: str,
        zone_id: str,
        state: str,
        lamport_ts: int,
        wall_ts: int,
        raw_distance_mm: Optional[int],
        leader_mac: Optional[str],
    ) -> bool:
        """Returns True if this update changed the stored state (i.e., clients
        should be notified). False if it was a duplicate or an out-of-order
        older message."""
        with self._lock:
            prev = self._spots.get(spot_id)
            # Reject strictly-older updates by Lamport time. Equal timestamps
            # fall back to wall_ts for determinism.
            if prev is not None:
                if lamport_ts < prev["lamport_ts"]:
                    return False
                if lamport_ts == prev["lamport_ts"] and wall_ts <= prev["wall_ts"]:
                    return False

            changed = prev is None or prev["state"] != state

            self._spots[spot_id] = {
                "zone_id": zone_id,
                "state": state,
                "lamport_ts": lamport_ts,
                "wall_ts": wall_ts,
                "raw_distance_mm": raw_distance_mm,
            }

        # Writes outside the lock — SQLite serializes at its own layer.
        db.ensure_zone(zone_id, zone_id.split("/")[0] if "/" in zone_id else "lotA")
        db.upsert_spot(spot_id, zone_id, state, lamport_ts, wall_ts, raw_distance_mm)
        db.record_event(spot_id, state, lamport_ts, wall_ts, raw_distance_mm, leader_mac)

        return changed

    def apply_leader_change(self, zone_id: str, new_leader_mac: str) -> None:
        with self._lock:
            self._zone_leaders[zone_id] = new_leader_mac
        db.update_zone_leader(zone_id, new_leader_mac)

    def snapshot(self) -> dict:
        with self._lock:
            zones: dict[str, dict] = {}
            for spot_id, s in self._spots.items():
                zid = s["zone_id"]
                z = zones.setdefault(zid, {
                    "zone_id": zid,
                    "leader_mac": self._zone_leaders.get(zid),
                    "spots": [],
                })
                z["spots"].append({
                    "spot_id": spot_id,
                    "state": s["state"],
                    "last_update": s["wall_ts"],
                    "lamport_ts": s["lamport_ts"],
                })
            return {"zones": list(zones.values())}


lot_state = LotState()
