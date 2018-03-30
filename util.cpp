#include <iostream>
#include <string>

#include "poker.h"

std::ostream &operator<<(std::ostream &os, Card c) {
  if (c % 16 < INVALID)
    os << "A23456789TJQK"[c % 16] << "dhcs"[c / 16] << " ";
  else
    os << "-- ";
  return os;
}

std::ostream &operator<<(std::ostream &os, Card64 cs) {
  for (unsigned i = 0; i <= 64 - 6; i += 6)
    os << Card((cs >> i) & 0x3F);
  return os;
}

const std::string pretty_hand(uint64_t b, bool value) {

  std::string s = "\n";

  if (value)
    s += "    | A | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | T | J | Q | K\n";

  s += "    +---+---+---+---+---+---+---+---+---+---+---+---+---+\n";

  for (int r = 3; r >= 0; --r) {
    s += value ? std::string("    ") : std::string("   ") + "dhcs"[r];

    for (int f = 0; f < 13; ++f)
      s += b & (1ULL << ((r * 16) + f)) ? "| X " : "|   ";

    s += "|\n    +---+---+---+---+---+---+---+---+---+---+---+---+---+\n";
  }

  return s;
}
