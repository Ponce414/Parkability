"""FastAPI entry point.

Run with:  uvicorn backend.main:app --reload --host 0.0.0.0 --port 8000
(from the repo root; `cd backend && uvicorn main:app` also works because
of the `__init__.py`.)
"""
from __future__ import annotations

import asyncio
import logging
from contextlib import asynccontextmanager

from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException
from fastapi.middleware.cors import CORSMiddleware

import db
import ingest_mqtt
from ingest_rest import router as ingest_router
from state import lot_state
from ws import manager

logging.basicConfig(level=logging.INFO, format="%(levelname)s %(name)s: %(message)s")
log = logging.getLogger("main")

DEFAULT_LOT_ID = "lotA"


@asynccontextmanager
async def lifespan(app: FastAPI):
    db.init_db()
    lot_state.reload_from_db(DEFAULT_LOT_ID)
    loop = asyncio.get_event_loop()
    ingest_mqtt.start_on_loop(loop)
    log.info("backend ready")
    yield
    ingest_mqtt.stop()


app = FastAPI(title="Parking Lot Backend", lifespan=lifespan)
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["GET", "POST", "OPTIONS"],
    allow_headers=["*"],
)
app.include_router(ingest_router)


@app.get("/health")
def health() -> dict:
    return {"ok": True}


@app.get("/lot/{lot_id}/state")
def lot_state_endpoint(lot_id: str) -> dict:
    snap = db.get_lot_snapshot(lot_id)
    if not snap["zones"] and lot_id != DEFAULT_LOT_ID:
        raise HTTPException(status_code=404, detail="lot not found")
    return snap


@app.get("/zone/{zone_id}/state")
def zone_state_endpoint(zone_id: str) -> dict:
    snap = db.get_zone_snapshot(zone_id)
    if not snap:
        raise HTTPException(status_code=404, detail="zone not found")
    return snap


@app.websocket("/ws")
async def ws_endpoint(ws: WebSocket) -> None:
    """App clients connect here to receive live updates.

    Flow:
      1. Accept and send an initial `zone_status` snapshot
      2. Read messages in a loop so we notice client disconnects
         (we don't currently accept any inbound commands)
    """
    snap = lot_state.snapshot()
    connected = await manager.connect(ws, snap, DEFAULT_LOT_ID)
    if not connected:
        return
    try:
        while True:
            await ws.receive_text()  # discard; just detecting disconnects
    except WebSocketDisconnect:
        pass
    finally:
        await manager.disconnect(ws)
