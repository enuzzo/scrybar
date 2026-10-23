#pragma once
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

using PGM_P = const char *;
using Uri = std::string;
using String = std::string;
enum HTTPMethod { HTTP_GET, HTTP_POST };
constexpr size_t CONTENT_LENGTH_UNKNOWN = size_t(-1);
namespace UiWebTest {
struct Reply {
  std::mutex mutex;
  std::string bytes;
  std::atomic<bool> complete{false};
  std::atomic<unsigned> directWrites{0};
  bool stopped = false;
};
}
class NetworkClient {
 public:
  std::shared_ptr<UiWebTest::Reply> reply;
  NetworkClient() = default;
  explicit NetworkClient(std::shared_ptr<UiWebTest::Reply> output) : reply(std::move(output)) {}
  int fd() const { return reply ? 42 : -1; }
  void stop() { if (reply) reply->stopped = true; }
  size_t write(const char *bytes, size_t length) {
    if (!reply) return 0;
    ++reply->directWrites;
    std::lock_guard<std::mutex> lock(reply->mutex); reply->bytes.append(bytes,length); return length;
  }
  size_t write_P(const char *bytes, size_t length) { return write(bytes,length); }
};
class WebServer {
 public:
  using THandlerFunction = std::function<void()>;
  explicit WebServer(int) {}
  virtual ~WebServer() = default;
  virtual NetworkClient &client() { return _currentClient; }
  virtual void handleClient() {
    Request request;
    { std::lock_guard<std::mutex> lock(requestMutex_); if (requests_.empty()) return;
      request=std::move(requests_.front());requests_.pop_front(); }
    _currentClient=NetworkClient(request.reply); body_=request.body; _chunked=false; headers_.clear();
    auto route=routes_.find(request.uri);
    if (route!=routes_.end()) route->second(); else if (notFound_) notFound_();
    request.reply->complete=true;
  }
  void on(const Uri &uri,HTTPMethod,THandlerFunction callback) { routes_[uri]=std::move(callback); }
  void onNotFound(THandlerFunction callback) { notFound_=std::move(callback); }
  std::shared_ptr<UiWebTest::Reply> enqueue(const Uri &uri,const String &body="") {
    auto reply=std::make_shared<UiWebTest::Reply>(); std::lock_guard<std::mutex> lock(requestMutex_);
    requests_.push_back({uri,body,reply}); return reply;
  }
  String arg(const char *) { return body_; }
  void sendHeader(const String &key,const String &value) { headers_+=key+": "+value+"\r\n"; }
  void setContentLength(size_t length) { _chunked=length==CONTENT_LENGTH_UNKNOWN; }
  void send(int code,const char *type,const String &body) {
    String header="HTTP/1.1 "+std::to_string(code)+" OK\r\nContent-Type: "+type+"\r\n";
    header+=_chunked?"Transfer-Encoding: chunked\r\n":"Content-Length: "+std::to_string(body.size())+"\r\n";
    header+=headers_+"Connection: close\r\n\r\n";
    _currentClientWrite(header.c_str(),header.size());
    if (!body.empty()) _currentClientWrite(body.data(),body.size());
    if (_chunked) _currentClient.write("\r\n",2); // actual LVGL adapter must quarantine this bypass
  }
  void send_P(int code,PGM_P type,PGM_P body,size_t length) {
    String header="HTTP/1.1 "+std::to_string(code)+" OK\r\nContent-Type: "+type+"\r\nContent-Length: "+std::to_string(length)+"\r\n\r\n";
    _currentClientWrite(header.data(),header.size()); _currentClientWrite_P(body,length);
  }
 protected:
  virtual size_t _currentClientWrite(const char *bytes,size_t length) { return _currentClient.write(bytes,length); }
  virtual size_t _currentClientWrite_P(PGM_P bytes,size_t length) { return _currentClient.write_P(bytes,length); }
  NetworkClient _currentClient;
  bool _chunked=false;
 private:
  struct Request { Uri uri;String body;std::shared_ptr<UiWebTest::Reply> reply; };
  std::mutex requestMutex_;
  std::deque<Request> requests_;
  std::map<Uri,THandlerFunction> routes_;
  THandlerFunction notFound_;
  String body_,headers_;
};
