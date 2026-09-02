#!/usr/bin/env node

import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { createCanvas, loadImage } from '@napi-rs/canvas';
import { JSDOM } from 'jsdom';

const toolDir = path.dirname(fileURLToPath(import.meta.url));
const projectRoot = path.resolve(toolDir, '..', '..');
const outputRoot = path.join(projectRoot, 'assets', 'meteocons', 'frames');
const forecastOutputRoot = path.join(projectRoot, 'assets', 'meteocons', 'forecast');
const sourceRoot = path.join(toolDir, 'node_modules', '@meteocons', 'lottie', 'fill');
const forecastSourceRoot = path.join(toolDir, 'node_modules', '@meteocons', 'lottie', 'monochrome');

const iconNames = [
  'clear-day',
  'clear-night',
  'mostly-clear-day',
  'mostly-clear-night',
  'partly-cloudy-day',
  'partly-cloudy-night',
  'overcast',
  'fog-day',
  'fog-night',
  'drizzle',
  'rain',
  'extreme-rain',
  'sleet',
  'snow',
  'extreme-snow',
  'thunderstorms-day',
  'thunderstorms-night',
  'thunderstorms-day-hail',
  'thunderstorms-night-hail',
];

const size = Number.parseInt(process.env.METEOCON_SIZE ?? '84', 10);
const frameCount = Number.parseInt(process.env.METEOCON_FRAMES ?? '10', 10);
const forecastSize = Number.parseInt(process.env.METEOCON_FORECAST_SIZE ?? '30', 10);
const sampleSize = 384;
if (!Number.isInteger(size) || size < 16 || size > 128) throw new Error(`Invalid METEOCON_SIZE: ${size}`);
if (!Number.isInteger(frameCount) || frameCount < 2 || frameCount > 12) throw new Error(`Invalid METEOCON_FRAMES: ${frameCount}`);
if (!Number.isInteger(forecastSize) || forecastSize < 16 || forecastSize > 64) throw new Error(`Invalid METEOCON_FORECAST_SIZE: ${forecastSize}`);

// Lottie Web's SVG renderer still probes a 2D canvas while loading. Give
// jsdom a native canvas-backed implementation, then rasterize each SVG frame.
const dom = new JSDOM('<!doctype html><div id="stage"></div>', { pretendToBeVisual: true });
const browserWindow = dom.window;
globalThis.window = browserWindow;
globalThis.document = browserWindow.document;
Object.defineProperty(globalThis, 'navigator', { value: browserWindow.navigator, configurable: true });
globalThis.Element = browserWindow.Element;
globalThis.SVGElement = browserWindow.SVGElement;
globalThis.requestAnimationFrame = browserWindow.requestAnimationFrame.bind(browserWindow);
globalThis.cancelAnimationFrame = browserWindow.cancelAnimationFrame.bind(browserWindow);
Object.defineProperty(browserWindow.HTMLCanvasElement.prototype, 'getContext', {
  value(type) {
    if (!this.nativeCanvas) this.nativeCanvas = createCanvas(this.width || 300, this.height || 150);
    return this.nativeCanvas.getContext(type);
  },
});

const lottie = (await import('lottie-web/build/player/lottie.js')).default;
const stage = document.getElementById('stage');
fs.mkdirSync(outputRoot, { recursive: true });
fs.mkdirSync(forecastOutputRoot, { recursive: true });

function alphaBounds(canvas) {
  const { data } = canvas.getContext('2d').getImageData(0, 0, canvas.width, canvas.height);
  let left = canvas.width;
  let top = canvas.height;
  let right = -1;
  let bottom = -1;
  for (let y = 0; y < canvas.height; y += 1) {
    for (let x = 0; x < canvas.width; x += 1) {
      if (data[((y * canvas.width + x) * 4) + 3] < 4) continue;
      left = Math.min(left, x);
      top = Math.min(top, y);
      right = Math.max(right, x);
      bottom = Math.max(bottom, y);
    }
  }
  if (right < left || bottom < top) return { left: 0, top: 0, right: canvas.width - 1, bottom: canvas.height - 1 };
  return { left, top, right, bottom };
}

function unionBounds(canvases) {
  return canvases.map(alphaBounds).reduce((union, bounds) => ({
    left: Math.min(union.left, bounds.left),
    top: Math.min(union.top, bounds.top),
    right: Math.max(union.right, bounds.right),
    bottom: Math.max(union.bottom, bounds.bottom),
  }));
}

function fitCropped(source, bounds, outputSize, padding) {
  const cropW = bounds.right - bounds.left + 1;
  const cropH = bounds.bottom - bounds.top + 1;
  const usable = outputSize - (padding * 2);
  const scale = Math.min(usable / cropW, usable / cropH);
  const drawW = cropW * scale;
  const drawH = cropH * scale;
  const canvas = createCanvas(outputSize, outputSize);
  const context = canvas.getContext('2d');
  context.imageSmoothingEnabled = true;
  context.imageSmoothingQuality = 'high';
  context.drawImage(
    source,
    bounds.left,
    bounds.top,
    cropW,
    cropH,
    (outputSize - drawW) / 2,
    (outputSize - drawH) / 2,
    drawW,
    drawH,
  );
  return canvas;
}

async function renderSourceFrames(sourcePath, count, startOffset = 0) {
  stage.replaceChildren();
  const animation = lottie.loadAnimation({
    container: stage,
    renderer: 'svg',
    loop: false,
    autoplay: false,
    animationData: JSON.parse(fs.readFileSync(sourcePath, 'utf8')),
    rendererSettings: { preserveAspectRatio: 'xMidYMid meet' },
  });
  await new Promise((resolve) => animation.addEventListener('DOMLoaded', resolve));

  const frames = [];
  for (let index = 0; index < count; index += 1) {
    const progress = (index + startOffset) / count;
    const frame = Math.min(animation.totalFrames - 1, Math.floor(animation.totalFrames * progress));
    animation.goToAndStop(frame, true);
    const svg = stage.querySelector('svg');
    if (!svg) throw new Error(`${path.basename(sourcePath)}: SVG renderer produced no image`);
    const image = await loadImage(Buffer.from(svg.outerHTML));
    const canvas = createCanvas(sampleSize, sampleSize);
    canvas.getContext('2d').drawImage(image, 0, 0, sampleSize, sampleSize);
    frames.push(canvas);
  }
  animation.destroy();
  return frames;
}

for (const iconName of iconNames) {
  const sourcePath = path.join(sourceRoot, `${iconName}.json`);
  const forecastSourcePath = path.join(forecastSourceRoot, `${iconName}.json`);
  if (!fs.existsSync(sourcePath)) throw new Error(`Missing Meteocon: ${sourcePath}`);
  if (!fs.existsSync(forecastSourcePath)) throw new Error(`Missing monochrome Meteocon: ${forecastSourcePath}`);

  const iconOutput = path.join(outputRoot, iconName);
  fs.mkdirSync(iconOutput, { recursive: true });
  for (const existing of fs.readdirSync(iconOutput)) {
    if (existing.endsWith('.png')) fs.unlinkSync(path.join(iconOutput, existing));
  }
  const sourceFrames = await renderSourceFrames(sourcePath, frameCount);
  const mainBounds = unionBounds(sourceFrames);
  for (let index = 0; index < frameCount; index += 1) {
    const canvas = fitCropped(sourceFrames[index], mainBounds, size, 4);
    const outputPath = path.join(iconOutput, `${String(index).padStart(2, '0')}.png`);
    fs.writeFileSync(outputPath, canvas.toBuffer('image/png'));
  }

  const forecastFrame = (await renderSourceFrames(forecastSourcePath, 1, 0.35))[0];
  const forecastCanvas = fitCropped(forecastFrame, alphaBounds(forecastFrame), forecastSize, 1);
  fs.writeFileSync(path.join(forecastOutputRoot, `${iconName}.png`), forecastCanvas.toBuffer('image/png'));
  console.log(`${iconName}: ${frameCount} fill frames + monochrome forecast`);
}

dom.window.close();
console.log(`Rendered ${iconNames.length * frameCount} Meteocons frames at ${size}x${size} and ${forecastSize}x${forecastSize} monochrome forecasts.`);
