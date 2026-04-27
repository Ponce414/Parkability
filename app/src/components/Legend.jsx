function Legend() {
  const items = [
    ['Available', 'bg-lime'],
    ['Occupied', 'bg-coral'],
    ['Offline', 'bg-slate-500'],
  ]

  return (
    <div className="flex flex-wrap items-center gap-3">
      {items.map(([label, color]) => (
        <div key={label} className="inline-flex items-center gap-2 rounded-full border border-slate-300/80 bg-white/80 px-4 py-2 text-sm text-slate-600 dark:border-white/10 dark:bg-white/5 dark:text-slate-200">
          <span className={`h-3 w-3 rounded-full ${color}`} />
          {label}
        </div>
      ))}
    </div>
  )
}

export default Legend
