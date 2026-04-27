import { Activity, AlertTriangle, CarFront, CircleGauge, RadioTower, Wifi } from 'lucide-react'
import StatusCard from './StatusCard'
import { formatTimestamp } from '../utils/formatters'

function StatusPanel({ derived, gateway, lastRefresh }) {
  const cards = [
    {
      label: 'Total spots',
      value: derived.totalSpots,
      caption: 'Sensors registered with their zone leader',
      icon: CircleGauge,
      tone: 'cyan',
    },
    {
      label: 'Open spots',
      value: derived.openSpots,
      caption: 'Immediately available for parking',
      icon: CarFront,
      tone: 'lime',
    },
    {
      label: 'Occupied',
      value: derived.occupiedSpots,
      caption: `${derived.occupancyRate}% current occupancy`,
      icon: Activity,
      tone: 'coral',
    },
    {
      label: 'Online sensors',
      value: derived.onlineSensors,
      caption: `${derived.offlineSpots} sensor${derived.offlineSpots === 1 ? '' : 's'} currently offline`,
      icon: Wifi,
      tone: derived.offlineSpots ? 'warning' : 'cyan',
    },
    {
      label: 'Last sync',
      value: formatTimestamp(gateway.lastSync),
      caption: gateway.lastSync ? 'Last observed state change' : 'Awaiting first event',
      icon: RadioTower,
      tone: gateway.online ? 'cyan' : 'warning',
    },
    {
      label: 'Backend link',
      value: gateway.online ? 'Online' : 'Offline',
      caption: gateway.online
        ? gateway.leaders && gateway.leaders.length > 0
          ? `Leader ${gateway.leaders[0]}`
          : 'WebSocket connected'
        : 'Awaiting WebSocket reconnection',
      icon: AlertTriangle,
      tone: gateway.online ? 'lime' : 'warning',
    },
  ]

  return (
    <section className="grid gap-4 md:grid-cols-2 xl:grid-cols-3">
      {cards.map((card) => (
        <StatusCard key={card.label} {...card} />
      ))}
    </section>
  )
}

export default StatusPanel
