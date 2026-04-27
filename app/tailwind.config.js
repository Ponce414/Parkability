/** @type {import('tailwindcss').Config} */
export default {
  content: ['./index.html', './src/**/*.{js,jsx}'],
  darkMode: 'class',
  theme: {
    extend: {
      colors: {
        midnight: '#07111f',
        ocean: '#0d1b2a',
        cyan: '#37d6ff',
        lime: '#3ddb96',
        coral: '#ff5d73',
        warning: '#ffcc66',
      },
      boxShadow: {
        panel: '0 24px 80px rgba(1, 12, 28, 0.45)',
        soft: '0 16px 40px rgba(8, 15, 30, 0.22)',
      },
      fontFamily: {
        sans: ['"Space Grotesk"', 'ui-sans-serif', 'system-ui', 'sans-serif'],
        display: ['"Sora"', 'ui-sans-serif', 'system-ui', 'sans-serif'],
      },
      backgroundImage: {
        'grid-fade':
          'linear-gradient(rgba(55,214,255,0.08) 1px, transparent 1px), linear-gradient(90deg, rgba(55,214,255,0.08) 1px, transparent 1px)',
      },
      animation: {
        float: 'float 8s ease-in-out infinite',
        pulseGlow: 'pulseGlow 4s ease-in-out infinite',
      },
      keyframes: {
        float: {
          '0%, 100%': { transform: 'translateY(0px)' },
          '50%': { transform: 'translateY(-12px)' },
        },
        pulseGlow: {
          '0%, 100%': { opacity: '0.5' },
          '50%': { opacity: '1' },
        },
      },
    },
  },
  plugins: [],
}
