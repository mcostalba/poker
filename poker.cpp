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
