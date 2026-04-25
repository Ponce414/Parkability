# App

Vite + React, no state-management libs, no UI framework. Connects to the
backend WebSocket at `ws://localhost:8000/ws` (override with
`VITE_WS_URL=...`).

## Run

```
make app-install
make app-run
```

Opens on http://localhost:5173.

## Files

- [src/App.jsx](src/App.jsx) — root component, connection badge
- [src/hooks/useLotState.js](src/hooks/useLotState.js) — WebSocket subscription, exponential-backoff reconnect, handles `zone_status`, `spot_state_changed`, `leader_changed`
- [src/components/LotView.jsx](src/components/LotView.jsx) — grid grouped by zone
- [src/components/Spot.jsx](src/components/Spot.jsx) — one tile (green/red/gray)
- [src/styles.css](src/styles.css) — plain CSS, CSS grid

See `protocols/app-backend-ws.md` for the message schema.
