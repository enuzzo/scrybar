#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#if __has_include("freertos/idf_additions.h")
#include "freertos/idf_additions.h"
#define SCRYBAR_PRBOOM_TASK_CAPS 1
#endif
#ifndef SCRYBAR_PRBOOM_TASK_CAPS
#define SCRYBAR_PRBOOM_TASK_CAPS 0
#endif
#include "freertos/task.h"

#include "scrybar_prboom_runtime.h"

extern "C" {
#include "prboom/d_event.h"
#include "prboom/doomtype.h"
#include "prboom/doomstat.h"
#include "prboom/doomdef.h"
#include "prboom/d_main.h"
#include "prboom/g_game.h"
#include "prboom/i_system.h"
#include "prboom/i_video.h"
#include "prboom/i_sound.h"
#include "prboom/i_main.h"
#include "prboom/m_argv.h"
#include "prboom/m_fixed.h"
#include "prboom/m_misc.h"
#include "prboom/r_draw.h"
#include "prboom/r_fps.h"
#include "prboom/s_sound.h"
#include "prboom/st_stuff.h"
#include "prboom/v_video.h"
#include "prboom/w_wad.h"
#include "prboom/z_zone.h"
}

extern const uint8_t scrybar_doom1_wad_start[] asm("scrybar_doom1_wad_start");
extern const uint8_t scrybar_doom1_wad_end[] asm("scrybar_doom1_wad_end");

namespace {

constexpr int kDoomSampleRate = 22050;
constexpr uint32_t kDoomTaskStack = 32768;
constexpr BaseType_t kDoomTaskPriority = 1;
constexpr size_t kDoomFrameBytes = 320 * 200;

struct DoomInputButtons {
  bool forward = false;
  bool back = false;
  bool left = false;
  bool right = false;
  bool use = false;
  bool fire = false;
};

TaskHandle_t g_doomTaskHandle = nullptr;
bool g_doomTaskStarted = false;
bool g_doomHasFrame = false;
char g_doomStatus[96] = "boot";
uint8_t *g_doomFrame8 = nullptr;
uint16_t g_doomPalette565[256] = {0};
DoomInputButtons g_prevButtons = {};
const char *g_doomArgv[] = {
    "doom",
    "-warp", "1", "1",
    "-skill", "3",
    "-nomusic",
    "-nosfx",
    nullptr,
};

void doomSetStatus(const char *status) {
  if (!status) status = "";
  if (strncmp(g_doomStatus, status, sizeof(g_doomStatus)) == 0) return;
  snprintf(g_doomStatus, sizeof(g_doomStatus), "%s", status);
  printf("[DOOM][CORE] %s\n", g_doomStatus);
}

void doomPostKeyEvent(bool pressed, int key) {
  event_t event = {};
  event.type = pressed ? ev_keydown : ev_keyup;
  event.data1 = key;
  D_PostEvent(&event);
}

void doomSyncButton(bool pressed, bool *previous, int key) {
  if (!previous || *previous == pressed) return;
  *previous = pressed;
  doomPostKeyEvent(pressed, key);
}

void doomDeleteCurrentTask() {
#if SCRYBAR_PRBOOM_TASK_CAPS
  vTaskDeleteWithCaps(nullptr);
#else
  vTaskDelete(nullptr);
#endif
}

void doomTaskMain(void *) {
  doomSetStatus("init");

  if (!g_doomFrame8) {
    g_doomFrame8 = static_cast<uint8_t *>(heap_caps_malloc(kDoomFrameBytes, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL));
    if (!g_doomFrame8) {
      g_doomFrame8 = static_cast<uint8_t *>(heap_caps_malloc(kDoomFrameBytes, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM));
      if (g_doomFrame8) {
        doomSetStatus("fb spiram");
      }
    }
    if (!g_doomFrame8) {
      char status[96];
      snprintf(status,
               sizeof(status),
               "fb alloc fail h=%u p=%u",
               (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
               (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
      doomSetStatus(status);
      doomDeleteCurrentTask();
      return;
    }
    memset(g_doomFrame8, 0, kDoomFrameBytes);
  }

  myargv = g_doomArgv;
  myargc = 8;
  SCREENWIDTH = 320;
  SCREENHEIGHT = 200;
  heap_caps_malloc_extmem_enable(0);

  doomSetStatus("running");
  D_DoomMain();

  doomSetStatus("stopped");
  doomDeleteCurrentTask();
}

}  // namespace

bool doomPrboomEnsureStarted() {
  if (g_doomTaskStarted) return true;
  BaseType_t taskResult = pdFAIL;
#if SCRYBAR_PRBOOM_TASK_CAPS
  taskResult = xTaskCreatePinnedToCoreWithCaps(doomTaskMain,
                                               "doom_prboom",
                                               kDoomTaskStack,
                                               nullptr,
                                               kDoomTaskPriority,
                                               &g_doomTaskHandle,
                                               1,
                                               MALLOC_CAP_SPIRAM);
#else
  taskResult = xTaskCreatePinnedToCore(doomTaskMain,
                                       "doom_prboom",
                                       kDoomTaskStack,
                                       nullptr,
                                       kDoomTaskPriority,
                                       &g_doomTaskHandle,
                                       1);
#endif
  g_doomTaskStarted = (taskResult == pdPASS);
  if (!g_doomTaskStarted) {
    char status[96];
    snprintf(status,
             sizeof(status),
             "task fail h=%u p=%u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    doomSetStatus(status);
  }
  return g_doomTaskStarted;
}

bool doomPrboomHasFrame() {
  return g_doomHasFrame;
}

bool doomPrboomIsRunning() {
  return g_doomTaskStarted;
}

const char *doomPrboomStatus() {
  return g_doomStatus;
}

extern "C" {

void doomPrboomReportFatal(const char *message) {
  doomSetStatus(message && *message ? message : "fatal");
}

int snd_card = 0;
int mus_card = 0;
int snd_samplerate = kDoomSampleRate;
int current_palette = 0;

void I_StartFrame(void) {}
void I_UpdateNoBlit(void) {}
bool I_StartDisplay(void) { return true; }
void I_EndDisplay(void) {}
void I_SetChannels(void) {}
void I_InitMusic(void) {}
void I_ShutdownMusic(void) {}
void I_UpdateMusic(void) {}
void I_ShutdownGraphics(void) {}
void I_UpdateVideoMode(void) {}

void I_FinishUpdate(void) {
  if (!g_doomFrame8) return;
  if (doomScrybarBlitIndexedFrame(g_doomFrame8, SCREENWIDTH, SCREENHEIGHT, g_doomPalette565, true)) {
    g_doomHasFrame = true;
  }
}

void I_SetPalette(int pal) {
  uint16_t *palette = static_cast<uint16_t *>(V_BuildPalette(pal, 16));
  if (!palette) return;
  for (int i = 0; i < 256; ++i) {
    const uint16_t raw565 = palette[i];
    g_doomPalette565[i] = (uint16_t)((raw565 << 8) | (raw565 >> 8));
  }
  Z_Free(palette);
  current_palette = pal;
}

void I_InitGraphics(void) {
  for (int i = 0; i < 3; ++i) {
    screens[i].width = SCREENWIDTH;
    screens[i].height = SCREENHEIGHT;
    screens[i].byte_pitch = SCREENWIDTH;
  }
  screens[0].data = g_doomFrame8;
  screens[0].not_on_heap = true;
  screens[4].width = SCREENWIDTH;
  screens[4].height = ST_SCALED_HEIGHT + 1;
  screens[4].byte_pitch = SCREENWIDTH;
}

int I_GetTimeMS(void) {
  return (int)(esp_timer_get_time() / 1000LL);
}

int I_GetTime(void) {
  return I_GetTimeMS() * TICRATE * realtic_clock_rate / 100000;
}

void I_uSleep(unsigned long usecs) {
  if (usecs >= 1000UL) {
    vTaskDelay(pdMS_TO_TICKS((usecs + 999UL) / 1000UL));
  } else {
    esp_rom_delay_us((uint32_t)usecs);
  }
}

void I_SafeExit(int) {
  doomSetStatus("exit");
  doomDeleteCurrentTask();
}

const char *I_DoomExeDir(void) {
  return "/";
}

const char *I_SigString(char *buf, size_t sz, int signum) {
  if (!buf || sz == 0) return "";
  snprintf(buf, sz, "SIG%d", signum);
  return buf;
}

int I_GetSfxLumpNum(sfxinfo_t *sfxinfo) {
  return sfxinfo ? W_GetNumForName(sfxinfo->name) : 0;
}

int I_StartSound(int, int, int, int, int, int) {
  return -1;
}

void I_StopSound(int) {}

bool I_SoundIsPlaying(int) {
  return false;
}

bool I_AnySoundStillPlaying(void) {
  return false;
}

void I_UpdateSoundParams(int, int, int, int) {}

void I_InitSound(void) {}

void I_ShutdownSound(void) {}

void I_PlaySong(int, int) {}

void I_PauseSong(int) {}

void I_ResumeSong(int) {}

void I_StopSong(int) {}

void I_UnRegisterSong(int) {}

int I_RegisterSong(const void *, size_t) {
  return 0;
}

void I_SetMusicVolume(int) {}

void I_StartTic(void) {
  int8_t moveBin = 0;
  int8_t turnBin = 0;
  uint8_t touchZone = 0;
  doomScrybarGetInputState(&moveBin, &turnBin, &touchZone);

  DoomInputButtons nextButtons = {};
  nextButtons.forward = (moveBin > 0);
  nextButtons.back = (moveBin < 0);
  nextButtons.left = (turnBin < 0);
  nextButtons.right = (turnBin > 0);
  nextButtons.use = (touchZone == 1);
  nextButtons.fire = (touchZone == 3);

  doomSyncButton(nextButtons.forward, &g_prevButtons.forward, key_up);
  doomSyncButton(nextButtons.back, &g_prevButtons.back, key_down);
  doomSyncButton(nextButtons.left, &g_prevButtons.left, key_left);
  doomSyncButton(nextButtons.right, &g_prevButtons.right, key_right);
  doomSyncButton(nextButtons.use, &g_prevButtons.use, key_use);
  doomSyncButton(nextButtons.fire, &g_prevButtons.fire, key_fire);
}

void I_Init(void) {
  snd_samplerate = kDoomSampleRate;
  snd_channels = 8;
  snd_MusicVolume = 0;
  snd_SfxVolume = 0;
  usegamma = 0;
}

const uint8_t *scrybar_prboom_iwad_start(void) {
  return scrybar_doom1_wad_start;
}

size_t scrybar_prboom_iwad_size(void) {
  return (size_t)(scrybar_doom1_wad_end - scrybar_doom1_wad_start);
}

}  // extern "C"
