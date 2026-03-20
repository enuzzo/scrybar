import { useState, useEffect, useRef } from 'react'
import { CLOCK_SENTENCES, LANGUAGES } from '../data'

export default function LanguagePreview() {
  const [langIdx, setLangIdx] = useState(0)
  const [transitioning, setTransitioning] = useState(false)
  const intervalRef = useRef(null)

  const lang = LANGUAGES[langIdx]
  const sentence = CLOCK_SENTENCES[lang.code]

  // Auto-cycle through languages
  useEffect(() => {
    intervalRef.current = setInterval(() => {
      setTransitioning(true)
      setTimeout(() => {
        setLangIdx((i) => (i + 1) % LANGUAGES.length)
        setTransitioning(false)
      }, 300)
    }, 3500)

    return () => clearInterval(intervalRef.current)
  }, [])

  const handleClick = (idx) => {
    clearInterval(intervalRef.current)
    setTransitioning(true)
    setTimeout(() => {
      setLangIdx(idx)
      setTransitioning(false)
      // Restart auto-cycle
      intervalRef.current = setInterval(() => {
        setTransitioning(true)
        setTimeout(() => {
          setLangIdx((i) => (i + 1) % LANGUAGES.length)
          setTransitioning(false)
        }, 300)
      }, 3500)
    }, 300)
  }

  return (
    <div className="lang-preview">
      <div className="lang-sentence-wrap">
        <p className={`lang-sentence ${transitioning ? 'fade-out' : 'fade-in'}`}>
          &ldquo;{sentence}&rdquo;
        </p>
        <span className={`lang-badge ${transitioning ? 'fade-out' : 'fade-in'}`}>
          {lang.flag} {lang.label}
        </span>
      </div>

      <div className="lang-dots">
        {LANGUAGES.map((l, i) => (
          <button
            key={l.code}
            className={`lang-dot ${i === langIdx ? 'active' : ''}`}
            onClick={() => handleClick(i)}
            aria-label={l.label}
            title={l.label}
          />
        ))}
      </div>
    </div>
  )
}
