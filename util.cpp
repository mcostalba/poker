#include <algorithm>
#include <chrono>
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

#include "poker.h"
#include "util.h"

typedef std::chrono::milliseconds::rep TimePoint; // A value in milliseconds

static inline TimePoint now() {
  return std::chrono::duration_cast<std::chrono::milliseconds>
        (std::chrono::steady_clock::now().time_since_epoch()).count();
}

const std::string pretty_hand(uint64_t b, bool headers) {

  std::string s = "\n";
  std::string hstr = headers ? "\n" : "---+---+---+\n";

  if (headers)
    s += "    | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | T | J | Q | K | A \n";

  s += "    +---+---+---+---+---+---+---+---+---+---+---+---+---+" + hstr;

  for (int r = 3; r >= 0; --r) {
    s += headers ? std::string("   ") + "dhcs"[r] : std::string("    ");

    for (int f = 0; f < (headers ? 13 : 16); ++f)
      s += b & (1ULL << ((r * 16) + f)) ? "| X " : "|   ";

    s += "|\n    +---+---+---+---+---+---+---+---+---+---+---+---+---+" + hstr;
  }

  return s;
}

std::ostream &operator<<(std::ostream &os, Flags f) {
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

std::ostream &operator<<(std::ostream &os, Card c) {
  if (c % 16 < INVALID)
    os << "23456789TJQKA"[c % 16] << "dhcs"[c / 16] << " ";
  else
    os << "-- ";
  return os;
}

std::ostream &operator<<(std::ostream &os, const Hand& h) {

  std::vector<Card> cards;
  uint64_t v = h.colors;

  while (v)
      cards.push_back(Card(pop_lsb(&v)));

  // Sort the cards in descending value
  auto comp = [](Card a, Card b){ return (a & 0xF) > (b & 0xF); };
  std::sort(cards.begin(), cards.end(), comp);

  os << "\n\nHand: ";
  for (Card c : cards)
      os << c;

  os << "\n" << pretty_hand(h.colors, true) << "\n"
     << "\nScore: (" << Flags(h.flags) << ")\n"
     << pretty_hand(h.score, false)   << "\n";

  return os;
}

// Quick hash, see https://stackoverflow.com/questions/13325125/
// lightweight-8-byte-hash-function-algorithm
struct Hash {

  static const uint64_t Mulp = 2654435789;
  uint64_t mix = 104395301;

  void operator<<(unsigned v) {
    mix += (v * Mulp) ^ (mix >> 23);
  }

  uint64_t get() {
    return mix ^ (mix << 37);
  }
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

      std::cout << "\nRunning " << NumGames / 1000
                << "K games with " << players << " players...";

      memset(results, 0, sizeof(results));
      Spot s(players);

      for (int i = 0; i < NumGames; i++) {
          s.run(results);

          for (size_t p = 0; p < players; p++)
              sig << results[p];
      }

      std::cout << "\nWins per player: ";
      for (size_t p = 0; p < players; p++)
          std::cout << results[p] << " ";

      hands += NumGames * players;
      spots += NumGames;
      std::cout << "\n" << std::endl;
  }

  elapsed = now() - elapsed + 1; // Ensure positivity to avoid a 'divide by zero'

  std::cerr << "\n==========================="
            << "\nTotal time  (ms): " << elapsed
            << "\nHands served (M): " << hands / 1000000
            << "\nSpots played (M): " << spots / 1000000
            << "\nSpots/second    : " << 1000 * spots / elapsed
            << "\nSignature       : " << sig.get();

  if (sig.get() == GoodSig)
      std::cerr << " (OK)" << std::endl;
  else
      std::cerr << " (FAILED!)" << std::endl;

  PRNG::init(); // Restore random before exit
}
