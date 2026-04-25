import React from 'react';

const STATE_CLASS = {
  FREE: 'spot-free',
  OCCUPIED: 'spot-occupied',
  UNKNOWN: 'spot-unknown',
};

export default function Spot({ spot }) {
  const cls = STATE_CLASS[spot.state] || STATE_CLASS.UNKNOWN;
  const shortId = spot.spotId.split('/').pop();
  return (
    <div className={`spot ${cls}`} title={`${spot.spotId} — Lamport ${spot.lamportTs ?? '?'}`}>
      <div className="spot-id">{shortId}</div>
      <div className="spot-state">{spot.state}</div>
    </div>
  );
}
