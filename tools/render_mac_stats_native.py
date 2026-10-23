#!/usr/bin/env python3
"""Render the current firmware's Mac Stats widgets with native LVGL 8.

Reads the .ino on each run; does not duplicate its layout, theme or font code.
Build products stay outside Dropbox. PNGs are software-render evidence only:
they do not establish panel behavior, physical performance or live telemetry.
Requires installed LVGL 8 sources, CMake, a C/C++ compiler and Pillow.
"""
from __future__ import annotations

import argparse
import concurrent.futures
import json
import pathlib
import re
import subprocess
import tempfile


ROOT = pathlib.Path(__file__).resolve().parents[1]


def block(source: str, start: int) -> str:
    """Read a balanced C/C++ brace block, respecting comments and literals."""
    opening = source.index("{", start)
    depth, i, state = 0, opening, "code"
    while i < len(source):
        char, pair = source[i], source[i:i + 2]
        if state == "line":
            if char == "\n": state = "code"
        elif state == "comment":
            if pair == "*/": state, i = "code", i + 1
        elif state in ('"', "'"):
            if char == "\\": i += 1
            elif char == state: state = "code"
        elif pair == "//": state, i = "line", i + 1
        elif pair == "/*": state, i = "comment", i + 1
        elif char in ('"', "'"): state = char
        elif char == "{": depth += 1
        elif char == "}":
            depth -= 1
            if not depth: return source[start:i + 1]
        i += 1
    raise ValueError("Unbalanced C++ block")


def function(source: str, name: str) -> str:
    pattern = re.compile(r"^static\s+[^;{}]*?\b" + re.escape(name) + r"\s*\([^;{}]*?\)\s*\{", re.M)
    match = pattern.search(source)
    if not match: raise ValueError(f"Missing function definition: {name}")
    return block(source, match.start())


def generated_source(source: str) -> tuple[str, list[str]]:
    theme_start = source.index("struct UiThemeWebTokens")
    theme_end = source.index(function(source, "activeUiThemeLabel"))
    themes = source[theme_start:theme_end] + function(source, "activeUiThemeLabel")
    theme_ids = re.findall(r'^    "([^"\n]+)",\n    "[^"\n]+",\n    \{', themes, re.M)
    macros = "\n".join(re.findall(r"^#define MAC_STATS_.*$", source, re.M))
    types = "\n".join(block(source, source.index("struct " + name)) + ";"
                      for name in ("MacStatsState", "MacStatsUi"))
    names = ("g_macStats", "g_macStatsUi", "g_cpuUsageHistory", "g_gpuUsageHistory",
             "g_statsHistHead", "g_statsHistCount", "g_networkHistory", "g_lvglMacStatsRoot")
    globals_ = "\n".join(re.search(r"^static[^\n;]*\b" + name + r"\b[^;]*;", source, re.M).group()
                         for name in names)
    ttl = re.search(r"^static constexpr uint32_t MAC_STATS_SYNC_TTL_MS[^;]*;", source, re.M).group()
    adapters = {
        "lvglSetLabelTextIfChanged": "static void lvglSetLabelTextIfChanged(lv_obj_t *o,const char *s){LvglUpdates::labelText(o,s);}",
        "lvglSetTextColorIfChanged": "static void lvglSetTextColorIfChanged(lv_obj_t *o,lv_color_t c,lv_style_selector_t s){LvglUpdates::localColor(o,LV_STYLE_TEXT_COLOR,c,s);}",
        "lvglSetBgColorIfChanged": "static void lvglSetBgColorIfChanged(lv_obj_t *o,lv_color_t c,lv_style_selector_t s){LvglUpdates::localColor(o,LV_STYLE_BG_COLOR,c,s);}",
    }
    selected: dict[str, str] = {}

    def collect(name: str) -> None:
        if name in selected or name in adapters: return
        code = function(source, name)
        selected[name] = code
        for called in re.findall(r"\b(lvgl[A-Z]\w*)\s*\(", code):
            if called != name: collect(called)

    collect("lvglInitMacStatsUi")
    collect("lvglUpdateMacStatsUi")
    collect("lvglMacStatsDrawIsolatedSamples")
    fonts = sorted(set(re.findall(r"\bscry_font_dosis_\w+\b", "\n".join(selected.values()))))
    declarations = "\n".join(f"LV_FONT_DECLARE({name});" for name in fonts)
    prototypes = "\n".join(code[:code.index("{")].strip() + ";" for code in selected.values())
    prelude = '''#include <lvgl.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <string>
#include "src/lvgl_updates.h"
#include "src/mac_stats_format.h"
#include "src/mac_stats_history.h"
using std::isfinite;
static uint32_t nativeNow = 100000;
static uint32_t millis() { return nativeNow; }
static int16_t canvasWidth() { return 640; }
static int16_t canvasHeight() { return 172; }
'''
    fixture = r'''
static uint8_t image[640*172*3] = {};
static uint32_t frames = 0;
static void nativeFlush(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *pixels) {
  for (int y=area->y1; y<=area->y2; ++y) for (int x=area->x1; x<=area->x2; ++x) {
    uint32_t rgb=lv_color_to32(*pixels++);
    if(x<0||x>=640||y<0||y>=172) continue;
    const size_t offset=(y*640+x)*3;
    image[offset]=rgb>>16; image[offset+1]=rgb>>8; image[offset+2]=rgb;
  }
  if(lv_disp_flush_is_last(driver)) ++frames;
  lv_disp_flush_ready(driver);
}
static void describe(lv_obj_t *obj) {
  if(lv_obj_has_flag(obj,LV_OBJ_FLAG_HIDDEN)) return;
  if(lv_obj_check_type(obj,&lv_label_class)) {
    lv_area_t a,p; lv_obj_get_coords(obj,&a); lv_obj_get_coords(lv_obj_get_parent(obj),&p);
    std::string text=lv_label_get_text(obj);
    for(char &c:text) if(c=='\t'||c=='\n'||c=='\r') c=' ';
    std::printf("LABEL\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%s\n",
      a.x1,a.y1,a.x2,a.y2,p.x1,p.y1,p.x2,p.y2,text.c_str());
  }
  for(uint32_t i=0;i<lv_obj_get_child_cnt(obj);++i) describe(lv_obj_get_child(obj,i));
}
static void fixture(const char *state) {
  auto &s=g_macStats;
  s.valid=std::strcmp(state,"waiting")!=0; s.receivedAtMs=nativeNow-500;
  s.cpuUsagePct=27; s.gpuUsagePct=14; s.cpuTempC=58; s.gpuTempC=52;
  s.hasCpuUsage=s.hasGpuUsage=s.hasCpuTemp=s.hasGpuTemp=true;
  s.ramUsedGB=19.8; s.ramTotalGB=32; s.diskUsedGB=583; s.diskTotalGB=1000;
  s.netDownBytesPerSec=12500000; s.netUpBytesPerSec=820000;
  s.networkAppDownBytesPerSec=8400000; s.networkAppUpBytesPerSec=420000;
  s.hasNetDown=s.hasNetUp=s.hasAppDown=s.hasAppUp=true;
  s.hasNetworkAttribution=s.networkAttributionAvailable=true;
  std::snprintf(s.hostName,sizeof(s.hostName),"Studio MacBook Pro");
  std::snprintf(s.networkApp,sizeof(s.networkApp),"Safari");
  std::snprintf(s.networkScope,sizeof(s.networkScope),"internet");
  if(!std::strcmp(state,"stale")) s.receivedAtMs=nativeNow-20000;
  if(!std::strcmp(state,"idle")) {
    s.netDownBytesPerSec=s.netUpBytesPerSec=0; s.networkApp[0]=0;
  }
  if(!std::strcmp(state,"unavailable")) {
    s.hasNetDown=s.hasNetUp=false; s.networkAttributionAvailable=false;
  }
  if(!std::strcmp(state,"legacy")) {
    s.hasNetDown=s.hasNetUp=s.hasNetworkAttribution=false; s.networkScope[0]=0;
  }
  if(!std::strcmp(state,"long")) {
    std::snprintf(s.hostName,sizeof(s.hostName),"Design Studio MacBook Pro - Editing and Motion Graphics");
    std::snprintf(s.networkApp,sizeof(s.networkApp),"Adobe Creative Cloud Content Synchronizer Helper");
    s.netDownBytesPerSec=987654321; s.netUpBytesPerSec=123456789;
    s.networkAppDownBytesPerSec=876543210; s.networkAppUpBytesPerSec=98765432;
    s.networkCoveragePartial=true;
  }
  if(!std::strcmp(state,"partial")) s.networkCoveragePartial=true;
  if(!std::strcmp(state,"local")) std::snprintf(s.networkScope,sizeof(s.networkScope),"local");
  if(!std::strcmp(state,"all")) std::snprintf(s.networkScope,sizeof(s.networkScope),"all");
  if(!std::strcmp(state,"down-only")) s.hasNetUp=s.hasAppUp=false;
  if(!std::strcmp(state,"up-only")) s.hasNetDown=s.hasAppDown=false;
  if(!std::strcmp(state,"waiting")) s.hostName[0]=0;
  for(unsigned i=0;i<MAC_STATS_HISTORY_SIZE;++i) {
    g_cpuUsageHistory[i]=12+(i*7)%38; g_gpuUsageHistory[i]=8+(i*11)%24;
  }
  g_statsHistCount=MAC_STATS_HISTORY_SIZE;
  if(!std::strcmp(state,"irregular")) {
    const uint32_t intervals[]={1042,1265,1016,1036,1042,1004,1008,1071,946,986,1032,1141,1067,1057,934,951,1277,870,1018,1062};
    uint32_t total=0; for(unsigned i=1;i<55;++i) total+=intervals[(i-1)%20];
    uint32_t at=nativeNow-total;
    g_networkHistory.clear();
    for(unsigned i=0;i<55;++i) {
      if(i) at+=intervals[(i-1)%20];
      const float up=i==37?10000000.f:(10.f+float(i%7))*10000.f;
      g_networkHistory.accept(at,s.hostName,s.networkScope,1000000,true,up,true);
      s.netDownBytesPerSec=1000000; s.netUpBytesPerSec=up;
    }
    s.networkAppDownBytesPerSec=800000; s.networkAppUpBytesPerSec=100000;
    return;
  }
  if(!std::strcmp(state,"isolated")||!std::strcmp(state,"isolated-empty")) {
    g_networkHistory.clear();
    for(unsigned i=0;i<5;++i) {
      const bool known=i==2&&!std::strcmp(state,"isolated");
      g_networkHistory.accept(nativeNow-4000+i*1000,s.hostName,s.networkScope,12500000,known,0,false);
    }
    s.hasNetDown=s.hasNetUp=false;
    return;
  }
  for(unsigned i=0;i<=60;++i) {
    if(!std::strcmp(state,"gap")&&i>24&&i<36) continue;
    const bool cadence=!std::strncmp(state,"cadence",7);
    const bool known=s.valid&&std::strcmp(state,"legacy")&&std::strcmp(state,"unavailable");
    const bool explicitUnknown=!std::strcmp(state,"cadence-unknown")&&i==30;
    const float peak=s.netDownBytesPerSec;
    const float peakUp=s.netUpBytesPerSec;
    const float wave=.15f+.4f*std::pow(std::sin(i*.23f),2.f)+.45f*std::pow(std::sin(i*.62f),8.f);
    g_networkHistory.accept(nativeNow-(60-i)*(cadence?1100:1000),s.hostName,s.networkScope,
      i==60?s.netDownBytesPerSec:peak*wave,known&&s.hasNetDown&&!explicitUnknown,
      i==60?s.netUpBytesPerSec:peakUp*(.2f+.6f*std::pow(std::cos(i*.32f),2.f)),known&&s.hasNetUp&&!explicitUnknown);
  }
}
int main(int argc,char **argv) {
  if(argc!=4&&argc!=5) return 2;
  g_uiThemeIndex=std::atoi(argv[1]);
  lv_init();
  static lv_color_t pixels[640*18];
  static lv_disp_draw_buf_t buffer; lv_disp_draw_buf_init(&buffer,pixels,nullptr,640*18);
  static lv_disp_drv_t driver; lv_disp_drv_init(&driver);
  driver.hor_res=640; driver.ver_res=172; driver.draw_buf=&buffer; driver.flush_cb=nativeFlush;
  lv_disp_t *display=lv_disp_drv_register(&driver); if(!display) return 3;
  lv_obj_set_style_pad_all(lv_scr_act(),0,0); lv_obj_set_style_border_width(lv_scr_act(),0,0);
  g_lvglMacStatsRoot=lv_obj_create(lv_scr_act());
  lv_obj_remove_style_all(g_lvglMacStatsRoot);
  lv_obj_set_size(g_lvglMacStatsRoot,640,172);
  lv_obj_clear_flag(g_lvglMacStatsRoot,LV_OBJ_FLAG_SCROLLABLE);
  lvglInitMacStatsUi(); fixture(argv[2]);
  if(argc==5) nativeNow+=std::strtoul(argv[4],nullptr,10);
  lvglUpdateMacStatsUi(true);
  lv_obj_update_layout(g_lvglMacStatsRoot); lv_refr_now(display);
  FILE *out=std::fopen(argv[3],"wb"); if(!out) return 4;
  std::fprintf(out,"P6\n640 172\n255\n"); std::fwrite(image,1,sizeof(image),out); std::fclose(out);
  describe(g_lvglMacStatsRoot);
  const auto history=g_networkHistory.window(nativeNow);
  std::printf("WINDOW\t%zu\t%.9g\n",history.count,history.peak);
  for(size_t i=0;i<history.count;++i) {
    const auto &p=history.points[i];
    std::printf("POINT\t%u\t%.9g\t%.9g\t%d\t%d\n",p.x,p.down,p.up,p.hasDown,p.hasUp);
  }
  lv_mem_monitor_t mem; lv_mem_monitor(&mem);
  std::printf("META\tframes=%u\tlvgl_free=%zu\tlvgl_used_pct=%u\n",frames,mem.free_size,mem.used_pct);
  lv_disp_remove(display);
}
'''
    cpp = "\n".join((prelude, declarations, themes, macros, types, globals_, ttl,
                      "\n".join(adapters.values()), prototypes,
                      "\n\n".join(selected.values()), fixture))
    return cpp, theme_ids


def run(command: list[str], log: pathlib.Path | None = None) -> str:
    result = subprocess.run(command, cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if log: log.write_text(result.stdout)
    if result.returncode: raise RuntimeError(f"Command failed: {command[0]}\n{result.stdout[-12000:]}")
    return result.stdout


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lvgl-root", type=pathlib.Path,
                        default=pathlib.Path.home()/"Documents/Arduino/libraries/lvgl")
    parser.add_argument("--build-dir", type=pathlib.Path,
                        default=pathlib.Path(tempfile.gettempdir())/"scrybar-stats-native")
    parser.add_argument("--out-dir", type=pathlib.Path, default=ROOT/"output/mac-stats-native")
    parser.add_argument("--states", nargs="+", default=["waiting","live","stale","idle","unavailable","legacy","long"])
    parser.add_argument("--themes", nargs="+", help="Theme IDs; default all firmware themes")
    parser.add_argument("--phase-ms", nargs="+", type=int, default=[0],
                        help="Advance only the clock after one fixed receipt history; capture temporal stability")
    parser.add_argument("--check-transitions", action="store_true",
                        help="Compare cached composition against actual LVGL page translations")
    parser.add_argument("--check-chart-only", action="store_true",
                        help="Verify chart-only scrolling matches the full Stats update with fewer dirty pixels")
    args = parser.parse_args()
    from PIL import Image
    build, out = args.build_dir.resolve(), args.out_dir.resolve()
    if "Dropbox" in build.parts: raise ValueError("Build directory must be outside Dropbox")
    build.mkdir(parents=True, exist_ok=True); out.mkdir(parents=True, exist_ok=True)
    source = (ROOT/"scrybar.ino").read_text()
    cpp, themes = generated_source(source)
    (build/"stats.cpp").write_text(cpp)
    library_build = build/"lvgl"
    run(["cmake","-S",str(args.lvgl_root),"-B",str(library_build),
         "-DLV_CONF_PATH="+str(ROOT/"lv_conf.h"),"-DCMAKE_BUILD_TYPE=Release"],build/"configure.log")
    run(["cmake","--build",str(library_build),"--target","lvgl","-j","8"],build/"lvgl-build.log")
    include = ["-I",str(ROOT),"-I",str(args.lvgl_root)]
    fonts = sorted(set(re.findall(r"LV_FONT_DECLARE\((scry_font_dosis_\w+)\)",cpp)))
    def compile_font(name: str) -> str:
        src, obj = ROOT/"src/fonts"/(name+".c"), build/(name+".o")
        if not obj.exists() or obj.stat().st_mtime < max(src.stat().st_mtime,(ROOT/"lv_conf.h").stat().st_mtime):
            run(["cc","-O2",*include,"-c",str(src),"-o",str(obj)])
        return str(obj)
    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
        objects = list(pool.map(compile_font,fonts))
    binary = build/"render-stats"
    run(["c++","-std=c++17","-O2",*include,str(build/"stats.cpp"),*objects,
         str(library_build/"lib/liblvgl.a"),"-o",str(binary)],build/"compile.log")
    if args.check_transitions:
        check = build/"check-transitions"
        run(["c++","-std=c++17","-O2",*include,
             '-DSCRYBAR_STATS_NATIVE_SOURCE="'+str(build/"stats.cpp")+'"',
             str(ROOT/"tests/page_transition_lvgl_test.cpp"),*objects,
             str(library_build/"lib/liblvgl.a"),"-o",str(check)],build/"transition-compile.log")
        for index, theme in enumerate(themes):
            if args.themes and theme not in args.themes: continue
            print(run([str(check),str(index)]).strip())
    if args.check_chart_only:
        check = build/"check-chart-only"
        run(["c++","-std=c++17","-O2",*include,
             '-DSCRYBAR_STATS_NATIVE_SOURCE="'+str(build/"stats.cpp")+'"',
             str(ROOT/"tests/mac_stats_chart_update_lvgl_test.cpp"),*objects,
             str(library_build/"lib/liblvgl.a"),"-o",str(check)],build/"chart-only-compile.log")
        for index, theme in enumerate(themes):
            if args.themes and theme not in args.themes: continue
            print(run([str(check),str(index)]).strip())
    report = []
    for index, theme in enumerate(themes):
        if args.themes and theme not in args.themes: continue
        for state in args.states:
            for phase in args.phase_ms:
                stem=theme+"--"+state+(f"--t{phase}" if len(args.phase_ms)>1 or phase else "")
                ppm=build/(stem+".ppm")
                text=run([str(binary),str(index),state,str(ppm),str(phase)])
                Image.open(ppm).save(out/(stem+".png"))
                labels=[]; points=[]; history={}
                for line in text.splitlines():
                    if line.startswith("LABEL\t"):
                        fields=line.split("\t",9); bounds=list(map(int,fields[1:9]))
                        labels.append({"text":fields[9],"bounds":bounds[:4],"parent":bounds[4:]})
                    elif line.startswith("WINDOW\t"):
                        _,count,peak=line.split("\t"); history={"count":int(count),"peak":float(peak)}
                    elif line.startswith("POINT\t"):
                        _,x,down,up,has_down,has_up=line.split("\t")
                        points.append({"x_10ms":int(x),"down":float(down),"up":float(up),
                                       "hasDown":bool(int(has_down)),"hasUp":bool(int(has_up))})
                history["points"]=points
                report.append({"theme":theme,"state":state,"phase_ms":phase,"png":stem+".png","labels":labels,
                               "history":history,"runtime":next((line for line in text.splitlines() if line.startswith("META")),"")})
    (out/"manifest.json").write_text(json.dumps({"evidence":"native LVGL framebuffer, synthetic data",
        "width":640,"height":172,"renders":report},indent=2))
    print(f"Rendered {len(report)} actual LVGL Stats frames to {out}")


if __name__ == "__main__":
    main()
