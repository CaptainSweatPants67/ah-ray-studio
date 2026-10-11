#pragma once
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <algorithm>
using std::min;
using std::max;
#define LOW 0
#define HIGH 1
extern "C" uint32_t millis();
extern "C" uint32_t micros();
inline long map(long x, long inMin, long inMax, long outMin, long outMax) {
  return (x - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
}
