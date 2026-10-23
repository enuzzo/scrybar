#!/usr/bin/env python3
"""Audit selected display roles and all UiStrings with the installed LVGL 8.

Extracts firmware palette/resolver/font functions at execution time. It measures
native glyph coverage and known widget bounds; it does not render all pages or
certify physical readability. Builds stay outside Dropbox. No USB or network.
Requires CMake and C/C++ compilers; no Python packages are installed or needed.
"""
from __future__ import annotations

import argparse
import concurrent.futures
import hashlib
import json
import pathlib
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from render_mac_stats_native import ROOT, block, function


def line_at(source: str, text: str) -> int:
    return source.count("\n", 0, source.index(text)) + 1


def arguments(source: str, name: str, start: int = 0) -> list[str]:
    """First C++ call's arguments; nested calls and literals are preserved."""
    match = re.search(r"\b" + re.escape(name) + r"\s*\(", source[start:])
    if not match:
        raise ValueError(f"Missing call: {name}")
    opening = start + match.end() - 1
    depth, state, begin, result, i = 0, "code", opening + 1, [], opening + 1
    while i < len(source):
        c, pair = source[i], source[i:i + 2]
        if state == "line":
            if c == "\n": state = "code"
        elif state == "comment":
            if pair == "*/": state, i = "code", i + 1
        elif state in ('"', "'"):
            if c == "\\": i += 1
            elif c == state: state = "code"
        elif pair == "//": state, i = "line", i + 1
        elif pair == "/*": state, i = "comment", i + 1
        elif c in ('"', "'"): state = c
        elif c in "([{": depth += 1
        elif c in ")]}":
            if c == ")" and depth == 0:
                result.append(source[begin:i].strip())
                return result
            depth -= 1
        elif c == "," and depth == 0:
            result.append(source[begin:i].strip()); begin = i + 1
        i += 1
    raise ValueError(f"Unbalanced call: {name}")


def assignment(source: str, name: str) -> str:
    match = re.search(r"\b" + re.escape(name) + r"\s*=\s*([^;]+);", source)
    if not match: raise ValueError(f"Missing assignment: {name}")
    return match[1].strip()


def text_hex(source: str, widget: str) -> str:
    call = "lvglSetTextHex(" + widget + ","
    return arguments(source, "lvglSetTextHex", source.index(call))[1]


def effective_text_hex(scopes: list[str], widget: str) -> str:
    for scope in scopes:
        if "lvglSetTextHex("+widget+"," in scope: return text_hex(scope,widget)
        if "lvglSetTextColorIfChanged("+widget+"," in scope: return color_expression(scope,widget)
    raise ValueError("Missing effective text color for "+widget)


def color_expression(source: str, widget: str) -> str:
    call = "lvglSetTextColorIfChanged(" + widget + ","
    value = arguments(source, "lvglSetTextColorIfChanged", source.index(call))[1]
    return arguments(value, "lv_color_hex")[0]


def font_expression(source: str, widget: str) -> str:
    match = re.search(r"lv_obj_set_style_text_font\(\s*" + re.escape(widget) + r"\s*,", source)
    if not match: raise ValueError(f"Missing font binding: {widget}")
    return arguments(source, "lv_obj_set_style_text_font", match.start())[1]


def style_expression(source: str, setter: str, widget: str) -> str:
    """Extract one concrete widget style binding; do not infer defaults."""
    match = re.search(r"\b" + re.escape(setter) + r"\(\s*" + re.escape(widget) + r"\s*,", source)
    if not match: raise ValueError(f"Missing style binding: {setter}({widget})")
    return arguments(source, setter, match.start())[1]


def effective_text_opacity(source: str, init: str, widget: str) -> str:
    """Resolve the specific init path's explicit setter/visibility override."""
    direct = re.search(r"lv_obj_set_style_text_opa\(\s*" + re.escape(widget) + r"\s*,", init)
    visible = re.search(r"lvglForceLabelVisible\(\s*" + re.escape(widget) + r"\s*\)", init)
    if visible and (not direct or visible.start() > direct.start()):
        helper = function(source, "lvglForceLabelVisible")
        start = helper.index("LV_STYLE_TEXT_OPA")
        call = helper.rfind("lvglSetLocalNumberIfChanged", 0, start)
        return arguments(helper, "lvglSetLocalNumberIfChanged", call)[2]
    return style_expression(init, "lv_obj_set_style_text_opa", widget) if direct else "LV_OPA_COVER"


FORMAT = re.compile(r"%(?:[-+ #0]*)(?:\d+)?(?:\.\d+)?([diuoxXfFeEgGaAcspn%])")


def format_signature(value: str) -> tuple[list[str], list[str]]:
    """Validate before Python/native formatting; never execute malformed C fmt."""
    signature, errors, i = [], [], 0
    while i < len(value):
        if value[i] != "%": i += 1; continue
        match = FORMAT.match(value, i)
        if not match:
            errors.append(f"Unsupported format at byte/character {i}")
            i += 1; continue
        kind = match[1]
        if kind in "aA": errors.append("Hex-float formatting needs a native-specific fixture")
        if kind != "%": signature.append("float" if kind in "fFeEgG" else kind)
        i = match.end()
    return signature, errors


def geometry(source: str, names: list[str]) -> str:
    """Extract only pure integer declarations needed by a known bound."""
    declarations: dict[str, str] = {}
    known = {"cW", "cH", "DISPLAY_WEATHER_PANEL_W", "DB_HAS_LVGL_METEOCONS", "METEOCON_SIZE"}
    ordered: list[str] = []
    for match in re.finditer(r"(?:const |constexpr )?(?:int16_t|uint16_t|lv_coord_t)\s+(\w+)\s*=\s*([^;]+);", source):
        declarations.setdefault(match[1], match[0])

    def collect(name: str) -> None:
        if name in known: return
        if name not in declarations: raise ValueError(f"Unknown geometry dependency: {name}")
        known.add(name)
        declaration = declarations[name]
        for dependency in re.findall(r"\b[A-Za-z_]\w*\b", declaration.split("=", 1)[1]):
            if dependency in declarations and dependency != name: collect(dependency)
        ordered.append(declaration)
    for name in names: collect(name)
    return "\n".join(ordered)


def generated_source(source: str, strings_source: str, config: str) -> tuple[str, dict]:
    theme_start = source.index("struct UiThemeWebTokens")
    themes = source[theme_start:source.index(function(source, "activeUiThemeLabel"))] + function(source, "activeUiThemeLabel")
    theme_ids = re.findall(r'^    "([^"\n]+)",\n    "[^"\n]+",\n    \{', themes, re.M)
    styles = function(source, "lvglApplyThemeStyles")
    stats = function(source, "lvglUpdateMacStatsUi")
    fonts_body = function(source, "lvglApplyThemeFonts")
    feed = function(source, "lvglInitFeedDeck")
    weather = function(source, "initLvglWeatherBodyWidgets")
    launch = function(source, "lvglInitLaunchUi")
    feed_update = function(source, "lvglUpdateFeedDeck")
    ui_fields = re.findall(r"const char\*\s+(\w+)\s*;", block(strings_source, strings_source.index("struct UiStrings")))
    languages = re.findall(r"static const UiStrings kUiLang_(\w+)\s*=", strings_source)
    weather_short = re.findall(r"static const WeatherShortLabels\s+(\w+)\s*=\s*\{[^;]+;", source)
    weather_tables = "\n".join(re.search(r"static const WeatherShortLabels\s+" + name + r"\s*=\s*\{[^;]+;", source)[0] for name in weather_short)
    language_weather = dict(re.findall(r'\{"(\w+)"\s*,\s*\w+\s*,\s*&([A-Za-z_]\w*)', source))

    selected: dict[str, str] = {}
    def collect(name: str) -> None:
        if name in selected: return
        code = function(source, name)
        selected[name] = code
        for called in re.findall(r"\b(lvgl[A-Z]\w*)\s*\(", code):
            if called != name: collect(called)

    roles: list[dict] = []
    def role(name: str, fg: str, bg: str, minimum: float = 4.5, *, at: str,
             context: str = "", kind: str = "small text", font: str = "") -> None:
        roles.append(dict(role=name, foreground_expression=fg, background_expression=bg,
                          minimum=minimum, source_line=line_at(source, at), kind=kind,
                          font_accessor=font, context=context))
        for called in re.findall(r"\b(lvgl[A-Z]\w*)\s*\(", fg + " " + bg + " " + context): collect(called)

    local_names = ["panelBg", "headerBg", "headerText", "headerMeta", "infoHeaderBg", "infoHeaderText",
                   "weatherBg", "weatherTextPrimary", "weatherTextSecondary", "weatherForecastBg", "forecastText"]
    locals_ = "const bool cyberpunk=lvglThemeIsCyberpunk(), minimal=lvglThemeIsMinimalBrutalistMono();\n"
    for name in local_names:
        expression = assignment(styles, name)
        locals_ += f"const uint32_t {name} = {expression};\n"
        for called in re.findall(r"\b(lvgl[A-Z]\w*)\s*\(", expression): collect(called)
    # LAUNCH roots remain transparent. Theme application changes the hero's
    # paint color, but the initializer's 20/255 veil still reveals screenBg.
    hero_opa=style_expression(launch,"lv_obj_set_style_bg_opa","g_launchUi.heroBg")
    hero_color=arguments(styles,"lvglSetBgFlat",styles.index("lvglSetBgFlat(g_launchUi.heroBg,"))[1]
    locals_ += f"const uint32_t launchHeroBg=auditBlendLvgl({hero_color},t.screenBg,{hero_opa});\n"
    collect("lvglContrastRatio"); collect("lvglThemeIsCyberpunk"); collect("lvglThemeIsMinimalBrutalistMono")
    for name, fg, bg, at in [
        ("header.title", "headerText", "headerBg", "lvglSetTextHex(g_transitUi.title"),
        ("feed.header.meta", "headerMeta", "headerBg", "lvglSetTextHex(d->meta"),
        ("info.header.endpoint", "infoHeaderText", "infoHeaderBg", "lvglSetTextHex(g_infoUi.endpoint"),
        ("stats.axis.meta", "t.auxMeta", "panelBg", '"NETWORK / AUTO --"'),
        ("feed.body", "t.auxText", "panelBg", "lvglSetTextHex(d->news"),
        ("feed.source", "t.auxSourceText", "panelBg", "lvglSetTextHex(d->sourceSite"),
        ("qr.hint", "t.auxQrHint", "t.screenBg", "lvglSetTextHex(d->qrHint"),
        ("weather.primary", "weatherTextPrimary", "weatherBg", "lvglResolvedWeatherPrimary(t, weatherBg)"),
        ("weather.secondary", "weatherTextSecondary", "weatherBg", "lvglResolvedWeatherSecondary(t, weatherBg"),
        ("weather.forecast", "forecastText", "weatherForecastBg", "lvglResolvedForecastText(t, weatherBg"),
    ]: role(name, fg, bg, at=at)
    # This is a raw token inventory, not a currently visible widget (legacy role).
    role("token.auxWhenText", "t.auxWhenText", "panelBg", at="uint32_t auxWhenText;", kind="token inventory")
    role("launch.header.fetch", text_hex(styles, "g_launchUi.fetchTime"), "headerBg", at="lvglSetTextHex(g_launchUi.fetchTime")
    role("launch.weather", "lvglLaunchWeatherAccent(t)", "launchHeroBg", at="lvglLaunchWeatherAccent(t)")
    for button, setter in [("QR", "lvglSetDeckQrButtonPressed"), ("SKIP", "lvglSetDeckRefreshButtonPressed"), ("NXT", "lvglSetDeckNextFeedButtonPressed")]:
        body = function(source, setter)
        background = assignment(body, "bgHex")
        foreground = assignment(body, "fgHex")
        for pressed in (False, True):
            role("feed.button." + button + (".pressed" if pressed else ".normal"), foreground, "bgHex",
                 at=body, context=f"const bool pressed={'true' if pressed else 'false'}; const uint32_t bgHex={background};",
                 font=font_expression(fonts_body, {"QR":"d->qrBtnText", "SKIP":"d->refreshBtnText", "NXT":"d->nextFeedBtnText"}[button]))
            for called in re.findall(r"\b(lvgl[A-Z]\w*)\s*\(", background): collect(called)
    usage = color_expression(stats, "value")
    temp = color_expression(stats, "temp")
    stats_init=function(source,"lvglInitMacStatsUi")
    usage_font=arguments(stats_init,"label",stats_init.index("value = label(bg,"))[2]
    usage_font_code=function(source,usage_font.removesuffix("()"))
    usage_font_size=re.search(r"\bscry_font_dosis_(?:semibold_)?(\d+)\b",usage_font_code)
    usage_large=bool(usage_font_size and int(usage_font_size[1])>=24)
    for level in (25, 65, 85):
        role(f"stats.usage.{level}", usage, "panelBg", 3 if usage_large else 4.5, at="lvglSetTextColorIfChanged(value,",
             context=f"const bool live=true,hasUsage=true;const float usage={level};",kind="large text" if usage_large else "small text",font=usage_font)
    for level in (45, 75, 90):
        role(f"stats.temperature.{level}", temp, "panelBg", at="lvglSetTextColorIfChanged(temp,",
             context=f"const bool live=true,hasTemp=true;const float temperature={level};")
    for widget in ("cpu", "gpu"):
        start = stats.index("hardware(g_macStatsUi." + widget + "UsageVal")
        accent = arguments(stats, "hardware", start)[-1]
        spark_at = stats.index("lvglSetBgColorIfChanged(spark[i]")
        spark_expr = arguments(arguments(stats, "lvglSetBgColorIfChanged", spark_at)[1], "lv_color_hex")[0]
        for percent in (40, 65, 85):
            role(f"stats.spark.{widget}.{percent}", spark_expr, "panelBg", 3,
                 at="lvglSetBgColorIfChanged(spark[i]", kind="data graphic",
                 context=f"const bool known=true;const unsigned percent={percent};const uint32_t accent={accent},track=0;")
    random_start = feed_update.index("const uint8_t wikiSlot")
    wiki_fg = assignment(feed_update[random_start:], "siteTextHex")
    role("wiki.random.badge", wiki_fg, "siteColorHex", at="siteColorHex = wikiFeedUiColorHex(wikiSlot)",
         context="const uint32_t siteColorHex=wikiFeedUiColorHex(2);")
    selected["wikiFeedUiColorHex"] = function(source, "wikiFeedUiColorHex")

    # Additional observed runtime states. Keep literal widget colors attached
    # to their actual updater/initializer instead of copying a palette here.
    transit=function(source,"lvglUpdateTransitUi")
    for value,state in ((5,"late"),(-2,"early")):
        role("transit.delay."+state,color_expression(transit,"g_transitUi.delay[i]"),"panelBg",
             at="lvglSetTextColorIfChanged(g_transitUi.delay[i],",
             context=f"struct {{int delayMin;}} d{{{value}}};")
    for realtime in (False,True):
        role("transit.platform."+("live" if realtime else "scheduled"),color_expression(transit,"g_transitUi.platform[i]"),"panelBg",
             at="lvglSetTextColorIfChanged(g_transitUi.platform[i],",
             context="struct {bool realTime;} d{"+("true" if realtime else "false")+"};")
    role("transit.destination.cancelled",color_expression(transit,"g_transitUi.dest[i]"),"panelBg",
         at="lvglSetTextColorIfChanged(g_transitUi.dest[i],")
    imminent=block(transit,transit.index("if (mins <= 0"))
    role("transit.departure.imminent",assignment(imminent,"timeColor"),"panelBg",at=imminent)
    past=re.findall(r"\btimeColor\s*=\s*([^;]+);",transit)[-1].strip()
    role("transit.departure.past",past,"panelBg",at="timeColor = "+past)
    arrival_start=transit.index('copyStringSafe(abuf,')
    role("transit.arrival.unknown",color_expression(transit[arrival_start:],"g_transitUi.arr[i]"),"panelBg",
         at=transit[arrival_start:arrival_start+40])
    # Labels are siblings of alternating rows, so their backdrop is the
    # composited row rather than an inherited parent color. Resolve the tint
    # from theme application when present, otherwise retain the init behavior.
    transit_init=function(source,"lvglInitTransitUi")
    row_opa=style_expression(transit_init,"lv_obj_set_style_bg_opa","g_transitUi.rowBg[i]")
    row_tint=assignment(transit_init,"altTintColor")
    row_tint_refresh="lvglSetBgFlat(g_transitUi.rowBg[i]," in styles
    if row_tint_refresh:
        row_tint=arguments(styles,"lvglSetBgFlat",styles.index("lvglSetBgFlat(g_transitUi.rowBg[i],"))[1]
    for existing in list(roles):
        if not existing["role"].startswith("transit.") or existing["background_expression"]!="panelBg": continue
        tints=[("odd",row_tint)]
        if not row_tint_refresh: tints.append(("odd_retained_tint",f"({row_tint}) == 0xFFFFFF ? 0x000000 : 0xFFFFFF"))
        for suffix,tint in tints:
            role(existing["role"]+"."+suffix,existing["foreground_expression"],
                 f"auditBlendLvgl(({tint}),panelBg,{row_opa})",existing["minimum"],
                 at=transit_init,context="const bool darkPanel=(lvglColorLuma(panelBg)<128u);"+existing["context"],
                 kind=existing["kind"],font=existing["font_accessor"])
    badge_start=transit.index("uint32_t badgeColor;")
    badge_end=transit.index("if (g_transitUi.lineBg[i])",badge_start)
    badge_code=transit[badge_start:badge_end]
    for name in ("transitCategoryColor","transitIsBus"): selected[name]=function(source,name)
    preferred=assignment(transit,"preferredText")
    badge_fg=assignment(transit,"textColor")
    badge_fixtures=[("regional","REGIONAL_RAIL",0,0,False),
                    ("highspeed","HIGH_SPEED_RAIL",0,0,False),
                    ("night","NIGHT_RAIL",0,0,False),
                    ("bus","BUS",0,0,False),
                    ("ferry","FERRY",0,0,False),
                    ("unknown","UNKNOWN",0,0,False),
                    ("cancelled","REGIONAL_RAIL",0,0,True),
                    ("pale_api_black_text","REGIONAL_RAIL",0xF2F5F5,0x111111,False),
                    ("bright_api_white_text","BUS",0xFFE600,0xFFFFFF,False),
                    ("dark_api_white_text","BUS",0x111111,0xFFFFFF,False),
                    ("mid_api_white_text","REGIONAL_RAIL",0x777777,0xFFFFFF,False)]
    for tag,category,color,preferred_color,cancelled in badge_fixtures:
        context=('struct {const char*category;uint32_t routeColor,routeTextColor;bool cancelled;} d{'+
                 f'"{category}",{color},{preferred_color},'+("true" if cancelled else "false")+'};'+
                 'const bool isBus=transitIsBus(d.category);'+badge_code+
                 'const uint32_t preferredText='+preferred+';')
        role("transit.badge."+tag,badge_fg,"badgeColor",at="uint32_t badgeColor;",context=context)

    launch_update=function(source,"lvglUpdateLaunchUi")
    launch_tick=function(source,"lvglTickLaunchCountdown")
    liftoff=block(launch_tick,launch_tick.index("if (delta <= 0)"))
    role("launch.liftoff.label",assignment(liftoff,"labelColor"),"launchHeroBg",at=liftoff)
    role("launch.liftoff.value",assignment(liftoff,"textColor"),"launchHeroBg",3,at=liftoff,kind="large text")
    role("launch.header.center",effective_text_hex([styles,launch],"g_launchUi.headerCenter"),"headerBg",
         at="g_launchUi.headerCenter = lv_label_create")
    role("launch.qr.overlay.hint",effective_text_hex([styles,launch],"g_launchUi.qrHint"),"t.screenBg",
         at="g_launchUi.qrHint = lv_label_create")
    hero_hint=effective_text_hex([styles,launch],"g_launchUi.heroQrHint")
    hint_opacity=effective_text_opacity(source,launch,"g_launchUi.heroQrHint")
    role("launch.qr.affordance",f"auditBlendLvgl({hero_hint},launchHeroBg,{hint_opacity})","launchHeroBg",
         at="lv_obj_set_style_text_opa(g_launchUi.heroQrHint,")
    provider=function(source,"launchProviderColor")
    selected["launchProviderColor"]=provider
    provider_slugs=re.findall(r'strstr\(slug,\s*"([^"]+)"',provider)+["unknown"]
    for slug in provider_slugs:
        for widget,tag,model in (("g_launchUi.heroBadgeLabel","hero","hero"),("g_launchUi.compactBadgeLabel[i]","compact","item")):
            role("launch.provider."+tag+"."+slug,effective_text_hex([launch_update,launch],widget),f"launchProviderColor({model}.providerSlug)",
                 at="static uint32_t launchProviderColor(",
                 context='struct {const char*providerSlug;} '+model+'{"'+slug+'"};')
    role("stats.header.stale", "headerText","headerBg",at='"%us ago"')
    dot=assignment(stats,"dot")
    for valid,live,tag in ((False,False,"waiting"),(True,True,"live"),(True,False,"stale")):
        role("stats.dot."+tag,dot,"headerBg",3,at="const uint32_t dot =",
             context="struct {bool valid;} s{"+("true" if valid else "false")+"};const bool live="+("true" if live else "false")+";",
             kind="redundant status graphic")

    # Cover-derived surfaces are inputs to the actual updater; these samples
    # probe both black/white selection branches and the mid-luminance edge.
    # They do not pretend to reproduce a particular user's cover image.
    now_init=function(source,"lvglInitNowPlayingUi")
    now_update=function(source,"lvglUpdateNowPlayingUi")
    header_opa=style_expression(now_init,"lv_obj_set_style_bg_opa","ui.header")
    backgrounds=[("native_fixture",0x182B3A),("requested_darkblue",0x1A2A38),
                 ("black",0x000000),("white",0xFFFFFF),("light_blue",0xDCEBFF),
                 ("light_mint",0xD9E9D2),("cream",0xFFE7B6),("midgray",0x777777),
                 ("crossover_gray",0x767676),("dark_midgray",0x555555),
                 ("light_midgray",0xA0A0A0),("muted_pink",0xB0657A),("muted_blue",0x617C98)]
    for tag,surface in backgrounds:
        context=f"const uint32_t bgSurface=0x{surface:06X};"
        for kind,name in (("uint16_t","bgLuma"),("bool","bgIsDark"),("uint32_t","headerBg"),("uint32_t","primaryText"),
                          ("uint32_t","headerSurface"),("uint32_t","headerText")):
            if re.search(r"\bconst\s+"+kind+r"\s+"+name+r"\s*=",now_update):
                context+=f"const {kind} {name}={assignment(now_update,name)};"
        context+=f"const uint32_t actualHeaderBg=auditBlendLvgl(headerBg,bgSurface,{header_opa});"
        for widget,bg,minimum,kind in (("ui.album","bgSurface",4.5,"small text"),
                                      ("ui.headerTime","actualHeaderBg",4.5,"small text"),
                                      ("ui.title","actualHeaderBg",4.5,"small text"),
                                      ("ui.artist","bgSurface",4.5,"small text"),
                                      ("ui.track","bgSurface",3,"large text")):
            opacity=effective_text_opacity(source,now_init,widget)
            role("nowplaying."+widget.split(".")[1]+"."+tag,
                 f"auditBlendLvgl({color_expression(now_update,widget)},{bg},{opacity})",bg,minimum,
                 at=now_init,context=context,kind=kind,font=font_expression(now_init,widget))
        dot_start=now_update.index("lvglSetBgColorIfChanged(ui.statusDot,")
        dot_fg=arguments(arguments(now_update,"lvglSetBgColorIfChanged",dot_start)[1],"lv_color_hex")[0]
        for synced in (False,True):
            role("nowplaying.dot."+("live." if synced else "waiting.")+tag,dot_fg,"actualHeaderBg",3,
                 at=now_update,context=context+"const bool displaySync="+("true" if synced else "false")+";",
                 kind="redundant status graphic")

    field_widgets = dict(windFmt="g_weatherUi.humidity", windNa="g_weatherUi.humidity",
                        forecast3h="g_weatherUi.forecastNow", forecastNow="g_weatherUi.forecastNow", forecastNa="g_weatherUi.forecastNow",
                        weatherOffline="g_weatherUi.desc", wifiOff="g_weatherUi.desc")
    string_fonts = {field: font_expression(fonts_body, field_widgets[field] if field in field_widgets else "d->news" if field.startswith("rss") else "d->qrHint") for field in ui_fields}
    literal_fonts={"launch.TBD":font_expression(launch,"g_launchUi.heroCountdown"),
                   "transit.unconfigured":font_expression(fonts_body,"g_transitUi.noData")}
    if re.search(r"^static[^;{}\n]*\blvglFontLaunchCountdown\(",source,re.M):
        # Match the real launch update's numericCountdown=hasT0||winOpen hook.
        literal_fonts.update({"launch.TBD":"lvglFontLaunchCountdown(false)",
                              "launch.numeric":"lvglFontLaunchCountdown(true)",
                              "launch.NET.time":"lvglFontLaunchCountdown(true)"})
    for expression in [*string_fonts.values(), *literal_fonts.values(), *[r["font_accessor"] for r in roles if r["font_accessor"]]]:
        for called in re.findall(r"\b(lvgl[A-Z]\w*)\s*\(", expression): collect(called)

    fonts = sorted(set(re.findall(r"\bscry_font_dosis_\w+\b", "\n".join(selected.values()))))
    font_accessors = sorted(set(string_fonts.values()) | set(literal_fonts.values()))
    font_index = {name: i for i,name in enumerate(font_accessors)}
    prototypes = "\n".join(code[:code.index("{")].strip()+";" for code in selected.values())
    declarations = "\n".join(f"LV_FONT_DECLARE({name});" for name in fonts)
    main_roles = []
    for index,r in enumerate(roles):
        main_roles.append("{" + r["context"] + f' emitContrast({index},{r["foreground_expression"]},{r["background_expression"]});' + "}")
    catalog = []
    for language in languages:
        for field in ui_fields:
            catalog.append(f'emitString("{language}","{field}",kUiLang_{language}.{field});')
        table = language_weather[language]
        for field in ("clear","cloudy","overcast","fog","rain","snow","storm","na"):
            catalog.append(f'emitShort("{language}",{table}.{field});')
    countdown = function(source,"lvglTickLaunchCountdown")
    countdown_literal = assignment(countdown,"netBuf[24]")
    transit_init = function(source,"lvglInitTransitUi")
    transit_text = arguments(transit_init,"lvglSetLabelTextIfChanged",
                             transit_init.index("lvglSetLabelTextIfChanged(g_transitUi.noData,"))[1]
    catalog += [f'emitString("@widget","launch.TBD",{countdown_literal});',
                '{struct {bool configured;} g_transitConfig{false};'+
                f'emitString("@widget","transit.unconfigured",{transit_text});'+'}']

    # Bounds below are expressions extracted from the actual 640x172 widgets.
    weather_geometry = geometry(weather,["weatherW","weatherTopTextW","weatherCardW"])
    wind_width = arguments(weather,"lv_obj_set_width",weather.index("lv_obj_set_width(g_weatherUi.humidity"))[1]
    desc_width = arguments(weather,"lv_obj_set_width",weather.index("lv_obj_set_width(g_weatherUi.desc"))[1]
    forecast_width = arguments(weather,"lv_obj_set_width",weather.index("lv_obj_set_width(g_weatherUi.forecastNow"))[1]
    feed_geometry = geometry(feed,["newsH","leftPaneW","hintW","qrOverlayH"])
    launch_geometry = geometry(launch,["rightW"])
    countdown_size = arguments(launch,"lv_obj_set_size",launch.index("lv_obj_set_size(g_launchUi.heroCountdown,"))
    qr_y = arguments(feed,"lv_obj_set_pos",feed.index("lv_obj_set_pos(d.qrHint"))[2]
    bounds = f'''{{{weather_geometry}
emitBound("wind",{wind_width},lvglFontMini()->line_height,"single");
emitBound("weather.description",{desc_width},lvglFontMeta()->line_height,"scroll");
emitBound("forecast",{forecast_width},lvglFontSmallBold()->line_height,"dot");}}
{{{feed_geometry}
emitBound("rss",leftPaneW,newsH,"dot-wrap");
emitBound("qr",hintW,qrOverlayH-({qr_y}),"wrap");}}
{{{launch_geometry}
emitBound("launch.countdown",{countdown_size[1]},{countdown_size[2]},"single");}}'''
    panel_width = re.search(r"^#define DISPLAY_WEATHER_PANEL_W\s+(\d+)",config,re.M)[1]
    meteo_path = ROOT/"assets/meteocons/generated/meteocons_lvgl.h"
    meteo_size = re.search(r"METEOCON_SIZE\s*=\s*(\d+)",meteo_path.read_text())[1] if meteo_path.exists() else "60"
    prelude = '''#include <lvgl.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "ui_strings_snapshot.h"
#include "src/lang_types.h"
'''+f"#define DISPLAY_WEATHER_PANEL_W {panel_width}\n#define DB_HAS_LVGL_METEOCONS {int(meteo_path.exists())}\n#define METEOCON_SIZE {meteo_size}\n"
    native = r'''
static void quoted(const std::string &s){std::cout<<'"';for(unsigned char c:s){switch(c){case '"':std::cout<<"\\\"";break;case '\\':std::cout<<"\\\\";break;case '\n':std::cout<<"\\n";break;case '\r':std::cout<<"\\r";break;case '\t':std::cout<<"\\t";break;default:if(c<32){char b[7];std::snprintf(b,sizeof b,"\\u%04x",c);std::cout<<b;}else std::cout<<c;}}std::cout<<'"';}
static_assert(LV_COLOR_DEPTH==16,"RGB565 audit requires the firmware's 16-bit color depth");
static uint32_t quantized(uint32_t c){return lv_color_to32(lv_color_hex(c))&0xFFFFFFu;}
static uint32_t auditBlendLvgl(uint32_t foreground,uint32_t background,lv_opa_t opacity){return lv_color_to32(lv_color_mix(lv_color_hex(foreground),lv_color_hex(background),opacity))&0xFFFFFFu;}
static void emitContrast(unsigned index,uint32_t fg,uint32_t bg){std::cout<<"{\"type\":\"contrast\",\"theme\":";quoted(activeUiThemeId());std::cout<<",\"role_index\":"<<index<<",\"fg\":"<<fg<<",\"bg\":"<<bg<<",\"ratio\":"<<lvglContrastRatio(fg,bg)<<",\"rgb565_fg\":"<<quantized(fg)<<",\"rgb565_bg\":"<<quantized(bg)<<",\"rgb565_ratio\":"<<lvglContrastRatio(quantized(fg),quantized(bg))<<"}\n";}
static void emitString(const char*lang,const char*field,const char*text){std::cout<<"{\"type\":\"string\",\"language\":";quoted(lang);std::cout<<",\"field\":";quoted(field);std::cout<<",\"text\":";quoted(text);std::cout<<"}\n";}
static void emitShort(const char*lang,const char*text){std::cout<<"{\"type\":\"weather_short\",\"language\":";quoted(lang);std::cout<<",\"text\":";quoted(text);std::cout<<"}\n";}
static void emitBound(const char*role,int width,int height,const char*mode){std::cout<<"{\"type\":\"bound\",\"role\":";quoted(role);std::cout<<",\"width\":"<<width<<",\"height\":"<<height<<",\"mode\":";quoted(mode);std::cout<<"}\n";}
static void measure(const std::vector<const lv_font_t*>&fonts){
  std::string line;
  while(std::getline(std::cin,line)){
    std::istringstream in(line);unsigned id,index;int width,height,letterSpace,lineSpace,verbose;std::string hex;
    if(!(in>>id>>index>>width>>height>>letterSpace>>lineSpace>>verbose>>hex)||index>=fonts.size())continue;
    std::string text;
    if(hex!="-")for(size_t i=0;i+1<hex.size();i+=2)text+=char(std::stoul(hex.substr(i,2),nullptr,16));
    auto f=fonts[index];lv_point_t full{},wrapped{};
    lv_txt_get_size(&full,text.c_str(),f,letterSpace,lineSpace,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
    lv_txt_get_size(&wrapped,text.c_str(),f,letterSpace,lineSpace,width>0?width:LV_COORD_MAX,LV_TEXT_FLAG_NONE);
    std::cout<<"{\"id\":"<<id<<",\"unwrapped_width\":"<<full.x<<",\"unwrapped_height\":"<<full.y<<",\"wrapped_width\":"<<wrapped.x<<",\"wrapped_height\":"<<wrapped.y<<",\"font_line_height\":"<<f->line_height;
    auto glyphs=[&](bool onlyIssues){
      uint32_t pos=0;bool first=true;
      while(text[pos]){
        auto cp=_lv_txt_encoded_next(text.c_str(),&pos);
        uint32_t nextPos=pos,next=text[pos]?_lv_txt_encoded_next(text.c_str(),&nextPos):0;
        if(cp=='\n'||cp=='\r'||cp=='\t')continue;
        lv_font_glyph_dsc_t d{};bool ok=lv_font_get_glyph_dsc(f,&d,cp,next);
        bool missing=!ok||d.is_placeholder,fallback=d.resolved_font&&d.resolved_font!=f;
        if(onlyIssues&&!missing&&!fallback)continue;
        if(!first)std::cout<<',';first=false;
        std::cout<<"{\"codepoint\":"<<cp<<",\"missing\":"<<(missing?"true":"false")<<",\"fallback\":"<<(fallback?"true":"false")<<",\"box_width\":"<<d.box_w<<",\"box_height\":"<<d.box_h<<",\"offset_y\":"<<d.ofs_y<<"}";
      }
    };
    std::cout<<",\"glyph_issues\":[";glyphs(true);std::cout<<"]";
    if(verbose){std::cout<<",\"glyphs\":[";glyphs(false);std::cout<<"]";}
    std::cout<<"}\n";
  }
}
'''
    main = '''int main(int argc,char**){lv_init();const std::vector<const lv_font_t*>fonts={'''+",".join(font_accessors)+'''};
if(argc>1){measure(fonts);return 0;}
for(g_uiThemeIndex=0;g_uiThemeIndex<UI_THEME_COUNT;++g_uiThemeIndex){const auto&t=activeUiTheme().lvgl;
'''+locals_+"\n"+"\n".join(main_roles)+"\n}\n"+"\n".join(catalog)+"\nconst int16_t cW=640,cH=172;\n"+bounds+"\n}"
    lifecycle=[]
    if not row_tint_refresh:
        lifecycle.append(dict(widget="g_transitUi.rowBg[i]",detail="Alternating row tint is initialized once; a dark/light theme switch retains the previous tint color."))
    for widget in ("g_launchUi.headerCenter","g_launchUi.heroQrHint","g_launchUi.qrHint"):
        if not re.search(r"lvglSetText(?:Hex|ColorIfChanged)\(\s*"+re.escape(widget)+r"\s*,",styles):
            lifecycle.append(dict(widget=widget,detail="No foreground update in theme application; an existing widget may retain its previous theme color."))
    metadata = dict(theme_ids=theme_ids,languages=languages,fields=ui_fields,roles=roles,string_fonts=string_fonts,literal_fonts=literal_fonts,lifecycle_findings=lifecycle,
                    font_accessors=font_accessors,font_index=font_index,fonts=fonts,
                    color_model=dict(depth=16,conversion="lv_color_to32(lv_color_hex(...))",opacity="lv_color_mix, then lv_color_to32; full glyph ink, not antialias edge pixels",
                                     covers={tag:f"#{surface:06X}" for tag,surface in backgrounds},
                                     nowplaying_header_opacity=header_opa,launch_hero_opacity=hero_opa,transit_odd_row_opacity=row_opa,
                                     transit_tint_reapplied_on_theme_change=row_tint_refresh),
                    source_functions={name:line_at(source,code) for name,code in selected.items()})
    return "\n".join([prelude,declarations,themes,weather_tables,prototypes,"\n".join(selected.values()),native,main]),metadata


def run(command: list[str], log: pathlib.Path | None = None, stdin: str | None = None) -> str:
    result=subprocess.run(command,cwd=ROOT,text=True,input=stdin,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    if log: log.write_text(result.stdout)
    if result.returncode: raise RuntimeError(f"Command failed: {command[0]}\n{result.stdout[-12000:]}")
    return result.stdout


def contrast_required(row: dict) -> bool:
    # Status dots repeat an adjacent text status; keep them visible in the
    # report without treating a redundant graphic as the only source of state.
    return row["kind"] not in ("token inventory","redundant status graphic")


def compare_native_surfaces(contrast: list[dict], samples: dict) -> list[dict]:
    """Cross-check the computed surface against independent rendered pixels."""
    roles={(row["theme"],row["role"]):row for row in contrast}
    checks=[]
    bindings=(("NOWPLAYING","header","nowplaying.headerTime.native_fixture"),
              ("NOWPLAYING","body","nowplaying.album.native_fixture"),
              ("LAUNCH","hero","launch.weather"),
              ("TRANSIT","normal_row","transit.delay.late"),
              ("TRANSIT","odd_row","transit.delay.late.odd"))
    for theme,pages in samples["themes"].items():
        for page,key,role_name in bindings:
            sample=pages[page]["samples"][key]
            rendered="#"+"".join(f"{channel:02X}" for channel in sample["rgb"])
            computed=roles[theme,role_name]["rgb565_bg"]
            checks.append(dict(theme=theme,page=page,surface=key,x=sample["x"],y=sample["y"],
                               rendered=rendered,computed=computed,matches=rendered==computed))
    return checks


def recommend(source: str, strings: str) -> list[dict]:
    items = [
        dict(id="pirate.wind_units",priority=1,detail="Wind uses a km/h value with a knots label; convert only Pirate by dividing by 1.852, retaining nautical character.",source="src/ui_strings.h:270",reference="https://www.aoml.noaa.gov/hrd/tcfaq/G4.html"),
        dict(id="pirate.forecast_time",priority=3,detail="Three bells are not three hours. Use an unambiguous 3h label with a Pirate suffix; retain the rest of the voice.",source="src/ui_strings.h:272",reference="https://www.mycg.uscg.mil/News/Article/3616329/eight-bells-and-all-is-well/"),
        dict(id="german.rss_sync",priority=3,detail="Replace broken hybrid 'RSS-Feed wird sync...' with 'RSS wird synchronisiert...' and check wrapping.",source="src/ui_strings.h:99"),
        dict(id="esperanto.touch",priority=3,detail="Tusu means cough; the touch imperative is Tuŝu. Dosis supports ŝ. Preserve this distinction rather than stripping the accent.",source="src/ui_strings.h:181",reference="https://www.retavortaro.de/revo/art/tus.html"),
        dict(id="accents",priority=3,detail="Restore accents in directly rendered French/Spanish fields (Météo, où, Génération, conexión). RSS content is still folded to ASCII by sanitizeAsciiBuffer: a table edit alone cannot fix that path.",source="src/ui_strings.h:75"),
        dict(id="voices",priority="constraint",detail="Preserve Bellazio slang (boh/tipo/slay), Pirate phrasing and Shakespeare's Prithee/anon; do not turn stylistic choices into generic English/Italian.",source="src/ui_strings.h:209"),
        dict(id="web_hero",priority=3,detail="Move the adverb: 'Everything you could check faster on your phone, but won't.' Keep the joke and tone.",source="scrybar.ino:4969"),
        dict(id="web_view_descriptions",priority=3,detail="INFO description currently refers to word clock, and Mac Stats omits network. Describe actual page contents.",source="scrybar.ino:5053"),
    ]
    present={
        "pirate.wind_units":bool(re.search(r"activeUiStrings\(\)->windFmt,\s*g_weather\.windKmh\s*\)",source)),
        "pirate.forecast_time":"In 3 bells" in strings,
        "german.rss_sync":"RSS-Feed wird sync" in strings,
        "esperanto.touch":"Tusu " in strings,
        "accents":any(value in strings for value in ("Meteo hors ligne","Tiempo sin conexion","n'importe ou pour fermer","Generation QR")),
        "web_hero":"could faster check" in source,
        "web_view_descriptions":"Word clock and ambient status page." in source or "CPU/GPU temperature, RAM and Disk from Companion." in source,
    }
    for item in items:
        item["status"]="preserve" if item["id"]=="voices" else "present" if present[item["id"]] else "not present in current source"
    if re.search(r"if\s*\(showIndex\s*>=\s*0\)\s*sanitizeAsciiBuffer\(title3",source):
        next(item for item in items if item["id"]=="accents")["detail"]=(
            "Supported accents are retained in internal localized RSS messages. External article text alone keeps its established ASCII fallback.")
    return items


def write_markdown(report: dict, path: pathlib.Path) -> None:
    summary=report["summary"]
    title="baseline" if path.name=="baseline.md" else "audit"
    lines=[f"# Display quality {title} — 2026-10-23","",report["evidence"],"",
           f"Firmware tag: `{report['build_tag']}`. Source SHA-256: `{report['source_sha256']}`.","",
           f"Checked {summary['themes']} themes × {summary['roles']} selected color roles = {summary['contrast_checks']} pairs, "
           f"{summary['languages']} languages / {summary['ui_strings']} UiStrings entries, "
           f"and {summary['text_measurements']} text fixtures with the actual installed LVGL/fonts.","",
           "The thresholds are design benchmarks from [W3C text contrast](https://www.w3.org/WAI/WCAG22/Understanding/contrast-minimum.html) "
           "and [non-text contrast](https://www.w3.org/WAI/WCAG22/Understanding/non-text-contrast.html), not a physical-display compliance claim. "
           "The JSON retains both token RGB888 and quantized RGB565 estimates. Antialiasing, panel calibration and ambient light require visual review.","",
           "Composited roles use the installed LVGL's `lv_color_mix` and `lv_color_to32`, including the transparent LAUNCH root's "
           "20/255 hero veil, TRANSIT's22/255 alternate rows and the effective final text opacity. "
           "Now Playing samples13 representative cover-derived surfaces, with separate body/header ink and both status-dot states. "
           "These are finite color fixtures, not an exhaustive proof for every image. "
           "Earlier baseline reports expanded RGB565 channels with integer scaling; this audit uses the same native conversion as the PNG renderer.","",
           "## Measured contrast findings","","Only active roles are counted as findings. Legacy token inventory is separately marked. "
           "Known runtime conditions (usage/temperature/pressed state) are synthetic; colors and expressions come from firmware.","",
           "| Theme | Role | RGB888 ratio | RGB565 ratio | Target | Source |","|---|---|---:|---:|---:|---|"]
    for row in report["contrast"]:
        if row["passes"] or not contrast_required(row): continue
        lines.append(f"| {row['theme']} | {row['role']} | {row['ratio']:.3f} | {row['rgb565_ratio']:.3f} | {row['minimum']:g} | scrybar.ino:{row['source_line']} |")
    if not any(not row["passes"] and contrast_required(row) for row in report["contrast"]):
        lines += ["","No required contrast findings in the selected roles."]
    advisory=[r for r in report["contrast"] if not contrast_required(r) and r["kind"]!="token inventory" and not r["passes"]]
    if advisory:
        lines += ["","### Supplemental status-marker checks","",
                  "These markers repeat adjacent text. Ratios below3:1 are advisory visibility observations, separate from required text/data findings.","",
                  "| Theme | Role | RGB888 ratio | RGB565 ratio |","|---|---|---:|---:|"]
        for r in advisory: lines.append(f"| {r['theme']} | {r['role']} | {r['ratio']:.3f} | {r['rgb565_ratio']:.3f} |")
    if report.get("lifecycle_findings"):
        lines += ["","### Theme mutation findings",""]
        for r in report["lifecycle_findings"]: lines.append(f"- `{r['widget']}`: {r['detail']}")
    if report.get("native_surface_comparison"):
        comparison=report["native_surface_comparison"]
        lines += ["","### Independent rendered-surface check","",
                  f"{len(comparison['checks'])} background samples from the full native page renderer; "
                  f"{sum(not c['matches'] for c in comparison['checks'])} mismatches. "
                  f"Renderer source SHA-256: `{comparison['renderer_source_sha256']}`. "
                  +("The renderer and this audit used the same source snapshot." if comparison["source_matches"] else
                    "The renderer used an earlier candidate: this corroborates the unchanged surfaces, not every later source edit.")]
    lines += ["","## Fonts, formats and text bounds","",
              f"Format signatures: **{summary['format_errors']} errors**. UiStrings missing glyph fixtures: **{summary['ui_missing_glyph_fixtures']}**. "
              f"UiStrings fallback fixtures: **{summary['ui_fallback_fixtures']}**.","",
              "Bounds use actual extracted geometry at 640×172. `scroll` and `dot` overflow is recorded as intentional behavior, "
              "not automatically treated as a defect. RSS and QR wrapping are measured with their actual fonts. "
              "Forecast fixtures cover every short weather label from the corresponding language table; wind fixtures include 0/18.52/123/999. "
              "Raw UiStrings and formatted variants are both retained in JSON. This checks font coverage, not grammatical correctness.","",
              "| Fixture | Font | Unwrapped width | Wrapped height | Bounds | Observation |","|---|---|---:|---:|---|---|"]
    for row in report["text"]:
        if row.get("fixture_kind")=="widget_literal" or row["glyph_issues"] or row.get("overflow_unhandled"):
            issues=", ".join(f"U+{g['codepoint']:04X} "+("missing" if g["missing"] else "fallback") for g in row["glyph_issues"])
            observation="unhandled overflow" if row.get("overflow_unhandled") else issues or ("covered; bounds not measured" if row["mode"]=="unbounded" else "fits")
            lines.append(f"| {row['id']} | {row['font_accessor']} | {row['unwrapped_width']} | {row['wrapped_height']} | {row['bound_width']}×{row['bound_height']} ({row['mode']}) | {observation} |")
    inactive=[f for f in report["string_fields"] if not f["currently_referenced"]]
    qr_connected=all(f["currently_referenced"] for f in report["string_fields"] if f["field"] in ("touchToCloseAnywhere","generatingQr"))
    qr_note=("Runtime QR paths reference the localized table. The unused shorter init-time field remains an inventory entry." if qr_connected else
             "QR paths still contain English literals; localized QR measurements are intended-table fixtures until those bindings are connected.")
    processing=report.get("text_processing",{})
    rss_note=("Internal localized RSS messages retain their Unicode accents; external article text alone uses the existing ASCII fallback." if processing.get("rss_internal_preserves_unicode") else
              "RSS text is folded to ASCII, so changing accents in that table does not by itself change the display output.")
    lines += ["","UiStrings fields currently without active callsites: "+(", ".join(f"`{f['field']}`" for f in inactive) or "none")+". "+qr_note,"",rss_note,"",
              "## Copy recommendations","","This is the editorial checklist established from the baseline, not automatic proofreading. "
              "Status indicates whether its known source pattern remains present; source pointers identify the original baseline location.",""]
    for item in report["copy_recommendations"]:
        suffix=f" ([source]({item['reference']}))" if item.get("reference") else ""
        priority="Constraint" if item["priority"]=="constraint" else "P"+str(item["priority"])
        status=" — "+item["status"] if "status" in item else ""
        lines.append(f"- **{priority} {item['id']}{status}** — {item['detail']} `{item['source']}`.{suffix}")
    lines += ["","## Separate visual and runtime verification","",
              "- Capture and inspect 7 themes × 10 pages (INFO, HOME, RSS/AUX, WIKI, NOW PLAYING, DOOM, TRANSIT, LAUNCH, MAC STATS, BAMBU). "
              "Mark disabled/unavailable pages explicitly; this audit alone does not stand in for 70 inspected screenshots. "
              "The completed native-render matrix and its limits are recorded separately in [native-pages.md](native-pages.md).",
              "- Exercise Mint warning values, imminent Transit times, Launch TBD/liftoff, Wiki Random without favicon, QR in 14 languages and long/offline/stale states.",
              "- Compare patched output against this source-hashed baseline, inspect a contact sheet, and confirm representative physical-panel results.",
              "- Preserve theme-specific geometry and hue. Bambu/DOOM/artwork and the unified screensaver intentionally contain theme-independent parts.",
              ("- The Control Surface consumes the firmware's Web theme tokens through `appendWebThemeCssVars()`. "
               "Its responsive and interaction checks are recorded separately in [the WebUI QA](../2026-10-23-webui/QA.md)."
               if report.get("web_theme_tokens_consumed") else
               "- This source snapshot does not consume the Web theme tokens through `appendWebThemeCssVars()`. "
               "Web behavior is outside this native display audit."),"",
              "## Reproduce","","```sh",report["reproduce"],"```","",
              "Build products, logs and native executables remain in the external temporary build directory. No firmware, USB, NVS, network or Git operation is performed.",""]
    path.write_text("\n".join(lines))


def main() -> None:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lvgl-root",type=pathlib.Path,default=pathlib.Path.home()/"Documents/Arduino/libraries/lvgl")
    parser.add_argument("--build-dir",type=pathlib.Path,default=pathlib.Path(tempfile.gettempdir())/"scrybar-quality-native")
    parser.add_argument("--out-prefix",type=pathlib.Path,default=pathlib.Path(tempfile.gettempdir())/"scrybar-ui-quality")
    parser.add_argument("--fail-on-findings",action="store_true")
    parser.add_argument("--native-surface-samples",type=pathlib.Path,help="Optional full-page renderer's pixel sample JSON")
    args=parser.parse_args()
    build=args.build_dir.resolve()
    if "Dropbox" in build.parts: raise ValueError("Build directory must be outside Dropbox")
    build.mkdir(parents=True,exist_ok=True)
    source=(ROOT/"scrybar.ino").read_text(); strings=(ROOT/"src/ui_strings.h").read_text(); config=(ROOT/"config.h").read_text()
    cpp,metadata=generated_source(source,strings,config)
    cpp_path=build/"audit-ui.cpp";cpp_path.write_text(cpp)
    # Compile exactly the table whose hash is reported, even if another editor
    # changes its live source while the native compiler is running.
    (build/"ui_strings_snapshot.h").write_text(strings)
    library=build/"lvgl"
    run(["cmake","-S",str(args.lvgl_root),"-B",str(library),"-DLV_CONF_PATH="+str(ROOT/"lv_conf.h"),"-DCMAKE_BUILD_TYPE=Release"],build/"configure.log")
    run(["cmake","--build",str(library),"--target","lvgl","-j","8"],build/"lvgl-build.log")
    include=["-I",str(ROOT),"-I",str(args.lvgl_root)]
    def compile_font(name: str) -> str:
        src=ROOT/"src/fonts"/(name+".c");obj=build/(name+".o")
        if not obj.exists() or obj.stat().st_mtime<max(src.stat().st_mtime,(ROOT/"lv_conf.h").stat().st_mtime):
            run(["cc","-O2",*include,"-c",str(src),"-o",str(obj)],build/(name+".log"))
        return str(obj)
    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool: objects=list(pool.map(compile_font,metadata["fonts"]))
    binary=build/"audit-ui"
    run(["c++","-std=c++17","-O2",*include,str(cpp_path),*objects,str(library/"lib/liblvgl.a"),"-o",str(binary)],build/"compile.log")
    catalog=[json.loads(line) for line in run([str(binary)]).splitlines() if line.startswith("{")]
    bounds={r["role"]:r for r in catalog if r["type"]=="bound"}
    contrast=[]
    for record in catalog:
        if record["type"]!="contrast": continue
        row={**metadata["roles"][record["role_index"]],**record};row.pop("type");row.pop("role_index");row.pop("context")
        row["passes"]=min(row["ratio"],row["rgb565_ratio"])>=row["minimum"]
        for key in ("fg","bg","rgb565_fg","rgb565_bg"): row[key]="#"+format(row[key],"06X")
        contrast.append(row)
    strings_rows=[r for r in catalog if r["type"]=="string" and r["language"]!="@widget"]
    short={language:[] for language in metadata["languages"]}
    for r in catalog:
        if r["type"]=="weather_short" and r["text"] not in short[r["language"]]: short[r["language"]].append(r["text"])
    fixtures=[];format_errors=[]
    for row in strings_rows:
        field=row["field"];expected=["float"] if field=="windFmt" else ["s"] if field in ("forecast3h","forecastNow") else []
        signature,errors=format_signature(row["text"])
        row["format_signature"]=signature;row["format_valid"]=signature==expected and not errors
        if not row["format_valid"]: format_errors.append(dict(language=row["language"],field=field,expected=expected,actual=signature,errors=errors))
        key="wind" if field in ("windFmt","windNa") else "forecast" if field.startswith("forecast") else "weather.description" if field in ("weatherOffline","wifiOff") else "rss" if field.startswith("rss") else "qr"
        variants=[("raw",row["text"])]
        if row["format_valid"]:
            if field=="windFmt": variants += [(str(value),row["text"]%value) for value in (0,18.52,123,999)]
            elif expected==["s"]: variants += [(str(i),row["text"]%value) for i,value in enumerate(short[row["language"]])]
        for variant,text in variants:
            fixtures.append(dict(id=row["language"]+"."+field+"."+variant,language=row["language"],field=field,text=text,
                                 fixture_kind="UiStrings",font_accessor=metadata["string_fonts"][field],bound_role=key,
                                 letter_space=0,line_space=3 if key=="rss" else 0,
                                 currently_referenced=bool(re.search(r"activeUiStrings\(\)->"+field+r"\b",source))))
    # A literal probe follows the source font binding; coverage remains useful
    # after a font fix even when an existing value is no longer the default.
    widget_literals={r["field"]:r["text"] for r in catalog if r["type"]=="string" and r["language"]=="@widget"}
    literals=[("launch.TBD",widget_literals["launch.TBD"],metadata["literal_fonts"]["launch.TBD"],"launch.countdown",-2),
              ("transit.unconfigured",widget_literals["transit.unconfigured"],metadata["literal_fonts"]["transit.unconfigured"],"none",0)]
    if "launch.numeric" in metadata["literal_fonts"]:
        literals += [("launch.numeric","T-00:05:42",metadata["literal_fonts"]["launch.numeric"],"launch.countdown",-2),
                     ("launch.NET.time","21:42",metadata["literal_fonts"]["launch.NET.time"],"launch.countdown",-2)]
    for identity,text,font,key,spacing in literals: fixtures.append(dict(id=identity,text=text,font_accessor=font,bound_role=key,fixture_kind="widget_literal",letter_space=spacing,line_space=0))
    requests=[]
    for i,row in enumerate(fixtures):
        bound=bounds.get(row["bound_role"],dict(width=0,height=0,mode="unbounded"))
        row.update(bound_width=bound["width"],bound_height=bound["height"],mode=bound["mode"])
        requests.append(f"{i} {metadata['font_index'][row['font_accessor']]} {bound['width']} {bound['height']} {row['letter_space']} {row['line_space']} {int(row['fixture_kind']=='widget_literal')} {row['text'].encode().hex() or '-'}")
    measured=[json.loads(line) for line in run([str(binary),"measure"],stdin="\n".join(requests)+"\n").splitlines() if line.startswith("{")]
    if len(measured)!=len(fixtures): raise RuntimeError("Incomplete native measurement output")
    for record in measured:
        row=fixtures[record.pop("id")];row.update(record)
        row["overflows_width"]=bool(row["bound_width"] and row["unwrapped_width"]>row["bound_width"])
        row["overflows_height"]=bool(row["bound_height"] and row["wrapped_height"]>row["bound_height"])
        row["overflow_unhandled"]=row["overflows_width"] if row["mode"]=="single" else row["overflows_height"] if row["mode"]=="wrap" else False
    summary=dict(themes=len(metadata["theme_ids"]),roles=len(metadata["roles"]),contrast_checks=len(contrast),
                 contrast_findings=sum(not r["passes"] and contrast_required(r) for r in contrast),
                 advisory_contrast_findings=sum(not r["passes"] and r["kind"]=="redundant status graphic" for r in contrast),
                 lifecycle_findings=len(metadata["lifecycle_findings"]),
                 languages=len(metadata["languages"]),ui_strings=len(strings_rows),text_measurements=len(fixtures),format_errors=len(format_errors),
                 ui_missing_glyph_fixtures=sum(r["fixture_kind"]=="UiStrings" and any(g["missing"] for g in r["glyph_issues"]) for r in fixtures),
                 ui_fallback_fixtures=sum(r["fixture_kind"]=="UiStrings" and any(g["fallback"] for g in r["glyph_issues"]) for r in fixtures),
                 widget_missing_glyph_fixtures=sum(r["fixture_kind"]=="widget_literal" and any(g["missing"] for g in r["glyph_issues"]) for r in fixtures),
                 widget_fallback_fixtures=sum(r["fixture_kind"]=="widget_literal" and any(g["fallback"] for g in r["glyph_issues"]) for r in fixtures),
                 unhandled_overflow_fixtures=sum(r["overflow_unhandled"] for r in fixtures))
    report=dict(evidence="Static source extraction plus native LVGL 8 glyph/geometry measurements. Synthetic conditions; no device, live data or physical acceptance.",
                build_tag=re.search(r'^#define FW_BUILD_TAG\s+"([^"]+)"',config,re.M)[1],
                source_sha256=hashlib.sha256(source.encode()).hexdigest(),strings_sha256=hashlib.sha256(strings.encode()).hexdigest(),
                web_theme_tokens_consumed=source.count("appendWebThemeCssVars(") > 1,
                lvgl_version=re.search(r'^version=(.+)',(args.lvgl_root/"library.properties").read_text(),re.M)[1],
                summary=summary,viewport=dict(width=640,height=172),themes=metadata["theme_ids"],bounds=bounds,color_model=metadata["color_model"],
                text_processing=dict(rss_internal_preserves_unicode=bool(re.search(r"if\s*\(showIndex\s*>=\s*0\)\s*sanitizeAsciiBuffer\(title3",source))),
                source_functions=metadata["source_functions"],lifecycle_findings=metadata["lifecycle_findings"],contrast=contrast,strings=strings_rows,format_errors=format_errors,text=fixtures,
                string_fields=[dict(field=field,font_accessor=metadata["string_fonts"][field],currently_referenced=bool(re.search(r"activeUiStrings\(\)->"+field+r"\b",source))) for field in metadata["fields"]],
                copy_recommendations=recommend(source,strings),
                reproduce="PYTHONDONTWRITEBYTECODE=1 python3 tools/audit_ui_quality_native.py --out-prefix "+str(args.out_prefix.relative_to(ROOT) if args.out_prefix.is_relative_to(ROOT) else args.out_prefix)+(" --fail-on-findings" if args.fail_on_findings else ""))
    if args.native_surface_samples:
        samples=json.loads(args.native_surface_samples.read_text())
        checks=compare_native_surfaces(contrast,samples)
        report["native_surface_comparison"]=dict(renderer_source_sha256=samples["source_sha256"],
                                                 source_matches=samples["source_sha256"]==report["source_sha256"],checks=checks)
        summary["native_surface_mismatches"]=sum(not c["matches"] for c in checks)
        summary["native_surface_checks"]=len(checks)
        sample_path=args.native_surface_samples
        report["reproduce"]+=" --native-surface-samples "+str(sample_path.relative_to(ROOT) if sample_path.is_relative_to(ROOT) else sample_path)
    prefix=args.out_prefix.resolve();prefix.parent.mkdir(parents=True,exist_ok=True)
    prefix.with_suffix(".json").write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n")
    write_markdown(report,prefix.with_suffix(".md"))
    print(json.dumps(summary,indent=2));print(f"Reports: {prefix}.json / .md")
    if args.fail_on_findings and any(summary[key] for key in (
        "contrast_findings","lifecycle_findings","format_errors","ui_missing_glyph_fixtures",
        "widget_missing_glyph_fixtures","widget_fallback_fixtures","unhandled_overflow_fixtures")) or (args.fail_on_findings and summary.get("native_surface_mismatches",0)): sys.exit(1)


if __name__=="__main__": main()
