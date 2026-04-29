"""SQLite access layer.

All other backend modules should go through this module rather than touching
sqlite3 directly. Functions are small and parameterized; no ORM by choice.
"""
from __future__ import annotations

import os
import sqlite3
import time
from contextlib import contextmanager
from pathlib import Path
from typing import Iterator, Optional

DB_PATH = Path(os.environ.get("PARKING_DB_PATH", "parking.db"))
SCHEMA_PATH = Path(__file__).parent / "schema.sql"


@contextmanager
def connect() -> Iterator[sqlite3.Connection]:
    """Yield a connection with foreign keys and row-factory set up."""
    conn = sqlite3.connect(DB_PATH, isolation_level=None)
    conn.execute("PRAGMA foreign_keys = ON")
    conn.row_factory = sqlite3.Row
    try:
        yield conn
    finally:
        conn.close()


def init_db() -> None:
    """Create tables if missing. Safe to call repeatedly."""
    with connect() as conn:
        conn.executescript(SCHEMA_PATH.read_text())


def ensure_zone(zone_id: str, lot_id: str) -> None:
    with connect() as conn:
        conn.execute(
            "INSERT OR IGNORE INTO zones (zone_id, lot_id) VALUES (?, ?)",
            (zone_id, lot_id),
        )


def upsert_spot(
    spot_id: str,
    zone_id: str,
    state: str,
    lamport_ts: int,
    received_wall_ts: int,
    raw_distance_mm: Optional[int],
) -> None:
    """Idempotent write of the current view.

    `received_wall_ts` is backend receive time, so the dashboard can show
    useful relative freshness even though device `wall_ts` is uptime-based.
    The append-only events table still preserves the raw device timestamp.
    """
    with connect() as conn:
        conn.execute(
            """
            INSERT INTO spots (spot_id, zone_id, current_state,
                               last_update_wall_ts, last_lamport_ts, last_raw_distance_mm)
            VALUES (?, ?, ?, ?, ?, ?)
            ON CONFLICT(spot_id) DO UPDATE SET
                current_state        = excluded.current_state,
                last_update_wall_ts  = excluded.last_update_wall_ts,
                last_lamport_ts      = excluded.last_lamport_ts,
                last_raw_distance_mm = excluded.last_raw_distance_mm
            """,
            (spot_id, zone_id, state, received_wall_ts, lamport_ts, raw_distance_mm),
        )


def record_event(
    spot_id: str,
    state: str,
    lamport_ts: int,
    wall_ts: int,
    raw_distance_mm: Optional[int],
    leader_mac: Optional[str],
) -> None:
    received = int(time.time() * 1000)
    with connect() as conn:
        conn.execute(
            """
            INSERT INTO events (spot_id, state, lamport_ts, wall_ts,
                                raw_distance_mm, leader_mac, received_wall_ts)
            VALUES (?, ?, ?, ?, ?, ?, ?)
            """,
            (spot_id, state, lamport_ts, wall_ts, raw_distance_mm,
             leader_mac, received),
        )


def update_zone_leader(zone_id: str, new_leader_mac: str) -> None:
    now_ms = int(time.time() * 1000)
    with connect() as conn:
        conn.execute(
            """
            UPDATE zones
               SET current_leader_mac = ?, last_leader_change_ts = ?
             WHERE zone_id = ?
            """,
            (new_leader_mac, now_ms, zone_id),
        )


def get_lot_snapshot(lot_id: str) -> dict:
    with connect() as conn:
        zones = conn.execute(
            "SELECT zone_id, current_leader_mac FROM zones WHERE lot_id = ?",
            (lot_id,),
        ).fetchall()
        result = {"lot_id": lot_id, "zones": []}
        for z in zones:
            spots = conn.execute(
                """SELECT spot_id, current_state, last_update_wall_ts,
                          last_lamport_ts, last_raw_distance_mm
                     FROM spots WHERE zone_id = ?""",
                (z["zone_id"],),
            ).fetchall()
            result["zones"].append({
                "zone_id": z["zone_id"],
                "leader_mac": z["current_leader_mac"],
                "spots": [dict(s) for s in spots],
            })
        return result


def get_zone_snapshot(zone_id: str) -> dict:
    with connect() as conn:
        z = conn.execute(
            "SELECT zone_id, lot_id, current_leader_mac FROM zones WHERE zone_id = ?",
            (zone_id,),
        ).fetchone()
        if z is None:
            return {}
        spots = conn.execute(
            """SELECT spot_id, current_state, last_update_wall_ts,
                      last_lamport_ts, last_raw_distance_mm
                 FROM spots WHERE zone_id = ?""",
            (zone_id,),
        ).fetchall()
        return {
            "zone_id": z["zone_id"],
            "lot_id": z["lot_id"],
            "leader_mac": z["current_leader_mac"],
            "spots": [dict(s) for s in spots],
        }


def get_events_since(spot_id: str, since_wall_ts: int) -> list[dict]:
    with connect() as conn:
        rows = conn.execute(
            """SELECT * FROM events
                WHERE spot_id = ? AND wall_ts >= ?
                ORDER BY wall_ts ASC""",
            (spot_id, since_wall_ts),
        ).fetchall()
        return [dict(r) for r in rows]
