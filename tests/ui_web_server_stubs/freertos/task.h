#pragma once
#include "FreeRTOS.h"
using TaskHandle_t = UiWebTest::Task *;
inline BaseType_t xTaskCreatePinnedToCore(void (*entry)(void *), const char *, uint32_t stack,
                                         void *argument, unsigned priority, TaskHandle_t *handle, unsigned core) {
  if (UiWebTest::failTaskCreation) return pdFALSE;
  if (stack != 4096 || priority != 1 || core != 0) std::terminate();
  auto task = std::make_unique<UiWebTest::Task>(); *handle = task.get();
  task->thread = std::thread([=] { entry(argument); });
  UiWebTest::tasks.push_back(std::move(task)); return pdPASS;
}
inline void vTaskDelay(TickType_t ticks) { std::this_thread::sleep_for(std::chrono::milliseconds(ticks)); }
inline void vTaskDelete(TaskHandle_t) {}
inline UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t) { return 1732; }
