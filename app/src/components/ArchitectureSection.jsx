import { motion } from 'framer-motion'
import { ArrowRight, Cpu, DatabaseZap, Radar, Router } from 'lucide-react'
import SectionHeader from './SectionHeader'

function ArchitectureSection() {
  const cards = [
    {
      title: 'ESP32-C3 sensor nodes',
      text: 'Each parking bay has its own ESP32-C3 polling a VL53L0X distance sensor. Debounced state changes are sent to the zone leader over ESP-NOW.',
      icon: Cpu,
    },
    {
      title: 'VL53L0X distance sensing',
      text: 'Time-of-flight ranging in millimeters, with raw distance preserved in every event so occupancy thresholds can be retuned offline.',
      icon: Radar,
    },
    {
      title: 'Per-zone leader',
      text: 'A second ESP32-C3 elected via the bully algorithm aggregates the zone, time-shares its radio between ESP-NOW and WiFi, and uplinks to the backend over MQTT or REST.',
      icon: Router,
    },
    {
      title: 'FastAPI + SQLite + WebSocket',
      text: 'Backend persists every event with a Lamport timestamp, exposes REST + MQTT ingestion paths, and pushes live updates to this dashboard over a WebSocket.',
      icon: DatabaseZap,
    },
  ]

  return (
    <section id="architecture" className="bg-[#020816] py-24">
      <div className="mx-auto max-w-7xl px-5 sm:px-6 lg:px-8">
        <SectionHeader
          eyebrow="System architecture"
          title="A simple distributed system that still feels production-ready."
          description="The design keeps each hardware responsibility easy to explain while showing how the full data path reaches a modern dashboard."
        />

        <div className="mt-12 grid gap-5 lg:grid-cols-4">
          {cards.map((card, index) => (
            <motion.article
              key={card.title}
              initial={{ opacity: 0, y: 20 }}
              whileInView={{ opacity: 1, y: 0 }}
              viewport={{ once: true, amount: 0.2 }}
              transition={{ delay: index * 0.06 }}
              className="rounded-[2rem] border border-white/10 bg-white/5 p-6 backdrop-blur-xl"
            >
              <div className="flex h-12 w-12 items-center justify-center rounded-2xl border border-cyan/20 bg-cyan/10 text-cyan">
                <card.icon className="h-6 w-6" />
              </div>
              <h3 className="mt-6 font-display text-2xl font-semibold text-white">{card.title}</h3>
              <p className="mt-4 text-sm leading-7 text-slate-300">{card.text}</p>
            </motion.article>
          ))}
        </div>

        <div className="mt-10 rounded-[2rem] border border-cyan/20 bg-[linear-gradient(120deg,rgba(55,214,255,0.12),rgba(255,255,255,0.05))] p-6 shadow-panel">
          <div className="grid gap-6 lg:grid-cols-[1fr_auto_1fr_auto_1fr] lg:items-center">
            <div>
              <p className="text-sm uppercase tracking-[0.32em] text-cyan">Sensor layer</p>
              <p className="mt-2 font-display text-2xl font-semibold text-white">ESP32-C3 + VL53L0X</p>
            </div>
            <ArrowRight className="hidden h-6 w-6 text-cyan lg:block" />
            <div>
              <p className="text-sm uppercase tracking-[0.32em] text-cyan">Leader layer</p>
              <p className="mt-2 font-display text-2xl font-semibold text-white">Bully-elected coordinator</p>
            </div>
            <ArrowRight className="hidden h-6 w-6 text-cyan lg:block" />
            <div>
              <p className="text-sm uppercase tracking-[0.32em] text-cyan">App layer</p>
              <p className="mt-2 font-display text-2xl font-semibold text-white">FastAPI + WebSocket dashboard</p>
            </div>
          </div>
        </div>
      </div>
    </section>
  )
}

export default ArchitectureSection
