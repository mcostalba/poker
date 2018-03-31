
#ifndef POKER_H_INCLUDED
#define POKER_H_INCLUDED

#include <cstdint>
#include <iostream>
#include <string>

#include "util.h"

enum Card : unsigned { INVALID = 13 };
enum Card64 : uint64_t {}; // 6 bit per card [1..53..64], 10 cards set

enum Flags {
  SFlushF   = 1 << 7,
  QuadF     = 1 << 6,
  FullHF    = 1 << 5,
  FlushF    = 1 << 4,
  StraightF = 1 << 3,
  SetF      = 1 << 2,
  DPairF    = 1 << 1,
  PairF     = 1 << 0
};

// Alter score according to combination tye. Needed only for few cases, when
// native score value is not enough. We use the last 3 unused bits of quad and
// set rows in values.
enum FlagScores : uint64_t {
  SFlushS   = 1ULL << (16 * 3 + 15),
  FullHS    = 1ULL << (16 * 2 + 15),
  FlushS    = 1ULL << (16 * 2 + 14),
  StraightS = 1ULL << (16 * 2 + 13)
};

constexpr uint64_t Rank1BB = 0xFFFFULL << (16 * 0);
constexpr uint64_t Rank2BB = 0xFFFFULL << (16 * 1);
constexpr uint64_t Rank3BB = 0xFFFFULL << (16 * 2);
constexpr uint64_t Rank4BB = 0xFFFFULL << (16 * 3);

struct Hand {

  uint64_t values; // 16bit (for each card num) * 4 (for pairs, set and quads)
  uint64_t colors; // 16bit for each color
  uint64_t score;  // Only the 5 best cards, used to compare hands
  uint32_t flags;  // One flag for each combination

  unsigned add(Card c) {

    if ((c & 0xF) >= INVALID)
      return 0;

    uint64_t n = 1 << (c & 0xF);

    if ((colors & (n << (c & 0x30)))) // Double card
      return 0;

    colors |= n << (c & 0x30);

    while (true) {
      if (!(values & n))
        return values |= n, 1;
      n <<= 16;
    }
  }

  template <int Limit> size_t add(Card64 cs, size_t& cnt) {
    for (unsigned i = 0; cnt < Limit && i <= 64 - 6; i += 6)
      cnt += add(Card((cs >> i) & 0x3F));
    return cnt;
  }

  uint64_t is_flush() {

    if (popcount(colors & Rank4BB) >= 5)
        values = (colors & Rank4BB) >> 48;

    else if (popcount(colors & Rank3BB) >= 5)
        values = (colors & Rank3BB) >> 32;

    else if (popcount(colors & Rank2BB) >= 5)
        values = (colors & Rank2BB) >> 16;

    else if (popcount(colors & Rank1BB) >= 5)
        values = colors & Rank1BB;

    else
        return 0;

    return values; // Could be more than 5, to check for a flush-straight
  }

  // See https://stackoverflow.com/questions/10911780/looping-through-bits-in-an-integer-ruby
  uint64_t is_straight() {

   uint64_t v = values & Rank1BB;
   v = (v << 1) | (v >> 12); // Duplicate an ace into first position
   v &= v >> 1, v &= v >> 1, v &= v >> 1, v &= v >> 1;
   return v ? values = (v << 3) : 0;
  }

  // Remove file of b from values
  template<int N>
  inline void drop(uint64_t b) {

    if (N == 4)
        b |= (b >> 16) | (b >> 32) | (b >> 48);
    else if (N == 3)
        b |= (b >> 16) | (b >> 32);
    else if (N == 2)
        b |= (b >> 16);

    assert((values & b) == b);

    values ^= b;
  }

  void do_score() {

    uint64_t v;
    int cnt = 5; // Pick and score the 5 best cards

    // is_flush() and is_straight() map values into Rank1BB, so all the other
    // checks on ranks above the first are always false.
    if (is_flush())
        flags |= FlushF, score |= FlushS;

    if (is_straight())
        flags |= StraightF, score |= StraightS;

    // We can't have quad and straight or flush at the same time
    if ((v = (values & Rank4BB)) != 0 && cnt >= 4)
      flags |= QuadF,  v = msb_bb(v), score |= v, drop<4>(v), cnt -= 4;

    if ((v = (values & Rank3BB)) != 0 && cnt >= 3)
      flags |= SetF,   v = msb_bb(v), score |= v, drop<3>(v), cnt -= 3;

    if ((v = (values & Rank2BB)) != 0 && cnt >= 2)
      flags |= PairF,  v = msb_bb(v), score |= v, drop<2>(v), cnt -= 2;

    if ((v = (values & Rank2BB)) != 0 && cnt >= 2)
      flags |= DPairF, v = msb_bb(v), score |= v, drop<2>(v), cnt -= 2;

    if ((flags & (FlushF | StraightF)) == (FlushF | StraightF))
      flags |= SFlushF, score |= SFlushS;

    if ((flags & (SetF | PairF)) == (SetF | PairF))
      flags |= FullHF, score |= FullHS;

    // Pick the highest 5 only
    v = values & Rank1BB;
    int p = popcount(v);
    while (p-- > cnt)
        v &= v - 1;

    score |= v;
  }

};

std::ostream &operator<<(std::ostream &os, const Hand& h);

class Spot {

  Hand hands[9];
  const size_t numPlayers;

public:
  explicit Spot(size_t n) : numPlayers(n) {}

  void run(unsigned results[]) {

    Hand common = Hand();
    uint64_t maxScore = 0;
    size_t cnt = 0;

    do
        common.add<5>(Card64(PRNG::next()), cnt);
    while (cnt < 5);

    for (size_t i = 0; i < numPlayers ; ++i) {
        hands[i] = common;

        do {
            cnt = 0;
            hands[i].add<2>(Card64(PRNG::next()), cnt);
        } while (cnt < 2);

        hands[i].do_score();

        if (maxScore < hands[i].score)
            maxScore = hands[i].score;
    }

    // Credit the winner, considering split results
    for (size_t i = 0; i < numPlayers ; ++i) {
        if (hands[i].score == maxScore)
            results[i]++;
    }
  }

  const Hand& get(size_t idx) const {
    assert(idx < numPlayers);
    return hands[idx];
  }

};

#endif // #ifndef POKER_H_INCLUDED
