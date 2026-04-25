# Architecture

```
   ┌──────────┐  ESP-NOW  ┌──────────┐  WiFi/MQTT  ┌──────────┐  WebSocket  ┌──────────┐
   │  Sensor  │ ────────▶ │  Leader  │ ──────────▶ │  Backend │ ──────────▶ │  React   │
   │ (per     │           │ (per     │             │ (FastAPI │             │   App    │
   │  spot)   │           │  zone)   │             │ + SQLite)│             │          │
   └──────────┘           └──────────┘             └──────────┘             └──────────┘
        ↑                      ↕                                                   ↑
        │                ESP-NOW (heartbeats,                                      │
        │                 bully election)                                          │
        │                      ↕                                                   │
        │                 ┌──────────┐                                             │
        │                 │  Backup  │                                             │
        │                 │  Leader  │                                             │
        │                 └──────────┘                                             │
        └─────── one VL53L0X per spot ──── live spot grid ───────────────────────────
```

## Tier responsibilities

| Tier | Failure mode | Recovery |
|---|---|---|
| Sensor | dies silently | leader notices stale state on display; manual reflash |
| Leader (active) | dies | bully election promotes backup |
| Leader (backup) | dies | active still works; rubric-relevant case is "active dies first" |
| Backend | dies | leader's MQTT messages queue at the broker; REST returns 5xx, no buffering |
| App | dies | irrelevant — re-renders on reconnect |

## Data flow

1. Sensor polls VL53L0X at 10Hz
2. Debounce: 3-of-5 readings agreeing flips state
3. On state change, sensor stamps a Lamport time and sends `MSG_SENSOR_UPDATE` over ESP-NOW
4. Leader receives, hands to aggregator
5. Aggregator publishes to backend via MQTT or REST (compile-time switch)
6. Backend updates in-memory snapshot and SQLite atomically
7. If state changed, broadcast `spot_state_changed` to all WebSocket clients
8. App's `useLotState` hook merges the delta into its state, re-renders affected `<Spot/>`

## What's where

- Firmware: [`firmware/`](../firmware/)
- Backend: [`backend/`](../backend/)
- App: [`app/`](../app/)
- Wire formats: [`protocols/`](../protocols/)
- Per-rubric-concept writeups with code pointers: [`concepts/`](../concepts/)
- Architecture decision records: [`docs/decisions/`](decisions/)
