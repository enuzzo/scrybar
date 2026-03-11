#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool doomPrboomEnsureStarted();
bool doomPrboomHasFrame();
bool doomPrboomIsRunning();
const char *doomPrboomStatus();
void doomPrboomReportFatal(const char *message);

bool doomScrybarBlitIndexedFrame(const uint8_t *pixels,
                                 int srcWidth,
                                 int srcHeight,
                                 const uint16_t *palette565be,
                                 bool forceFlush);
bool doomScrybarPageVisible();
bool doomScrybarGetInputState(int8_t *moveBin, int8_t *turnBin, uint8_t *touchZone);

#ifdef __cplusplus
}
#endif
