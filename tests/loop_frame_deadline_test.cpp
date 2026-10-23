// The runner includes the current firmware's exact loop(), not a copied branch.
#include "../src/display_performance.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#define TEST_DISPLAY 1
#define TEST_NTP 1
#define TEST_LVGL_UI 1
#define DISPLAY_BACKEND_ESP_LCD 1
#define TEST_WIFI 1
#define TEST_BATTERY 1
#define TEST_IMU 1
#define RSS_ENABLED 1
#define SCREENSAVER_ENABLED 1
#define HEARTBEAT_INTERVAL_MS 5000
#define SUMMARY_INTERVAL_MS 30000
struct Transition { bool active=false,pending=false; DisplayPerformance::AbsoluteFramePacer pacer; } g_pageTransition;
struct { bool ntpSynced=true; } g_clock;
struct { uint32_t lastNtpAttemptMs=0; } g_wifiSt;
struct { template<class... T> void printf(const char*,T...) {} } Serial;
struct { unsigned getFreeHeap(){return 0;} unsigned getFreePsram(){return 0;} } ESP;
struct lv_mem_monitor_t { unsigned free_size=0,used_pct=0,frag_pct=0; };
void lv_mem_monitor(lv_mem_monitor_t*) {}
constexpr int UI_PAGE_HOME=0,UI_PAGE_LAUNCH=7;
int g_uiPageMode=UI_PAGE_HOME;
bool g_uiNeedsRedraw=false;
uint32_t clockUs=0,waitedUs=0,waitCalls=0,presented=0;
uint32_t millis(){return clockUs/1000;}
uint32_t micros(){return clockUs;}
void delayMicroseconds(uint32_t us){clockUs+=us;waitedUs+=us;++waitCalls;}
using Stage=DisplayPerformance::MotionWorkTimings::Stage;
struct MotionWorkProbe { explicit MotionWorkProbe(Stage) {} };
enum Call { power,nav,serial,wifi,setup,http,battery,energy,ntp,imu,weather,transit,launch,rss,wiki,meta,visible,touch,benchmark,saver,model,display,doom,count };
unsigned calls[count]={};
void handlePowerButtonLoop(uint32_t){++calls[power];}
void handleNavFirstButtonLoop(uint32_t){++calls[nav];}
void pollSerialCommands(){++calls[serial];}
void handleWiFiReconnectLoop(uint32_t){++calls[wifi];}
void wifiHandleSetupModeLoop(uint32_t){++calls[setup];}
void handleWebConfigServerLoop(){++calls[http];}
void sampleBatteryNow(uint32_t,bool){++calls[battery];}
void applyEnergyPolicy(uint32_t,bool){++calls[energy];}
bool wifiIsConnectedNow(){return false;}
void runNtpTimeTest(){++calls[ntp];}
void printRuntimeSummary(uint32_t){}
void syncImuActiveForUi(){}
void runImuLoop(){++calls[imu];}
void updateWeatherFromApi(bool){++calls[weather];}
void updateTransitFromApi(bool){++calls[transit];}
void updateLaunchFromApi(bool){++calls[launch];}
void updateRssFromFeed(bool){++calls[rss];}
void updateWikiFromFeed(bool){++calls[wiki];}
void wikiPreloadMetaStep(){++calls[meta];}
void wikiPreloadVisibleItemStep(){++calls[visible];}
void handleTouchSwipeInput(){++calls[touch];clockUs+=1000;}
void runPerformanceBenchmark(uint32_t){++calls[benchmark];}
void lvglTickLaunchCountdown(){}
void handleScreenSaverLoop(uint32_t){++calls[saver];}
void updateLvglUi(bool){++calls[model];}
void doomRenderSpike(bool){++calls[doom];}
void runLvglLoop(){
 ++calls[display];
 if(g_pageTransition.active&&g_pageTransition.pacer.due(clockUs)) {
  ++presented;g_pageTransition.pacer.advance(clockUs,clockUs+13000);clockUs+=13000;
 }
}
#include "current_firmware_loop.inc"
void reset(uint32_t now,bool active,bool pending=false){
 std::memset(calls,0,sizeof(calls));clockUs=now;waitedUs=waitCalls=presented=0;
 g_pageTransition.active=active;g_pageTransition.pending=pending;g_pageTransition.pacer.reset(16667);
}
void assertReserved(){
 assert(calls[display]==1&&presented==1);
 for(unsigned i=0;i<count;++i) if(i!=display) assert(calls[i]==0);
}
int main(){
 reset(14667,true);loop();assertReserved();assert(waitCalls==1&&waitedUs==2000);
 reset(14666,true);loop();assert(waitCalls==0&&calls[touch]==1&&calls[http]==1);
 assert(calls[wifi]==0&&calls[battery]==0&&calls[weather]==0);
 reset(16670,true);loop();assertReserved();assert(waitCalls==0);
 reset(UINT32_MAX-999,true);g_pageTransition.pacer.reset(120);loop();assertReserved();assert(waitCalls==1&&waitedUs==1120);
 reset(0,false,true);loop();assert(waitCalls==0&&calls[touch]==1&&calls[model]==1&&calls[display]==1);
 assert(calls[wifi]==0&&calls[battery]==0&&calls[weather]==0);
 reset(0,false);loop();assert(waitCalls==0&&calls[touch]==1&&calls[model]==1&&calls[display]==1);
 assert(calls[wifi]==1&&calls[setup]==1&&calls[http]==1&&calls[battery]==1&&calls[energy]==1);
 assert(calls[weather]==1&&calls[transit]==1&&calls[launch]==1&&calls[rss]==1&&calls[wiki]==1);
}
