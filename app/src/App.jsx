import { useEffect, useState } from 'react'
import HomePage from './pages/HomePage'

function App() {
  const [theme, setTheme] = useState(() => {
    const savedTheme = window.localStorage.getItem('parking-theme')
    return savedTheme ?? 'dark'
  })

  useEffect(() => {
    document.documentElement.classList.toggle('dark', theme === 'dark')
    window.localStorage.setItem('parking-theme', theme)
  }, [theme])

  return (
    <HomePage
      theme={theme}
      onToggleTheme={() => setTheme((current) => (current === 'dark' ? 'light' : 'dark'))}
    />
  )
}

export default App
