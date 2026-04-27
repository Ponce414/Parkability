import { Moon, RadioTower, SunMedium } from 'lucide-react'
import { motion } from 'framer-motion'

function Navbar({ theme, onToggleTheme }) {
  return (
    <motion.header
      initial={{ opacity: 0, y: -24 }}
      animate={{ opacity: 1, y: 0 }}
      className="sticky top-0 z-50 border-b border-white/10 bg-white/70 backdrop-blur-xl dark:bg-slate-950/55"
    >
      <div className="mx-auto flex max-w-7xl items-center justify-between px-5 py-4 sm:px-6 lg:px-8">
        <a href="#top" className="flex items-center gap-3">
          <div className="flex h-11 w-11 items-center justify-center rounded-2xl border border-cyan/30 bg-cyan/10 text-cyan shadow-soft">
            <RadioTower className="h-5 w-5" />
          </div>
          <div>
            <p className="font-display text-sm uppercase tracking-[0.35em] text-slate-500 dark:text-slate-400">Distributed IoT</p>
            <p className="font-display text-base font-semibold text-slate-900 dark:text-white">Parking Control</p>
          </div>
        </a>

        <nav className="hidden items-center gap-6 text-sm text-slate-600 dark:text-slate-300 md:flex">
          <a href="#dashboard" className="transition hover:text-cyan">Dashboard</a>
          <a href="#architecture" className="transition hover:text-cyan">Architecture</a>
          <a href="#footer" className="transition hover:text-cyan">Project</a>
        </nav>

        <button
          type="button"
          onClick={onToggleTheme}
          className="inline-flex items-center gap-2 rounded-full border border-slate-300/70 bg-white/80 px-4 py-2 text-sm font-medium text-slate-700 transition hover:-translate-y-0.5 hover:border-cyan/40 hover:text-cyan dark:border-white/10 dark:bg-white/5 dark:text-slate-100"
        >
          {theme === 'dark' ? <SunMedium className="h-4 w-4" /> : <Moon className="h-4 w-4" />}
          {theme === 'dark' ? 'Light mode' : 'Dark mode'}
        </button>
      </div>
    </motion.header>
  )
}

export default Navbar
