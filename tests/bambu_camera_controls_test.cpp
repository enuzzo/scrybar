#include "../src/bambu_camera_controls.h"
#include <cassert>
using namespace BambuCameraControls;
int main() {
  // The whole visible CAM button, including its widened right edge, is active.
  for (int x = camera.x; x < camera.x + camera.width; ++x)
    for (int y = camera.y; y < camera.y + camera.height; ++y)
      assert(hit(false, false, x, y) == open);
  assert(hit(false, false, 353, 15) == none);
  assert(hit(false, false, 466, 15) == none);
  assert(hit(false, false, 410, 35) == none);
  // A deliberate press and ordinary finger drift must not become a page swipe.
  assert(validRelease(open, open, 500, 18, 8));
  assert(validRelease(open, open, 1400, -32, 0));
  assert(!validRelease(open, open, 1401, 0, 0));
  assert(!validRelease(open, open, 200, 40, 0));
  assert(!validRelease(open, none, 200, 0, 0));
  assert(!validRelease(none, open, 200, 0, 0));
  // Ending on another control must never toggle the light accidentally.
  assert(!validRelease(close, toggleLight, 200, 0, 30));
  assert(hit(true, true, 550, 80) == toggleLight);
  assert(hit(true, false, 550, 80) == none);
  assert(hit(true, false, 550, 35) == none);
  assert(hit(true, true, 550, 130) == close);
  assert(hit(true, true, 410, 15) == none);
}
