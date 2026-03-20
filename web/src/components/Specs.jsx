import { SPECS } from '../data'

export default function Specs() {
  return (
    <section id="specs">
      <div className="container">
        <span className="section-label">Hardware</span>
        <h2>What's inside</h2>
        <p className="section-sub">
          Waveshare ESP32-S3-Touch-LCD-3.49 — a bar-shaped AMOLED touchscreen
          with enough power to run a game engine.
        </p>

        <div className="specs-table">
          {SPECS.map((s, i) => (
            <div key={i} className="spec-row">
              <span className="spec-label">{s.label}</span>
              <span className="spec-value">{s.value}</span>
            </div>
          ))}
        </div>
      </div>
    </section>
  )
}
