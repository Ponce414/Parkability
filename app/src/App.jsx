import React from 'react';
import LotView from './components/LotView.jsx';
import { useLotState } from './hooks/useLotState.js';

const WS_URL = import.meta.env.VITE_WS_URL || 'ws://localhost:8000/ws';

export default function App() {
  const { lotState, connectionStatus } = useLotState(WS_URL);

  return (
    <div className="app">
      <header className="app-header">
        <h1>Parking Lot — Live</h1>
        <ConnectionBadge status={connectionStatus} />
      </header>
      <main>
        <LotView lotState={lotState} />
      </main>
    </div>
  );
}

function ConnectionBadge({ status }) {
  const label = {
    connecting: 'Connecting…',
    connected: 'Connected',
    reconnecting: 'Reconnecting…',
    disconnected: 'Disconnected',
  }[status] || status;
  return <span className={`badge badge-${status}`}>{label}</span>;
}
