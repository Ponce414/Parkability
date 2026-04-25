# 01 — Network Communication

How the system uses different network technologies for different purposes.

## What we did

Three transport layers are in play, each chosen for its tier:

| Tier | Transport | Why |
|---|---|---|
| Sensor ↔ Leader | ESP-NOW (L2, no IP) | No router needed, low power, low latency, peer-to-peer |
| Leader ↔ Backend | MQTT or REST over WiFi/TCP | Crosses the LAN; MQTT for fan-in, REST as a simpler fallback |
| Backend ↔ App | WebSocket over TCP | Browser-native, bidirectional, push-based |

Each transport has its own message format, defined explicitly:
- ESP-NOW: packed C structs in [`firmware/shared/messages.h`](../../firmware/shared/messages.h)
- MQTT/REST: JSON, schema in [`protocols/leader-backend-api.md`](../../protocols/leader-backend-api.md)
- WebSocket: JSON, schema in [`protocols/app-backend-ws.md`](../../protocols/app-backend-ws.md)

## Key code

- [`firmware/shared/messages.h`](../../firmware/shared/messages.h) — ESP-NOW frame layouts
- [`backend/ingest_mqtt.py`](../../backend/ingest_mqtt.py) — MQTT subscriber bridging to asyncio
- [`backend/ingest_rest.py`](../../backend/ingest_rest.py) — REST endpoints for leaders
- [`backend/ws.py`](../../backend/ws.py) — WebSocket manager

## Tradeoff we made

ESP-NOW skips IP entirely, saving ~30ms of association time per send and
avoiding any need for a router on-site, but caps us at ~250-byte payloads
and ~20 peers per node. That cap is exactly why we have a hierarchical zone
architecture (see concept 08).
