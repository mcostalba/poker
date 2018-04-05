
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

// We use a 32 bit split in 4 parts (5 bit  each) inited
// with 11 and add 1 for every card to the slot
// according to card's color, if one slot reaches 16, then
// we have a flush.
constexpr uint32_t COLOR_INIT = 11 | (11 << 5) | (11 << 10) | (11 << 15);

constexpr uint32_t COLOR_SLOTS[4] = {1, 1 << 5, 1 << 10, 1 << 15};
constexpr uint32_t IS_FLUSH = 16 | (16 << 5) | (16 << 10) | (16 << 15);

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

constexpr uint64_t RanksBB[] = {Rank1BB, Rank2BB, Rank3BB, Rank4BB};

struct Hand {

  uint64_t score; // 16bit (for each card num) * 4 (for pairs, set and quads)
  uint64_t cards; // 16bit for each color
  uint32_t colors;

  friend std::ostream &operator<<(std::ostream &, const Hand &);

  unsigned add(Card c, uint64_t all) {

    uint64_t n = 1ULL << c;

    if ((cards | all) & n) // Double card or invalid
      return 0;

    cards |= n;
    n = 1 << (c & 0xF);

    colors += COLOR_SLOTS[(c & 0x30) >> 4];

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
      colors += holes.colors;
      return;
    }
    // We are unlucky: add one by one
    uint64_t v = holes.cards;
    while (v)
      add(Card(pop_lsb(&v)), 0);
  }

  void do_score() {

    if (colors & IS_FLUSH) {
      unsigned r = lsb(colors & IS_FLUSH) / 5;
      score = FlushS | ((cards & RanksBB[r]) >> (16 * r));
    }

    // Check for a straight, see
    // https://stackoverflow.com/questions/10911780/looping-through-bits-in-an-integer-ruby
    uint64_t v = score & Rank1BB;
    v = (v << 1) | (v >> 12); // Duplicate an ace into first position
    v &= v >> 1, v &= v >> 1, v &= v >> 1, v &= v >> 1;
    if (v) {
      score &= FLAGS_AREA;
      score |= (score & FlushS) ? SFlushS | StraightS : StraightS;
      score |= (v << 3) | (v << 2); // At least 2 bit for ScoreMask
    }

    // Drop all bits below the set ones so that msb()
    // returns values on different files.
    v = (score ^ (score >> 16)) & ~FLAGS_AREA;

    // Mask out needed bits to get the score
    unsigned cnt = pop_msb(&v) << 6;
    v = ScoreMask[cnt + msb(v)];
    score = (score | FullHS) & v;

    // Drop the lowest cards so that 5 remains
    cnt = (unsigned(v) >> 13) & 0x7;
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
