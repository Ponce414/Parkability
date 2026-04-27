import { motion, useScroll, useTransform } from 'framer-motion'
import { ArrowDownRight, Radar, ShieldCheck, Wifi } from 'lucide-react'
import { useRef } from 'react'
import { useParkingData } from '../hooks/useParkingData'

function HeroSection() {
  const sectionRef = useRef(null)
  const { scrollYProgress } = useScroll({
    target: sectionRef,
    offset: ['start start', 'end start'],
  })

  const backgroundY = useTransform(scrollYProgress, [0, 1], ['0%', '28%'])
  const orbY = useTransform(scrollYProgress, [0, 1], ['0%', '40%'])

  // Pull live numbers so the hero card matches the dashboard.
  const { snapshot, derived } = useParkingData()

  const previewSpots = (snapshot?.spots ?? []).slice(0, 6)
  const totalSpots = derived?.totalSpots ?? 0
  const onlineSensors = derived?.onlineSensors ?? 0

  return (
    <section
      id="top"
      ref={sectionRef}
      className="relative overflow-hidden border-b border-white/10 bg-[radial-gradient(circle_at_top,#17365e_0%,#07111f_36%,#020816_100%)]"
    >
      <motion.div
        style={{ y: backgroundY }}
        className="absolute inset-0 bg-grid-fade bg-[size:72px_72px] opacity-40"
      />
      <motion.div
        style={{ y: orbY }}
        className="absolute -left-20 top-24 h-72 w-72 rounded-full bg-cyan/20 blur-3xl"
      />
      <div className="absolute inset-x-0 bottom-0 h-40 bg-gradient-to-b from-transparent to-[#020816]" />

      <div className="relative mx-auto grid min-h-[88vh] max-w-7xl items-center gap-16 px-5 py-20 sm:px-6 lg:grid-cols-[1.1fr_0.9fr] lg:px-8 lg:py-24">
        <div>
          <motion.p
            initial={{ opacity: 0, y: 24 }}
            animate={{ opacity: 1, y: 0 }}
            transition={{ delay: 0.1 }}
            className="mb-6 inline-flex rounded-full border border-cyan/25 bg-cyan/10 px-4 py-2 text-xs uppercase tracking-[0.35em] text-cyan"
          >
            Smart City Parking Telemetry
          </motion.p>
          <motion.h1
            initial={{ opacity: 0, y: 24 }}
            animate={{ opacity: 1, y: 0 }}
            transition={{ delay: 0.2 }}
            className="max-w-4xl font-display text-5xl font-semibold tracking-tight text-white sm:text-6xl xl:text-7xl"
          >
            Smart Parking Lot Monitoring System
          </motion.h1>
          <motion.p
            initial={{ opacity: 0, y: 24 }}
            animate={{ opacity: 1, y: 0 }}
            transition={{ delay: 0.3 }}
            className="mt-6 max-w-2xl text-lg leading-8 text-slate-300"
          >
            ESP32-C3 sensor nodes feed zone leaders over ESP-NOW, leaders uplink to a FastAPI
            backend over MQTT, and this dashboard rides a live WebSocket — every state change
            arrives in milliseconds.
          </motion.p>

          <motion.div
            initial={{ opacity: 0, y: 24 }}
            animate={{ opacity: 1, y: 0 }}
            transition={{ delay: 0.4 }}
            className="mt-10 flex flex-col gap-4 sm:flex-row"
          >
            <a
              href="#dashboard"
              className="inline-flex items-center justify-center gap-2 rounded-full bg-cyan px-6 py-3.5 font-semibold text-slate-950 transition hover:-translate-y-1 hover:shadow-[0_18px_40px_rgba(55,214,255,0.28)]"
            >
              View Live Dashboard
              <ArrowDownRight className="h-4 w-4" />
            </a>
            <a
              href="#architecture"
              className="inline-flex items-center justify-center rounded-full border border-white/15 bg-white/5 px-6 py-3.5 font-semibold text-white transition hover:-translate-y-1 hover:border-cyan/40 hover:bg-cyan/10"
            >
              Explore System Design
            </a>
          </motion.div>

          <div className="mt-12 grid gap-4 sm:grid-cols-3">
            {[
              ['Sensor mesh', 'ESP-NOW intra-zone', Radar],
              ['Leader uplink', 'MQTT or REST to FastAPI', Wifi],
              ['Resilience', 'Bully election + Lamport clocks', ShieldCheck],
            ].map(([title, subtitle, Icon]) => (
              <motion.div
                key={title}
                initial={{ opacity: 0, y: 28 }}
                animate={{ opacity: 1, y: 0 }}
                transition={{ delay: 0.5 }}
                className="rounded-3xl border border-white/10 bg-white/5 p-5 backdrop-blur-md"
              >
                <Icon className="h-5 w-5 text-cyan" />
                <p className="mt-4 font-display text-lg font-semibold text-white">{title}</p>
                <p className="mt-2 text-sm leading-6 text-slate-300">{subtitle}</p>
              </motion.div>
            ))}
          </div>
        </div>

        <motion.div
          initial={{ opacity: 0, scale: 0.94, y: 20 }}
          animate={{ opacity: 1, scale: 1, y: 0 }}
          transition={{ duration: 0.7, delay: 0.3 }}
          className="relative"
        >
          <div className="absolute -inset-8 rounded-[2rem] bg-cyan/10 blur-3xl" />
          <div className="relative overflow-hidden rounded-[2rem] border border-white/10 bg-white/10 p-5 shadow-panel backdrop-blur-2xl">
            <div className="grid gap-4 sm:grid-cols-2">
              <div className="rounded-3xl border border-white/10 bg-slate-950/55 p-5">
                <p className="text-sm uppercase tracking-[0.3em] text-slate-400">Sensors online</p>
                <p className="mt-4 font-display text-4xl font-semibold text-white">
                  {totalSpots > 0 ? `${onlineSensors} / ${totalSpots}` : '— / —'}
                </p>
                <p className="mt-2 text-sm text-slate-300">Live count from registered VL53L0X nodes.</p>
              </div>
              <div className="rounded-3xl border border-cyan/20 bg-cyan/10 p-5">
                <p className="text-sm uppercase tracking-[0.3em] text-cyan/80">Transport</p>
                <p className="mt-4 font-display text-4xl font-semibold text-white">WS</p>
                <p className="mt-2 text-sm text-slate-200">Push-based updates, no polling layer.</p>
              </div>
            </div>

            <div className="mt-4 rounded-[1.75rem] border border-white/10 bg-[linear-gradient(135deg,rgba(15,23,42,0.9),rgba(10,47,71,0.72))] p-6">
              <div className="flex items-center justify-between">
                <div>
                  <p className="text-sm uppercase tracking-[0.35em] text-slate-400">Live command view</p>
                  <p className="mt-2 font-display text-2xl font-semibold text-white">Parking lot pulse</p>
                </div>
                <div className="h-3 w-3 animate-pulse rounded-full bg-lime shadow-[0_0_24px_rgba(61,219,150,0.8)]" />
              </div>
              <div className="mt-8 grid grid-cols-[repeat(3,minmax(0,1fr))] gap-4">
                {previewSpots.length > 0 ? (
                  previewSpots.map((spot) => {
                    const state = !spot.sensorOnline
                      ? 'Offline'
                      : spot.occupied
                        ? 'Occupied'
                        : 'Available'
                    const styleClass =
                      state === 'Occupied'
                        ? 'border-coral/30 bg-coral/15'
                        : state === 'Offline'
                          ? 'border-warning/25 bg-warning/10'
                          : 'border-lime/30 bg-lime/10'
                    return (
                      <div key={spot.id} className={`rounded-2xl border p-4 ${styleClass}`}>
                        <p className="text-xs uppercase tracking-[0.3em] text-slate-400">
                          Spot {spot.displayId ?? spot.id}
                        </p>
                        <p className="mt-4 font-medium text-white">{state}</p>
                      </div>
                    )
                  })
                ) : (
                  Array.from({ length: 6 }).map((_, index) => (
                    <div
                      key={index}
                      className="rounded-2xl border border-white/10 bg-white/5 p-4"
                    >
                      <p className="text-xs uppercase tracking-[0.3em] text-slate-400">Spot —</p>
                      <p className="mt-4 font-medium text-white/60">Awaiting</p>
                    </div>
                  ))
                )}
              </div>
            </div>
          </div>
        </motion.div>
      </div>
    </section>
  )
}

export default HeroSection
