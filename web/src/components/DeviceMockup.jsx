import { useRef, useEffect } from 'react'
import { THEME_SCREENSHOTS } from '../data'

export default function DeviceMockup({ theme }) {
  const deviceRef = useRef(null)
  const glareRef = useRef(null)

  useEffect(() => {
    const device = deviceRef.current
    const glare = glareRef.current
    if (!device || !glare) return

    const stage = device.closest('.device-stage')
    if (!stage) return

    const onMove = (e) => {
      const rect = device.getBoundingClientRect()
      const offsetX = (e.clientX - rect.left) / rect.width
      const offsetY = (e.clientY - rect.top) / rect.height
      // Diagonal glare: weighted X + Y for natural angular movement
      const glarePos = (offsetX * 0.75 + offsetY * 0.25) * 120 - 10
      glare.style.setProperty('--glare-pos', `${glarePos}%`)
    }

    const onLeave = () => {
      glare.style.setProperty('--glare-pos', '120%')
    }

    stage.addEventListener('mousemove', onMove)
    stage.addEventListener('mouseleave', onLeave)
    return () => {
      stage.removeEventListener('mousemove', onMove)
      stage.removeEventListener('mouseleave', onLeave)
    }
  }, [])

  return (
    <div className="device-stage">
      <div className="device-anchor">
        <div className="device" ref={deviceRef}>
          {/* Front bezel */}
          <div className="device-bezel">
            <div className="device-screen">
              {Object.entries(THEME_SCREENSHOTS).map(([id, src]) => (
                <img
                  key={id}
                  src={src}
                  alt={`ScryBar ${id} theme preview`}
                  className={`device-screen__img ${id === theme ? 'active' : ''}`}
                  loading="eager"
                />
              ))}
              {/* Glass glare overlay */}
              <div className="device-glare" ref={glareRef} />
            </div>
          </div>

          {/* 3D depth edges */}
          <div className="device-edge device-edge--top" />
          <div className="device-edge device-edge--bottom" />
          <div className="device-edge device-edge--right" />
          <div className="device-edge device-edge--left" />

          {/* Hardware details */}
          <div className="device-usbc" />
          <div className="device-led" />
        </div>
      </div>

      {/* Reflected glow on "desk" */}
      <div className="device-glow" />
    </div>
  )
}
