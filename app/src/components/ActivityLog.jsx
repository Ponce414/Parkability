import { motion } from 'framer-motion'
import { BellRing } from 'lucide-react'
import { formatTimestamp, getEventTone } from '../utils/formatters'

function ActivityLog({ events }) {
  return (
    <section className="rounded-[2rem] border border-slate-200/70 bg-white/80 p-6 shadow-soft backdrop-blur-xl dark:border-white/10 dark:bg-white/5">
      <div className="flex items-center justify-between">
        <div>
          <p className="text-sm uppercase tracking-[0.32em] text-slate-500 dark:text-slate-400">Event stream</p>
          <h3 className="mt-2 font-display text-2xl font-semibold text-slate-900 dark:text-white">Recent activity</h3>
        </div>
        <div className="rounded-2xl border border-cyan/20 bg-cyan/10 p-3 text-cyan">
          <BellRing className="h-5 w-5" />
        </div>
      </div>

      <div className="mt-6 space-y-3">
        {events.length ? (
          events.map((event, index) => (
            <motion.article
              key={event.id}
              initial={{ opacity: 0, x: 10 }}
              animate={{ opacity: 1, x: 0 }}
              transition={{ delay: index * 0.06 }}
              className="rounded-2xl border border-slate-200/80 bg-slate-50/80 p-4 dark:border-white/10 dark:bg-slate-950/45"
            >
              <div className="flex flex-wrap items-center gap-3">
                <span className={`rounded-full border px-3 py-1 text-xs font-medium uppercase tracking-[0.2em] ${getEventTone(event.type)}`}>
                  {event.type}
                </span>
                <span className="text-xs uppercase tracking-[0.22em] text-slate-500 dark:text-slate-400">{formatTimestamp(event.time)}</span>
              </div>
              <p className="mt-3 text-sm leading-6 text-slate-700 dark:text-slate-200">{event.message}</p>
            </motion.article>
          ))
        ) : (
          <div className="rounded-2xl border border-dashed border-slate-300/80 p-6 text-sm text-slate-500 dark:border-white/10 dark:text-slate-300">
            No events yet. Once a sensor changes state, the WebSocket push will land here.
          </div>
        )}
      </div>
    </section>
  )
}

export default ActivityLog
