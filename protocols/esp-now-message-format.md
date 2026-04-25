# ESP-NOW message format

Canonical: [`firmware/shared/messages.h`](../firmware/shared/messages.h).
This doc is the human-readable companion; the header is the source of truth.

## Framing

Every ESP-NOW payload starts with a `MessageHeader`:

| Offset | Field | Size | Notes |
|---|---|---|---|
| 0 | `protocol_version` | 1 | currently `1` |
| 1 | `msg_type` | 1 | `MessageType` enum |
| 2 | `sender_mac` | 6 | sender STA MAC |
| 8 | `lamport_ts` | 4 | little-endian, matches host byte order on ESP32 |

Receivers MUST drop frames with an unknown `protocol_version`.

ESP-NOW frames are capped at ~250 bytes. All message structs below stay well
under that.

## Message types

### `MSG_SENSOR_UPDATE` (0x01)

Sensor → Leader. Sent on debounced state transitions only, not on every poll.

| Field | Size | Notes |
|---|---|---|
| header | 12 | |
| `spot_id[24]` | 24 | null-terminated, `"lot/zone/spot"` |
| `state` | 1 | `SpotState`: 0=UNKNOWN, 1=FREE, 2=OCCUPIED |
| `raw_distance_mm` | 2 | last raw reading; backend uses this for offline re-calibration |
| `wall_ts` | 4 | sensor's monotonic ms since boot |

### `MSG_HEARTBEAT` (0x02)

Leader ↔ Leader (broadcast). Liveness ping on `HEARTBEAT_INTERVAL_MS`.

| Field | Notes |
|---|---|
| header | |
| `node_priority` | uint32, derived from low 4 bytes of MAC |
| `is_current_leader` | 0 or 1 |

### `MSG_ELECTION` (0x03) / `MSG_OK` (0x04) / `MSG_COORDINATOR` (0x05)

Bully election traffic. See [`firmware/leader-node/src/bully.h`](../firmware/leader-node/src/bully.h)
for the state machine.

### `MSG_REGISTER` (0x06) / `MSG_REGISTER_ACK` (0x07)

Sensor → Leader on boot; Leader → Sensor reply. Idempotent by `spot_id`.

## Channel

All ESP-NOW traffic on channel 1 (configurable via `ESPNOW_CHANNEL` in each
node's config). Sensors and leaders must agree on the channel before pairing.

## Known quirks

- Bidirectional ESP-NOW between ESP32-C3 and older ESP32 variants has been
  reported as one-way flaky. Bring-up test: verify both directions.
- Peer limit is ~20 encrypted / ~10 unencrypted. Caps zone size — see
  [`concepts/08-scalability/`](../concepts/08-scalability/README.md).
