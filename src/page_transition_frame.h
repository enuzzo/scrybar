#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// A landscape X translation is a native-row translation on ScryBar's panel.
// Cache the two pages in native wire order once, then compose DMA strips with
// at most two contiguous copies. Pixel byte order is preserved throughout.
namespace PageTransitionFrame {

constexpr uint16_t landscapeWidth = 640;
constexpr uint16_t landscapeHeight = 172;
constexpr uint16_t nativeWidth = landscapeHeight;
constexpr uint16_t nativeHeight = landscapeWidth;
constexpr size_t framePixels = size_t(landscapeWidth) * landscapeHeight;
constexpr size_t frameBytes = framePixels * sizeof(uint16_t);

enum class Direction : uint8_t { next, previous };

struct NativeFrame {
  const uint16_t *pixels;
  size_t pixelCount;
};

// Source and destination must not overlap. DISPLAY_FLIP_180 is applied by the
// panel's hardware mirror, so it must NOT alter this cache or composition map.
inline bool rotateLandscapeToNative(uint16_t *dst, size_t dstPixels,
                                    const uint16_t *landscape,
                                    size_t landscapePixels) {
  if (!dst || !landscape || dstPixels < framePixels ||
      landscapePixels < framePixels) return false;
  // Match dispRotateChunk, with small tiles to retain source-cache locality.
  constexpr uint16_t tile = 16;
  for (uint16_t row0 = 0; row0 < nativeHeight; row0 += tile) {
    for (uint16_t col0 = 0; col0 < nativeWidth; col0 += tile) {
      const uint16_t rowEnd = row0 + tile < nativeHeight ? row0 + tile : nativeHeight;
      const uint16_t colEnd = col0 + tile < nativeWidth ? col0 + tile : nativeWidth;
      for (uint16_t row = row0; row < rowEnd; ++row) {
        for (uint16_t col = col0; col < colEnd; ++col) {
          dst[size_t(row) * nativeWidth + col] =
              landscape[size_t(landscapeHeight - 1 - col) * landscapeWidth + row];
        }
      }
    }
  }
  return true;
}

// progressPixels: 0 = outgoing page; landscapeWidth = incoming page.
// next: outgoing moves left, incoming enters from the right; previous reverses
// those directions. dst is a packed strip whose first row is firstNativeRow.
// Source frames are immutable and must not overlap dst. Both capacities are
// checked before any write, and an invalid request leaves dst untouched.
inline bool composeNativeRows(uint16_t *dst, size_t dstPixels,
                              NativeFrame outgoing, NativeFrame incoming,
                              Direction direction, uint16_t progressPixels,
                              uint16_t firstNativeRow, uint16_t rowCount) {
  if ((direction != Direction::next && direction != Direction::previous) ||
      progressPixels > landscapeWidth || firstNativeRow > nativeHeight ||
      rowCount > nativeHeight - firstNativeRow) return false;
  if (!rowCount) return true;
  if (!dst || dstPixels < size_t(rowCount) * nativeWidth ||
      !outgoing.pixels || outgoing.pixelCount < framePixels ||
      !incoming.pixels || incoming.pixelCount < framePixels) return false;

  const uint16_t seam = direction == Direction::next
      ? landscapeWidth - progressPixels : progressPixels;
  const uint16_t firstCount = firstNativeRow < seam
      ? (rowCount < seam - firstNativeRow ? rowCount : seam - firstNativeRow) : 0;

  if (firstCount) {
    const uint16_t *source = direction == Direction::next
        ? outgoing.pixels + size_t(firstNativeRow + progressPixels) * nativeWidth
        : incoming.pixels + size_t(firstNativeRow + landscapeWidth - progressPixels) * nativeWidth;
    memcpy(dst, source, size_t(firstCount) * nativeWidth * sizeof(*dst));
  }
  const uint16_t remaining = rowCount - firstCount;
  if (remaining) {
    const uint16_t row = firstNativeRow + firstCount;
    const uint16_t *source = direction == Direction::next
        ? incoming.pixels + size_t(row - seam) * nativeWidth
        : outgoing.pixels + size_t(row - progressPixels) * nativeWidth;
    memcpy(dst + size_t(firstCount) * nativeWidth, source,
           size_t(remaining) * nativeWidth * sizeof(*dst));
  }
  return true;
}

// On-demand snapshots retain the existing logical 640x172 wire contract.
// This costs a transpose only when SNAP is requested, never per slide frame.
inline bool composeLandscapeFrame(uint16_t *dst, size_t dstPixels,
                                  NativeFrame outgoing, NativeFrame incoming,
                                  Direction direction, uint16_t progressPixels) {
  if (!dst || dstPixels < framePixels || !outgoing.pixels || !incoming.pixels ||
      outgoing.pixelCount < framePixels || incoming.pixelCount < framePixels ||
      progressPixels > landscapeWidth ||
      (direction != Direction::next && direction != Direction::previous)) return false;
  uint16_t row[nativeWidth];
  for (uint16_t x = 0; x < landscapeWidth; ++x) {
    if (!composeNativeRows(row, nativeWidth, outgoing, incoming, direction,
                           progressPixels, x, 1)) return false;
    for (uint16_t col = 0; col < nativeWidth; ++col)
      dst[size_t(landscapeHeight - 1 - col) * landscapeWidth + x] = row[col];
  }
  return true;
}

}  // namespace PageTransitionFrame
