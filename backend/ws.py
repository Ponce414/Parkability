"""WebSocket manager for the React app.

On connect: send `zone_status` snapshot.
On state change: broadcast `spot_state_changed` to all connected clients.

Schema lives in protocols/app-backend-ws.md.
"""
from __future__ import annotations

import asyncio
import json
import logging
from typing import Set

from fastapi import WebSocket

log = logging.getLogger("ws")


class WSManager:
    def __init__(self) -> None:
        self._clients: Set[WebSocket] = set()
        self._lock = asyncio.Lock()

    async def connect(self, ws: WebSocket, snapshot: dict, lot_id: str) -> bool:
        await ws.accept()
        payload = {
            "type": "zone_status",
            "lot_id": lot_id,
            "zones": snapshot["zones"],
        }
        try:
            await ws.send_text(json.dumps(payload))
        except Exception as e:
            log.warning("client disconnected during initial snapshot: %s", e)
            return False
        async with self._lock:
            self._clients.add(ws)
        return True

    async def disconnect(self, ws: WebSocket) -> None:
        async with self._lock:
            self._clients.discard(ws)

    async def broadcast_spot_change(
        self,
        spot_id: str,
        state: str,
        lamport_ts: int,
        wall_ts: int,
        raw_distance_mm: int | None = None,
    ) -> None:
        payload = json.dumps({
            "type": "spot_state_changed",
            "spot_id": spot_id,
            "state": state,
            "lamport_ts": lamport_ts,
            "wall_ts": wall_ts,
            "raw_distance_mm": raw_distance_mm,
        })
        stale: list[WebSocket] = []
        async with self._lock:
            targets = list(self._clients)
        for ws in targets:
            try:
                await ws.send_text(payload)
            except Exception as e:
                log.warning("dropping client: %s", e)
                stale.append(ws)
        if stale:
            async with self._lock:
                for ws in stale:
                    self._clients.discard(ws)

    async def broadcast_leader_change(self, zone_id: str, new_leader_mac: str) -> None:
        payload = json.dumps({
            "type": "leader_changed",
            "zone_id": zone_id,
            "leader_mac": new_leader_mac,
        })
        async with self._lock:
            targets = list(self._clients)
        for ws in targets:
            try:
                await ws.send_text(payload)
            except Exception:
                pass


manager = WSManager()
