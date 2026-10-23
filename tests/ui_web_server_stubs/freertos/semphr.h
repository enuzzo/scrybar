#pragma once
#include "FreeRTOS.h"
using SemaphoreHandle_t = UiWebTest::Semaphore *;
inline SemaphoreHandle_t xSemaphoreCreateBinary() { return new UiWebTest::Semaphore; }
inline void vSemaphoreDelete(SemaphoreHandle_t semaphore) { delete semaphore; }
inline BaseType_t xSemaphoreGive(SemaphoreHandle_t semaphore) {
  std::lock_guard<std::mutex> guard(semaphore->mutex);
  semaphore->ready = true; semaphore->cv.notify_one(); return pdTRUE;
}
inline BaseType_t xSemaphoreTake(SemaphoreHandle_t semaphore, TickType_t timeout) {
  std::unique_lock<std::mutex> guard(semaphore->mutex);
  if (timeout == portMAX_DELAY) semaphore->cv.wait(guard,[&] { return semaphore->ready; });
  else if (!semaphore->cv.wait_for(guard,std::chrono::milliseconds(timeout),[&] { return semaphore->ready; })) return pdFALSE;
  semaphore->ready = false; return pdTRUE;
}
