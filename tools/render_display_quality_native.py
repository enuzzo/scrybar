#!/usr/bin/env python3
"""Render extracted firmware pages with LVGL8 and anonymous deterministic data.

This is source-render evidence, not an emulator or a physical-panel test. Widget
creation, styles, fonts and update functions are read from the .ino each run.
Only hardware/network/time seams are adapted. Build output stays outside Dropbox.
"""
from __future__ import annotations
import argparse
import concurrent.futures
import hashlib
import json
import pathlib
import re
import sys
import tempfile

sys.dont_write_bytecode = True
from render_mac_stats_native import ROOT, block, function, generated_source as stats_source, run

PAGES = ["HOME", "INFO", "RSS", "WIKI", "NOWPLAYING", "DOOM", "TRANSIT", "LAUNCH", "MACSTATS", "BAMBU"]


def generated_source(source: str, config: str, strings: str) -> tuple[str, list[str], dict]:
    base, themes = stats_source(source)
    base = base[:base.rindex("int main(")]
    existing = set(re.findall(r"^static\s+[^;{}]*?\b(\w+)\s*\([^;{}]*?\)\s*\{", base, re.M))
    adapters = {"activeUiStrings", "bambuCameraUpdate", "wifiBuildSetupPortalUrl",
                "lvglSetLocalNumberIfChanged", "wifiIsReconnectingUiState", "doomPrboomHasFrame", "doomPrboomStatus"}
    selected: dict[str, str] = {}
    roots = ["lvglCreatePageRoot", "initLvglInfoPanel", "initLvglClockPanel", "initLvglWeatherPanel",
             "lvglInitTransitUi", "lvglUpdateTransitUi", "lvglInitLaunchUi", "lvglUpdateLaunchUi",
             "lvglTickLaunchCountdown", "lvglOpenLaunchQr", "lvglInitFeedDeck", "lvglUpdateFeedDeck",
             "lvglInitNowPlayingUi", "lvglUpdateNowPlayingUi", "lvglInitBambuUi", "lvglUpdateBambuUi",
             "lvglApplyThemeStyles", "lvglShowWeatherMainIcon", "lvglShowWeatherForecastIcon",
             "lvglApplyClockSentenceAutoFit", "lvglSetDeckQrModalOpen", "lvglMeteoconFrameTimerCb", "doomBlitTitlePicToCanvas"]
    extra_pure = {"copyStringSafe", "transitIsBus", "transitCategoryColor", "transitMinutesUntil",
                  "launchProviderColor", "utf8Degree", "weatherImageFromCode", "weatherForecastImageFromCode",
                  "weatherMeteoconIdFromCode", "weatherGlyphText", "formatRssTextToLabelBounds",
                  "buildRssStoryBlock", "buildRssWhenLabel", "extractRssHost", "rssResolveSourceHost",
                  "buildRssSiteShortName", "buildRssSiteBadge", "rssSiteColorHexFromHost", "sanitizeAsciiBuffer",
                  "wikiFeedUiName", "wikiFeedUiBadge", "wikiFeedUiColorHex", "liveNowPlayingAvailable",
                  "liveNowPlayingDisplayInSync", "liveNowPlayingSourceLabel", "resolveFakeNowPlayingTrack",
                  "wifiSignalBarsFromRssi", "decodeHtmlEntitiesToAscii", "foldUtf8ToAscii",
                  "htmlNamedEntityToCodepoint", "appendAsciiFoldedCodepoint", "decodeNextUtf8Codepoint", "startsWithHttp"}
    def collect(name: str) -> None:
        if name in existing or name in selected or name in adapters: return
        code = function(source, name)
        selected[name] = code
        for called in re.findall(r"\b([A-Za-z_]\w*)\s*\(", code):
            if called != name and (called.startswith("lvgl") or called.startswith("initLvgl") or called.startswith("doom") or called in extra_pure):
                collect(called)
    for name in roots: collect(name)
    declarations = "\n".join(re.findall(r"^LV_FONT_DECLARE\([^;]+;", source, re.M))
    macros = "\n".join(re.findall(r"^#define (?:TRANSIT_|LAUNCH_|RSS_MAX_ITEMS|RSS_ROTATE_MS|DISPLAY_WEATHER_PANEL_W|FW_BUILD_TAG)[^\n]*", config, re.M))
    types = "\n".join(block(config, config.index("struct " + name)) + ";" for name in ("TransitDeparture", "TransitConfig", "LaunchItem"))
    type_names = ["WeatherState", "RssItem", "RssState", "TransitState", "LvglClockUi", "LvglInfoUi", "LvglWeatherUi",
                  "FeedDeckUi", "NowPlayingUi", "LiveNowPlayingState", "LiveNowPlayingArtwork", "FakeNowPlayingTrack",
                  "BambuPrintState", "BambuPrintUi", "LvglTransitUi", "LaunchState", "LvglLaunchUi", "DoomState"]
    types += "\nstatic constexpr uint8_t BAMBU_PRINT_FILAMENT_MAX=8;\n"
    types += "\n".join(block(source, source.index("struct " + name + " {")) + ";" for name in type_names)
    enum = block(source, source.index("enum UiPageMode : uint8_t {")) + ";"
    globals_names = ["g_weather", "g_rss", "g_wiki", "g_transitState", "g_transitConfig", "g_transitOrgMode", "g_transitOrgModeMs",
                     "g_clockUi", "g_infoUi", "g_weatherUi", "g_auxDeck", "g_wikiDeck", "g_nowPlayingUi", "g_liveNowPlaying",
                     "g_liveNowPlayingArtwork", "g_bambuPrint", "g_bambuPrintUi", "g_transitUi", "g_launchUi", "g_launchState",
                     "g_lvglHomeRoot", "g_lvglAuxRoot", "g_lvglWikiRoot", "g_lvglNowPlayingRoot", "g_lvglDoomRoot",
                     "g_lvglTransitRoot", "g_lvglLaunchRoot", "g_lvglBambuRoot", "g_bambuCameraButton"]
    globals_ = "\n".join(re.search(r"^static[^\n;]*\b"+name+r"\b[^;]*;", source, re.M)[0] for name in globals_names)
    fake = block(source, source.index("static constexpr FakeNowPlayingTrack kFakeNowPlayingTracks")) + ";"
    cover = block(source, source.index("static const lv_img_dsc_t kNowPlayingRealCover150")) + ";"
    constants = "\n".join(re.search(r"^static constexpr uint32_t "+name+r"[^;]+;", source, re.M)[0]
                         for name in ("NOW_PLAYING_FAKE_ROTATE_MS", "NOW_PLAYING_SYNC_TTL_MS"))
    constants += "\n" + "\n".join(re.findall(r"^static constexpr [^;\n]+\bkDoom\w+[^;]*;", source, re.M))
    constants += "\n" + block(source, source.index("static const uint16_t kCgaPalette16")) + ";"
    prelude = r'''
#include <ctime>
#include <cstdarg>
#include <cctype>
#include <strings.h>
#include "src/lang_types.h"
#include "src/bambu_camera_controls.h"
#include "quality_ui_strings.h"
#include "assets/meteocons/generated/meteocons_lvgl.h"
#include "assets/img_test/cover-test-150-rgb565.h"
#include "src/doom/doom_titlepic.h"
#define TEST_WIFI 1
#define TEST_NTP 1
#define TEST_DISPLAY 1
#define TEST_LVGL_UI 1
#define RSS_ENABLED 1
#define SCREENSAVER_ENABLED 0
#define DISPLAY_BACKEND_ESP_LCD 0
#define DB_HAS_LVGL_METEOCONS 1
#define WEB_CONFIG_ENABLED 0
#define TEST_BATTERY 0
#define TEST_IMU 1
#define DB_HAS_PRBOOM_DONOR 1
#define DB_CANVAS_W 640
#define DB_CANVAS_H 172
#define LCD_WIDTH 172
#define LCD_HEIGHT 640
#define DOOM_TOUCH_NONE 0
#define DOOM_TOUCH_LEFT 1
#define DOOM_TOUCH_RIGHT 3
using std::max;
using std::min;
struct String : std::string {
 using std::string::string;
 String(const std::string &v):std::string(v){}
 void trim(){auto b=find_first_not_of(" \t\r\n");if(b==npos){clear();return;}auto e=find_last_not_of(" \t\r\n");*this=substr(b,e-b+1);}
 String substring(size_t a,size_t b=npos)const{return substr(a,b==npos?b:b-a);}
 int lastIndexOf(char c)const{auto p=rfind(c);return p==npos?-1:int(p);}
 int indexOf(char c,size_t p=0)const{auto i=find(c,p);return i==npos?-1:int(i);}
 int indexOf(const char*c,size_t p=0)const{auto i=find(c,p);return i==npos?-1:int(i);}
 void remove(size_t i){erase(i);}
 void toLowerCase(){for(char &c:*this)c=std::tolower((unsigned char)c);}
 bool startsWith(const char*s)const{return rfind(s,0)==0;}
 bool endsWith(const char*s)const{return size()>=strlen(s)&&compare(size()-strlen(s),strlen(s),s)==0;}
};
struct NativeSerial {template<class...A>void printf(const char*,A...){ }void println(const char*){ }} Serial;
static void*ps_calloc(size_t n,size_t s){return calloc(n,s);}
static time_t nativeEpoch=1792756800;
static time_t nativeTime(time_t *p){if(p)*p=nativeEpoch;return nativeEpoch;}
#define time nativeTime
static bool getLocalTime(tm *p,uint32_t=0){gmtime_r(&nativeEpoch,p);return true;}
enum{WL_CONNECTED=3};using wl_status_t=int;
struct NativeIp { String toString()const{return "192.0.2.10";} };
struct NativeWifi {int status()const{return WL_CONNECTED;}int RSSI()const{return -52;}NativeIp localIP()const{return {};}
 String SSID()const{return "Example Network";}String macAddress()const{return "02:00:00:00:00:01";} } WiFi;
struct{bool connected=true,setupApActive=false;}g_wifiSt;
struct{char mdnsHost[32]="scrybar-example";}g_webCfg;
static const char*wlStatusToStr(int){return "OK";}
static const UiStrings*nativeStrings=&kUiLang_it;
static const UiStrings*activeUiStrings(){return nativeStrings;}
static void wifiBuildSetupPortalUrl(char*b,size_t n){snprintf(b,n,"http://192.0.2.10:8080");}
static bool wifiIsReconnectingUiState(){return false;}
static void bambuCameraUpdate(){}
static bool doomPrboomHasFrame(){return false;}
static const char*doomPrboomStatus(){return "fixture";}
static uint16_t qualityCanvas[640*172]={};
static struct{uint16_t*canvasBuf=qualityCanvas;}g_dispHw;
static struct{bool ready=true;}g_imu;
static void markUserInteraction(uint32_t){}
static constexpr unsigned kFaviconSize=32,RSS_FEED_SLOT_COUNT=5;
struct RuntimeRssFeedConfig{const char*url;};
static const RuntimeRssFeedConfig*runtimeRssFeedBySlot(uint8_t){return nullptr;}
static lv_obj_t*g_bambuCameraOverlay=nullptr,*g_bambuCameraImage=nullptr,*g_bambuCameraCaption=nullptr;
static lv_obj_t*g_bambuCameraLightDot=nullptr,*g_bambuCameraLightState=nullptr,*g_bambuCameraLightButton=nullptr,*g_bambuCameraLightLabel=nullptr;
static void lvglSetLocalNumberIfChanged(lv_obj_t*o,lv_style_prop_t p,int32_t n,lv_style_selector_t s){LvglUpdates::localNumber(o,p,n,s);}
static bool g_uiNeedsRedraw=true;
static void*g_netMutex=nullptr;
static void xSemaphoreTake(void*,int){}
static void xSemaphoreGive(void*){}
#define portMAX_DELAY 0
#define WEB_CONFIG_PORT 8080
static constexpr unsigned RSS_MAX_ITEMS=10;
static constexpr unsigned kFaviconCacheSlots=1;
struct{bool valid=false;char host[96]={};uint32_t lastAccessMs=0;lv_img_dsc_t imgDsc={};}g_faviconCache[kFaviconCacheSlots];
'''
    # Keep real embedded artwork and descriptor; camera/live image I/O is outside this fixture.
    adapters_code = r'''
static lv_img_dsc_t g_nowPlayingLiveCoverImage={};
static UiPageMode g_uiPageMode=UI_PAGE_HOME;
static uint8_t g_rssActiveFeed=0;
static DoomState g_doom;
static const lv_img_dsc_t*lvglNowPlayingCoverImageDsc(bool){return &kNowPlayingRealCover150;}
static uint32_t lvglNowPlayingCoverBackgroundColor(bool){return 0x182B3A;}
'''
    # These two artwork accessors need a descriptor prepared by hardware I/O;
    # the fixture supplies the embedded real artwork and an explicit surface.
    for name in ("lvglNowPlayingCoverImageDsc", "lvglNowPlayingCoverBackgroundColor"):
        selected.pop(name, None)
    prototypes = "\n".join(code[:code.index("{")].strip() + ";" for code in selected.values())
    definitions=[]
    for code in selected.values():
        opening=code.index("{")
        definitions.append(re.sub(r"\s*=\s*[^,)]+(?=[,)])", "", code[:opening])+code[opening:])
    metadata = dict(extracted_functions=sorted(selected), artwork_seam="embedded firmware cover; deterministic cover background",
                    doom_scope="Actual source TITLEPIC+HUD fallback, neutral IMU fixture; not gameplay or PrBoom execution",
                    source_sha256=hashlib.sha256(source.encode()).hexdigest(),
                    strings_sha256=hashlib.sha256(strings.encode()).hexdigest(), themes=themes)
    cpp="\n".join((base, prelude, declarations, macros, enum, types, globals_, fake, cover, constants,
                    adapters_code, prototypes, "\n\n".join(definitions)))
    metadata["native_unit_sha256"]=hashlib.sha256(cpp.encode()).hexdigest()
    return cpp, themes, metadata


def main() -> None:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source",type=pathlib.Path,default=ROOT/"scrybar.ino")
    parser.add_argument("--strings",type=pathlib.Path,default=ROOT/"src/ui_strings.h")
    parser.add_argument("--config",type=pathlib.Path,default=ROOT/"config.h")
    parser.add_argument("--lvgl-root",type=pathlib.Path,default=pathlib.Path.home()/"Documents/Arduino/libraries/lvgl")
    parser.add_argument("--build-dir",type=pathlib.Path,default=pathlib.Path(tempfile.gettempdir())/"scrybar-display-quality-native")
    parser.add_argument("--out-dir",type=pathlib.Path,default=ROOT/"screenshots/display-quality-native")
    parser.add_argument("--pages",nargs="+",choices=PAGES,default=PAGES)
    parser.add_argument("--themes",nargs="+")
    parser.add_argument("--states",nargs="+",default=["normal"])
    args=parser.parse_args()
    from PIL import Image,ImageDraw
    build,out=args.build_dir.resolve(),args.out_dir.resolve()
    if "Dropbox" in build.parts:raise ValueError("Build directory must be outside Dropbox")
    build.mkdir(parents=True,exist_ok=True);out.mkdir(parents=True,exist_ok=True)
    cpp,themes,metadata=generated_source(args.source.read_text(),args.config.read_text(),args.strings.read_text())
    (build/"quality_ui_strings.h").write_text(args.strings.read_text())
    (build/"pgmspace.h").write_text("#pragma once\n#include <stdint.h>\n#define PROGMEM\n#define pgm_read_byte(p) (*(const uint8_t*)(p))\n")
    fixture=(ROOT/"tests/fixtures/display_quality_native.cpp").read_text()
    (build/"display-quality.cpp").write_text(cpp+"\n"+fixture)
    # Reuse the already built LVGL8 library when available; CMake checks config.
    library=build/"lvgl"
    run(["cmake","-S",str(args.lvgl_root),"-B",str(library),"-DLV_CONF_PATH="+str(ROOT/"lv_conf.h"),"-DCMAKE_BUILD_TYPE=Release"],build/"configure.log")
    run(["cmake","--build",str(library),"--target","lvgl","-j","8"],build/"lvgl-build.log")
    include=["-I",str(ROOT),"-I",str(build),"-I",str(args.lvgl_root)]
    fonts=sorted(set(re.findall(r"LV_FONT_DECLARE\((scry_font_dosis_\w+)\)",cpp)))
    def font(name):
        src,obj=ROOT/"src/fonts"/(name+".c"),build/(name+".o")
        if not obj.exists() or obj.stat().st_mtime<src.stat().st_mtime:run(["cc","-O2",*include,"-c",str(src),"-o",str(obj)])
        return str(obj)
    with concurrent.futures.ThreadPoolExecutor(max_workers=8)as pool:objects=list(pool.map(font,fonts))
    binary=build/"render-display-quality"
    run(["c++","-std=c++17","-O2","-Wno-c99-designator",*include,str(build/"display-quality.cpp"),*objects,str(library/"lib/liblvgl.a"),"-o",str(binary)],build/"compile.log")
    records=[]
    for index,theme in enumerate(themes):
        if args.themes and theme not in args.themes:continue
        for page in args.pages:
            for state in args.states:
                stem=f"{theme}--{page.lower()}--{state}"
                ppm=build/(stem+".ppm")
                stdout=run([str(binary),str(index),page,state,str(ppm)])
                png=out/(stem+".png");Image.open(ppm).save(png)
                labels=[];roles=[]
                for line in stdout.splitlines():
                    if line.startswith("LABEL\t"):
                        parts=line.split("\t",9);bounds=list(map(int,parts[1:9]))
                        labels.append(dict(text=parts[9],bounds=bounds[:4],parent=bounds[4:]))
                    elif line.startswith("ROLE\t"):
                        parts=line.split("\t")
                        roles.append(dict(role=parts[1],foreground=parts[2],text_opacity=int(parts[3]),
                                          object_opacity=int(parts[4]),font_height=int(parts[5]),
                                          bounds=list(map(int,parts[6:10]))))
                records.append(dict(theme=theme,page=page,state=state,image=png.name,labels=labels,roles=roles))
    metadata.update(records=records,scope="Native LVGL software rendering; anonymous fixtures; no physical acceptance")
    (out/"manifest.json").write_text(json.dumps(metadata,indent=2,ensure_ascii=False)+"\n")
    # One contact sheet per page retains readable native-size images.
    for page in args.pages:
        entries=[r for r in records if r["page"]==page]
        groups=[("",entries)] if len(entries)<=21 else [(theme,[r for r in entries if r["theme"]==theme])for theme in themes]
        for theme,group in groups:
            if not group:continue
            columns=2 if len(group)>7 else 1;rows=(len(group)+columns-1)//columns
            sheet=Image.new("RGB",(660*columns,rows*202),"#191919");draw=ImageDraw.Draw(sheet)
            for index,record in enumerate(group):
                x=(index%columns)*660;y=(index//columns)*202
                draw.text((x+10,y+5),record["theme"]+" / "+record["state"],fill="white")
                sheet.paste(Image.open(out/record["image"]),(x+10,y+25))
            sheet.save(out/(page.lower()+"--"+(theme+"--"if theme else"")+"contact.png"))
    print(f"Rendered {len(records)} native source page frames to {out}")

if __name__=="__main__":main()
