import React from 'react';
import Spot from './Spot.jsx';

export default function LotView({ lotState }) {
  if (!lotState.zones || lotState.zones.length === 0) {
    return (
      <div className="empty">
        No spots reporting yet. Flash a sensor and pair it with a leader to
        start seeing live state here.
      </div>
    );
  }

  return (
    <div className="lot">
      {lotState.zones.map(zone => (
        <section key={zone.zoneId} className="zone">
          <h2>
            {zone.zoneId}
            {zone.leaderMac && <span className="leader-mac"> — leader {zone.leaderMac}</span>}
          </h2>
          <div className="spot-grid">
            {zone.spots.map(spot => (
              <Spot key={spot.spotId} spot={spot} />
            ))}
          </div>
        </section>
      ))}
    </div>
  );
}
