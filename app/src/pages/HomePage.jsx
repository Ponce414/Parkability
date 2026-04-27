import Footer from '../components/Footer'
import Dashboard from '../components/Dashboard'
import HeroSection from '../components/HeroSection'
import Navbar from '../components/Navbar'
import ArchitectureSection from '../components/ArchitectureSection'
import ParallaxSection from '../components/ParallaxSection'

function HomePage({ theme, onToggleTheme }) {
  return (
    <div className="min-h-screen bg-slate-100 text-slate-900 transition-colors duration-500 dark:bg-[#020816] dark:text-white">
      <Navbar theme={theme} onToggleTheme={onToggleTheme} />
      <main>
        <HeroSection />
        <ParallaxSection />
        <Dashboard />
        <ArchitectureSection />
      </main>
      <Footer />
    </div>
  )
}

export default HomePage
