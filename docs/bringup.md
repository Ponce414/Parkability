# Bring-up procedure

End-to-end checklist for getting one zone live: backend → app → leader →
sensor. Follow in order. Each step has a "you should see" line so you can
catch failures early.

## 0. Prerequisites

- Python 3.11+, Node 18+, PlatformIO (`pip install platformio`)
- One ESP32-C5 wired as **leader** (no sensor; just on USB power)
- One or more ESP32-C3 + VL53L0X as **sensors** (SDA=GPIO4, SCL=GPIO5, 3V3, GND)
- A WiFi AP the leader can reach (2.4 GHz, WPA2-PSK)
- Optional: Mosquitto broker on the same network for MQTT mode

## 1. Bring up the backend

```bash
make backend-install
make db-init                # creates backend/parking.db
make backend-run            # leave running in terminal 1
```

You should see uvicorn boot on port 8000 and a log line for the schema.

Smoke test:
```bash
curl http://localhost:8000/api/zones/zone1
```
Returns JSON with an empty `spots` list and `current_leader_mac: null`.

## 2. Bring up the app

```bash
make app-install
make app-run                # terminal 2
```

Open http://localhost:5173. Header should say **Connected**, lot view should
say "no spots yet". This confirms the WebSocket plumbing.

## 3. Configure and flash the leader

Edit [`firmware/leader-node/src/config.h`](../firmware/leader-node/src/config.h):

| Setting | Notes |
|---|---|
| `ZONE_LOT`, `ZONE_ID` | Match what the backend expects (`lotA` / `zone1` by default) |
| `WIFI_SSID`, `WIFI_PASSWORD` | The AP the leader will associate with |
| `UPLINK_PROTOCOL_MQTT` vs `_REST` | Pick one; comment the other |
| `MQTT_BROKER_HOST` or `REST_BACKEND_HOST` | The IP of the machine running the backend |
| `ESPNOW_CHANNEL` | Must match sensor-node config |

Flash:
```bash
cd firmware/leader-node && pio run -t upload && pio device monitor
```

You should see, in order:
1. `bully init priority=0x... state=IDLE`
2. `uplink init MQTT mqtt://...` (or `uplink init REST http://...`)
3. `wifi got IP`
4. `mqtt connected` (MQTT mode only)
5. After ~3.5s with no other leaders heard: `declared self coordinator`
6. The backend should log `leader change zone=zone1 mac=...` and the app
   header should show the leader MAC.

## 4. Pair and flash sensors

Capture the leader's MAC from its UART log line `bully init` (or from your
router's DHCP table). Then for each sensor:

Edit [`firmware/sensor-node/src/config.h`](../firmware/sensor-node/src/config.h):

| Setting | Notes |
|---|---|
| `LEADER_MAC` | Paste the leader's MAC bytes |
| `SPOT_ID` | Unique per sensor, e.g. `"lotA/zone1/spot17"` |
| `ESPNOW_CHANNEL` | Same as leader |
| `I2C_SDA_GPIO`, `I2C_SCL_GPIO` | Defaults are GPIO4/GPIO5; firmware auto-probes common alternate pairs |

Flash:
```bash
cd firmware/sensor-node && pio run -t upload && pio device monitor
```

You should see:
1. `vl53 init ok`
2. `register sent` — sensor announces itself to the leader
3. Leader log: `register spot=... fw=1 ack_rc=0`
4. App: a tile appears for the spot, defaulting to UNKNOWN
5. Wave a hand in front of the sensor → tile flips to OCCUPIED → FREE

## 5. Verify the seven concepts

Once at least one sensor is reporting steady-state:

| Concept | How to verify | Where to look |
|---|---|---|
| 01 Network communication | App updates within 1-2s of physical change | Browser tile color |
| 02 Concurrency | `analysis/radio_scheduler_stats.py` shows mode switches | Leader UART log + JSON dump |
| 04 Naming | All spot tiles use `lot/zone/spot` form | App + `events` table |
| 05 Consistency | `analysis/propagation_delay.py` produces percentiles | `backend/parking.db` |
| 06 Fault tolerance | Power off the leader; backup wins within ~5s | App header changes leader; `analysis/bully_timing.py` |
| 07 Logical time | `analysis/lamport_vs_wall.py` runs without error | `events.lamport_ts` column |
| 08 Scalability | Add sensors until peer table fills (~19 max) | Leader log `peer table full` |

## 6. Failure modes you may hit

| Symptom | Likely cause |
|---|---|
| Leader logs `wifi disconnected, retrying` repeatedly | Wrong SSID/password or 5GHz-only AP |
| Leader logs `mqtt disconnected` | Broker host unreachable or port blocked |
| Sensor UART silent after `register sent` | Wrong `LEADER_MAC` or wrong `ESPNOW_CHANNEL` |
| App says "Connected" but no spots | Sensor never registered — check leader log for `register spot=...` |
| Spot tile flickers OCCUPIED/FREE | Lower `OCCUPANCY_THRESHOLD_MM` or raise `DEBOUNCE_COUNT` |
| Two leaders both claim coordinator | Channel mismatch — they can't hear each other's heartbeats |

## 7. Repeating bring-up for a second zone

Flash a second leader with `ZONE_ID="zone2"` and unique `LEADER_FIRMWARE_VERSION`
priority. Backend will create the zone row on first event. Leaders only run
bully *within* a zone — across zones they ignore each other.
