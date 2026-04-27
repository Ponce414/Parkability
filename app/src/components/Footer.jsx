import { Globe, RadioTower, Waypoints } from 'lucide-react'

function Footer() {
  return (
    <footer id="footer" className="border-t border-white/10 bg-slate-950 py-10">
      <div className="mx-auto flex max-w-7xl flex-col gap-6 px-5 text-sm text-slate-400 sm:px-6 lg:flex-row lg:items-center lg:justify-between lg:px-8">
        <div>
          <p className="font-display text-lg font-semibold text-white">Distributed Parking Lot Availability System</p>
          <p className="mt-2">CECS 327 course project • California State University, Long Beach • 2026</p>
        </div>
        <div className="flex items-center gap-3">
          <a href="https://github.com/Ponce414/Parkability" target="_blank" rel="noreferrer" className="rounded-full border border-white/10 p-3 transition hover:border-cyan/40 hover:text-cyan">
            <Waypoints className="h-4 w-4" />
          </a>
          <a href="https://linkedin.com" target="_blank" rel="noreferrer" className="rounded-full border border-white/10 p-3 transition hover:border-cyan/40 hover:text-cyan">
            <RadioTower className="h-4 w-4" />
          </a>
          <a href="https://example.com" target="_blank" rel="noreferrer" className="rounded-full border border-white/10 p-3 transition hover:border-cyan/40 hover:text-cyan">
            <Globe className="h-4 w-4" />
          </a>
        </div>
      </div>
    </footer>
  )
}

export default Footer
