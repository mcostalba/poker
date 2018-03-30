
#ifndef UTIL_H_INCLUDED
#define UTIL_H_INCLUDED

#include <cassert>
#include <cstdint>

/// popcount() counts the number of non-zero bits in a uint64_t

inline int popcount(uint64_t b) {

#ifndef USE_POPCNT

  extern uint8_t PopCnt16[1 << 16];
  union { uint64_t bb; uint16_t u[4]; } v = { b };
  return PopCnt16[v.u[0]] + PopCnt16[v.u[1]] + PopCnt16[v.u[2]] + PopCnt16[v.u[3]];

#elif defined(_MSC_VER) || defined(__INTEL_COMPILER)

  return (int)_mm_popcnt_u64(b);

#else // Assumed gcc or compatible compiler

  return __builtin_popcountll(b);

#endif
}


/// lsb() and msb() return the least/most significant bit in a non-zero uint64_t

#if defined(__GNUC__)  // GCC, Clang, ICC

inline int lsb(uint64_t b) {
  assert(b);
  return __builtin_ctzll(b);
}

inline int msb(uint64_t b) {
  assert(b);
  return 63 ^ __builtin_clzll(b);
}

#elif defined(_MSC_VER)  // MSVC

#ifdef _WIN64  // MSVC, WIN64

inline unsigned long lsb(uint64_t b) {
  assert(b);
  unsigned long idx;
  _BitScanForward64(&idx, b);
  return idx;
}

inline unsigned long msb(uint64_t b) {
  assert(b);
  unsigned long idx;
  _BitScanReverse64(&idx, b);
  return idx;
}

#else  // MSVC, WIN32

inline unsigned long lsb(uint64_t b) {
  assert(b);
  unsigned long idx;

  if (b & 0xffffffff) {
      _BitScanForward(&idx, int32_t(b));
      return idx;
  } else {
      _BitScanForward(&idx, int32_t(b >> 32));
      return idx + 32;
  }
}

inline unsigned long msb(uint64_t b) {
  assert(b);
  unsigned long idx;

  if (b >> 32) {
      _BitScanReverse(&idx, int32_t(b >> 32));
      return idx + 32;
  } else {
      _BitScanReverse(&idx, int32_t(b));
      return idx;
  }
}

#endif

#else  // Compiler is neither GCC nor MSVC compatible

#error "Compiler not supported."

#endif


inline uint64_t msb_bb(uint64_t b) {
  return 1ULL << msb(b);
}

#endif // #ifndef UTIL_H_INCLUDED
