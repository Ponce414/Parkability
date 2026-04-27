import { AnimatePresence, motion } from 'framer-motion'
import { AlertCircle, Search, Signal, TrendingUp } from 'lucide-react'
import { useMemo, useState } from 'react'
import { useParkingData } from '../hooks/useParkingData'
import { formatTimestamp, getSpotVisualState } from '../utils/formatters'
import ActivityLog from './ActivityLog'
import Legend from './Legend'
import ParkingLotMap from './ParkingLotMap'
import SectionHeader from './SectionHeader'
import StatusPanel from './StatusPanel'

const FILTERS = ['all', 'open', 'occupied', 'offline']

function statusToneClass(status) {
  if (status === 'connected') return 'border-lime/30 bg-lime/10 text-lime'
  if (status === 'reconnecting') return 'border-warning/30 bg-warning/10 text-warning'
  if (status === 'disconnected') return 'border-coral/30 bg-coral/10 text-coral'
  return 'border-cyan/30 bg-cyan/10 text-cyan'
}

function Dashboard() {
  const { snapshot, derived, loading, error, lastRefresh, dataSource, connectionStatus } =
    useParkingData()
  const [activeFilter, setActiveFilter] = useState('all')
  const [searchValue, setSearchValue] = useState('')

  const filteredSpots = useMemo(() => {
    if (!snapshot) {
      return []
    }

    const q = searchValue.trim().toLowerCase()
    return snapshot.spots.filter((spot) => {
      if (q) {
        const haystack = `${spot.id} ${spot.displayId ?? ''}`.toLowerCase()
        if (!haystack.includes(q)) return false
      }

      if (activeFilter === 'open') {
        return !spot.occupied && spot.sensorOnline
      }
      if (activeFilter === 'occupied') {
        return spot.occupied && spot.sensorOnline
      }
      if (activeFilter === 'offline') {
        return !spot.sensorOnline
      }
      return true
    })
  }, [activeFilter, searchValue, snapshot])

  const visibleSummary = useMemo(() => {
    return filteredSpots
      .map((spot) => `Spot ${spot.displayId ?? spot.id}: ${getSpotVisualState(spot).label}`)
      .join(' • ')
  }, [filteredSpots])

  return (
    <section id="dashboard" className="relative overflow-hidden bg-slate-100 py-24 dark:bg-[#06101d]">
      <div className="absolute inset-x-0 top-0 h-72 bg-[radial-gradient(circle_at_top,rgba(55,214,255,0.12),transparent_55%)]" />
      <div className="mx-auto max-w-7xl px-5 sm:px-6 lg:px-8">
        <SectionHeader
          eyebrow="Live dashboard"
          title="Operational overview for your distributed parking lot"
          description="Backed by a WebSocket push from the FastAPI backend. Every state change reaches this view within milliseconds — no polling, no client-side staleness."
        />

        <div className="mt-10 flex flex-col gap-4 xl:flex-row xl:items-center xl:justify-between">
          <Legend />
          <div className="flex flex-col gap-3 md:flex-row">
            <label className="flex items-center gap-3 rounded-full border border-slate-300/80 bg-white/80 px-4 py-3 text-sm shadow-soft dark:border-white/10 dark:bg-white/5">
              <Search className="h-4 w-4 text-slate-500" />
              <input
                value={searchValue}
                onChange={(event) => setSearchValue(event.target.value)}
                placeholder="Search by spot id"
                className="w-40 bg-transparent outline-none placeholder:text-slate-400"
              />
            </label>
            <div
              className={`inline-flex items-center justify-center gap-2 rounded-full border px-4 py-3 text-sm font-medium ${statusToneClass(connectionStatus)}`}
            >
              <Signal className="h-4 w-4" />
              {dataSource}
            </div>
          </div>
        </div>

        <div className="mt-6 flex flex-wrap gap-3">
          {FILTERS.map((filter) => (
            <button
              key={filter}
              type="button"
              onClick={() => setActiveFilter(filter)}
              className={`rounded-full px-4 py-2 text-sm font-medium capitalize transition ${activeFilter === filter ? 'bg-slate-900 text-white dark:bg-cyan dark:text-slate-950' : 'border border-slate-300/80 bg-white/80 text-slate-600 dark:border-white/10 dark:bg-white/5 dark:text-slate-200'}`}
            >
              {filter}
            </button>
          ))}
        </div>

        <AnimatePresence mode="wait">
          {loading ? (
            <motion.div key="loading" initial={{ opacity: 0 }} animate={{ opacity: 1 }} exit={{ opacity: 0 }} className="mt-10 grid gap-5 lg:grid-cols-[1.3fr_0.7fr]">
              <div className="h-[480px] animate-pulse rounded-[2rem] border border-slate-200/70 bg-white/70 dark:border-white/10 dark:bg-white/5" />
              <div className="space-y-5">
                <div className="h-48 animate-pulse rounded-[2rem] border border-slate-200/70 bg-white/70 dark:border-white/10 dark:bg-white/5" />
                <div className="h-64 animate-pulse rounded-[2rem] border border-slate-200/70 bg-white/70 dark:border-white/10 dark:bg-white/5" />
              </div>
            </motion.div>
          ) : error ? (
            <motion.div key="error" initial={{ opacity: 0 }} animate={{ opacity: 1 }} exit={{ opacity: 0 }} className="mt-10 rounded-[2rem] border border-coral/30 bg-coral/10 p-8">
              <div className="flex items-start gap-4">
                <AlertCircle className="mt-1 h-5 w-5 text-coral" />
                <div>
                  <p className="font-display text-2xl font-semibold text-slate-900 dark:text-white">Unable to load dashboard data</p>
                  <p className="mt-2 text-sm text-slate-700 dark:text-slate-200">{error}</p>
                </div>
              </div>
            </motion.div>
          ) : (
            snapshot &&
            derived && (
              <motion.div key="ready" initial={{ opacity: 0 }} animate={{ opacity: 1 }} exit={{ opacity: 0 }} className="mt-10 space-y-6">
                <div className="grid gap-4 xl:grid-cols-[1.45fr_0.55fr]">
                  <div className="space-y-6">
                    <StatusPanel derived={derived} gateway={snapshot.gateway} lastRefresh={lastRefresh} />
                    <ParkingLotMap spots={snapshot.spots} highlightedIds={filteredSpots.map((spot) => spot.id)} />
                  </div>

                  <div className="space-y-6">
                    <div className="rounded-[2rem] border border-slate-200/70 bg-white/80 p-6 shadow-soft backdrop-blur-xl dark:border-white/10 dark:bg-white/5">
                      <p className="text-sm uppercase tracking-[0.32em] text-slate-500 dark:text-slate-400">Control summary</p>
                      <h3 className="mt-2 font-display text-2xl font-semibold text-slate-900 dark:text-white">Lot health snapshot</h3>
                      <div className="mt-6 space-y-4">
                        <div className="rounded-2xl border border-slate-200/80 bg-slate-50/80 p-4 dark:border-white/10 dark:bg-slate-950/45">
                          <p className="text-sm text-slate-500 dark:text-slate-400">System health</p>
                          <p className="mt-2 font-display text-3xl font-semibold text-slate-900 dark:text-white">{derived.systemHealth}</p>
                        </div>
                        <div className="rounded-2xl border border-slate-200/80 bg-slate-50/80 p-4 dark:border-white/10 dark:bg-slate-950/45">
                          <p className="text-sm text-slate-500 dark:text-slate-400">Visible spots</p>
                          <p className="mt-2 text-sm leading-7 text-slate-700 dark:text-slate-200">
                            {visibleSummary || 'No spaces match the current filter.'}
                          </p>
                        </div>
                        <div className="grid gap-4 sm:grid-cols-3 xl:grid-cols-1">
                          {[
                            ['Occupancy', `${derived.occupancyRate}% in use`, TrendingUp],
                            ['Transport', 'WebSocket push', Signal],
                            ['Last update', formatTimestamp(lastRefresh), Signal],
                          ].map(([label, value, Icon]) => (
                            <div key={label} className="rounded-2xl border border-slate-200/80 bg-slate-50/80 p-4 dark:border-white/10 dark:bg-slate-950/45">
                              <div className="flex items-center gap-3">
                                <div className="rounded-2xl border border-cyan/20 bg-cyan/10 p-2 text-cyan">
                                  <Icon className="h-4 w-4" />
                                </div>
                                <div>
                                  <p className="text-xs uppercase tracking-[0.25em] text-slate-500 dark:text-slate-400">{label}</p>
                                  <p className="mt-1 text-sm font-medium text-slate-800 dark:text-slate-100">{value}</p>
                                </div>
                              </div>
                            </div>
                          ))}
                        </div>
                      </div>
                    </div>

                    <ActivityLog events={snapshot.events} />
                  </div>
                </div>
              </motion.div>
            )
          )}
        </AnimatePresence>
      </div>
    </section>
  )
}

export default Dashboard
