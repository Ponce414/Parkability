# Leader → Backend API

Two transports are supported. Each leader firmware picks one at compile time
via `-DUPLINK_PROTOCOL_MQTT` (default) or `-DUPLINK_PROTOCOL_REST`. The backend
always accepts both.

## MQTT (primary)

Broker runs on the same host as the backend (external Mosquitto or similar;
not bundled). Default port 1883.

### Topics

| Topic | Direction | Payload |
|---|---|---|
| `parking/<lot_id>/<zone_id>/events` | Leader → Backend | sensor event JSON |
| `parking/<lot_id>/<zone_id>/leader` | Leader → Backend | leader-change JSON |

### Sensor event payload

```json
{
  "spot_id": "lotA/zone1/spot5",
  "state": "OCCUPIED",
  "lamport_ts": 42,
  "wall_ts": 1714000100,
  "raw_distance_mm": 820,
  "leader_mac": "AA:BB:CC:DD:EE:FF"
}
```

### Leader-change payload

```json
{
  "zone_id": "zone1",
  "leader_mac": "AA:BB:CC:DD:EE:FF"
}
```

## REST (fallback)

Default port 8000.

| Method | Path | Body | Response |
|---|---|---|---|
| POST | `/ingest/event` | same JSON as MQTT events | `{"changed": bool}` |
| POST | `/ingest/leader` | same JSON as MQTT leader | `{"ok": true}` |
| GET  | `/lot/{lot_id}/state` | — | lot snapshot |
| GET  | `/zone/{zone_id}/state` | — | zone snapshot |
| GET  | `/health` | — | `{"ok": true}` |

## Semantics

- `state` is one of `OCCUPIED`, `FREE`, `UNKNOWN`.
- Duplicate or out-of-order events (by Lamport timestamp) are rejected by
  the backend without error — see [`backend/state.py`](../backend/state.py).
- `record_event` is append-only; the backend always logs the event even when
  it doesn't change the current snapshot.
