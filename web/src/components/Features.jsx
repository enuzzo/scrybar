import { FEATURES } from '../data'

export default function Features() {
  return (
    <section id="features">
      <div className="container">
        <span className="section-label">Capabilities</span>
        <h2>Everything on a 3.49" bar</h2>
        <p className="section-sub">
          Five swipeable views. Thirteen languages. One desk companion
          that does more than it probably should.
        </p>

        <div className="features-grid">
          {FEATURES.map((f, i) => (
            <div
              key={i}
              className="vm-card feature-card"
              style={{ animationDelay: `${i * 60}ms` }}
            >
              <span className="feature-icon">{f.icon}</span>
              <h3>{f.title}</h3>
              <p>{f.desc}</p>
            </div>
          ))}
        </div>
      </div>
    </section>
  )
}
