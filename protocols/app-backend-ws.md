# App ↔ Backend WebSocket

Endpoint: `ws://<backend-host>:8000/ws`.

## Lifecycle

1. Client connects.
2. Server immediately sends a `zone_status` snapshot.
3. Server pushes `spot_state_changed` on every accepted sensor event.
4. Server pushes `leader_changed` whenever a zone's leader changes.
5. Client disconnect is detected on any read error; [`backend/ws.py`](../backend/ws.py) evicts.

## Event: `zone_status` (server → client, on connect)

```json
{
  "type": "zone_status",
  "lot_id": "lotA",
  "zones": [
    {
      "zone_id": "zone1",
      "leader_mac": "AA:BB:CC:DD:EE:FF",
      "spots": [
        { "spot_id": "lotA/zone1/spot1", "state": "FREE", "last_update": 1714000000 }
      ]
    }
  ]
}
```

## Event: `spot_state_changed` (server → client)

```json
{
  "type": "spot_state_changed",
  "spot_id": "lotA/zone1/spot5",
  "state": "OCCUPIED",
  "lamport_ts": 42,
  "wall_ts": 1714000100
}
```

## Event: `leader_changed` (server → client)

```json
{
  "type": "leader_changed",
  "zone_id": "zone1",
  "leader_mac": "AA:BB:CC:DD:EE:FF"
}
```

## Client-side considerations

- Clients must not rely on per-spot ordering from the wire; treat the first
  `zone_status` after every (re)connect as authoritative and overlay deltas
  on top.
- `lamport_ts` may arrive slightly out of `wall_ts` order. That's expected —
  see [`concepts/07-logical-time/`](../concepts/07-logical-time/README.md).
- No inbound messages are accepted in v1.
