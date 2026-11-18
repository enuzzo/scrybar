// Native test with the small RTOS/WebServer stubs in ui_web_server_stubs/.
// c++ -std=c++17 -pthread -Wall -Wextra -Werror -I tests/ui_web_server_stubs \
//     tests/ui_web_server_test.cpp -o /tmp/scrybar-ui-web-test
#include "../src/ui_web_server.h"
#include <cassert>
#include <functional>
#include <iostream>

class TestServer : public UiWebServer {
 public:
  TestServer() : UiWebServer(80) {}
  ~TestServer() override { stopUiWorker(); }
  std::function<int(size_t, size_t)> policy = [](size_t length, size_t) { return int(length); };
  bool failAllocation = false;
  size_t writes = 0;
  std::atomic<unsigned> uiSocketWrites{0};
  std::thread::id uiThread = std::this_thread::get_id();
  void headerOnly() {
    static constexpr char header[] = "HTTP/1.1 200 OK\r\nContent-Length: 999\r\n\r\n";
    _currentClientWrite(header, sizeof(header)-1);
  }
 protected:
  int writeSocketSome(const uint8_t *bytes, size_t length) override {
    if (std::this_thread::get_id() == uiThread) ++uiSocketWrites;
    const int result=policy(length,writes++);
    if (result < 0) { errno=result == -2 ? ECONNRESET : EAGAIN; return -1; }
    if (result > 0 && size_t(result) <= length) {
      auto reply=_currentClient.reply;
      assert(reply);
      std::lock_guard<std::mutex> lock(reply->mutex);
      reply->bytes.append(reinterpret_cast<const char *>(bytes),size_t(result));
    }
    return result;
  }
  void *resizeResponse(void *buffer, size_t capacity) override {
    return failAllocation ? nullptr : UiWebServer::resizeResponse(buffer,capacity);
  }
};

static TestServer *server;
static unsigned calls=0;
static UiWebServer::WriteFailure lastFailure{};
static void echo() {
  assert(std::this_thread::get_id()==server->uiThread);
  ++calls;
  assert(!server->serviceUiRequest(false)); // no recursive claim of the running callback
  server->sendHeader("X-Test","buffered");
  server->send(200,"application/octet-stream",server->arg("plain"));
}
static void flashResponse() {
  ++calls;
  static constexpr char binary[]={'A','\0','B','\r','\n'};
  server->send_P(200,"application/octet-stream",binary,sizeof(binary));
}
static void overflow() { ++calls; server->send(200,"text/plain",String(UiWebServer::responseCapacityLimit+1,'x')); }
static void chunked() { ++calls; server->setContentLength(CONTENT_LENGTH_UNKNOWN); server->send(200,"text/plain","unsupported"); }
static void raw() { ++calls; server->client().write("forbidden",9); server->send(200,"text/plain","must fail"); }
static void incomplete() { ++calls; server->headerOnly(); }
static void slow() {
  ++calls;
  std::this_thread::sleep_for(std::chrono::milliseconds(1250));
  server->send(200,"text/plain","claimed callback completes");
}
static void missing() { ++calls; server->send(404,"text/plain","missing"); }

static void waitFor(const std::function<bool()> &done, bool motion=false, bool service=true, unsigned timeout=4500) {
  const auto start=std::chrono::steady_clock::now();
  while (!done()) {
    if (service) server->serviceUiRequest(motion);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(std::chrono::steady_clock::now()-start<std::chrono::milliseconds(timeout));
  }
}
static String bytes(const std::shared_ptr<UiWebTest::Reply> &reply) {
  assert(reply->complete.load()); std::lock_guard<std::mutex> lock(reply->mutex); return reply->bytes;
}
static String body(const String &response) {
  const size_t split=response.find("\r\n\r\n"); assert(split!=String::npos); return response.substr(split+4);
}
static std::shared_ptr<UiWebTest::Reply> request(const char *path,const String &payload="",bool motion=false) {
  auto reply=server->enqueue(path,payload);
  waitFor([&] {return reply->complete.load();},motion);
  assert(reply->directWrites==0); assert(server->uiSocketWrites==0);
  return reply;
}

int main() {
  {
    TestServer instance;server=&instance;
    server->onUi("/echo",HTTP_POST,echo);
    server->onUi("/fast",HTTP_POST,echo,true);
    server->onHttp("/network",HTTP_GET,[] {
      assert(std::this_thread::get_id()!=server->uiThread);
      server->send(200,"text/plain",server->arg("plain"));
    });
    server->onHttp("/sample",HTTP_GET,[] {
      server->send_P(200,"text/plain","preview",7);
    });
    server->onUi("/flash",HTTP_GET,flashResponse);
    server->onUi("/overflow",HTTP_GET,overflow);
    server->onUi("/chunked",HTTP_GET,chunked);
    server->onUi("/raw",HTTP_GET,raw);
    server->onUi("/incomplete",HTTP_GET,incomplete);
    server->onUi("/slow",HTTP_GET,slow);
    server->onUiNotFound(missing);
    assert(server->startUiWorker());
    assert(server->startUiWorker()); // no duplicate task or semaphores
    assert(server->workerStackHighWater()==1732); // bytes, not StackType_t words

    auto direct=server->enqueue("/network","worker response");
    waitFor([&] {return direct->complete.load();},false,false);
    assert(body(bytes(direct))=="worker response");
    assert(direct->directWrites==0 && server->diagnostics().queued==0);

    auto pending=server->enqueue("/echo","kept request arguments");
    waitFor([] {return server->uiRequestPending();},false,false);
    const unsigned before=calls;
    assert(!server->serviceUiRequest(true)); assert(calls==before);
    assert(server->serviceUiRequest(false));
    waitFor([&] {return pending->complete.load();});
    assert(body(bytes(pending))=="kept request arguments"); assert(pending->directWrites==0);

    assert(body(bytes(request("/fast","during held finger",true)))=="during held finger");
    auto expired=server->enqueue("/echo","unclaimed while moving");
    const unsigned prior=calls;
    waitFor([&] {return expired->complete.load();},true);
    assert(calls==prior); assert(bytes(expired).find("503 Service Unavailable")!=String::npos);
    assert(bytes(expired).find("Retry-After: 1\r\n")!=String::npos);
    assert(body(bytes(expired))=="Service unavailable\n");
    assert(!server->serviceUiRequest(false)); assert(calls==prior);

    server->writes=0;
    server->policy=[](size_t length,size_t call) { const int chunks[]={3,0,-1,17,1,0}; return call<6?std::min(int(length),chunks[call]):int(length); };
    String payload(47754,'\0');for(size_t i=0;i<payload.size();++i)payload[i]=char((i*31)%256);
    assert(body(bytes(request("/fast",payload,true)))==payload);
    assert(server->diagnostics().responseBytesMax>=payload.size());
    server->policy=[](size_t length,size_t) { return int(length); };
    const String flash("A\0B\r\n",5);
    assert(body(bytes(request("/flash")))==flash);

    server->failAllocation=true;
    // Force growth beyond the existing 64 KiB buffer to exercise realloc failure.
    auto allocation=request("/fast",String(100000,'a'),true);
    assert(bytes(allocation).find("503 Service Unavailable")!=String::npos);
    assert(server->diagnostics().allocationFailures==1);
    server->failAllocation=false;
    assert(body(bytes(request("/fast","recovered",true)))=="recovered");
    for (const char *path : {"/overflow","/chunked","/raw","/incomplete"}) {
      auto rejected=request(path);
      assert(bytes(rejected).find("503 Service Unavailable")!=String::npos);
      assert(bytes(rejected).find("200 OK")==String::npos);
    }

    // Once claimed, even a callback longer than the pending timeout owns its
    // arguments until completion; it must not become a stale 503 race.
    assert(body(bytes(request("/slow")))=="claimed callback completes");
    assert(server->diagnostics().slowCallbacks>=1);
    assert(body(bytes(request("/not-found")))=="missing");

    std::vector<std::shared_ptr<UiWebTest::Reply>> queued;
    for(unsigned i=0;i<60;++i)queued.push_back(server->enqueue("/fast",std::to_string(i)));
    waitFor([&] {return queued.back()->complete.load();},true);
    for(unsigned i=0;i<queued.size();++i)assert(body(bytes(queued[i]))==std::to_string(i));

    server->onWriteFailure([](const UiWebServer::WriteFailure &failure) { lastFailure = failure; });
    // A direct flash response sends the HTTP header and the body in separate calls.
    server->writes=0;
    uint32_t headerTime=0;
    server->policy=[&](size_t length,size_t call) {
      if (call==0) headerTime=millis();
      else if (millis()-headerTime<UiWebServer::writeStallTimeoutMs+120) return -1;
      return int(length);
    };
    assert(body(bytes(request("/sample")))=="preview");
    assert(server->diagnostics().recoveredStalls==1);
    assert(server->diagnostics().longestRecoveryMs>=UiWebServer::writeStallTimeoutMs);
    const auto failures=server->diagnostics().writeFailures;
    server->policy=[](size_t,size_t) {return -1;};
    auto stalled=request("/fast","never claims transport success",true);
    assert(bytes(stalled).empty()); assert(stalled->stopped);
    assert(server->diagnostics().writeFailures==failures+1);
    assert(String(lastFailure.reason)=="stall" && lastFailure.socketError==EAGAIN);
    assert(lastFailure.stalledMs>=UiWebServer::writeStallTimeoutMs);
    server->writes=0;
    server->policy=[](size_t length,size_t call) {return call==0?int(length):-2;};
    auto disconnected=request("/fast",String(3000,'a'),true);
    assert(bytes(disconnected).size()==1460); assert(disconnected->stopped);
    assert(String(lastFailure.reason)=="socket" && String(lastFailure.phase)=="buffered");
    assert(lastFailure.sent==1460 && lastFailure.expected>3000);
    assert(lastFailure.socketError==ECONNRESET);
    assert(server->diagnostics().writeFailures==failures+2);
    assert(server->uiSocketWrites==0);
    assert(server->diagnostics().busy==1);
    assert(server->diagnostics().responseFailures==5);
    server->writes=0;
    server->policy=[](size_t length,size_t call) {return call==0?int(length):-1;};
    auto timedOut=server->enqueue("/sample");
    waitFor([&] {return timedOut->complete.load();},false,false,UiWebServer::committedStallTimeoutMs+1500);
    assert(timedOut->stopped && String(lastFailure.reason)=="stall");
    assert(String(lastFailure.phase)=="flash" && lastFailure.sent==0 && lastFailure.expected==7);
    assert(lastFailure.stalledMs>=UiWebServer::committedStallTimeoutMs);
    const auto beforeHeaderFailure=server->diagnostics().writeFailures;
    server->policy=[](size_t,size_t) {return -2;};
    auto failedHeader=request("/sample");
    assert(failedHeader->stopped && bytes(failedHeader).empty());
    assert(server->diagnostics().writeFailures==beforeHeaderFailure+1);
    assert(String(lastFailure.phase)=="direct" && lastFailure.socketError==ECONNRESET);
    server->stopUiWorker();assert(!server->uiWorkerReady());
  }
  UiWebTest::joinTasks();
  {
    TestServer instance;server=&instance;server->onUi("/echo",HTTP_POST,echo);
    UiWebTest::failTaskCreation=true;assert(!server->startUiWorker());
    UiWebTest::failTaskCreation=false;
    auto legacy=server->enqueue("/echo","legacy fallback");server->handleClient();
    assert(body(bytes(legacy))=="legacy fallback");assert(legacy->directWrites>0);
  }
  UiWebTest::joinTasks();
  std::cout<<"UiWebServer ownership/concurrency/deadline/partial-write/PSRAM-cap/response-integrity/fallback PASS\n";
}
