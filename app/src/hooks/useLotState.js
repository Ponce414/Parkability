import { useEffect, useRef, useState, useCallback } from 'react';

function normalizeSnapshot(payload) {
  return {
    lotId: payload.lot_id ?? payload.lotId ?? null,
    zones: (payload.zones || []).map(z => ({
      zoneId: z.zone_id ?? z.zoneId,
      leaderMac: z.leader_mac ?? z.leaderMac,
      spots: (z.spots || []).map(s => ({
        spotId: s.spot_id,
        state: s.state || s.current_state,
        lastUpdate: s.last_update ?? s.last_update_wall_ts,
        lamportTs: s.lamport_ts ?? s.last_lamport_ts,
        rawDistanceMm: s.raw_distance_mm ?? s.last_raw_distance_mm ?? null,
        leaderMac: s.leader_mac ?? s.last_leader_mac ?? s.leaderMac ?? null,
      })),
    })),
  };
}

/**
 * Subscribe to the backend WebSocket and maintain a live snapshot of the lot.
 *
 * Returns { lotState, connectionStatus }.
 * lotState shape:
 *   { lotId: string|null,
 *     zones: Array<{ zoneId, leaderMac, spots: Array<{ spotId, state, lastUpdate, lamportTs }> }>
 *   }
 *
 * Reconnects with exponential backoff capped at 10s. Drops nothing on
 * reconnect — the next `zone_status` from the server is treated as truth.
 */
export function useLotState(wsUrl, stateUrl) {
  const [lotState, setLotState] = useState({ lotId: null, zones: [] });
  const [connectionStatus, setConnectionStatus] = useState('connecting');
  const wsRef = useRef(null);
  const backoffRef = useRef(500);
  const closedByCleanupRef = useRef(false);

  const handleMessage = useCallback((event) => {
    let msg;
    try { msg = JSON.parse(event.data); } catch { return; }

    if (msg.type === 'zone_status') {
      setLotState(normalizeSnapshot(msg));
    } else if (msg.type === 'spot_state_changed') {
      setLotState(prev => {
        const zones = prev.zones.map(z => {
          let touched = false;
          const spots = z.spots.map(s => {
            if (s.spotId === msg.spot_id) {
              touched = true;
              return {
                ...s,
                state: msg.state,
                lastUpdate: msg.wall_ts,
                lamportTs: msg.lamport_ts,
                rawDistanceMm: msg.raw_distance_mm ?? null,
                leaderMac: msg.leader_mac ?? s.leaderMac ?? null,
              };
            }
            return s;
          });
          if (!touched && msg.spot_id.startsWith(`${prev.lotId}/${z.zoneId}/`)) {
            spots.push({
              spotId: msg.spot_id,
              state: msg.state,
              lastUpdate: msg.wall_ts,
              lamportTs: msg.lamport_ts,
              rawDistanceMm: msg.raw_distance_mm ?? null,
              leaderMac: msg.leader_mac ?? null,
            });
          }
          return { ...z, spots };
        });
        return { ...prev, zones };
      });
    } else if (msg.type === 'leader_changed') {
      setLotState(prev => ({
        ...prev,
        zones: prev.zones.map(z =>
          z.zoneId === msg.zone_id ? { ...z, leaderMac: msg.leader_mac } : z
        ),
      }));
    }
  }, []);

  useEffect(() => {
    closedByCleanupRef.current = false;

    function connect() {
      setConnectionStatus(prev => prev === 'connected' ? 'reconnecting' : 'connecting');
      const ws = new WebSocket(wsUrl);
      wsRef.current = ws;

      ws.onopen = () => {
        backoffRef.current = 500;
        setConnectionStatus('connected');
      };
      ws.onmessage = handleMessage;
      ws.onerror = () => { /* close handler will fire next */ };
      ws.onclose = () => {
        if (closedByCleanupRef.current) return;
        setConnectionStatus('reconnecting');
        const delay = Math.min(backoffRef.current, 10000);
        backoffRef.current = Math.min(backoffRef.current * 2, 10000);
        setTimeout(connect, delay);
      };
    }

    connect();

    return () => {
      closedByCleanupRef.current = true;
      if (wsRef.current) wsRef.current.close();
    };
  }, [wsUrl, handleMessage]);

  const refreshFromBackend = useCallback(async () => {
    const response = await fetch(stateUrl, { cache: 'no-store' });
    if (!response.ok) {
      throw new Error(`Backend refresh failed (${response.status})`);
    }
    const payload = await response.json();
    setLotState(normalizeSnapshot(payload));
    return payload;
  }, [stateUrl]);

  return { lotState, connectionStatus, refreshFromBackend };
}
