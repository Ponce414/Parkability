import { useEffect, useMemo, useRef, useState } from 'react'
import { useLotState } from './useLotState'

/**
 * Adapter: bridges our WebSocket-driven lot state into the snapshot shape
 * the dashboard UI was originally built around (id, occupied, sensorOnline,
 * distance, lastUpdated; gateway; events; meta).
 *
 * Important contract differences from the original polling version:
 *   - We don't poll. WS pushes state on change; "lastRefresh" is the last
 *     time we observed any spot transition.
 *   - Events are derived client-side from state transitions, since the
 *     backend WS protocol doesn't push a separate event log.
 *   - Auto-refresh and manual refresh are no-ops; kept on the return type
 *     for compatibility with components that still reference them.
 */

const WS_URL = import.meta.env.VITE_WS_URL || 'ws://localhost:8000/ws'
const EVENT_LOG_MAX = 8

function shortSpotId(spotId) {
  // "lotA/zone1/spot17" -> "17"; falls through to whole id otherwise.
  if (typeof spotId !== 'string') return String(spotId)
  const parts = spotId.split('/')
  const last = parts[parts.length - 1]
  return last.replace(/^spot/, '') || last
}

function adaptSpot(s) {
  return {
    id: s.spotId,
    displayId: shortSpotId(s.spotId),
    occupied: s.state === 'OCCUPIED',
    sensorOnline: s.state === 'OCCUPIED' || s.state === 'FREE',
    distance: null, // raw distance isn't pushed over WS today
    lastUpdated: s.lastUpdate ? new Date(s.lastUpdate).toISOString() : null,
  }
}

function buildDerivedState(snapshot) {
  const totalSpots = snapshot.spots.length
  if (totalSpots === 0) {
    return {
      totalSpots: 0,
      occupiedSpots: 0,
      openSpots: 0,
      offlineSpots: 0,
      onlineSensors: 0,
      occupancyRate: 0,
      systemHealth: snapshot.gateway.online ? 'Awaiting sensors' : 'Offline',
    }
  }
  const occupiedSpots = snapshot.spots.filter((s) => s.occupied && s.sensorOnline).length
  const openSpots = snapshot.spots.filter((s) => !s.occupied && s.sensorOnline).length
  const offlineSpots = snapshot.spots.filter((s) => !s.sensorOnline).length
  const onlineSensors = totalSpots - offlineSpots
  const occupancyRate = Math.round((occupiedSpots / totalSpots) * 100)

  let systemHealth = 'Optimal'
  if (!snapshot.gateway.online) systemHealth = 'Offline'
  else if (offlineSpots > 1) systemHealth = 'Degraded'
  else if (offlineSpots === 1) systemHealth = 'Monitoring'

  return {
    totalSpots,
    occupiedSpots,
    openSpots,
    offlineSpots,
    onlineSensors,
    occupancyRate,
    systemHealth,
  }
}

export function useParkingData() {
  const { lotState, connectionStatus } = useLotState(WS_URL)

  const [events, setEvents] = useState([])
  const [lastRefresh, setLastRefresh] = useState(null)
  const prevStateRef = useRef(new Map()) // spotId -> state
  const prevLeaderRef = useRef(new Map()) // zoneId -> mac

  // Flatten zones->spots for the UI.
  const allSpots = useMemo(
    () =>
      (lotState.zones || []).flatMap((z) =>
        (z.spots || []).map((s) => ({ ...s, zoneId: z.zoneId })),
      ),
    [lotState],
  )

  // Derive client-side event log from state transitions and leader changes.
  useEffect(() => {
    const nextStates = new Map()
    const newEvents = []
    const now = new Date().toISOString()

    for (const s of allSpots) {
      nextStates.set(s.spotId, s.state)
      const prev = prevStateRef.current.get(s.spotId)
      if (prev !== undefined && prev !== s.state) {
        const display = shortSpotId(s.spotId)
        if (s.state === 'OCCUPIED') {
          newEvents.push({
            id: `${s.spotId}-occ-${Date.now()}-${Math.random()}`,
            message: `Spot ${display} changed to occupied`,
            time: now,
            type: 'occupancy',
          })
        } else if (s.state === 'FREE') {
          newEvents.push({
            id: `${s.spotId}-free-${Date.now()}-${Math.random()}`,
            message: `Spot ${display} became available`,
            time: now,
            type: 'availability',
          })
        } else {
          newEvents.push({
            id: `${s.spotId}-off-${Date.now()}-${Math.random()}`,
            message: `Sensor for spot ${display} went offline`,
            time: now,
            type: 'sensor',
          })
        }
      }
    }

    for (const z of lotState.zones || []) {
      const prevLeader = prevLeaderRef.current.get(z.zoneId)
      if (prevLeader !== undefined && prevLeader !== z.leaderMac && z.leaderMac) {
        newEvents.push({
          id: `${z.zoneId}-leader-${Date.now()}`,
          message: `Zone ${z.zoneId} leader changed to ${z.leaderMac}`,
          time: now,
          type: 'leader',
        })
      }
      prevLeaderRef.current.set(z.zoneId, z.leaderMac)
    }

    prevStateRef.current = nextStates
    if (newEvents.length > 0) {
      setEvents((prev) => [...newEvents, ...prev].slice(0, EVENT_LOG_MAX))
      setLastRefresh(now)
    }
  }, [allSpots, lotState.zones])

  const snapshot = useMemo(() => {
    const leaderMacs = (lotState.zones || []).map((z) => z.leaderMac).filter(Boolean)
    return {
      spots: allSpots.map(adaptSpot),
      gateway: {
        online: connectionStatus === 'connected',
        lastSync: lastRefresh,
        leaders: leaderMacs,
      },
      events,
      meta: {
        mode: 'live',
        source: connectionStatus === 'connected' ? 'WebSocket push' : 'Reconnecting',
        lastIngestedAt: lastRefresh,
      },
    }
  }, [allSpots, connectionStatus, lastRefresh, events, lotState.zones])

  const derived = useMemo(() => buildDerivedState(snapshot), [snapshot])

  return {
    snapshot,
    derived,
    loading: connectionStatus === 'connecting',
    error: '',
    lastRefresh,
    connectionStatus,
    // Compatibility shims — refresh is meaningless when the backend pushes.
    isAutoRefresh: true,
    setIsAutoRefresh: () => {},
    dataSource:
      connectionStatus === 'connected'
        ? 'WebSocket • live'
        : connectionStatus === 'reconnecting'
          ? 'Reconnecting'
          : connectionStatus === 'disconnected'
            ? 'Disconnected'
            : 'Connecting',
    refreshNow: () => {
      // No-op for WS, but we can bump lastRefresh so the UI re-renders relative-times.
      setLastRefresh(new Date().toISOString())
    },
  }
}
