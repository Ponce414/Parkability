import { motion, useScroll, useTransform } from 'framer-motion'
import { Cpu, Network, ScanLine } from 'lucide-react'
import { useRef } from 'react'

function ParallaxSection() {
  const sectionRef = useRef(null)
  const { scrollYProgress } = useScroll({
    target: sectionRef,
    offset: ['start end', 'end start'],
  })

  const leftCardY = useTransform(scrollYProgress, [0, 1], [60, -60])
  const rightCardY = useTransform(scrollYProgress, [0, 1], [-40, 70])

  return (
    <section ref={sectionRef} className="relative overflow-hidden bg-[#020816] py-20">
      <div className="absolute inset-x-0 top-0 h-px bg-gradient-to-r from-transparent via-cyan/30 to-transparent" />
      <div className="mx-auto grid max-w-7xl gap-6 px-5 sm:px-6 lg:grid-cols-[0.9fr_1.1fr] lg:px-8">
        <motion.div style={{ y: leftCardY }} className="rounded-[2rem] border border-white/10 bg-white/5 p-8 shadow-panel backdrop-blur-xl">
          <p className="text-sm uppercase tracking-[0.35em] text-cyan">Why this frontend matters</p>
          <h2 className="mt-4 font-display text-3xl font-semibold text-white">Built like a control room, not a class demo.</h2>
          <p className="mt-4 max-w-xl text-base leading-7 text-slate-300">
            Transport, state derivation, and visual presentation are decoupled. The UI hooks
            into a backend WebSocket — swap that for any other push channel without touching
            the dashboard layout.
          </p>
        </motion.div>

        <motion.div style={{ y: rightCardY }} className="grid gap-4 md:grid-cols-3">
          {[
            ['Sensor nodes', 'Each parking bay is an autonomous ESP32-C3 + VL53L0X telemetry source.', ScanLine],
            ['Leader relay', 'Per-zone leaders run bully election and aggregate ESP-NOW frames before MQTT uplink.', Network],
            ['Backend pipeline', 'FastAPI persists every event with Lamport timestamps and pushes WebSocket updates.', Cpu],
          ].map(([title, text, Icon]) => (
            <div key={title} className="rounded-[2rem] border border-white/10 bg-slate-900/60 p-6">
              <Icon className="h-6 w-6 text-cyan" />
              <h3 className="mt-5 font-display text-xl font-semibold text-white">{title}</h3>
              <p className="mt-3 text-sm leading-6 text-slate-300">{text}</p>
            </div>
          ))}
        </motion.div>
      </div>
    </section>
  )
}

export default ParallaxSection
