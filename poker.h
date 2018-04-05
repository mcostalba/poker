
#ifndef POKER_H_INCLUDED
#define POKER_H_INCLUDED

#include <cstdint>
#include <iostream>
#include <string>

#include "util.h"

constexpr int PLAYERS_NB = 9;
constexpr int HOLE_NB = 2;

constexpr uint64_t LAST = 0xE000;
constexpr uint64_t FLAGS_AREA =
    LAST | (LAST << 16) | (LAST << 32) | (LAST << 48);

enum Card : unsigned { NO_CARD = 0, INVALID = 13 };
enum Card64 : uint64_t {
}; // 6 bit per card [1..53], 2 msb is color, 4 lsb is value

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

  uint64_t score; // 16bit (for each card num) * 4 (for pairs, set and quads)
  uint64_t cards; // 16bit for each color

  friend std::ostream &operator<<(std::ostream &, const Hand &);

  unsigned add(Card c, uint64_t all) {

    uint64_t n = 1ULL << c;

    if ((cards | all) & n) // Double card or invalid
      return 0;

    cards |= n;
    n = 1 << (c & 0xF);

    while (true) {
      if (!(score & n))
        return score |= n, 1;
      n <<= 16;
    }
  }

  void merge(const Hand &holes) {

    if ((score & holes.score) == 0) { // Common case
      score |= holes.score;
      cards |= holes.cards;
      return;
    }
    // We are unlucky: add one by one
    uint64_t v = holes.cards;
    while (v)
      add(Card(pop_lsb(&v)), 0);
  }

  void check_flush() {

    // Score could be more than 5, to check for a flush-straight
    if (popcount(cards & Rank4BB) >= 5)
      score = FlushS | ((cards & Rank4BB) >> 48);

    else if (popcount(cards & Rank3BB) >= 5)
      score = FlushS | ((cards & Rank3BB) >> 32);

    else if (popcount(cards & Rank2BB) >= 5)
      score = FlushS | ((cards & Rank2BB) >> 16);

    else if (popcount(cards & Rank1BB) >= 5)
      score = FlushS | (cards & Rank1BB);
  }

  // See
  // https://stackoverflow.com/questions/10911780/looping-through-bits-in-an-integer-ruby
  void check_straight() {

    uint64_t v = score & Rank1BB;
    v = (v << 1) | (v >> 12); // Duplicate an ace into first position
    v &= v >> 1, v &= v >> 1, v &= v >> 1, v &= v >> 1;
    if (v) {
      uint64_t f = score & FLAGS_AREA;
      f |= (f & FlushS) ? SFlushS | StraightS : StraightS;
      score = f | (v << 3) | (v << 2); // At least 2 bit for ScoreMask
    }
  }

  void do_score() {

    // check_flush() and check_straight() map score into Rank1BB and
    // set the corresponding flags if needed.
    check_flush();
    check_straight();

    // Drop all bits below the set ones so that msb()
    // returns values on different files.
    uint64_t b = (score ^ (score >> 16)) & ~FLAGS_AREA;

    // Mask out needed bits to get the score
    unsigned cnt = pop_msb(&b) << 6;
    b = ScoreMask[cnt + msb(b)];
    score = (score | FullHS) & b;

    // Drop the lowest cards so that 5 remains
    cnt = (unsigned(b) >> 13) & 0x7;
    unsigned p = popcount(score & Rank1BB);
    while (p-- > cnt)
      score &= score - 1;
  }
};

class Spot {

  int fill[PLAYERS_NB * HOLE_NB + 1];
  Hand givenHoles[PLAYERS_NB];
  Hand hands[PLAYERS_NB];
  Hand givenCommon;

  PRNG *prng;
  size_t numPlayers;
  unsigned commonsNum;
  uint64_t allMask;
  bool ready;

  friend std::ostream &operator<<(std::ostream &, const Spot &);

public:
  Spot() = default;
  explicit Spot(const std::string &pos);
  void run(unsigned results[]);
  void set_prng(PRNG *p) { prng = p; }
  bool valid() const { return ready; }
  size_t players() const { return numPlayers; }
};

#endif // #ifndef POKER_H_INCLUDED
