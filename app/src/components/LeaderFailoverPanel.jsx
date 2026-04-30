import { Activity, Cpu, RadioTower, Route, ShieldCheck, WifiOff } from 'lucide-react'
import { formatRelativeMinutes } from '../utils/formatters'

function toneClasses(tone) {
  if (tone === 'lime') return 'border-lime/35 bg-lime/10 text-lime'
  if (tone === 'coral') return 'border-coral/35 bg-coral/10 text-coral'
  if (tone === 'warning') return 'border-warning/35 bg-warning/10 text-warning'
  return 'border-cyan/35 bg-cyan/10 text-cyan'
}

function spotList(spots, fallback = 'None') {
  if (!spots || spots.length === 0) return fallback
  return spots
    .map((spot) => (spot && typeof spot === 'object' ? spot.displayId : spot))
    .map((id) => `S${id}`)
    .join(' ')
}

function LeaderNodeCard({ node }) {
  const StatusIcon = node.online ? ShieldCheck : WifiOff

  return (
    <div className="rounded-2xl border border-slate-200/80 bg-slate-50/80 p-4 dark:border-white/10 dark:bg-slate-950/45">
      <div className="flex items-start justify-between gap-3">
        <div>
          <div className="flex items-center gap-2">
            <div className={`rounded-xl border p-2 ${toneClasses(node.tone)}`}>
              <Cpu className="h-4 w-4" />
            </div>
            <div>
              <p className="font-display text-base font-semibold text-slate-900 dark:text-white">{node.label}</p>
              <p className="font-mono text-[11px] text-slate-500 dark:text-slate-400">{node.mac}</p>
            </div>
          </div>
        </div>
        <span className={`inline-flex shrink-0 items-center gap-1 rounded-full border px-2.5 py-1 text-xs font-medium ${toneClasses(node.tone)}`}>
          <StatusIcon className="h-3.5 w-3.5" />
          {node.status}
        </span>
      </div>

      <div className="mt-4 grid gap-3 text-sm">
        <div className="flex items-center justify-between gap-3 rounded-xl border border-slate-200/80 bg-white/80 px-3 py-2 dark:border-white/10 dark:bg-white/5">
          <span className="text-slate-500 dark:text-slate-400">Home group</span>
          <span className="font-medium text-slate-900 dark:text-white">{spotList(node.homeSpots)}</span>
        </div>
        <div className="flex items-center justify-between gap-3 rounded-xl border border-slate-200/80 bg-white/80 px-3 py-2 dark:border-white/10 dark:bg-white/5">
          <span className="text-slate-500 dark:text-slate-400">Maintaining</span>
          <span className="font-medium text-slate-900 dark:text-white">{spotList(node.servedSpots)}</span>
        </div>
        <div className="flex items-center justify-between gap-3 rounded-xl border border-slate-200/80 bg-white/80 px-3 py-2 dark:border-white/10 dark:bg-white/5">
          <span className="text-slate-500 dark:text-slate-400">Last traffic</span>
          <span className="font-medium text-slate-900 dark:text-white">{formatRelativeMinutes(node.lastSeen)}</span>
        </div>
      </div>

      {node.actingForSpots.length > 0 ? (
        <div className="mt-3 rounded-xl border border-lime/25 bg-lime/10 px-3 py-2 text-sm text-lime">
          Acting for {spotList(node.actingForSpots)}
        </div>
      ) : null}
      {node.failedOverHomeSpots.length > 0 ? (
        <div className="mt-3 rounded-xl border border-coral/25 bg-coral/10 px-3 py-2 text-sm text-coral">
          Home group carried by backup: {spotList(node.failedOverHomeSpots)}
        </div>
      ) : null}
    </div>
  )
}

function LeaderFailoverPanel({ controllerStatus }) {
  if (!controllerStatus) return null
  const SummaryIcon = controllerStatus.failoverActive ? Route : RadioTower

  return (
    <section className="rounded-[2rem] border border-slate-200/70 bg-white/80 p-6 shadow-soft backdrop-blur-xl dark:border-white/10 dark:bg-white/5">
      <div className="flex flex-col gap-4 sm:flex-row sm:items-start sm:justify-between">
        <div>
          <p className="text-sm uppercase tracking-[0.32em] text-slate-500 dark:text-slate-400">Leader mesh</p>
          <h3 className="mt-2 font-display text-2xl font-semibold text-slate-900 dark:text-white">{controllerStatus.summary}</h3>
          <p className="mt-2 text-sm leading-6 text-slate-600 dark:text-slate-300">{controllerStatus.detail}</p>
        </div>
        <div className={`inline-flex items-center gap-2 rounded-2xl border px-3 py-2 text-sm font-medium ${controllerStatus.failoverActive ? toneClasses('lime') : toneClasses(controllerStatus.onlineCount === controllerStatus.expectedCount ? 'cyan' : 'warning')}`}>
          <SummaryIcon className="h-4 w-4" />
          {controllerStatus.onlineCount}/{controllerStatus.expectedCount} C5 visible
        </div>
      </div>

      <div className="mt-5 grid gap-4">
        {controllerStatus.nodes.map((node) => (
          <LeaderNodeCard key={node.id} node={node} />
        ))}
      </div>

      <div className="mt-5 flex items-center gap-2 rounded-2xl border border-slate-200/80 bg-slate-50/80 px-4 py-3 text-sm text-slate-600 dark:border-white/10 dark:bg-slate-950/45 dark:text-slate-300">
        <Activity className="h-4 w-4 text-cyan" />
        Spot ownership is based on the C5 MAC attached to each live sensor update.
      </div>
    </section>
  )
}

export default LeaderFailoverPanel
