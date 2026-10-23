// Anonymous data adapters for tools/render_display_quality_native.py. Layout,
// fonts, palette application and normal per-page updaters come from firmware.
static void qualityText(char *dst, size_t size, const char *value) {
  std::snprintf(dst, size, "%s", value);
}
static void qualityApplyTheme() {
  // Production creates every page before applying the theme; other page
  // initialisers have already resolved layout by that point. Resolve the same
  // geometry when this harness creates just one isolated page.
  lv_obj_update_layout(lv_scr_act());
  lvglApplyThemeStyles(false);
}
static void qualityLaunch(const char *state) {
  g_launchState.valid=true;g_launchState.count=3;
  qualityText(g_launchState.fetchedAt,sizeof(g_launchState.fetchedAt),"12:00");
  for(unsigned i=0;i<3;++i) {
    auto &item=g_launchState.items[i];
    qualityText(item.name,sizeof(item.name),i==0?"Orbital Research Mission - Flight 12":i==1?"Earth Observation Pathfinder":"Lunar Communications Demonstrator");
    qualityText(item.provider,sizeof(item.provider),i==1?"Rocket Lab":"SpaceX");
    qualityText(item.providerSlug,sizeof(item.providerSlug),i==1?"rocket-lab":"spacex");
    qualityText(item.vehicle,sizeof(item.vehicle),i==1?"Electron":"Falcon 9 Block 5");
    qualityText(item.pad,sizeof(item.pad),"Launch Complex 39A");
    qualityText(item.location,sizeof(item.location),"Kennedy Space Center");
    qualityText(item.country,sizeof(item.country),"United States");
    qualityText(item.weatherCondition,sizeof(item.weatherCondition),"Partly cloudy");
    qualityText(item.weatherTemp,sizeof(item.weatherTemp),"74");
    item.hasT0=true;item.t0Epoch=nativeEpoch+3600*(i+1);
  }
  if(!strcmp(state,"tbd"))g_launchState.items[0].hasT0=false;
  if(!strcmp(state,"liftoff"))g_launchState.items[0].t0Epoch=nativeEpoch-1;
  if(!strcmp(state,"final-minute"))g_launchState.items[0].t0Epoch=nativeEpoch+23;
  if(!strcmp(state,"long"))qualityText(g_launchState.items[0].name,sizeof(g_launchState.items[0].name),
      "International Orbital Research and Communications Pathfinder Demonstration Flight");
  g_uiPageMode=UI_PAGE_LAUNCH;
  lvglInitLaunchUi();qualityApplyTheme();lvglUpdateLaunchUi(true);
  if(!strcmp(state,"compact"))lvglSetLaunchView(1);
  lvglTickLaunchCountdown();
  if(!strncmp(state,"qr-",3))lvglOpenLaunchQr(0);
}
static void qualityTransit(const char *state) {
  g_transitConfig.configured=true;
  qualityText(g_transitConfig.station,sizeof(g_transitConfig.station),"Central Station");
  qualityText(g_transitState.stationName,sizeof(g_transitState.stationName),"Central Station");
  if(!strcmp(state,"long"))qualityText(g_transitState.stationName,sizeof(g_transitState.stationName),
      "International Central Station and Airport West");
  qualityText(g_transitState.fetchedAt,sizeof(g_transitState.fetchedAt),"12:00");
  g_transitState.valid=true;g_transitState.count=4;
  tm now;getLocalTime(&now);
  const int offsets[]={0,2,26,95};
  const char*lines[]={"S30","BUS 12","Elizabeth","IC 3"};
  const char*destinations[]={"International Airport","University Research Campus","West End and Riverside","Harbour Terminal"};
  for(unsigned i=0;i<4;++i){
    auto &d=g_transitState.departures[i];
    qualityText(d.line,sizeof(d.line),lines[i]);qualityText(d.destination,sizeof(d.destination),destinations[i]);
    qualityText(d.category,sizeof(d.category),i==1?"BUS":"RAIL");
    qualityText(d.platform,sizeof(d.platform),i==1?"":"12");
    const int departure=(now.tm_hour*60+now.tm_min+offsets[i])%1440;
    d.depHour=departure/60;d.depMinute=departure%60;d.arrHour=(d.depHour+1)%24;d.arrMinute=d.depMinute;
    d.hasArr=true;d.realTime=true;d.valid=true;d.hasDelay=true;d.delayMin=i==2?5:0;
    d.routeColor=i==3?0xF2F5F5:0;
  }
  lvglInitTransitUi();qualityApplyTheme();lvglUpdateTransitUi(true);
}
static void qualityFeed(bool wiki,const char *state) {
  auto &content=wiki?g_wiki:g_rss;auto &deck=wiki?g_wikiDeck:g_auxDeck;
  content.valid=true;content.itemCount=3;content.lastFetchMs=nativeNow-1000;content.lastRotateMs=nativeNow;
  qualityText(content.fetchedAt,sizeof(content.fetchedAt),"23 Oct 12:00");
  auto &item=content.items[0];item.feedSlot=wiki?2:0;
  qualityText(item.title,sizeof(item.title),wiki?"A small observatory with a surprisingly long history":"New research brings cleaner energy storage closer");
  qualityText(item.summary,sizeof(item.summary),wiki?"The observatory was founded in 1891. Its instruments helped researchers map the night sky, compare distant stars and share their discoveries with visitors.":"Researchers have published a new study of reusable batteries. The work combines careful measurements, open data and a design intended to last longer in everyday use.");
  qualityText(item.link,sizeof(item.link),wiki?"https://en.wikipedia.org/wiki/Observatory":"https://example.org/science/energy-storage");
  qualityText(item.pubDate,sizeof(item.pubDate),"Fri, 23 Oct 2026 12:00:00 GMT");
  lvglInitFeedDeck(deck,wiki?g_lvglWikiRoot:g_lvglAuxRoot,wiki);
  qualityApplyTheme();lvglUpdateFeedDeck(deck,content,wiki,true);
  if(!strncmp(state,"qr-",3)){lvglSetDeckQrModalOpen(deck,true);lvglUpdateFeedDeck(deck,content,wiki,true);}
}
static void qualityBambu() {
  auto &s=g_bambuPrint;s.valid=s.connected=true;s.receivedAtMs=nativeNow;
  s.progressPercent=63;s.remainingMinutes=87;s.currentLayer=210;s.totalLayers=332;
  s.nozzleTempC=220;s.nozzleTargetC=220;s.bedTempC=55;s.bedTargetC=55;s.chamberTempC=38;
  s.speedPercent=100;s.coolingFanPercent=75;s.auxiliaryFanPercent=30;s.amsHumidityPercent=24;
  s.activeFilamentRemainingPercent=63;s.filamentChangesCompleted=5;s.filamentChangesTotal=8;s.amsTemperatureC=29;
  qualityText(s.status,sizeof(s.status),"RUNNING");qualityText(s.stage,sizeof(s.stage),"Printing outer wall");
  qualityText(s.activeFilamentLabel,sizeof(s.activeFilamentLabel),"PLA Basic");
  qualityText(s.activeFilamentColorHex,sizeof(s.activeFilamentColorHex),"#57AC89");
  qualityText(s.jobName,sizeof(s.jobName),"Desk organizer");
  lvglInitBambuUi();qualityApplyTheme();lvglUpdateBambuUi(true);
}
static void qualityHome() {
  initLvglClockPanel(g_lvglHomeRoot);initLvglWeatherPanel(g_lvglHomeRoot);qualityApplyTheme();
  lvglApplyClockSentenceAutoFit("It is a little past twelve");
  lvglSetLabelTextIfChanged(g_clockUi.date,"Friday 23 October 2026");
  lvglSetLabelTextIfChanged(g_weatherUi.city,"Example City");
  lvglSetLabelTextIfChanged(g_weatherUi.temp,"21\xC2\xB0, 64%");
  lvglSetLabelTextIfChanged(g_weatherUi.desc,"Partly cloudy");
  lvglSetLabelTextIfChanged(g_weatherUi.humidity,"Wind 12 km/h");
  lvglSetLabelTextIfChanged(g_weatherUi.sun,"07:31 / 18:22");
  lvglSetLabelTextIfChanged(g_weatherUi.forecastNow,"In 3h: clear");
  lvglShowWeatherMainIcon(2,true,"");lvglShowWeatherForecastIcon(0,true);
}
static void qualityInfo() {
  initLvglInfoPanel(lv_scr_act());qualityApplyTheme();
  // INFO data formatting is a hardware seam; no real address/SSID is read.
  lvglSetLabelTextIfChanged(g_infoUi.title,"ScryBar Info / Example");
  lvglSetLabelTextIfChanged(g_infoUi.endpoint,"192.0.2.24:8080");
  lvglSetLabelTextIfChanged(g_infoUi.bodyLeft,"wifi: OK -52dBm\nssid: ExampleNetwork\nbattery: 82%\npower: USB / EXTERNAL\nclock: SYNCED\nmac: 02:00:00:00:00:24");
  const char*url="https://example.invalid";
  lv_qrcode_update(g_infoUi.webQr,url,strlen(url));
}
static void qualityNowPlaying() {
  auto&s=g_liveNowPlaying;s.valid=s.isPlaying=s.inSync=true;s.receivedAtMs=nativeNow;s.contentToken=1;
  s.durationSec=287;s.elapsedSec=123;
  qualityText(s.title,sizeof(s.title),"A Beautiful Journey Through the Quiet City");
  qualityText(s.artist,sizeof(s.artist),"The Example Ensemble");
  qualityText(s.album,sizeof(s.album),"Sounds from the Observatory");qualityText(s.source,sizeof(s.source),"Music");
  lvglInitNowPlayingUi(g_nowPlayingUi,g_lvglNowPlayingRoot);qualityApplyTheme();
  lvglUpdateNowPlayingUi(g_nowPlayingUi,true);
}
static void qualityLanguage(const char *state) {
  if(strncmp(state,"qr-",3))return;
  const char*language=state+3;
#define LANGUAGE(code) if(!strcmp(language,#code)){nativeStrings=&kUiLang_##code;return;}
  LANGUAGE(it) LANGUAGE(en) LANGUAGE(fr) LANGUAGE(de) LANGUAGE(es) LANGUAGE(pt) LANGUAGE(la)
  LANGUAGE(eo) LANGUAGE(l33t) LANGUAGE(sha) LANGUAGE(val) LANGUAGE(bellazio) LANGUAGE(pir) LANGUAGE(tlh)
#undef LANGUAGE
}
static void qualityRole(const char*name,lv_obj_t*label) {
  if(!label||lv_obj_has_flag(label,LV_OBJ_FLAG_HIDDEN))return;
  lv_area_t area;lv_obj_get_coords(label,&area);
  const uint32_t rgb=lv_color_to32(lv_obj_get_style_text_color(label,0))&0xFFFFFF;
  const lv_font_t*font=lv_obj_get_style_text_font(label,0);
  std::printf("ROLE\t%s\t%06x\t%u\t%u\t%u\t%d\t%d\t%d\t%d\n",name,rgb,
      lv_obj_get_style_text_opa(label,0),lv_obj_get_style_opa(label,0),font?font->line_height:0,
      area.x1,area.y1,area.x2,area.y2);
}
int main(int argc,char**argv) {
  if(argc!=5)return 2;
  g_uiThemeIndex=std::atoi(argv[1]);const char*page=argv[2],*state=argv[3];qualityLanguage(state);
  lv_init();static lv_color_t pixels[640*18];static lv_disp_draw_buf_t buffer;
  lv_disp_draw_buf_init(&buffer,pixels,nullptr,640*18);
  static lv_disp_drv_t driver;lv_disp_drv_init(&driver);driver.hor_res=640;driver.ver_res=172;
  driver.draw_buf=&buffer;driver.flush_cb=nativeFlush;
  lv_disp_t*display=lv_disp_drv_register(&driver);if(!display)return 3;
  lv_obj_remove_style_all(lv_scr_act());lv_obj_set_size(lv_scr_act(),640,172);
  lv_obj_set_style_bg_opa(lv_scr_act(),LV_OPA_COVER,0);lv_obj_clear_flag(lv_scr_act(),LV_OBJ_FLAG_SCROLLABLE);
  auto root=[&](){return lvglCreatePageRoot(lv_scr_act(),640,172);};
  if(!strcmp(page,"HOME")){g_lvglHomeRoot=root();qualityHome();}
  else if(!strcmp(page,"INFO"))qualityInfo();
  else if(!strcmp(page,"RSS")){g_lvglAuxRoot=root();qualityFeed(false,state);}
  else if(!strcmp(page,"WIKI")){g_lvglWikiRoot=root();qualityFeed(true,state);}
  else if(!strcmp(page,"NOWPLAYING")){g_lvglNowPlayingRoot=root();qualityNowPlaying();}
  else if(!strcmp(page,"TRANSIT")){g_lvglTransitRoot=root();qualityTransit(state);}
  else if(!strcmp(page,"LAUNCH")){g_lvglLaunchRoot=root();qualityLaunch(state);}
  else if(!strcmp(page,"MACSTATS")){
    g_lvglMacStatsRoot=root();lvglInitMacStatsUi();
    fixture(!strcmp(state,"stale")?"stale":!strcmp(state,"waiting")?"waiting":"live");
    if(!strcmp(state,"warning")){g_macStats.cpuUsagePct=65;g_macStats.gpuUsagePct=70;g_macStats.cpuTempC=75;g_macStats.gpuTempC=80;}
    if(!strcmp(state,"critical")){g_macStats.cpuUsagePct=85;g_macStats.gpuUsagePct=92;g_macStats.cpuTempC=90;g_macStats.gpuTempC=95;}
    qualityApplyTheme();lvglUpdateMacStatsUi(true);
  }
  else if(!strcmp(page,"BAMBU")){g_lvglBambuRoot=root();qualityBambu();}
  else if(!strcmp(page,"DOOM")){
    g_doom.neutralReady=true;doomBlitTitlePicToCanvas();
    for(size_t i=0;i<640*172;++i){lv_color_t color;color.full=qualityCanvas[i];
      uint32_t rgb=lv_color_to32(color);image[i*3]=rgb>>16;image[i*3+1]=rgb>>8;image[i*3+2]=rgb;}
  }
  else return 4;
  if(strcmp(page,"DOOM")){lv_obj_update_layout(lv_scr_act());lv_refr_now(display);}
  FILE*out=std::fopen(argv[4],"wb");if(!out)return 5;
  std::fprintf(out,"P6\n640 172\n255\n");std::fwrite(image,1,sizeof(image),out);std::fclose(out);
  qualityRole("now.album",g_nowPlayingUi.album);
  qualityRole("now.headerTime",g_nowPlayingUi.headerTime);qualityRole("now.title",g_nowPlayingUi.title);
  qualityRole("now.artist",g_nowPlayingUi.artist);qualityRole("now.track",g_nowPlayingUi.track);
  qualityRole("launch.header.title",g_launchUi.title);qualityRole("launch.header.center",g_launchUi.headerCenter);
  qualityRole("launch.header.fetch",g_launchUi.fetchTime);qualityRole("launch.qr.hint",g_launchUi.qrHint);
  qualityRole("wiki.qr.hint",g_wikiDeck.qrHint);
  describe(lv_scr_act());lv_disp_remove(display);return 0;
}
