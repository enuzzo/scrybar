#!/usr/bin/env python3
"""Exercise the firmware's complete configuration write using per-key faults."""
import pathlib
import subprocess
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
from render_mac_stats_native import ROOT, function


class ConfigPersistenceTest(unittest.TestCase):
    def test_every_write_and_commit_is_required_for_success(self):
        source = (ROOT / "scrybar.ino").read_text()
        with tempfile.TemporaryDirectory() as directory:
            tmp = pathlib.Path(directory)
            (tmp / "Preferences.h").write_text(r'''
#pragma once
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
static std::string failed, staged;
static std::set<std::string> keys;
static bool failOpen=false, failCommit=false;
static bool writeKey(const char *key){keys.insert(key);return failed!=key;}
class Preferences {
protected:
  uint32_t _handle=1;
  bool _started=false, _readOnly=false;
public:
  bool begin(const char*,bool){return _started=!failOpen;}
  void end(){_started=false;}
  size_t putUChar(const char*k,uint8_t){return writeKey(k)?1:0;}
  size_t putFloat(const char*k,float){return writeKey(k)?sizeof(float):0;}
  size_t putBytes(const char*k,const void*,size_t n){return writeKey(k)?n:0;}
};
''')
            (tmp / "nvs.h").write_text(r'''
#pragma once
constexpr int ESP_OK=0;
static int nvs_set_str(uint32_t,const char*k,const char*) {
  staged=k;keys.insert(k);return (failCommit || failed!=k)?ESP_OK:-1;
}
static int nvs_commit(uint32_t){return failCommit && failed==staged?-1:ESP_OK;}
''')
            body = r'''
#include "src/config_preferences.h"
struct {void println(const char*){} void printf(const char*,const char*){}} Serial;
constexpr int RSS_FEED_SLOT_COUNT=5,WIFI_RUNTIME_CREDENTIALS_MAX=3,UI_VIEW_ORDER_COUNT=5,UI_VIEW_MASK_DEFAULT=31;
struct Feed{const char*name="";const char*url="";uint8_t maxItems=1;};
struct {bool ready=true;const char*weatherCity="Example";float weatherLat=0,weatherLon=0;
  Feed rssFeeds[5];const char *logoUrl="",*uiTheme="letterpress";uint8_t enabledViewsMask=31;
  uint8_t viewOrder[5]={},backlightUsbPct=90,backlightBatPct=60;} g_runtimeNetConfig;
struct Cred{const char*ssid="";const char*password="";};
struct {uint8_t runtimeCredCount=1;Cred runtimeCreds[3];const char*preferredSsid="",*setupMode="off";}g_wifiSt;
struct {const char*station="",*arrStation="",*stopId="";}g_transitConfig;
const char *g_wordClockLang="en",*g_wikiLang="en";
static uint8_t g_savedBacklightUsbPct=90,g_savedBacklightBatPct=60;
static bool g_configSavePending=false;
static const char* runtimeRssFeedUrl(){return "";}
static uint8_t normalizeRuntimeViewMask(uint8_t v){return v;}
static void buildRssNvsKey(char*b,size_t n,uint8_t i,const char*s){snprintf(b,n,"rss_%u_%s",i,s);}
'''
            for name in ("nvsSaveBacklightConfig", "saveRuntimeWiFiCredentialsToPrefs",
                         "saveRuntimeNetConfigToNvs"):
                body += function(source, name) + "\n"
            body += r'''
int main(){
  assert(saveRuntimeNetConfigToNvs() && !g_configSavePending);
  const auto all=keys;
  assert(all.count("ui_theme") && all.count("rss_4_url") && all.count("transit_sid") &&
         all.count("wifi_dp3") && all.count("ui_order") && all.count("bl_batt"));
  for(const auto &key:all){
    failed=key;assert(!saveRuntimeNetConfigToNvs() && g_configSavePending);
    failed.clear();assert(saveRuntimeNetConfigToNvs() && !g_configSavePending);
  }
  failCommit=true;
  for(const char *key:{"ui_theme","transit_sid","wifi_dp3","rss_4_url"}){
    failed=key;assert(!saveRuntimeNetConfigToNvs() && g_configSavePending);
    failed.clear();assert(saveRuntimeNetConfigToNvs() && !g_configSavePending);
  }
  failOpen=true;assert(!saveRuntimeNetConfigToNvs() && g_configSavePending);
  failOpen=false;assert(saveRuntimeNetConfigToNvs() && !g_configSavePending);
}
'''
            cpp = tmp / "test.cpp"
            cpp.write_text(body)
            subprocess.run(["c++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                            f"-I{ROOT}", f"-I{tmp}", str(cpp), "-o", str(tmp / "test")], check=True)
            subprocess.run([str(tmp / "test")], check=True)


if __name__ == "__main__":
    unittest.main()
