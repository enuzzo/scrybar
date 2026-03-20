// ============================================
// ScryBar Landing — Content Data
// ============================================

import imgDefault from './assets/screenshots/home_weather_scrybar-default.png'
import imgCyberpunk from './assets/screenshots/home_weather_cyberpunk-2077.png'
import imgToxic from './assets/screenshots/home_weather_toxic-candy.png'
import imgTokyo from './assets/screenshots/home_weather_tokyo-transit.png'
import imgBrutalist from './assets/screenshots/home_weather_minimal-brutalist-mono.png'
import imgDoom from './assets/screenshots/doom_bunker_console.png'
import imgRss from './assets/screenshots/aux_rss.png'
import imgWiki from './assets/screenshots/wiki_stream.png'
import imgInfo from './assets/screenshots/info_panel.png'

// ---- Themes ----
export const THEMES = [
  { id: 'scrybar-default', label: 'Default',     shortLabel: 'Default' },
  { id: 'cyberpunk-2077',  label: 'Cyberpunk',    shortLabel: 'Cyber' },
  { id: 'toxic-candy',     label: 'Toxic Candy',  shortLabel: 'Toxic' },
  { id: 'tokyo-transit',   label: 'Tokyo Transit', shortLabel: 'Tokyo' },
  { id: 'minimal-brutalist-mono', label: 'Brutalist', shortLabel: 'Mono' },
]

export const THEME_SCREENSHOTS = {
  'scrybar-default': imgDefault,
  'cyberpunk-2077': imgCyberpunk,
  'toxic-candy': imgToxic,
  'tokyo-transit': imgTokyo,
  'minimal-brutalist-mono': imgBrutalist,
}

// ---- Word clock demo sentences ----
export const CLOCK_SENTENCES = {
  it:   'sono le undici e ventitré',
  en:   "it's eleven twenty-three",
  fr:   'il est onze heures vingt-trois',
  de:   'es ist dreiundzwanzig nach elf',
  es:   'son las once y veintitrés',
  pt:   'são onze e vinte e três',
  la:   'hora undecima viginti tres minutae',
  eo:   'estas la dek-unua kaj dudek tri',
  tlh:  "DaH wa'maH wa' rep cha'maH wej tup",
  l33t: '17z 3L3V3N 7W3N7Y-7HR33',
  sha:  "Hark! 'Tis eleven and three-and-twenty!",
  val:  'it\'s like, eleven twenty-three?',
  bellazio: 'boh, tipo le undici e ventitré fra',
}

export const LANGUAGES = [
  { code: 'it',   label: 'Italiano',      flag: '🇮🇹' },
  { code: 'en',   label: 'English',       flag: '🇬🇧' },
  { code: 'fr',   label: 'Français',      flag: '🇫🇷' },
  { code: 'de',   label: 'Deutsch',       flag: '🇩🇪' },
  { code: 'es',   label: 'Español',       flag: '🇪🇸' },
  { code: 'pt',   label: 'Português',     flag: '🇵🇹' },
  { code: 'la',   label: 'Latina',        flag: '🏛️' },
  { code: 'eo',   label: 'Esperanto',     flag: '🌍' },
  { code: 'tlh',  label: 'Klingon',       flag: '🖖' },
  { code: 'l33t', label: '1337 5P34K',    flag: '💻' },
  { code: 'sha',  label: 'Shakespearean', flag: '🎭' },
  { code: 'val',  label: 'Valley Girl',   flag: '💅' },
  { code: 'bellazio', label: 'Bellazio',   flag: '🇮🇹' },
]

// ---- Features ----
export const FEATURES = [
  {
    icon: '🕐',
    title: 'Word Clock',
    desc: 'Natural language time in 13 languages. From Italian to Klingon to Shakespearean English.',
  },
  {
    icon: '🌤',
    title: 'Live Weather',
    desc: 'Current conditions with forecasts. Configurable location. Updates every 15 minutes.',
  },
  {
    icon: '📰',
    title: 'RSS Feeds',
    desc: 'Up to 5 news feeds with QR codes for instant mobile reading. Build your own deck.',
  },
  {
    icon: '📚',
    title: 'Wikipedia',
    desc: 'Featured articles, On This Day, and random discoveries. 8 languages supported.',
  },
  {
    icon: '👾',
    title: 'DOOM',
    desc: 'Yes, it runs DOOM. Tilt to move, tap to shoot. IMU-controlled. Real id Tech 1.',
  },
  {
    icon: '🎵',
    title: 'Now Playing',
    desc: 'macOS companion pushes what\'s playing. Album art, progress bar, source detection.',
  },
  {
    icon: '🎨',
    title: '5 Themes',
    desc: 'Deep navy to cyberpunk neon to pure monochrome. Theme drives both device and web UI.',
  },
  {
    icon: '📡',
    title: 'LAN Config',
    desc: 'Full web control surface over WiFi. Theme, language, feeds, weather — no reflash needed.',
  },
]

// ---- View screenshots for features section ----
export const VIEW_SCREENSHOTS = {
  doom: imgDoom,
  rss: imgRss,
  wiki: imgWiki,
  info: imgInfo,
}

// ---- Hardware specs ----
export const SPECS = [
  { label: 'MCU',        value: 'ESP32-S3' },
  { label: 'Display',    value: '3.49" AMOLED 640×172' },
  { label: 'Controller', value: 'AXS15231B' },
  { label: 'Touch',      value: 'Capacitive (multi-point)' },
  { label: 'Flash',      value: '16 MB' },
  { label: 'PSRAM',      value: '8 MB OPI' },
  { label: 'WiFi',       value: '2.4 GHz 802.11 b/g/n' },
  { label: 'IMU',        value: 'QMI8658 (accel + gyro)' },
  { label: 'Interface',  value: 'USB-C (data + power)' },
  { label: 'Battery',    value: 'Onboard monitor (ADC)' },
  { label: 'Framework',  value: 'Arduino + LVGL 8' },
  { label: 'License',    value: 'MIT' },
]
