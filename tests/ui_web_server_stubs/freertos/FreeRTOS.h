#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

using BaseType_t = int;
using UBaseType_t = unsigned;
using StackType_t = uint32_t;  // deliberately not bytes: IDF high-water API is bytes
using TickType_t = uint32_t;
constexpr BaseType_t pdTRUE = 1, pdFALSE = 0, pdPASS = 1;
constexpr TickType_t portMAX_DELAY = UINT32_MAX;
#define pdMS_TO_TICKS(ms) (ms)

namespace UiWebTest {
inline auto epoch = std::chrono::steady_clock::now();
inline bool failTaskCreation = false;
struct Semaphore { std::mutex mutex; std::condition_variable cv; bool ready = false; };
struct Task { std::thread thread; };
inline std::vector<std::unique_ptr<Task>> tasks;
inline void joinTasks() { for (auto &task : tasks) if (task->thread.joinable()) task->thread.join(); tasks.clear(); }
}
inline uint32_t millis() {
  return uint32_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-UiWebTest::epoch).count());
}
inline uint32_t micros() {
  return uint32_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-UiWebTest::epoch).count());
}
