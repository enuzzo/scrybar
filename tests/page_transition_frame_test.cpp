#include "../src/page_transition_frame.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <vector>

namespace Frame = PageTransitionFrame;

static uint16_t fixture(unsigned page, unsigned x, unsigned y) {
  // Nonuniform channels expose transpose, byte-order, seam and stride errors.
  return uint16_t((x * 73U) ^ (y * 1543U) ^ (page * 0x5A39U) ^ ((x + y) << 7));
}

static uint16_t referenceLogical(const std::vector<uint16_t> &outgoing,
                                 const std::vector<uint16_t> &incoming,
                                 Frame::Direction direction, unsigned progress,
                                 unsigned x, unsigned y) {
  // An independent landscape-space reference: translate both complete pages
  // and ask which page covers this destination pixel. No native-row formula.
  const int outLeft = direction == Frame::Direction::next ? -int(progress) : int(progress);
  const int inLeft = direction == Frame::Direction::next
      ? int(Frame::landscapeWidth - progress) : int(progress) - Frame::landscapeWidth;
  const int outX = int(x) - outLeft;
  const int inX = int(x) - inLeft;
  if (outX >= 0 && outX < Frame::landscapeWidth)
    return outgoing[size_t(y) * Frame::landscapeWidth + outX];
  assert(inX >= 0 && inX < Frame::landscapeWidth);
  return incoming[size_t(y) * Frame::landscapeWidth + inX];
}

static void checkInvalidRequests(const std::vector<uint16_t> &outNative,
                                 const std::vector<uint16_t> &inNative) {
  const Frame::NativeFrame out{outNative.data(), outNative.size()};
  const Frame::NativeFrame in{inNative.data(), inNative.size()};
  std::vector<uint16_t> strip(64U * Frame::nativeWidth + 2U, 0xA55A);
  const auto pristine = strip;
  auto render = [&](Frame::NativeFrame outgoing, Frame::NativeFrame incoming,
                    Frame::Direction direction, uint16_t progress,
                    uint16_t first, uint16_t count, size_t capacity) {
    const bool result = Frame::composeNativeRows(strip.data() + 1, capacity,
        outgoing, incoming, direction, progress, first, count);
    assert(strip == pristine);
    return result;
  };
  const size_t capacity = 64U * Frame::nativeWidth;
  assert(!render(out, in, Frame::Direction::next, 641, 0, 64, capacity));
  assert(!render(out, in, Frame::Direction::previous, 100, 641, 0, capacity));
  assert(!render(out, in, Frame::Direction::next, 100, 639, 2, capacity));
  assert(!render(out, in, Frame::Direction::next, 100, 0, 64, capacity - 1));
  assert(!render({out.pixels, out.pixelCount - 1}, in, Frame::Direction::next, 0, 0, 64, capacity));
  assert(!render(out, {in.pixels, in.pixelCount - 1}, Frame::Direction::next, 640, 0, 64, capacity));
  assert(!render({nullptr, out.pixelCount}, in, Frame::Direction::next, 0, 0, 64, capacity));
  assert(!render(out, in, static_cast<Frame::Direction>(17), 0, 0, 64, capacity));
  assert(!Frame::composeNativeRows(nullptr, capacity, out, in,
      Frame::Direction::next, 100, 0, 64));
  // An empty, otherwise bounded request does not access frames or destination.
  assert(Frame::composeNativeRows(nullptr, 0, {nullptr, 0}, {nullptr, 0},
      Frame::Direction::next, 640, 640, 0));
}

int main() {
  static_assert(Frame::frameBytes == 220160, "ScryBar cache geometry changed");
  const size_t guard = 7;
  std::vector<uint16_t> outgoing(Frame::framePixels), incoming(Frame::framePixels);
  std::vector<uint16_t> outStorage(Frame::framePixels + 2 * guard, 0xCAFE);
  std::vector<uint16_t> inStorage(Frame::framePixels + 2 * guard, 0xCAFE);
  for (unsigned y = 0; y < Frame::landscapeHeight; ++y)
    for (unsigned x = 0; x < Frame::landscapeWidth; ++x) {
      outgoing[size_t(y) * Frame::landscapeWidth + x] = fixture(0, x, y);
      incoming[size_t(y) * Frame::landscapeWidth + x] = fixture(1, x, y);
    }
  auto *outNative = outStorage.data() + guard;
  auto *inNative = inStorage.data() + guard;
  assert(Frame::rotateLandscapeToNative(outNative, Frame::framePixels, outgoing.data(), outgoing.size()));
  assert(Frame::rotateLandscapeToNative(inNative, Frame::framePixels, incoming.data(), incoming.size()));
  for (size_t i = 0; i < guard; ++i) {
    assert(outStorage[i] == 0xCAFE && outStorage[outStorage.size() - 1 - i] == 0xCAFE);
    assert(inStorage[i] == 0xCAFE && inStorage[inStorage.size() - 1 - i] == 0xCAFE);
  }
  for (unsigned y = 0; y < Frame::landscapeHeight; ++y)
    for (unsigned x = 0; x < Frame::landscapeWidth; ++x) {
      const size_t nativeAt = size_t(x) * Frame::nativeWidth + Frame::landscapeHeight - 1 - y;
      assert(outNative[nativeAt] == outgoing[size_t(y) * Frame::landscapeWidth + x]);
      assert(inNative[nativeAt] == incoming[size_t(y) * Frame::landscapeWidth + x]);
    }

  const Frame::NativeFrame out{outNative, Frame::framePixels};
  const Frame::NativeFrame in{inNative, Frame::framePixels};
  std::vector<uint16_t> nativeFrame(Frame::framePixels);
  std::vector<uint16_t> logicalSnapshot(Frame::framePixels + 2 * guard, 0xB44B);
  const unsigned stripHeights[] = {1, 17, 64, 172, 640};
  size_t checked = 0;
  for (Frame::Direction direction : {Frame::Direction::next, Frame::Direction::previous}) {
    for (unsigned progress = 0; progress <= Frame::landscapeWidth; ++progress) {
      // Exhaust every seam position with the production 64-row DMA chunks;
      // also exercise different clipping sizes on endpoint/odd/seam fixtures.
      for (unsigned stripHeight : stripHeights) {
        if (stripHeight != 64 && progress != 0 && progress != 1 && progress != 319 &&
            progress != 320 && progress != 639 && progress != 640) continue;
        for (unsigned row = 0; row < Frame::nativeHeight; row += stripHeight) {
          const unsigned count = std::min(stripHeight, unsigned(Frame::nativeHeight) - row);
          const size_t pixels = size_t(count) * Frame::nativeWidth;
          std::vector<uint16_t> strip(pixels + 2 * guard, 0xC33C);
          assert(Frame::composeNativeRows(strip.data() + guard, pixels, out, in,
              direction, uint16_t(progress), uint16_t(row), uint16_t(count)));
          for (size_t i = 0; i < guard; ++i) {
            assert(strip[i] == 0xC33C && strip[strip.size() - 1 - i] == 0xC33C);
          }
          std::copy(strip.begin() + guard, strip.end() - guard,
                    nativeFrame.begin() + size_t(row) * Frame::nativeWidth);
        }
        for (unsigned y = 0; y < Frame::landscapeHeight; ++y)
          for (unsigned x = 0; x < Frame::landscapeWidth; ++x) {
            const size_t nativeAt = size_t(x) * Frame::nativeWidth + Frame::landscapeHeight - 1 - y;
            assert(nativeFrame[nativeAt] == referenceLogical(outgoing, incoming, direction, progress, x, y));
            ++checked;
          }
        if (stripHeight == 64 && (progress == 0 || progress == 137 || progress == 639 || progress == 640)) {
          assert(Frame::composeLandscapeFrame(logicalSnapshot.data() + guard,
              Frame::framePixels, out, in, direction, uint16_t(progress)));
          for (unsigned y = 0; y < Frame::landscapeHeight; ++y)
            for (unsigned x = 0; x < Frame::landscapeWidth; ++x)
              assert(logicalSnapshot[guard + size_t(y) * Frame::landscapeWidth + x] ==
                  referenceLogical(outgoing, incoming, direction, progress, x, y));
          for (size_t i = 0; i < guard; ++i)
            assert(logicalSnapshot[i] == 0xB44B && logicalSnapshot[logicalSnapshot.size() - 1 - i] == 0xB44B);
        }
        // DISPLAY_FLIP_180 mirrors the panel's native axes, after composition.
        // Verify actual physical pixels against the independent logical slide.
        if (stripHeight == 64 && (progress == 0 || progress == 137 || progress == 639 || progress == 640)) {
          for (bool flip180 : {false, true})
            for (unsigned physicalRow = 0; physicalRow < Frame::nativeHeight; ++physicalRow)
              for (unsigned physicalCol = 0; physicalCol < Frame::nativeWidth; ++physicalCol) {
                const unsigned memoryRow = flip180 ? Frame::nativeHeight - 1 - physicalRow : physicalRow;
                const unsigned memoryCol = flip180 ? Frame::nativeWidth - 1 - physicalCol : physicalCol;
                const unsigned logicalX = flip180 ? Frame::landscapeWidth - 1 - physicalRow : physicalRow;
                const unsigned logicalY = flip180 ? physicalCol : Frame::landscapeHeight - 1 - physicalCol;
                assert(nativeFrame[size_t(memoryRow) * Frame::nativeWidth + memoryCol] ==
                    referenceLogical(outgoing, incoming, direction, progress, logicalX, logicalY));
              }
        }
      }
    }
  }
  checkInvalidRequests(std::vector<uint16_t>(outNative, outNative + Frame::framePixels),
                       std::vector<uint16_t>(inNative, inNative + Frame::framePixels));
  uint16_t untouched = 0x1234;
  assert(!Frame::rotateLandscapeToNative(&untouched, 1, outgoing.data(), outgoing.size()));
  assert(!Frame::rotateLandscapeToNative(outNative, Frame::framePixels, incoming.data(), 1));
  assert(untouched == 0x1234);
  std::printf("page_transition_frame: %zu pixel-reference comparisons passed; all seams, DMA clips and physical mirrors\n", checked);
}
