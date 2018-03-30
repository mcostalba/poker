#include <cassert>
#include <iostream>
#include <string>

const std::string pretty_hand(uint64_t b, bool value) {

  std::string s = "\n";

  if (value)
    s += "    | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | T | J | Q | K | A \n";

  s += "    +---+---+---+---+---+---+---+---+---+---+---+---+---+\n";

  for (int r = 3; r >= 0; --r) {
    s += value ? std::string("    ") : std::string("   ") + "dhcs"[r];

    for (int f = 0; f < 13; ++f)
      s += b & (1ULL << ((r * 16) + f)) ? "| X " : "|   ";

    s += "|\n    +---+---+---+---+---+---+---+---+---+---+---+---+---+\n";
  }

  return s;
}
