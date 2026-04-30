import { useCallback, useEffect, useMemo, useRef, useState } from 'react'
import { useLotState } from './useLotState'

/**
 * Adapter: bridges our WebSocket-driven lot state into the snapshot shape
 * the dashboard UI was originally built around (id, occupied, sensorOnline,
 * distance, lastUpdated; gateway; events; meta).
 *
 * Important contract differences from the original polling version:
 *   - WS pushes state on change; the Refresh button also polls the backend
 *     every second for one minute for hands-on sensor checks.
 *   - Events are derived client-side from state transitions, since the
 *     backend WS protocol doesn't push a separate event log.
 *   - "lastRefresh" tracks the newest observed spot update.
 */

function defaultWsUrl() {
  const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:'
  return `${protocol}//${window.location.hostname}:8000/ws`
}

function defaultStateUrl() {
  return `${window.location.protocol}//${window.location.hostname}:8000/lot/lotA/state`
}

const WS_URL = import.meta.env.VITE_WS_URL || defaultWsUrl()
const STATE_URL = import.meta.env.VITE_STATE_URL || defaultStateUrl()
const EVENT_LOG_MAX = 8
const REFRESH_WINDOW_MS = 60000
const REFRESH_POLL_MS = 1000
const OCCUPANCY_THRESHOLD_MM = 30
const C5_NODES = [
  {
    id: 'c5-a',
    label: 'ESP32-C5 A',
    shortLabel: 'C5 A',
    mac: '3C:DC:75:85:A8:D0',
    homeSpots: ['1', '2', '3'],
  },
  {
    id: 'c5-b',
    label: 'ESP32-C5 B',
    shortLabel: 'C5 B',
    mac: '3C:DC:75:88:FA:4C',
    homeSpots: ['4', '5', '6'],
  },
]

function normalizeMac(mac) {
  return typeof mac === 'string' && mac ? mac.toUpperCase() : null
}

function shortSpotId(spotId) {
  // "lotA/zone1/spot17" -> "17"; falls through to whole id otherwise.
  if (typeof spotId !== 'string') return String(spotId)
  const parts = spotId.split('/')
  const last = parts[parts.length - 1]
  return last.replace(/^spot/, '') || last
}

function homeNodeForSpot(spot) {
  return C5_NODES.find((node) => node.homeSpots.includes(String(spot.displayId)))
}

function adaptSpot(s) {
  const lastUpdateMs = Number(s.lastUpdate) || 0
  const sensorOnline = lastUpdateMs > 0 && Date.now() - lastUpdateMs < 30000
  const distance = s.rawDistanceMm == null ? null : Number(s.rawDistanceMm)
  const hasDistance = Number.isFinite(distance) && distance > 0
  const state =
    s.state !== 'UNKNOWN' && hasDistance
      ? distance <= OCCUPANCY_THRESHOLD_MM
        ? 'OCCUPIED'
        : 'FREE'
      : s.state

  return {
    id: s.spotId,
    displayId: shortSpotId(s.spotId),
    zoneId: s.zoneId,
    state,
    occupied: state === 'OCCUPIED',
    sensorOnline,
    distance: sensorOnline && hasDistance ? distance : null,
    lastUpdated: lastUpdateMs ? new Date(lastUpdateMs).toISOString() : null,
    leaderMac: normalizeMac(s.leaderMac),
  }
}

function buildControllerStatus(spots, zoneLeaderMacs) {
  const activeZoneLeaders = zoneLeaderMacs.map(normalizeMac).filter(Boolean)
  const nodes = C5_NODES.map((node) => {
    const homeSpots = spots.filter((spot) => node.homeSpots.includes(String(spot.displayId)))
    const servedSpots = spots.filter((spot) => spot.sensorOnline && spot.leaderMac === node.mac)
    const actingForSpots = servedSpots.filter((spot) => homeNodeForSpot(spot)?.mac !== node.mac)
    const failedOverHomeSpots = homeSpots.filter(
      (spot) => spot.sensorOnline && spot.leaderMac && spot.leaderMac !== node.mac,
    )
    const newestServedMs = servedSpots.reduce((latest, spot) => {
      return Math.max(latest, spot.lastUpdated ? new Date(spot.lastUpdated).getTime() : 0)
    }, 0)
    const isCoordinator = activeZoneLeaders.includes(node.mac)
    const online = isCoordinator || servedSpots.length > 0

    let status = 'No live traffic'
    let tone = 'warning'
    if (actingForSpots.length > 0) {
      status = 'Acting leader'
      tone = 'lime'
    } else if (servedSpots.length > 0) {
      status = isCoordinator ? 'Coordinator' : 'Maintaining'
      tone = 'cyan'
    } else if (failedOverHomeSpots.length > 0) {
      status = 'Failing over'
      tone = 'coral'
    } else if (isCoordinator) {
      status = 'Coordinator idle'
      tone = 'cyan'
    }

    return {
      ...node,
      online,
      status,
      tone,
      isCoordinator,
      homeSpots,
      servedSpots,
      actingForSpots,
      failedOverHomeSpots,
      lastSeen: newestServedMs ? new Date(newestServedMs).toISOString() : null,
    }
  })

  const actingNodes = nodes.filter((node) => node.actingForSpots.length > 0)
  const failedNodes = nodes.filter((node) => node.failedOverHomeSpots.length > 0 && node.servedSpots.length === 0)
  const onlineCount = nodes.filter((node) => node.online).length
  const failoverActive = actingNodes.length > 0 || failedNodes.length > 0

  let summary = 'No C5 traffic'
  let detail = 'Waiting for the leaders to publish fresh spot telemetry.'
  if (failoverActive) {
    summary = 'Failover active'
    const actingText = actingNodes
      .map((node) => `${node.shortLabel} is carrying ${node.actingForSpots.map((spot) => `spot ${spot.displayId}`).join(', ')}`)
      .join(' and ')
    detail = actingText || 'One C5 home group is being served by the other C5.'
  } else if (onlineCount === C5_NODES.length) {
    summary = 'Both C5s active'
    detail = 'Each leader is maintaining live telemetry for its assigned group.'
  } else if (onlineCount === 1) {
    summary = 'Single C5 active'
    detail = 'Only one C5 is visible through live spot telemetry right now.'
  }

  return {
    nodes,
    summary,
    detail,
    failoverActive,
    onlineCount,
    expectedCount: C5_NODES.length,
    activeLeaderMacs: activeZoneLeaders,
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
  const occupiedSpots = snapshot.spots.filter((s) => s.state === 'OCCUPIED' && s.sensorOnline).length
  const openSpots = snapshot.spots.filter((s) => s.state === 'FREE' && s.sensorOnline).length
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
  const { lotState, connectionStatus, refreshFromBackend } = useLotState(WS_URL, STATE_URL)

  const [events, setEvents] = useState([])
  const [lastRefresh, setLastRefresh] = useState(null)
  const [clockTick, setClockTick] = useState(() => Date.now())
  const [refreshActiveUntil, setRefreshActiveUntil] = useState(0)
  const [isRefreshing, setIsRefreshing] = useState(false)
  const [refreshError, setRefreshError] = useState('')
  const prevStateRef = useRef(new Map()) // spotId -> state
  const prevLeaderRef = useRef(new Map()) // zoneId -> mac

  useEffect(() => {
    const id = window.setInterval(() => setClockTick(Date.now()), 1000)
    return () => window.clearInterval(id)
  }, [])

  const pullBackendSnapshot = useCallback(async () => {
    setIsRefreshing(true)
    setRefreshError('')
    try {
      await refreshFromBackend()
      setLastRefresh(new Date().toISOString())
    } catch (err) {
      setRefreshError(err instanceof Error ? err.message : 'Backend refresh failed')
    } finally {
      setIsRefreshing(false)
    }
  }, [refreshFromBackend])

  useEffect(() => {
    if (!refreshActiveUntil) return undefined

    const id = window.setInterval(() => {
      if (Date.now() >= refreshActiveUntil) {
        setRefreshActiveUntil(0)
        return
      }
      pullBackendSnapshot()
    }, REFRESH_POLL_MS)

    return () => window.clearInterval(id)
  }, [pullBackendSnapshot, refreshActiveUntil])

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

  useEffect(() => {
    const newestSpotUpdate = allSpots.reduce((latest, spot) => {
      return Math.max(latest, Number(spot.lastUpdate) || 0)
    }, 0)
    if (newestSpotUpdate) {
      setLastRefresh(new Date(newestSpotUpdate).toISOString())
    }
  }, [allSpots])

  const snapshot = useMemo(() => {
    const leaderMacs = (lotState.zones || []).map((z) => z.leaderMac).filter(Boolean)
    const newestSpotUpdate = allSpots.reduce((latest, spot) => {
      return Math.max(latest, Number(spot.lastUpdate) || 0)
    }, 0)
    const adaptedSpots = allSpots.map(adaptSpot)
    return {
      spots: adaptedSpots,
      gateway: {
        online: connectionStatus === 'connected',
        lastSync: newestSpotUpdate ? new Date(newestSpotUpdate).toISOString() : lastRefresh,
        leaders: leaderMacs,
      },
      controllerStatus: buildControllerStatus(adaptedSpots, leaderMacs),
      events,
      meta: {
        mode: 'live',
        source: connectionStatus === 'connected' ? 'WebSocket push' : 'Reconnecting',
        lastIngestedAt: newestSpotUpdate ? new Date(newestSpotUpdate).toISOString() : lastRefresh,
        clockTick,
      },
    }
  }, [allSpots, connectionStatus, lastRefresh, events, lotState.zones, clockTick])

  const derived = useMemo(() => buildDerivedState(snapshot), [snapshot])

  return {
    snapshot,
    derived,
    loading: connectionStatus === 'connecting',
    error: '',
    refreshError,
    lastRefresh,
    connectionStatus,
    isRefreshing,
    refreshWindowActive: refreshActiveUntil > clockTick,
    refreshSecondsRemaining: Math.max(0, Math.ceil((refreshActiveUntil - clockTick) / 1000)),
    isAutoRefresh: refreshActiveUntil > clockTick,
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
      setRefreshActiveUntil(Date.now() + REFRESH_WINDOW_MS)
      pullBackendSnapshot()
    },
  }
}
