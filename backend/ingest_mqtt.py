"""MQTT ingestion from zone leaders.

Subscribes to `parking/+/+/events` (lot/zone wildcarded) and applies each
message to LotState. Uses paho-mqtt with a background thread, which we
bridge back to the asyncio loop for WebSocket broadcasts.
"""
from __future__ import annotations

import asyncio
import json
import logging
import os
from typing import Optional

import paho.mqtt.client as mqtt

from state import lot_state
from ws import manager

log = logging.getLogger("ingest_mqtt")

MQTT_HOST = os.environ.get("MQTT_HOST", "localhost")
MQTT_PORT = int(os.environ.get("MQTT_PORT", "1883"))
MQTT_TOPIC_EVENTS = "parking/+/+/events"
MQTT_TOPIC_LEADER = "parking/+/+/leader"


class MQTTIngest:
    def __init__(self, loop: asyncio.AbstractEventLoop) -> None:
        self._loop = loop
        self._client = mqtt.Client()
        self._client.on_connect = self._on_connect
        self._client.on_message = self._on_message

    def start(self) -> None:
        try:
            self._client.connect(MQTT_HOST, MQTT_PORT, keepalive=60)
        except Exception as e:
            log.warning("MQTT connect failed (%s); running without MQTT ingestion", e)
            return
        self._client.loop_start()

    def stop(self) -> None:
        try:
            self._client.loop_stop()
            self._client.disconnect()
        except Exception:
            pass

    def _on_connect(self, client, userdata, flags, rc):
        log.info("MQTT connected rc=%s", rc)
        client.subscribe(MQTT_TOPIC_EVENTS)
        client.subscribe(MQTT_TOPIC_LEADER)

    def _on_message(self, client, userdata, msg):
        # Topic structure: parking/<lot>/<zone>/(events|leader)
        parts = msg.topic.split("/")
        if len(parts) != 4:
            return
        _, lot_id, zone_id, kind = parts

        try:
            payload = json.loads(msg.payload.decode("utf-8"))
        except Exception as e:
            log.warning("bad MQTT payload: %s", e)
            return

        if kind == "events":
            self._handle_sensor_event(lot_id, zone_id, payload)
        elif kind == "leader":
            self._handle_leader_change(zone_id, payload)

    def _handle_sensor_event(self, lot_id: str, zone_id: str, p: dict) -> None:
        try:
            spot_id    = p["spot_id"]
            state      = p["state"]
            lamport_ts = int(p["lamport_ts"])
            wall_ts    = int(p["wall_ts"])
            raw        = p.get("raw_distance_mm")
            leader_mac = p.get("leader_mac")
        except (KeyError, ValueError) as e:
            log.warning("malformed event: %s", e)
            return

        received_wall_ts = lot_state.apply_sensor_update(
            spot_id, zone_id, state, lamport_ts, wall_ts, raw, leader_mac,
        )
        if received_wall_ts is not None:
            asyncio.run_coroutine_threadsafe(
                manager.broadcast_spot_change(spot_id, state, lamport_ts, received_wall_ts, raw),
                self._loop,
            )

    def _handle_leader_change(self, zone_id: str, p: dict) -> None:
        new_mac = p.get("leader_mac")
        if not new_mac:
            return
        lot_state.apply_leader_change(zone_id, new_mac)
        asyncio.run_coroutine_threadsafe(
            manager.broadcast_leader_change(zone_id, new_mac),
            self._loop,
        )


_ingest: Optional[MQTTIngest] = None


def start_on_loop(loop: asyncio.AbstractEventLoop) -> None:
    global _ingest
    _ingest = MQTTIngest(loop)
    _ingest.start()


def stop() -> None:
    global _ingest
    if _ingest is not None:
        _ingest.stop()
        _ingest = None
