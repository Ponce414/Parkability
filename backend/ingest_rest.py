"""REST ingestion — fallback for leaders that don't speak MQTT.

Same semantics as ingest_mqtt.py: accept a sensor event, apply to state,
broadcast on the WebSocket. Kept separate so we can decide per-deployment
which is primary.
"""
from __future__ import annotations

from typing import Optional

from fastapi import APIRouter, HTTPException
from pydantic import BaseModel, Field

from state import lot_state
from ws import manager

router = APIRouter(prefix="/ingest", tags=["ingest"])


class SensorEvent(BaseModel):
    spot_id: str
    zone_id: str
    state: str = Field(pattern="^(OCCUPIED|FREE|UNKNOWN)$")
    lamport_ts: int
    wall_ts: int
    raw_distance_mm: Optional[int] = None
    leader_mac: Optional[str] = None


class LeaderChange(BaseModel):
    zone_id: str
    leader_mac: str


@router.post("/event")
async def ingest_event(evt: SensorEvent) -> dict:
    changed = lot_state.apply_sensor_update(
        evt.spot_id, evt.zone_id, evt.state,
        evt.lamport_ts, evt.wall_ts,
        evt.raw_distance_mm, evt.leader_mac,
    )
    if changed:
        await manager.broadcast_spot_change(
            evt.spot_id, evt.state, evt.lamport_ts, evt.wall_ts,
        )
    return {"changed": changed}


@router.post("/leader")
async def ingest_leader_change(msg: LeaderChange) -> dict:
    lot_state.apply_leader_change(msg.zone_id, msg.leader_mac)
    await manager.broadcast_leader_change(msg.zone_id, msg.leader_mac)
    return {"ok": True}
