
#ifndef POKER_H_INCLUDED
#define POKER_H_INCLUDED

#include <cstdint>
#include <iostream>
#include <string>

#include "util.h"

constexpr int PLAYERS_NB = 9;
constexpr int HOLE_NB = 2;

constexpr uint64_t LAST = 0xE000;
constexpr uint64_t INVALID_BB = LAST | (LAST << 16) | (LAST << 32) | (LAST << 48);

enum Card : unsigned { NO_CARD = 0, INVALID = 13 };
enum Card64 : uint64_t {}; // 6 bit per card [1..53], 2 msb is color, 4 lsb is value

enum Flags {
  SFlushF = 1 << 7,
  QuadF = 1 << 6,
  FullHF = 1 << 5,
  FlushF = 1 << 4,
  StraightF = 1 << 3,
  SetF = 1 << 2,
  DPairF = 1 << 1,
  PairF = 1 << 0
};

// Alter score according to combination type. Needed only for few cases, when
// native score value is not enough. We use the last 3 unused bits of quad and
// set rows in values.
enum FlagScores : uint64_t {
  SFlushS = 1ULL << (16 * 3 + 15),
  FullHS = 1ULL << (16 * 2 + 15),
  FlushS = 1ULL << (16 * 2 + 14),
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

  friend std::ostream &operator<<(std::ostream &, const Hand &);

  unsigned add(Card c, uint64_t all) {

    uint64_t n = 1ULL << c;

    if ((colors | all) & n) // Double card or invalid
      return 0;

    colors |= n;
    n = 1 << (c & 0xF);

    while (true) {
      if (!(values & n))
        return values |= n, 1;
      n <<= 16;
    }
  }

  void merge(const Hand &holes) {

    if ((values & holes.values) == 0) { // Common case
      values |= holes.values;
      colors |= holes.colors;
      return;
    }
    // We are unlucky: add one by one
    uint64_t v = holes.colors;
    while (v)
      add(Card(pop_lsb(&v)), 0);
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

  // See
  // https://stackoverflow.com/questions/10911780/looping-through-bits-in-an-integer-ruby
  uint64_t is_straight() {

    uint64_t v = values & Rank1BB;
    v = (v << 1) | (v >> 12); // Duplicate an ace into first position
    v &= v >> 1, v &= v >> 1, v &= v >> 1, v &= v >> 1;
    return v ? values = (v << 3) : 0;
  }

  // Remove file of b from values
  template <int N> inline void drop(uint64_t b) {

    if (N == 4)
      b |= (b >> 16) | (b >> 32) | (b >> 48);
    else if (N == 3)
      b |= (b >> 16) | (b >> 32);
    else if (N == 2)
      b |= (b >> 16);

    assert((values & b) == b);

    values ^= b;
  }

  template<Flags F>
  inline uint64_t process(uint64_t v, int& cnt) {
    constexpr int N = (F == QuadF ? 4 : F == SetF ? 3 : 2);
    flags |= F, cnt -= N, v = msb_bb(v), drop<N>(v);
    return v;
  }

  void do_score() {

    int cnt = 5; // Pick and score the 5 best cards

    // is_flush() and is_straight() map values into Rank1BB, so all the other
    // checks on ranks above the first are always false.
    if (is_flush())
      flags |= FlushF, score |= FlushS;

    if (is_straight())
      flags |= StraightF, score |= StraightS;

    // We can't have quad and straight or flush at the same time
    if ((values & Rank4BB) != 0 && cnt >= 4)
        score |= process<QuadF>(values & Rank4BB, cnt);

    if ((values & Rank3BB) != 0 && cnt >= 3)
        score |= process<SetF>(values & Rank3BB, cnt);

    if ((values & Rank2BB) != 0 && cnt >= 2)
        score |= process<PairF>(values & Rank2BB, cnt);

    if ((values & Rank2BB) != 0 && cnt >= 2)
        score |= process<DPairF>(values & Rank2BB, cnt);

    if ((flags & (FlushF | StraightF)) == (FlushF | StraightF))
      flags |= SFlushF, score |= SFlushS;

    if ((flags & (SetF | PairF)) == (SetF | PairF))
      flags |= FullHF, score |= FullHS;

    // Pick the highest 5 only
    uint64_t v = values & Rank1BB;
    int p = popcount(v);
    while (p-- > cnt)
      v &= v - 1;

    score |= v;
  }
};

class Spot {

  int fill[PLAYERS_NB * HOLE_NB + 1];
  Hand givenHoles[PLAYERS_NB];
  Hand hands[PLAYERS_NB];
  Hand givenCommon;

  PRNG* prng;
  size_t numPlayers;
  unsigned commonsNum;
  uint64_t allMask;
  bool ready;

  friend std::ostream &operator<<(std::ostream &, const Spot &);

public:
  Spot() = default;
  explicit Spot(const std::string &pos);
  void run(unsigned results[]);
  void set_prng(PRNG* p) { prng = p; }
  bool valid() const { return ready; }
  size_t players() const { return numPlayers; }
};

#endif // #ifndef POKER_H_INCLUDED
