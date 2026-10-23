#pragma once

#include <WebServer.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <lwip/sockets.h>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>

// One HTTP owner parses and sends on its worker. Registered UI callbacks run
// only from serviceUiRequest(), while that worker waits. This retains the
// existing WebServer arguments and LVGL's single-thread ownership.
//
// Supported responses: exactly one known-length send()/send_P() response with
// optional sendHeader(). Chunking, streaming, raw client access, upload/raw
// callbacks and SSE are unsupported. Register every route through onUi(),
// including the not-found handler. Register routes and begin() before starting
// the worker; do not mutate routes/listener concurrently with worker parsing.
// DNS/AP lifecycle stays on the UI thread. Never recursively service a request
// from a UI callback. Callback code must itself avoid blocking I/O during motion.
class UiWebServer : public WebServer {
 public:
  using UiCallback = void (*)();
  static constexpr size_t responseCapacityLimit = 256U * 1024U;
  static constexpr uint32_t pendingTimeoutMs = 1200;
  static constexpr uint32_t writeStallTimeoutMs = 2000;
  struct Diagnostics {
    uint32_t queued = 0, completed = 0, busy = 0;
    uint32_t allocationFailures = 0, responseFailures = 0, writeFailures = 0;
    uint32_t responseBytesMax = 0, callbackMaxUs = 0, slowCallbacks = 0;
    uint32_t workerStackFreeBytes = 0;
  };

  explicit UiWebServer(uint16_t port = 80) : WebServer(port) {}
  ~UiWebServer() override {
    stopUiWorker();
    if (response_) heap_caps_free(response_);
  }
  UiWebServer(const UiWebServer &) = delete;
  UiWebServer &operator=(const UiWebServer &) = delete;

  void onUi(const Uri &uri, HTTPMethod method, UiCallback callback,
            bool fastDuringMotion = false) {
    WebServer::on(uri, method, [this, callback, fastDuringMotion]() {
      dispatch(callback, fastDuringMotion);
    });
  }
  void onUiNotFound(UiCallback callback) {
    WebServer::onNotFound([this, callback]() { dispatch(callback, false); });
  }

  bool startUiWorker(uint8_t core = 0) {
    if (workerReady_.load(std::memory_order_acquire)) return true;
    callbackDone_ = xSemaphoreCreateBinary();
    workerExited_ = xSemaphoreCreateBinary();
    if (!callbackDone_ || !workerExited_) { releaseSemaphores(); return false; }
    stopping_.store(false, std::memory_order_release);
    state_.store(State::idle, std::memory_order_release);
    // Set before task creation: a runnable worker must never use legacy dispatch.
    workerReady_.store(true, std::memory_order_release);
    if (xTaskCreatePinnedToCore(workerEntry, "ui_http", 4096, this, 1,
                                &workerTask_, core) != pdPASS) {
      workerReady_.store(false, std::memory_order_release);
      workerTask_ = nullptr;
      releaseSemaphores();
      return false;
    }
    return true;
  }

  // Call only on the UI owner, outside a callback. Joining may await the base
  // parser's timeout; normal firmware keeps this static worker for its lifetime.
  void stopUiWorker() {
    if (!workerReady_.load(std::memory_order_acquire)) return;
    stopping_.store(true, std::memory_order_release);
    xSemaphoreGive(callbackDone_);
    xSemaphoreTake(workerExited_, portMAX_DELAY);
    workerReady_.store(false, std::memory_order_release);
    workerTask_ = nullptr;
    releaseSemaphores();
  }
  bool uiWorkerReady() const { return workerReady_.load(std::memory_order_acquire); }
  bool uiRequestPending() const { return state_.load(std::memory_order_acquire) == State::pending; }

  bool serviceUiRequest(bool motionActive) {
    if (state_.load(std::memory_order_acquire) != State::pending) return false;
    State expected = State::pending;
    if (!state_.compare_exchange_strong(expected, State::running,
                                        std::memory_order_acq_rel)) return false;
    // Claim before reading non-atomic callback metadata: the worker may cancel
    // an unclaimed request at its deadline, then publish the next request.
    if (motionActive && !fastDuringMotion_) {
      state_.store(State::pending, std::memory_order_release);
      return false;
    }
    responseSize_ = 0;
    responseFailed_ = false;
    capturing_ = true;
    // Quarantine the socket during the callback. Unsupported WebServer methods
    // that bypass its virtual write hooks cannot perform network I/O on the UI.
    NetworkClient socket = _currentClient;
    _currentClient = NetworkClient();
    const uint32_t started = micros();
    if (callback_) callback_();
    const uint32_t elapsed = micros() - started;
    _currentClient = socket;
    capturing_ = false;
    recordMax(callbackMaxUs_, elapsed);
    if (elapsed > 2000) slowCallbacks_.fetch_add(1, std::memory_order_relaxed);
    recordMax(responseBytesMax_, static_cast<uint32_t>(responseSize_));
    completed_.fetch_add(1, std::memory_order_relaxed);
    state_.store(State::complete, std::memory_order_release);
    xSemaphoreGive(callbackDone_);
    return true;
  }

  uint32_t workerStackHighWater() const {
    // ESP-IDF reports bytes here, unlike vanilla FreeRTOS ports.
    return workerTask_ ? static_cast<uint32_t>(uxTaskGetStackHighWaterMark(workerTask_)) : 0;
  }
  Diagnostics diagnostics() const {
    return {queued_.load(), completed_.load(), busy_.load(), allocationFailures_.load(),
            responseFailures_.load(), writeFailures_.load(), responseBytesMax_.load(),
            callbackMaxUs_.load(), slowCallbacks_.load(), workerStackHighWater()};
  }
  NetworkClient &client() override {
    if (capturing_) responseFailed_ = true;  // raw access is outside the contract
    return WebServer::client();
  }

 protected:
  size_t _currentClientWrite(const char *bytes, size_t length) override {
    if (capturing_) return capture(bytes, length);
    if (!uiWorkerReady()) return WebServer::_currentClientWrite(bytes, length);
    return writeAll(reinterpret_cast<const uint8_t *>(bytes), length);
  }
  size_t _currentClientWrite_P(PGM_P bytes, size_t length) override {
    if (capturing_) return capture(bytes, length);  // ESP32 flash is memory mapped
    if (!uiWorkerReady()) return WebServer::_currentClientWrite_P(bytes, length);
    return writeAll(reinterpret_cast<const uint8_t *>(bytes), length);
  }
  // Narrow test seam, and a deliberate bypass of NetworkClient::write(): its
  // ten 1-second select retries can otherwise stall this server for ten seconds.
  virtual int writeSocketSome(const uint8_t *bytes, size_t length) {
    return ::send(_currentClient.fd(), bytes, length, MSG_DONTWAIT);
  }
  virtual void *resizeResponse(void *buffer, size_t capacity) {
    return heap_caps_realloc(buffer, capacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }

 private:
  enum class State : uint8_t { idle, pending, running, complete };
  std::atomic<State> state_{State::idle};
  std::atomic<bool> workerReady_{false}, stopping_{false};
  TaskHandle_t workerTask_ = nullptr;
  SemaphoreHandle_t callbackDone_ = nullptr, workerExited_ = nullptr;
  UiCallback callback_ = nullptr;
  bool fastDuringMotion_ = false, capturing_ = false, responseFailed_ = false;
  uint8_t *response_ = nullptr;
  size_t responseSize_ = 0, responseCapacity_ = 0;
  std::atomic<uint32_t> queued_{0}, completed_{0}, busy_{0};
  std::atomic<uint32_t> allocationFailures_{0}, responseFailures_{0}, writeFailures_{0};
  std::atomic<uint32_t> responseBytesMax_{0}, callbackMaxUs_{0}, slowCallbacks_{0};

  static void recordMax(std::atomic<uint32_t> &target, uint32_t value) {
    uint32_t previous = target.load(std::memory_order_relaxed);
    while (previous < value && !target.compare_exchange_weak(previous, value, std::memory_order_relaxed)) {}
  }
  void releaseSemaphores() {
    if (callbackDone_) vSemaphoreDelete(callbackDone_);
    if (workerExited_) vSemaphoreDelete(workerExited_);
    callbackDone_ = workerExited_ = nullptr;
  }
  static void workerEntry(void *argument) {
    auto *self = static_cast<UiWebServer *>(argument);
    while (!self->stopping_.load(std::memory_order_acquire)) {
      self->WebServer::handleClient();
      vTaskDelay(pdMS_TO_TICKS(1));
    }
    xSemaphoreGive(self->workerExited_);
    vTaskDelete(nullptr);
  }

  void dispatch(UiCallback callback, bool fastDuringMotion) {
    if (!uiWorkerReady()) { if (callback) callback(); return; }
    while (xSemaphoreTake(callbackDone_, 0) == pdTRUE) {}  // discard an old completion token
    callback_ = callback;
    fastDuringMotion_ = fastDuringMotion;
    const uint32_t queuedAt = millis();
    queued_.fetch_add(1, std::memory_order_relaxed);
    state_.store(State::pending, std::memory_order_release);
    for (;;) {
      State current = state_.load(std::memory_order_acquire);
      if (current == State::complete) break;
      if (current == State::pending &&
          (stopping_.load(std::memory_order_acquire) || millis() - queuedAt >= pendingTimeoutMs)) {
        State expected = State::pending;
        if (state_.compare_exchange_strong(expected, State::idle, std::memory_order_acq_rel)) {
          busy_.fetch_add(1, std::memory_order_relaxed);
          unavailable();
          return;
        }
      }
      // A claimed callback must finish before the parser may reuse its args or
      // socket. Only unclaimed requests may time out and become a 503 response.
      xSemaphoreTake(callbackDone_, pdMS_TO_TICKS(10));
    }
    if (responseFailed_ || !validResponse()) {
      responseFailures_.fetch_add(1, std::memory_order_relaxed);
      unavailable();
    } else {
      (void)writeAll(response_, responseSize_);
    }
    state_.store(State::idle, std::memory_order_release);
  }

  size_t capture(const char *bytes, size_t length) {
    if (responseFailed_) return 0;
    if (_chunked || length > responseCapacityLimit - responseSize_) {
      responseFailed_ = true;
      return 0;
    }
    const size_t needed = responseSize_ + length;
    if (needed > responseCapacity_) {
      size_t next = responseCapacity_ ? responseCapacity_ : 1024;
      while (next < needed && next < responseCapacityLimit) next *= 2;
      auto *replacement = static_cast<uint8_t *>(resizeResponse(response_, next));
      if (!replacement) {
        allocationFailures_.fetch_add(1, std::memory_order_relaxed);
        responseFailed_ = true;
        return 0;
      }
      response_ = replacement;
      responseCapacity_ = next;
    }
    if (length) std::memcpy(response_ + responseSize_, bytes, length);
    responseSize_ = needed;
    return length;
  }

  bool validResponse() const {
    // Validate the captured length as well as the API contract: a streamFile()
    // bypass must never turn its header-only capture into a truncated 200.
    if (!response_ || responseSize_ < 12 || std::memcmp(response_, "HTTP/1.", 7)) return false;
    size_t end = 0;
    for (; end + 3 < responseSize_; ++end)
      if (!std::memcmp(response_ + end, "\r\n\r\n", 4)) break;
    if (end + 3 >= responseSize_) return false;
    size_t contentLength = 0;
    bool foundLength = false;
    for (size_t line = 0; line < end;) {
      size_t lineEnd = line;
      while (lineEnd < end && response_[lineEnd] != '\r') ++lineEnd;
      constexpr char field[] = "Content-Length:";
      if (lineEnd - line >= sizeof(field) - 1 && !std::memcmp(response_ + line, field, sizeof(field) - 1)) {
        if (foundLength) return false;
        foundLength = true;
        size_t at = line + sizeof(field) - 1;
        while (at < lineEnd && response_[at] == ' ') ++at;
        if (at == lineEnd) return false;
        for (; at < lineEnd; ++at) {
          if (response_[at] < '0' || response_[at] > '9') return false;
          const unsigned digit = response_[at] - '0';
          if (contentLength > (responseCapacityLimit - digit) / 10) return false;
          contentLength = contentLength * 10 + digit;
        }
      }
      line = lineEnd + 2;
    }
    return foundLength && contentLength == responseSize_ - end - 4;
  }

  size_t writeAll(const uint8_t *bytes, size_t length) {
    size_t sent = 0;
    uint32_t lastProgress = millis();
    while (sent < length && !stopping_.load(std::memory_order_acquire)) {
      const size_t chunk = length - sent > 1460 ? 1460 : length - sent;
      const int result = writeSocketSome(bytes + sent, chunk);
      if (result > 0 && static_cast<size_t>(result) <= chunk) {
        sent += static_cast<size_t>(result);
        lastProgress = millis();
        continue;
      }
      if ((result < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) ||
          result > static_cast<int>(chunk) || millis() - lastProgress >= writeStallTimeoutMs) break;
      vTaskDelay(pdMS_TO_TICKS(1));
    }
    if (sent != length) {
      writeFailures_.fetch_add(1, std::memory_order_relaxed);
      _currentClient.stop();
    }
    return sent;
  }
  void unavailable() {
    // Worker-only, complete known-length failure. Do not reuse a captured 200
    // header or claim that an allocation/partial transport failure succeeded.
    _chunked = false;
    static constexpr char response[] =
        "HTTP/1.1 503 Service Unavailable\r\n"
        "Content-Type: text/plain\r\nConnection: close\r\n"
        "Retry-After: 1\r\nContent-Length: 20\r\n\r\nService unavailable\n";
    (void)writeAll(reinterpret_cast<const uint8_t *>(response), sizeof(response) - 1);
  }
};
