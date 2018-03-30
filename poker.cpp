#include <iostream>

#include "poker.h"
#include "util.h"

const std::string pretty_hand(uint64_t b, bool value);

std::ostream &operator<<(std::ostream &os, Card c) {
  if (c % 16 < INVALID)
    os << "23456789TJQKA"[c % 16] << "dhcs"[c / 16] << " ";
  else
    os << "-- ";
  return os;
}

std::ostream &operator<<(std::ostream &os, Card64 cs) {
  for (unsigned i = 0; i <= 64 - 6; i += 6)
    os << Card((cs >> i) & 0x3F);
  return os;
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

void test() {

  static const int Limit = 7;

  PRNG::init();
  Hand hands[10 * 1000];
  int hit = 0;

  for (int i = 0; i < 1000; i++) {
      for (auto &h : hands) {

          h = Hand();
          int cnt = 0;

          while (cnt < Limit) {

              Card64 c = Card64(PRNG::next()); // Cards are pseudo-legal

              //std::cout << c << "\n";

              h.add<Limit>(c, cnt);
          }

          h.do_score();

          if ((h.flags & QuadF) && (h.score & (1ULL << (12 + 48)))) {

              std::cout << Flags(h.flags) << "\n";

              std::cout << pretty_hand(h.values, true) << "\n"
                        << pretty_hand(h.colors, false) << "\n"
                        << pretty_hand(h.score, true) << "\n";

              if (++hit == 20)
                  exit(0);
          }
      }
  }

  std::cout << std::endl;
}
