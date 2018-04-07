#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include "poker.h"
#include "thread.h"
#include "util.h"

using namespace std;

// Table contains 1326 masks for all the possible combinations of c1, c2
// where c2 < c1 and c1 = [0..63]
// Index is (c1 << 6) + c2 and highest is 63 * 64 + 63 = 4095
// Indeed because valid cards have (c 0xF) < 13, max index is 3899
uint64_t ScoreMask[4096];

const vector<string> Defaults = {
    "2P 3d",
    "3P KhKs - Ac Ad 7c Ts Qs",
    "4P AcTc TdTh - 5h 6h 9c",
    "5P 2c3d KsTc AhTd - 4d 5d 9c 9d",
    "6P Ac Ad KsKd 3c - 2c 2h 7c 7h 8c",
    "7P Ad Kc QhJh 3s4s - 2c 2h 7c 5h 8c",
    "8P - Ac Ah 3d 7h 8c",
    "9P",
    "4P AhAd AcTh 7c6s 2h3h - 2c 3c 4c",
    "4P AhAd AcTh 7c6s 2h3h",
};

typedef chrono::milliseconds::rep TimePoint; // A value in milliseconds

static inline TimePoint now() {
  return chrono::duration_cast<chrono::milliseconds>(
             chrono::steady_clock::now().time_since_epoch())
      .count();
}

uint64_t below(uint64_t b) { return (b >> 16) | (b >> 32) | (b >> 48); }
uint64_t to_pick(unsigned n) { return n << 13; }
uint64_t up_to(uint64_t b) {
  if (b & Rank4BB)
    return (b - 1) & Rank4BB;
  if (b & Rank3BB)
    return (b - 1) & Rank3BB;
  if (b & Rank2BB)
    return (b - 1) & Rank2BB;
  if (b & Rank1BB)
    return (b - 1) & Rank1BB;
  return b;
}

void init_score_mask() {

  uint64_t Fixed = FullHS | DPairS | to_pick(7);

  for (unsigned c1 = 0; c1 < 64; c1++) {

    if ((c1 & 0xF) >= INVALID)
      continue;

    for (unsigned c2 = 0; c2 < c1; c2++) {

      if ((c2 & 0xF) >= INVALID)
        continue;

      unsigned idx = (c1 << 6) + c2;

      uint64_t bh = 1ULL << c1;
      uint64_t bl = 1ULL << c2;

      // High card
      if (bh & Rank1BB) {
        ScoreMask[idx] = ~Fixed | to_pick(5);
      }
      // Pair
      else if ((bh & Rank2BB) && (bl & Rank1BB)) {
        ScoreMask[idx] = ~(Fixed | below(bh)) | to_pick(3);
      }
      // Double Pair (there could be also a third one that is dropped)
      else if ((bh & Rank2BB) && (bl & Rank2BB)) {
        ScoreMask[idx] =
            ~(Fixed | below(bh) | below(bl) | up_to(bl)) | DPairS | to_pick(1);
      }
      // Set
      else if ((bh & Rank3BB) && (bl & Rank1BB)) {
        ScoreMask[idx] = ~(Fixed | below(bh)) | to_pick(2);
      }
      // Full house (there could be also a second pair that is dropped)
      else if ((bh & Rank3BB) && (bl & Rank2BB)) {
        ScoreMask[idx] =
            ~(Fixed | below(bh) | below(bl) | up_to(bl)) | FullHS | to_pick(0);
        ScoreMask[idx] &= ~Rank1BB; // Drop all first line
      }
      // Double set: it's a full house, second set is counted as a pair
      else if ((bh & Rank3BB) && (bl & Rank3BB)) {
        ScoreMask[idx] = ~(Fixed | below(bh) | below(bl) | up_to(bh));
        ScoreMask[idx] |= (bl >> 16) | FullHS | to_pick(0);
        ScoreMask[idx] &= ~Rank1BB; // Drop all first line
      }
      // Quad: drop anything else but first rank
      else if ((bh & Rank4BB)) {
        ScoreMask[idx] =
            ~(Fixed | below(bh) | up_to(bh) | Rank3BB | Rank2BB) | to_pick(1);
      } else
        assert(false);
    }
  }
}

const string pretty_hand(uint64_t b, bool headers) {

  string s = "\n";
  string hstr = headers ? "\n" : "---+---+---+\n";

  if (headers)
    s += "    | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | T | J | Q | K | A \n";

  s += "    +---+---+---+---+---+---+---+---+---+---+---+---+---+" + hstr;

  for (int r = 3; r >= 0; --r) {
    s += headers ? string("   ") + "dhcs"[r] : string("    ");

    for (int f = 0; f < (headers ? 13 : 16); ++f)
      s += b & (1ULL << ((r * 16) + f)) ? "| X " : "|   ";

    s += "|\n    +---+---+---+---+---+---+---+---+---+---+---+---+---+" + hstr;
  }

  return s;
}

ostream &operator<<(ostream &os, Flags f) {
  if (f & SFlushF)
    os << "SF ";
  if (f & QuadF)
    os << "QD ";
  if (f & FullHF)
    os << "FH ";
  if (f & FlushF)
    os << "FL ";
  if (f & StraightF)
    os << "ST ";
  if (f & SetF)
    os << "S ";
  if (f & DPairF)
    os << "DP ";
  if (f & PairF)
    os << "P ";
  return os;
}

ostream &operator<<(ostream &os, Card c) {
  if (c % 16 < INVALID)
    os << "23456789TJQKA"[c % 16] << "dhcs"[c / 16] << " ";
  else
    os << "-- ";
  return os;
}

ostream &operator<<(ostream &os, const Hand &h) {

  vector<Card> cards;
  uint64_t v = h.cards;

  while (v)
    cards.push_back(Card(pop_lsb(&v)));

  // Sort the cards in descending value
  auto comp = [](Card a, Card b) { return (a & 0xF) > (b & 0xF); };
  sort(cards.begin(), cards.end(), comp);

  os << "\n\nHand: ";
  for (Card c : cards)
    os << c;

  os << "\n" << pretty_hand(h.cards, true) << "\n";

  if (h.score)
    os << "\nScore:\n" << pretty_hand(h.score, false) << "\n";

  return os;
}

std::ostream &operator<<(std::ostream &os, const Spot &s) {

  string np = {char('0' + s.numPlayers), 'P'};

  os << "Spot: " << np << " ";

  uint64_t common = s.hands[0].cards & s.hands[1].cards;

  // Additional debug info
  //
  os << "\n\ncommon:" << pretty_hand(common, false);
  for (size_t i = 0; i < s.numPlayers; ++i)
    os << "Player " << i + 1 << ":" << s.hands[i] << "\n";

  os << endl;
  return os;
}

void print_results(unsigned *results, size_t players) {

  size_t sum = 0;
  for (size_t p = 0; p < players; p++)
    sum += results[p];

  cout << "Equity: ";
  for (size_t p = 0; p < players; p++)
    cout << std::setprecision(4) << float(results[p] * 100) / sum << "%  ";
  cout << endl;
}

// Quick hash, see https://stackoverflow.com/questions/13325125/
// lightweight-8-byte-hash-function-algorithm
struct Hash {

  static const uint64_t Mulp = 2654435789;
  uint64_t mix = 104395301;

  void operator<<(unsigned v) { mix += (v * Mulp) ^ (mix >> 23); }

  uint64_t get() { return mix ^ (mix << 37); }
};

void bench(istringstream &is) {

  constexpr uint64_t GoodSig = 12795375867761621917ULL;
  constexpr int NumGames = 1000 * 1000;

  Threads.set(0); // Re-init prng for each thread

  unsigned results[10];
  string token;
  uint64_t cards = 0, spots = 0, cnt = 1;
  Hash sig;

  int threadsNum = (is >> token) ? stoi(token) : 1;
  Threads.set(threadsNum);

  TimePoint elapsed = now();

  for (const string &v : Defaults) {

    cout << "\nP" << cnt++ << ": " << v;
    memset(results, 0, sizeof(results));
    Spot s(v);
    Threads.run(s, NumGames, results);

    for (size_t p = 0; p < s.players(); p++)
      sig << results[p];
    cout << "\n";

    print_results(results, s.players());

    cards += NumGames * (s.players() * 2 + 5);
    spots += NumGames;
  }

  elapsed =
      now() - elapsed + 1; // Ensure positivity to avoid a 'divide by zero'

  cerr << "\n==========================="
       << "\nTotal time  (ms): " << elapsed
       << "\nSpots played (M): " << spots / 1000000
       << "\nCards/second    : " << 1000 * cards / elapsed
       << "\nSpots/second    : " << 1000 * spots / elapsed
       << "\nSignature       : " << sig.get();

  if (sig.get() == GoodSig)
    cerr << " (OK)" << endl;
  else
    cerr << " (FAIL)" << endl;
}
