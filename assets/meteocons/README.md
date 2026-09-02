# Meteocons on ScryBar

ScryBar uses the **Fill** animations from [Meteocons](https://github.com/basmilius/meteocons),
rendered into ten tightly cropped 84 x 84 frames per weather variant and converted
to LVGL 8 `LV_IMG_CF_TRUE_COLOR_ALPHA` data. The ESP32 cycles the frames at 5 fps
only while Home is visible; it does not run SVG, JavaScript, or Lottie at runtime.

The three-hour forecast uses a separate native 30 x 30 render from Meteocons'
**Monochrome** set. Firmware recolors that image black or white from the resolved
weather-card luminance, avoiding low-contrast details and runtime downscaling.

- Upstream: `basmilius/meteocons`
- Package: `@meteocons/lottie` 0.1.0
- Reference commit used: `70dfb1d6e30dc9e791cfb0e4c5b5e5e60e972aa0`
- Style: Fill
- License: MIT; see `LICENSE`

Regenerate the assets with:

```sh
npm ci --prefix tools/meteocons
npm run --prefix tools/meteocons render
python3 tools/generate_meteocons.py
```

Renderer defaults are `METEOCON_SIZE=84`, `METEOCON_FRAMES=10`, and
`METEOCON_FORECAST_SIZE=30`. Every animated icon uses a union alpha crop across
all frames, which preserves stable alignment while removing transparent padding.

The generated PNG frames and LVGL header are committed so normal firmware
builds do not need Node.js or the rendering dependencies.
