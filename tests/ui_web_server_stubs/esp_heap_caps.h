#pragma once
#include <cstdlib>
constexpr unsigned MALLOC_CAP_SPIRAM = 1, MALLOC_CAP_8BIT = 2;
inline void *heap_caps_realloc(void *memory, size_t bytes, unsigned) { return std::realloc(memory,bytes); }
inline void heap_caps_free(void *memory) { std::free(memory); }
