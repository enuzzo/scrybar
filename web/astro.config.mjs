import { defineConfig } from 'astro/config';

// GitHub Pages project site needs base: '/scrybar'.
// Set PAGES_BASE=/scrybar in CI; local dev uses '/' by default.
const base = process.env.PAGES_BASE || '/';

export default defineConfig({
  site: process.env.PAGES_SITE || undefined,
  base,
  server: { port: 5173 },
  build: { assets: 'assets' },
  // Keep inter-element whitespace: the default (true) eats the space between
  // text and inline elements split across source lines ("drop<strong>…"),
  // gluing words together. Pages serves gzip, so the size cost is negligible.
  compressHTML: false,
});
