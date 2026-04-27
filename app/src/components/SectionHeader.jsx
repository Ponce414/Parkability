function SectionHeader({ eyebrow, title, description, align = 'left' }) {
  const alignment = align === 'center' ? 'mx-auto text-center' : ''

  return (
    <div className={alignment}>
      <p className="text-sm uppercase tracking-[0.35em] text-cyan">{eyebrow}</p>
      <h2 className="mt-4 font-display text-3xl font-semibold text-slate-900 dark:text-white sm:text-4xl">{title}</h2>
      <p className="mt-4 max-w-2xl text-base leading-7 text-slate-600 dark:text-slate-300">{description}</p>
    </div>
  )
}

export default SectionHeader
