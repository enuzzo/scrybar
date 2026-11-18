#!/usr/bin/env python3
"""Exercise the actual sketch's Wi-Fi events, deadline and fallback offline."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from render_mac_stats_native import ROOT, function


class WiFiAssociationTest(unittest.TestCase):
    def test_retry_and_rotation(self):
        source = (ROOT / "scrybar.ino").read_text()
        config = (ROOT / "config.h").read_text()
        settings = ("WIFI_CONNECT_TIMEOUT_MS", "WIFI_RETRY_STEP_DELAY_MS",
                    "WIFI_RETRY_FAILS_BEFORE_RADIO_RESET")
        macros = []
        for key in settings:
            match = re.search(r"^#define " + key + r"\s+(\d+[uUlL]*)\b", config + "\n" + source, re.M)
            if not match:
                match = re.search(r"\b" + key + r"\s*=\s*(\d+[uUlL]*)\s*;", source)
            self.assertIsNotNone(match, key)
            macros.append(f"#define {key} {match[1]}")
        cpp = """
#include <cassert>
#include <cstdint>
#include <string>
struct String : std::string {
  using std::string::string;
  bool equals(const char *s) const { return *this == s; }
};
struct Address { String toString() const { return "192.0.2.1"; } };
enum WiFiEvent_t { ARDUINO_EVENT_WIFI_STA_CONNECTED, ARDUINO_EVENT_WIFI_STA_GOT_IP,
                   ARDUINO_EVENT_WIFI_STA_DISCONNECTED };
enum wifi_err_reason_t { WIFI_REASON_ASSOC_EXPIRE=4, WIFI_REASON_ASSOC_LEAVE=8, WIFI_REASON_NO_AP_FOUND=201 };
struct WiFiEventInfo_t { struct { int reason; } wifi_sta_disconnected; };
struct { template <class... Args> void printf(const char*, Args...) {}
         void println(const char*) {} } Serial;
static uint32_t now=100;
static uint32_t millis(){return now;}
static void delay(unsigned duration){now+=duration;}
struct {
  unsigned disconnects=0;
  void disconnect(bool,bool){++disconnects;}
  Address localIP() const {return {};}
  Address dnsIP(int) const {return {};}
  String SSID() const {return "fixture-main";}
  int status() const {return 0;}
  const char *disconnectReasonName(wifi_err_reason_t) const {return "fixture";}
} WiFi;
struct State {
  const char *credSsids[2]={"fixture-main","fixture-spare"};
  uint8_t credCount=2,reconnectIdx=0,consecutiveFailCount=0;
  bool reconnectAttemptActive=true,connected=false,everConnected=false,internalDisconnect=false;
  uint32_t reconnectNextAttemptMs=0,reconnectAttemptStartMs=100,lastConnectMs=0,lastDisconnectMs=0,noLinkSinceMs=0;
  int lastDiscReason=-1;
};
static State g_wifiSt;
static int started=-1;
static void wifiBeginAttempt(uint8_t idx){started=idx;g_wifiSt.reconnectAttemptActive=true;g_wifiSt.reconnectAttemptStartMs=millis();}
static bool wifiIsConnectedNow(){return g_wifiSt.connected;}
static const char* wlStatusToStr(int){return "fixture";}
static bool applyWiFiDnsOverrideIfEnabled(bool){return true;}
static void wifiRearmStationRadio(uint32_t,const char*){assert(false && "unexpected radio reset");}
"""
        names = ("wifiScheduleNextAttempt", "wifiRotateCredentialIndex", "wifiHandleFailure",
                 "onWiFiEvent", "handleWiFiReconnectLoop")
        cpp += "\n".join(macros) + "\n" + "\n".join(function(source, name) for name in names)
        cpp += """
static void loss(int reason){onWiFiEvent(ARDUINO_EVENT_WIFI_STA_DISCONNECTED,{{reason}});}
static void reset(){g_wifiSt=State{};now=100;started=-1;WiFi.disconnects=0;}
int main(){
  reset();
  loss(WIFI_REASON_ASSOC_EXPIRE);now+=50;loss(WIFI_REASON_ASSOC_LEAVE);
  assert(g_wifiSt.reconnectAttemptActive && g_wifiSt.reconnectAttemptStartMs==100);
  assert(g_wifiSt.reconnectIdx==0 && g_wifiSt.consecutiveFailCount==0);
  onWiFiEvent(ARDUINO_EVENT_WIFI_STA_CONNECTED,{});
  assert(!g_wifiSt.connected && g_wifiSt.reconnectAttemptActive);
  onWiFiEvent(ARDUINO_EVENT_WIFI_STA_GOT_IP,{});
  assert(g_wifiSt.connected && g_wifiSt.everConnected && !g_wifiSt.reconnectAttemptActive);
  assert(g_wifiSt.reconnectIdx==0);

  reset();
  loss(WIFI_REASON_ASSOC_EXPIRE);
  now=100+WIFI_CONNECT_TIMEOUT_MS-1;loss(WIFI_REASON_ASSOC_LEAVE);
  handleWiFiReconnectLoop(now);
  assert(g_wifiSt.reconnectAttemptActive && WiFi.disconnects==0);
  ++now;const auto deadline=now;handleWiFiReconnectLoop(now);
  assert(!g_wifiSt.reconnectAttemptActive && WiFi.disconnects==1);
  assert(g_wifiSt.reconnectIdx==1 && g_wifiSt.consecutiveFailCount==1);
  now=deadline+WIFI_RETRY_STEP_DELAY_MS-1;handleWiFiReconnectLoop(now);assert(started==-1);
  ++now;handleWiFiReconnectLoop(now);assert(started==1);

  reset();loss(WIFI_REASON_NO_AP_FOUND);
  assert(!g_wifiSt.reconnectAttemptActive && g_wifiSt.reconnectIdx==1 && g_wifiSt.consecutiveFailCount==1);
  reset();g_wifiSt.reconnectAttemptActive=false;g_wifiSt.connected=true;loss(WIFI_REASON_ASSOC_LEAVE);
  assert(!g_wifiSt.connected && g_wifiSt.reconnectIdx==0 && g_wifiSt.consecutiveFailCount==0);
  assert(g_wifiSt.reconnectNextAttemptMs==now+WIFI_RETRY_STEP_DELAY_MS);
  reset();g_wifiSt.internalDisconnect=true;loss(WIFI_REASON_NO_AP_FOUND);
  assert(g_wifiSt.reconnectIdx==0 && !g_wifiSt.reconnectAttemptActive);
}
"""
        with tempfile.TemporaryDirectory() as directory:
            file = Path(directory) / "test.cpp"
            file.write_text(cpp)
            binary = file.with_suffix("")
            subprocess.run(["c++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                            str(file), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
