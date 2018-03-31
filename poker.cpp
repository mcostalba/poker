#include <algorithm>
#include <iostream>
#include <vector>

#include "poker.h"
#include "util.h"

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
