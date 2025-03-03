#include "types.h"

extern uint ticks;

static uint32 splitmix32(uint32 x) {
  x += 0x9e3779b9;
  x = (x ^ (x >> 15)) * 0x85ebca6b;
  x = (x ^ (x >> 13)) * 0xc2b2ae35;
  x = x ^ (x >> 16);
  return x;
}

uint32 rand() {
  return splitmix32(ticks);
}

uint32 rand_range(uint32 min, uint32 max) {
  if (max <= min) return min;
  return rand() % (max - min + 1) + min;
}
