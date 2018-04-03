#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>

#include "poker.h"
#include "thread.h"
#include "util.h"

using namespace std;

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
     << pretty_hand(h.colors, true) << "\n";

  if (h.score)
     os << "\nScore: (" << Flags(h.flags) << ")\n"
         << pretty_hand(h.score, false) << "\n";

  return os;
}

std::ostream &operator<<(std::ostream &os, const Spot &s) {

  string np = {char('0' + s.numPlayers), 'P'};

  os << "Spot: " << np << " ";

  uint64_t common = s.hands[0].colors & s.hands[1].colors;

  // Additional debug info
  //
  os << "\n\ncommon:" << pretty_hand(common, false);
  for (size_t i = 0; i < s.numPlayers ; ++i)
      os << "Player " << i+1 << ":" << s.hands[i] << "\n";

  os << endl;
  return os;
}

void print_results(unsigned* results, size_t players) {

    size_t sum = 0;
    for (size_t p = 0; p < players; p++)
      sum += results[p];

    cout << "Equity: ";
    for (size_t p = 0; p < players; p++)
      cout << (results[p] * 100) / sum << "%  ";
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

  const int NumGames = 500 * 1000;

  PRNG::init();
  Threads.set(0); // Re-init prng for each thread

  unsigned results[10];
  string token;
  uint64_t hands = 0, spots = 0, cnt = 1;
  Hash sig;

  int threadsNum = (is >> token) ? stoi(token) : 1;
  Threads.set(threadsNum);

  TimePoint elapsed = now();

  for (const string& v : Defaults) {

    cout << "\nP" << cnt++ << ": " << v;
    memset(results, 0, sizeof(results));
    Spot s(v);
    Threads.run(s, NumGames, results);

    for (size_t p = 0; p < s.players(); p++)
        sig << results[p];
    cout << "\n";

    print_results(results, s.players());

    hands += NumGames * s.players();
    spots += NumGames;
  }

  elapsed =
      now() - elapsed + 1; // Ensure positivity to avoid a 'divide by zero'

  cerr << "\n==========================="
       << "\nTotal time  (ms): " << elapsed
       << "\nHands served (M): " << hands / 1000000
       << "\nSpots played (M): " << spots / 1000000
       << "\nSpots/second    : " << 1000 * spots / elapsed
       << "\nSignature       : " << sig.get() << endl;
}
