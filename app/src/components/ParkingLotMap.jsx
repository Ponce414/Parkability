import { AnimatePresence, motion } from 'framer-motion'
import ParkingSpotCard from './ParkingSpotCard'

/**
 * Two-row lot view with a drive lane between rows. Splits spots evenly so
 * any count works (1 spot, 6 spots, 19 spots — bottom row gets the extra
 * if the count is odd).
 */
function ParkingLotMap({ spots, highlightedIds }) {
  const half = Math.ceil(spots.length / 2)
  const topRow = spots.slice(0, half)
  const bottomRow = spots.slice(half)
  const shouldMute = highlightedIds.length > 0 && highlightedIds.length < spots.length

  // Cap visual columns at 4 to prevent cards getting tiny on big lots.
  const colsClass = (count) => {
    if (count <= 1) return 'md:grid-cols-1'
    if (count <= 2) return 'md:grid-cols-2'
    if (count <= 3) return 'md:grid-cols-3'
    return 'md:grid-cols-4'
  }

  const isHighlighted = (id) => highlightedIds.includes(id)

  return (
    <section className="rounded-[2rem] border border-slate-200/70 bg-white/80 p-4 shadow-soft backdrop-blur-xl dark:border-white/10 dark:bg-white/5 sm:p-6">
      <div className="mb-5 flex items-center justify-between">
        <div>
          <p className="text-sm uppercase tracking-[0.32em] text-slate-500 dark:text-slate-400">Parking lot map</p>
          <p className="mt-2 font-display text-2xl font-semibold text-slate-900 dark:text-white">Live lot view</p>
        </div>
        <div className="hidden rounded-full border border-slate-300/80 px-4 py-2 text-xs uppercase tracking-[0.3em] text-slate-500 dark:border-white/10 dark:text-slate-300 sm:inline-flex">
          Drive lane centered
        </div>
      </div>

      {spots.length === 0 ? (
        <div className="rounded-[1.8rem] border border-dashed border-slate-300/80 p-12 text-center text-sm text-slate-500 dark:border-white/10 dark:text-slate-300">
          No spots reporting yet. Flash a sensor and pair it with a leader.
        </div>
      ) : (
        <div className="overflow-hidden rounded-[1.8rem] border border-white/10 bg-[linear-gradient(180deg,#102033,#08111f)] p-4 sm:p-6">
          <div className="grid gap-4 lg:grid-rows-[1fr_auto_1fr]">
            <div className={`grid gap-4 ${colsClass(topRow.length)}`}>
              <AnimatePresence>
                {topRow.map((spot, index) => (
                  <motion.div
                    key={spot.id}
                    layout
                    initial={{ opacity: 0, y: 20 }}
                    animate={{ opacity: 1, y: 0 }}
                    exit={{ opacity: 0 }}
                    transition={{ delay: index * 0.05 }}
                    className={index % 2 === 0 ? 'md:-rotate-[2deg]' : 'md:rotate-[1.5deg]'}
                  >
                    <ParkingSpotCard spot={spot} compact muted={shouldMute && !isHighlighted(spot.id)} />
                  </motion.div>
                ))}
              </AnimatePresence>
            </div>

            <div className="relative h-24 rounded-[1.8rem] border border-white/5 bg-[linear-gradient(180deg,rgba(16,30,48,0.95),rgba(8,17,31,0.85))]">
              <div className="absolute inset-y-1/2 left-6 right-6 -translate-y-1/2 border-t-2 border-dashed border-cyan/35" />
              <div className="absolute inset-y-0 left-1/2 w-px -translate-x-1/2 bg-white/10" />
              <div className="absolute left-4 top-1/2 -translate-y-1/2 rounded-full border border-cyan/30 bg-cyan/10 px-3 py-1 text-xs uppercase tracking-[0.3em] text-cyan">
                Entry
              </div>
              <div className="absolute right-4 top-1/2 -translate-y-1/2 rounded-full border border-cyan/30 bg-cyan/10 px-3 py-1 text-xs uppercase tracking-[0.3em] text-cyan">
                Exit
              </div>
            </div>

            <div className={`grid gap-4 ${colsClass(bottomRow.length)}`}>
              <AnimatePresence>
                {bottomRow.map((spot, index) => (
                  <motion.div
                    key={spot.id}
                    layout
                    initial={{ opacity: 0, y: 20 }}
                    animate={{ opacity: 1, y: 0 }}
                    exit={{ opacity: 0 }}
                    transition={{ delay: index * 0.05 }}
                    className={index % 2 === 0 ? 'md:rotate-[2deg]' : 'md:-rotate-[1.5deg]'}
                  >
                    <ParkingSpotCard spot={spot} compact muted={shouldMute && !isHighlighted(spot.id)} />
                  </motion.div>
                ))}
              </AnimatePresence>
            </div>
          </div>
        </div>
      )}
    </section>
  )
}

export default ParkingLotMap
