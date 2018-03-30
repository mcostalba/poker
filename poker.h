
#ifndef POKER_H_INCLUDED
#define POKER_H_INCLUDED

#include <cstdint>

namespace PRNG {

void init();
uint64_t next();

} // namespace PRNG

enum Card : unsigned { INVALID = 13 };
enum Card64 : uint64_t {}; // 6 bit per card [1..53..64], 10 cards set

struct Hand {
  uint64_t value; // 16bit (for each card num) * 4 (for pairs, set and quads)
  uint64_t color; // 16bit for each color

  int add(Card c) {

    if ((c & 0xF) >= INVALID)
      return 0;

    uint64_t n = 1 << (c & 0xF);

    if ((color & (n << (c & 0x30)))) // Double card
      return 0;

    color |= n << (c & 0x30);

    while (true) {
      if (!(value & n))
        return value |= n, 1;
      n <<= 16;
    }
  }

  template <int Limit> int add(Card64 cs, int &cnt) {
    for (unsigned i = 0; cnt < Limit && i <= 64 - 6; i += 6)
      cnt += add(Card((cs >> i) & 0x3F));
    return cnt;
  }
};

#endif // #ifndef POKER_H_INCLUDED
