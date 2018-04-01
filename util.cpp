#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <vector>

#include "poker.h"
#include "util.h"

using namespace std;

const vector<string> Defaults = {
    "4P AcTc TdTh - 5h 6h 9c",
    "2P 3d",
    "2P KhKs - Ac Ad 7c Ts Qs",
    "6P",
};

typedef vector<Card> CardVec;

typedef chrono::milliseconds::rep TimePoint; // A value in milliseconds

static inline TimePoint now() {
  return chrono::duration_cast<chrono::milliseconds>(
             chrono::steady_clock::now().time_since_epoch())
      .count();
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
    os << "23456789TJQKA"[c % 16] << "dhcs"[c / 16];
  else
    os << "-- ";
  return os;
}

ostream &operator<<(ostream &os, const Hand &h) {

  vector<Card> cards;
  uint64_t v = h.colors;

  while (v)
    cards.push_back(Card(pop_lsb(&v)));

  // Sort the cards in descending value
  auto comp = [](Card a, Card b) { return (a & 0xF) > (b & 0xF); };
  sort(cards.begin(), cards.end(), comp);

  os << "\n\nHand: ";
  for (Card c : cards)
    os << c;

  os << "\n"
     << pretty_hand(h.colors, true) << "\n"
     << "\nScore: (" << Flags(h.flags) << ")\n"
     << pretty_hand(h.score, false) << "\n";

  return os;
}

std::ostream &operator<<(std::ostream &os, const Spot &s) {

  const Card *ptr;
  string np = {char('0' + s.numPlayers), 'P'};

  os << "Spot: " << np << " ";

  int n = -1;
  while (s.holes[++n][0]) {
    ptr = s.holes[n];
    while (*ptr)
      os << *ptr++;
    os << " ";
  }

  if (s.commons[0]) {
    os << "- ";
    ptr = s.commons;
    while (*ptr)
      os << *ptr++ << " ";
  }

  os << endl;
  return os;
}

// Quick hash, see https://stackoverflow.com/questions/13325125/
// lightweight-8-byte-hash-function-algorithm
struct Hash {

  static const uint64_t Mulp = 2654435789;
  uint64_t mix = 104395301;

  void operator<<(unsigned v) { mix += (v * Mulp) ^ (mix >> 23); }

  uint64_t get() { return mix ^ (mix << 37); }
};

void bench() {

  const int NumGames = 1500 * 1000;
  const uint64_t GoodSig = 12289633340404457000ULL;

  PRNG::init(0x4209920184674cbfULL);

  unsigned results[10];
  uint64_t hands = 0, spots = 0;
  Hash sig;

  TimePoint elapsed = now();

  for (size_t players = 2; players < 10; players++) {

    cout << "\nRunning " << NumGames / 1000 << "K games with " << players
         << " players...";

    memset(results, 0, sizeof(results));
    string pos = {char('0' + players), 'P'};
    Spot s(pos);

    for (int i = 0; i < NumGames; i++) {
      s.run(results);

      for (size_t p = 0; p < players; p++)
        sig << results[p];
    }

    cout << "\nWins per player: ";
    for (size_t p = 0; p < players; p++)
      cout << results[p] << " ";

    hands += NumGames * players;
    spots += NumGames;
    cout << "\n" << endl;
  }

  elapsed =
      now() - elapsed + 1; // Ensure positivity to avoid a 'divide by zero'

  cerr << "\n==========================="
       << "\nTotal time  (ms): " << elapsed
       << "\nHands served (M): " << hands / 1000000
       << "\nSpots played (M): " << spots / 1000000
       << "\nSpots/second    : " << 1000 * spots / elapsed
       << "\nSignature       : " << sig.get();

  if (sig.get() == GoodSig)
    cerr << " (OK)" << endl;
  else
    cerr << " (FAILED!)" << endl;

  PRNG::init(); // Restore random before exit
}
