import { THEMES } from '../data'

export default function ThemeSelector({ active, onChange }) {
  return (
    <div className="theme-selector">
      {THEMES.map((t) => (
        <button
          key={t.id}
          className={`theme-pill ${t.id === active ? 'active' : ''}`}
          onClick={() => onChange(t.id)}
          aria-label={`Switch to ${t.label} theme`}
          data-theme-id={t.id}
        >
          <span className="theme-pill__dot" />
          <span className="theme-pill__label">{t.shortLabel}</span>
        </button>
      ))}
    </div>
  )
}
