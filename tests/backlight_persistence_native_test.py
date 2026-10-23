#!/usr/bin/env python3
"""Exercise the actual backlight NVS helpers with a native Preferences seam.

Builds and runs outside Dropbox; never reads the device or its stored values.
"""
import pathlib
import re
import subprocess
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
from render_mac_stats_native import ROOT, function


class BacklightPersistenceTest(unittest.TestCase):
    def test_actual_helpers_preserve_preview_and_per_key_commit_state(self):
        source = (ROOT / "scrybar.ino").read_text()
        config = (ROOT / "config.h").read_text()
        battery = re.search(r"^#define ENERGY_BACKLIGHT_ON_BATTERY\s+(\d+)", config, re.M)[1]
        state = "\n".join(re.findall(r"^static uint8_t g_savedBacklight\w+\s*=\s*[^;]+;", source, re.M))
        self.assertEqual(state.count("static uint8_t"), 2)
        names = ("nvsLoadBacklightConfig", "nvsSaveBacklightConfig",
                 "runtimeSavedBacklightUsbPct", "runtimeSavedBacklightBatPct")
        bodies = "\n".join(function(source, name) for name in names)
        self.assertIn("nvsLoadBacklightConfig(prefs, loadedAny)", function(source, "loadRuntimeNetConfigFromNvs"))
        save = function(source, "saveRuntimeNetConfigToNvs")
        self.assertIn("nvsSaveBacklightConfig(prefs)", save)
        self.assertRegex(save, r"const bool ok\s*=\s*[^;]+&& backlightSaved;")
        preview = function(source, "handleWebBrightnessApi")
        self.assertNotIn("g_savedBacklight", preview)
        self.assertNotIn("nvsSaveBacklightConfig", preview)
        cpp = r'''
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <iostream>
class Preferences {
public:
  std::map<std::string,uint8_t> values;
  std::map<std::string,size_t> writeResults;
  unsigned writes=0;
  bool isKey(const char*key)const{return values.count(key);}
  uint8_t getUChar(const char*key,uint8_t fallback)const {
    auto it=values.find(key);return it==values.end()?fallback:it->second;
  }
  size_t putUChar(const char*key,uint8_t value){
    ++writes;auto it=writeResults.find(key);
    const size_t result=it==writeResults.end()?sizeof(uint8_t):it->second;
    if(result==sizeof(uint8_t))values[key]=value;
    return result;
  }
};
struct RuntimeConfig {uint8_t backlightUsbPct=100,backlightBatPct=ENERGY_BACKLIGHT_ON_BATTERY;};
static RuntimeConfig g_runtimeNetConfig;
static unsigned ensures=0;
static void ensureRuntimeNetConfig(){++ensures;}
'''
        cpp = f"#define ENERGY_BACKLIGHT_ON_BATTERY {battery}\n" + cpp + state + "\n" + bodies
        cpp += r'''
static void reset(){
  g_runtimeNetConfig={};g_savedBacklightUsbPct=100;g_savedBacklightBatPct=ENERGY_BACKLIGHT_ON_BATTERY;
}
int main(){
  // No saved namespace/keys: read-only defaults are also the reboot values.
  reset();Preferences prefs;bool loaded=false;nvsLoadBacklightConfig(prefs,loaded);
  assert(!loaded&&prefs.writes==0);
  assert(runtimeSavedBacklightUsbPct()==100);
  assert(runtimeSavedBacklightBatPct()==ENERGY_BACKLIGHT_ON_BATTERY);

  // Load actual persisted values, preview new levels, then reopen the page.
  prefs.values={{"bl_usb",90},{"bl_batt",60}};
  nvsLoadBacklightConfig(prefs,loaded);assert(loaded);
  g_runtimeNetConfig.backlightUsbPct=80;g_runtimeNetConfig.backlightBatPct=50;
  for(unsigned page=0;page<3;++page){
    assert(runtimeSavedBacklightUsbPct()==90&&runtimeSavedBacklightBatPct()==60);
    assert(g_runtimeNetConfig.backlightUsbPct==80&&g_runtimeNetConfig.backlightBatPct==50);
    assert(prefs.writes==0); // getters/reloads cannot persist the preview
  }

  // Undo a preview returns to clean without a flash write.
  g_runtimeNetConfig.backlightUsbPct=90;g_runtimeNetConfig.backlightBatPct=60;
  assert(g_runtimeNetConfig.backlightUsbPct==runtimeSavedBacklightUsbPct());
  assert(g_runtimeNetConfig.backlightBatPct==runtimeSavedBacklightBatPct());
  assert(prefs.writes==0);

  // Failed writes retain the old saved snapshot and the user's live preview.
  g_runtimeNetConfig.backlightUsbPct=80;g_runtimeNetConfig.backlightBatPct=50;
  prefs.writeResults={{"bl_usb",0},{"bl_batt",0}};
  assert(!nvsSaveBacklightConfig(prefs));
  assert(runtimeSavedBacklightUsbPct()==90&&runtimeSavedBacklightBatPct()==60);
  assert(g_runtimeNetConfig.backlightUsbPct==80&&g_runtimeNetConfig.backlightBatPct==50);

  // A partial save advances only the key actually committed by Preferences.
  prefs.writeResults={{"bl_batt",0}};
  assert(!nvsSaveBacklightConfig(prefs));
  assert(runtimeSavedBacklightUsbPct()==80&&runtimeSavedBacklightBatPct()==60);
  assert(prefs.values["bl_usb"]==80&&prefs.values["bl_batt"]==60);
  prefs.writeResults.clear();assert(nvsSaveBacklightConfig(prefs));
  assert(runtimeSavedBacklightUsbPct()==80&&runtimeSavedBacklightBatPct()==50);
  assert(prefs.values["bl_usb"]==80&&prefs.values["bl_batt"]==50);

  // Defensive width check: an unexpected return cannot advertise persistence.
  g_runtimeNetConfig.backlightUsbPct=70;
  prefs.writeResults={{"bl_usb",2}};assert(!nvsSaveBacklightConfig(prefs));
  assert(runtimeSavedBacklightUsbPct()==80);

  // Existing clamp semantics are preserved for the effective reboot levels.
  reset();Preferences invalid;invalid.values={{"bl_usb",0},{"bl_batt",255}};
  nvsLoadBacklightConfig(invalid,loaded);
  assert(runtimeSavedBacklightUsbPct()==10&&runtimeSavedBacklightBatPct()==100);
  assert(g_runtimeNetConfig.backlightUsbPct==10&&g_runtimeNetConfig.backlightBatPct==100);
  assert(invalid.writes==0&&ensures>0);
  std::cout<<"Backlight persistence: defaults, load, preview/reopen, undo, failure, partial save, commit, clamp PASS\n";
}
'''
        with tempfile.TemporaryDirectory(prefix="scrybar-backlight-test-") as directory:
            path = pathlib.Path(directory)
            (path / "test.cpp").write_text(cpp)
            subprocess.run(["c++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
                            str(path / "test.cpp"), "-o", str(path / "test")], check=True)
            subprocess.run([str(path / "test")], check=True)


if __name__ == "__main__":
    unittest.main()
