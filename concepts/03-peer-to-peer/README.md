# 03 — Peer-to-Peer

Where the system uses peer-to-peer rather than client-server.

## What we did

The intra-zone tier (sensors and leaders) is fully peer-to-peer over ESP-NOW:
- No central broker for sensor → leader traffic; sensors send directly.
- Leaders heartbeat each other directly to monitor liveness.
- Bully election runs entirely peer-to-peer with no external coordinator.

The leader-to-backend tier is deliberately *client-server*. It made the
contrast clear and pragmatic: sensors don't have to know about IPs, but the
backend is a natural single source of truth for the app.

## Key code

- [`firmware/leader-node/src/espnow_handler.c`](../../firmware/leader-node/src/espnow_handler.c) — peer registration, broadcast send
- [`firmware/leader-node/src/heartbeat.c`](../../firmware/leader-node/src/heartbeat.c) — peer-to-peer liveness
- [`firmware/leader-node/src/bully.c`](../../firmware/leader-node/src/bully.c) — distributed election with no central coordinator
- Contrast: [`firmware/leader-node/src/wifi_uplink.c`](../../firmware/leader-node/src/wifi_uplink.c) is plain client-server

## Tradeoff we made

ESP-NOW peer-to-peer eliminates the leader as a bottleneck for *receiving*
sensor data, but the leader is still the egress point to the backend, so
we get the failure mode of a single point of failure per zone — solved by
the bully algorithm electing a backup.
