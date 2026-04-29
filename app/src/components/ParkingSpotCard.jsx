import { motion } from 'framer-motion'
import { CarFront, CircleAlert, Wifi, WifiOff } from 'lucide-react'
import { formatRelativeMinutes, getSpotVisualState } from '../utils/formatters'

function ParkingSpotCard({ spot, compact = false, muted = false }) {
  const visualState = getSpotVisualState(spot)
  const display = spot.displayId ?? spot.id
  const hasDistance = Number.isFinite(spot.distance) && spot.distance > 0

  return (
    <motion.div
      layout
      transition={{ duration: 0.45 }}
      className={`group relative overflow-hidden rounded-[1.6rem] border border-white/10 bg-gradient-to-br ${visualState.colorClass} p-4 text-white shadow-2xl transition ${compact ? 'min-h-[160px]' : 'min-h-[190px]'} ${muted ? 'opacity-35 saturate-50' : ''}`}
    >
      <div className="absolute inset-0 bg-[radial-gradient(circle_at_top,rgba(255,255,255,0.24),transparent_45%)] opacity-60 transition duration-500 group-hover:opacity-100" />
      <div className="relative flex h-full flex-col justify-between">
        <div className="flex items-start justify-between gap-4">
          <div>
            <p className="text-xs uppercase tracking-[0.34em] text-white/80">Spot {display}</p>
            <p className="mt-3 font-display text-2xl font-semibold">{visualState.label}</p>
          </div>
          <div className="rounded-2xl border border-white/20 bg-black/10 p-3 backdrop-blur-sm">
            {spot.sensorOnline ? <Wifi className="h-5 w-5" /> : <WifiOff className="h-5 w-5" />}
          </div>
        </div>

        <div className="space-y-3">
          <div className="flex items-center justify-between rounded-2xl border border-white/15 bg-black/10 px-4 py-3 text-sm backdrop-blur-sm">
            <span>Sensor</span>
            <span className="font-medium">{spot.sensorOnline ? 'Online' : 'Offline'}</span>
          </div>
          <div className="flex items-center justify-between rounded-2xl border border-white/15 bg-black/10 px-4 py-3 text-sm backdrop-blur-sm">
            <span>Distance</span>
            <span className="font-medium">{hasDistance ? `${spot.distance} mm` : '—'}</span>
          </div>
          <div className="flex items-center justify-between text-sm text-white/85">
            <span>{formatRelativeMinutes(spot.lastUpdated)}</span>
            <span className="inline-flex items-center gap-2">
              {spot.sensorOnline ? <CarFront className="h-4 w-4" /> : <CircleAlert className="h-4 w-4" />}
              {spot.sensorOnline ? 'Telemetry active' : 'Attention required'}
            </span>
          </div>
        </div>
      </div>
    </motion.div>
  )
}

export default ParkingSpotCard
