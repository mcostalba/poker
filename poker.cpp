#include <iostream>

#include "poker.h"

std::ostream &operator<<(std::ostream &os, Card c);
std::ostream &operator<<(std::ostream &os, Card64 cs);
const std::string pretty_hand(uint64_t b, bool value);

void test() {

  static const int Limit = 7;

  PRNG::init();
  Hand hands[30];

  for (auto &h : hands) {

    h = {0, 0};
    int cnt = 0;

    while (cnt < Limit) {

      Card64 c = Card64(PRNG::next()); // Cards are pseudo-legal

      std::cout << c << "\n";

      h.add<Limit>(c, cnt);
    }

    std::cout << pretty_hand(h.value, true) << "\n"
              << pretty_hand(h.color, false) << "\n";
  }

  std::cout << std::endl;
}
