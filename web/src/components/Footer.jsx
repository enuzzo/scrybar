export default function Footer() {
  return (
    <footer className="site-footer">
      <div className="container">
        <div className="footer-inner">
          <div className="footer-brand">
            <span className="footer-wordmark">ScryBar</span>
            <span className="footer-studio">
              A project by <strong>Netmilk Studio</strong>
            </span>
          </div>
          <div className="footer-links">
            <a
              href="https://github.com/enuzzo/scrybar"
              target="_blank"
              rel="noopener noreferrer"
            >
              GitHub
            </a>
            <span className="footer-sep">/</span>
            <a
              href="https://github.com/enuzzo/scrybar/blob/main/LICENSE"
              target="_blank"
              rel="noopener noreferrer"
            >
              MIT License
            </a>
            <span className="footer-sep">/</span>
            <a
              href="https://netmilk.ch"
              target="_blank"
              rel="noopener noreferrer"
            >
              netmilk.ch
            </a>
          </div>
          <p className="footer-copy">
            Open source. Feel free to steal, fork, remix, and ship.
          </p>
        </div>
      </div>
    </footer>
  )
}
