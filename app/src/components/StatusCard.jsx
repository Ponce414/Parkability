import { motion } from 'framer-motion'

function StatusCard({ label, value, caption, icon: Icon, tone = 'cyan' }) {
  const toneStyles = {
    cyan: 'border-cyan/20 bg-cyan/10 text-cyan',
    lime: 'border-lime/20 bg-lime/10 text-lime',
    coral: 'border-coral/20 bg-coral/10 text-coral',
    warning: 'border-warning/20 bg-warning/10 text-warning',
  }

  return (
    <motion.article
      whileHover={{ y: -6 }}
      className="rounded-[1.6rem] border border-slate-200/80 bg-white/85 p-5 shadow-soft backdrop-blur-xl dark:border-white/10 dark:bg-white/5"
    >
      <div className="flex items-start justify-between gap-4">
        <div>
          <p className="text-sm uppercase tracking-[0.24em] text-slate-500 dark:text-slate-400">{label}</p>
          <p className="mt-4 font-display text-4xl font-semibold text-slate-900 dark:text-white">{value}</p>
          <p className="mt-3 text-sm text-slate-600 dark:text-slate-300">{caption}</p>
        </div>
        <div className={`rounded-2xl border p-3 ${toneStyles[tone]}`}>
          <Icon className="h-5 w-5" />
        </div>
      </div>
    </motion.article>
  )
}

export default StatusCard
