# App

Vite + React + Tailwind dashboard. Connects to the backend WebSocket at
`ws://localhost:8000/ws` (override with `VITE_WS_URL=...`).

## Run

```
make app-install
make app-run
```

Opens on http://localhost:5173.

## Architecture

Two layers, intentionally decoupled:

1. **Transport** — [`src/hooks/useLotState.js`](src/hooks/useLotState.js):
   raw WebSocket subscription, exponential-backoff reconnect, handles
   `zone_status`, `spot_state_changed`, `leader_changed` from the backend.
   This file knows nothing about the UI.
2. **Presentation** — [`src/hooks/useParkingData.js`](src/hooks/useParkingData.js):
   adapts our zone/spot shape into the snapshot the dashboard renders
   (`{spots, gateway, events, meta}`), derives an event log from state
   transitions, and exposes a `connectionStatus`. Components import
   only from this hook.

Everything visual is built around the second layer, so a backend
protocol change only touches `useLotState.js` + the adapter.

## Files

- [`src/App.jsx`](src/App.jsx) — theme toggle + `<HomePage />`
- [`src/pages/HomePage.jsx`](src/pages/HomePage.jsx) — section composition
- [`src/components/Navbar.jsx`](src/components/Navbar.jsx)
- [`src/components/HeroSection.jsx`](src/components/HeroSection.jsx) — pulls live spot count
- [`src/components/ParallaxSection.jsx`](src/components/ParallaxSection.jsx)
- [`src/components/Dashboard.jsx`](src/components/Dashboard.jsx) — search + filters + status panel + lot map + activity log
- [`src/components/StatusPanel.jsx`](src/components/StatusPanel.jsx) + [`StatusCard.jsx`](src/components/StatusCard.jsx)
- [`src/components/ParkingLotMap.jsx`](src/components/ParkingLotMap.jsx) — two-row drive lane, tolerant of any spot count
- [`src/components/ParkingSpotCard.jsx`](src/components/ParkingSpotCard.jsx)
- [`src/components/ActivityLog.jsx`](src/components/ActivityLog.jsx) — recent events derived client-side
- [`src/components/Legend.jsx`](src/components/Legend.jsx), [`SectionHeader.jsx`](src/components/SectionHeader.jsx), [`Footer.jsx`](src/components/Footer.jsx), [`ArchitectureSection.jsx`](src/components/ArchitectureSection.jsx)
- [`src/utils/formatters.js`](src/utils/formatters.js) — timestamp + visual-state helpers
- [`src/index.css`](src/index.css) — Tailwind base + Sora/Space Grotesk fonts

See `protocols/app-backend-ws.md` for the WebSocket message schema.

## Stack

| Dep | Why |
|---|---|
| react / react-dom 18 | UI |
| vite 5 | dev server + bundler |
| tailwindcss 3 | utility CSS |
| framer-motion | scroll parallax + card transitions |
| lucide-react | iconography |
