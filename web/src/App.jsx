import { useState, useEffect, useCallback } from 'react'
import DeviceMockup from './components/DeviceMockup'
import ThemeSelector from './components/ThemeSelector'
import LanguagePreview from './components/LanguagePreview'
import Features from './components/Features'
import FlashSection from './components/FlashSection'
import Specs from './components/Specs'
import Footer from './components/Footer'

export default function App() {
  const [theme, setTheme] = useState('scrybar-default')

  const handleThemeChange = useCallback((id) => {
    setTheme(id)
    document.documentElement.setAttribute('data-theme', id)
  }, [])

  // Scroll-reveal observer
  useEffect(() => {
    const observer = new IntersectionObserver(
      (entries) => {
        entries.forEach((entry) => {
          if (entry.isIntersecting) {
            entry.target.classList.add('revealed')
            observer.unobserve(entry.target)
          }
        })
      },
      { threshold: 0.08, rootMargin: '0px 0px -40px 0px' }
    )

    document.querySelectorAll('.reveal').forEach((el) => observer.observe(el))
    return () => observer.disconnect()
  }, [])

  return (
    <>
      {/* Ambient background layers */}
      <div className="page-glow" />
      <div className="page-noise" />

      <div className="content">
        {/* ============ HERO ============ */}
        <section className="hero">
          <div className="container">
            <nav className="hero-nav entrance entrance--1">
              <span className="hero-wordmark">ScryBar</span>
              <div className="hero-nav-links">
                <a href="#features">Features</a>
                <a href="#flash">Flash</a>
                <a href="#specs">Specs</a>
                <a
                  href="https://github.com/enuzzo/scrybar"
                  target="_blank"
                  rel="noopener noreferrer"
                  className="hero-nav-gh"
                >
                  GitHub
                </a>
              </div>
            </nav>

            <div className="hero-content">
              <div className="hero-badges entrance entrance--2">
                <span className="vm-badge vm-badge--brand">ESP32-S3</span>
                <span className="vm-badge vm-badge--info">Open Source</span>
              </div>

              <h1 className="entrance entrance--3">
                Your desk knows<br />
                things now.
              </h1>
              <p className="hero-lede entrance entrance--4">
                A mass of sensors, pixels, and unresolved ambition,
                pretending to be furniture. Word clock in 13 languages,
                live weather, RSS feeds, Wikipedia, and DOOM — on a
                3.49&quot; AMOLED touchscreen.
              </p>

              <div className="hero-ctas entrance entrance--5">
                <a href="#flash" className="vm-btn vm-btn--primary vm-btn--lg">
                  Flash Firmware
                </a>
                <a href="#features" className="vm-btn vm-btn--secondary vm-btn--lg">
                  Explore Features
                </a>
              </div>
            </div>

            {/* Interactive device mockup */}
            <div className="entrance entrance--6">
              <DeviceMockup theme={theme} />
            </div>

            {/* Theme switcher */}
            <div className="entrance entrance--7">
              <ThemeSelector active={theme} onChange={handleThemeChange} />
            </div>

            {/* Language cycling preview */}
            <div className="entrance entrance--8">
              <LanguagePreview />
            </div>
          </div>
        </section>

        {/* ============ FEATURES ============ */}
        <div className="reveal">
          <Features />
        </div>

        {/* ============ FLASH ============ */}
        <div className="reveal">
          <FlashSection />
        </div>

        {/* ============ SPECS ============ */}
        <div className="reveal">
          <Specs />
        </div>

        {/* ============ FOOTER ============ */}
        <Footer />
      </div>
    </>
  )
}
