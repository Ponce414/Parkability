export function formatTimestamp(timestamp) {
  if (!timestamp) {
    return 'Waiting for sync'
  }

  return new Intl.DateTimeFormat('en-US', {
    hour: 'numeric',
    minute: '2-digit',
    second: '2-digit',
    month: 'short',
    day: 'numeric',
  }).format(new Date(timestamp))
}

export function formatRelativeMinutes(timestamp) {
  if (!timestamp) {
    return 'No updates yet'
  }

  const diffMs = Date.now() - new Date(timestamp).getTime()
  const diffSeconds = Math.max(0, Math.round(diffMs / 1000))

  if (diffSeconds < 5) {
    return 'Updated just now'
  }
  if (diffSeconds < 60) {
    return `Updated ${diffSeconds}s ago`
  }
  const diffMinutes = Math.round(diffSeconds / 60)
  if (diffMinutes === 1) {
    return 'Updated 1 min ago'
  }
  return `Updated ${diffMinutes} min ago`
}

export function getSpotVisualState(spot) {
  if (!spot.sensorOnline) {
    return {
      label: 'Offline',
      colorClass: 'from-slate-500/60 to-slate-700/70',
      glowClass: 'shadow-slate-900/50',
      accentClass: 'text-warning',
    }
  }

  if (spot.occupied) {
    return {
      label: 'Occupied',
      colorClass: 'from-rose-500/80 to-red-700/90',
      glowClass: 'shadow-red-950/60',
      accentClass: 'text-coral',
    }
  }

  if (spot.state === 'UNKNOWN') {
    return {
      label: 'No reading',
      colorClass: 'from-amber-400/80 to-orange-600/90',
      glowClass: 'shadow-orange-950/60',
      accentClass: 'text-warning',
    }
  }

  return {
    label: 'Available',
    colorClass: 'from-emerald-400/80 to-lime/90',
    glowClass: 'shadow-emerald-950/60',
    accentClass: 'text-lime',
  }
}

export function getEventTone(type) {
  switch (type) {
    case 'sensor':
      return 'border-warning/40 bg-warning/10 text-warning'
    case 'availability':
      return 'border-lime/40 bg-lime/10 text-lime'
    case 'leader':
      return 'border-cyan/40 bg-cyan/10 text-cyan'
    default:
      return 'border-coral/40 bg-coral/10 text-coral'
  }
}
