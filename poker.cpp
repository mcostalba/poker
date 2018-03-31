#include <iostream>

#include "poker.h"
#include "util.h"

const std::string pretty_hand(uint64_t b, bool value);

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

  uint64_t v = h.colors;

  os << "Cards: ";
  while (v)
      os << Card(pop_lsb(&v));

  os << "\nFlags: " << Flags(h.flags) << "\n"
     << pretty_hand(h.colors, false) << "\n"
     << pretty_hand(h.score, true)   << "\n";

  return os;
}

void test() {

  PRNG::init();
  int hit = 0;

  const size_t Players = 5;
  int results[Players] = {};

  Spot s(Players);

  for (int i = 0; i < 4 * 1000 * 1000; i++) {

      s.run(results);

//      const Hand& h = s.get(w);
//      if ((h.flags & QuadF) && (h.score & (1ULL << (12 + 48)))) {

      if (0) {

    //      std::cout << "\n\nWinner is player: " << w+1 << std::endl;

          for (size_t p = 0; p < Players; p++)
              std::cout << "\nPlayer " << p+1 << ":\n\n" << s.get(p) << std::endl;

          if (++hit == 4)
              break;
      }
  }

  std::cout << "\nResults per player: ";
  for (size_t p = 0; p < Players; p++)
      std::cout << results[p] << " ";
  std::cout << std::endl;
}
