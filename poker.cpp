#include <algorithm>
#include <cstdint>
#include <cstring>
#include <ctype.h>
#include <iostream>
#include <sstream>
#include <string>

#include "poker.h"

using namespace std;

static bool parse_cards(const string &token, Hand &h, Hand &all, unsigned max) {

  const string Values = "23456789TJQKA";
  const string Colors = "dhcs";
  size_t v, c;

  // Should be even number of chars (2 per card) and not exceeding the max
  if (token.length() % 2 || popcount(h.cards) + token.length() / 2 > max)
    return false;

  for (size_t i = 0; i < token.length(); i += 2) {

    if ((v = Values.find(token[i])) == string::npos ||
        (c = Colors.find(token[i + 1])) == string::npos)
      return false;

    Card card = Card(16 * c + v);

    if (!all.add(card, 0))
      return false;

    if (!h.add(card, 0)) {
      assert(false);
      return false;
    }
  }
  return true;
}

// Initializes a spot from a given setup like:
//
//   4P AcTc TdTh - 5h 6h 9c
//
// That is 4 players, first 2 with AcTc and TdTh and
// with flopped common cards 5h 6h 9c.
Spot::Spot(const std::string &pos) {

  Hand all = Hand();
  string token;
  stringstream ss(pos);

  memset(fill, 0, sizeof(fill));
  memset(givenHoles, 0, sizeof(givenHoles));
  memset(hands, 0, sizeof(hands));

  givenCommon = Hand();
  givenCommon.colors = ColorInit; // Only givenCommon is set
  prng = nullptr;
  ready = false;

  ss >> skipws >> token;

  // Parse 4P -> 4 players table
  if (token.length() != 2 || tolower(token[1]) != 'p')
    return;

  numPlayers = token[0] - '0';
  if (numPlayers < 1 || numPlayers > 9)
    return;

  bool singlePos = (numPlayers == 1);

  // Parse spot setup like AcTc TdTh - 5h 6h 9c
  //
  // Or single positions like Ac Tc Td Th 5h 6h 9c
  //
  // First hole cards. One token per player, up to 2 cards per token
  if (!singlePos) {
      int n = -1, *f = fill;
      while (ss >> token && token != "-") {
          if (!parse_cards(token, givenHoles[++n], all, 2))
              return;

          // Populate fill vector with missing cards to reach hole number
          for (int i = 0; i < 2 - popcount(givenHoles[n].cards); ++i)
              *f++ = n;
      }
      // Populate fill vector for missing players
      for (int i = n + 1; i < int(numPlayers); ++i)
          *f++ = i, *f++ = i;
      *f = -1; // EOL
  }

  // Then common cards up to 5 (or 7 in case of single position),
  // split or in a single token.
  while (ss >> token)
    if (!parse_cards(token, givenCommon, all, singlePos ? 7 : 5))
      return;

  allMask = all.cards | FLAGS_AREA;
  commonsNum = popcount(givenCommon.cards);
  if (singlePos)
      givenCommon.do_score(); // Single position
  ready = true;
}

// Run a single spot and update results vector
void Spot::run(unsigned results[]) {

  uint64_t maxScore = 0;
  bool split = false;
  Hand common = givenCommon;

  unsigned cnt = 5 - commonsNum;
  while (cnt) {
    uint64_t n = prng->next();
    for (unsigned i = 0; i <= 64 - 6; i += 6)
      if (common.add(Card((n >> i) & 0x3F), allMask) && --cnt == 0)
        break;
  }

  for (size_t i = 0; i < numPlayers; ++i) {
    hands[i] = common;
    hands[i].merge(givenHoles[i]);
  }

  const int *f = fill;
  while (*f != -1) {
    uint64_t n = prng->next();
    for (unsigned i = 0; i <= 64 - 6; i += 6)
      if (hands[*f].add(Card((n >> i) & 0x3F), allMask) && *(++f) == -1)
        break;
  }

  for (size_t i = 0; i < numPlayers; ++i) {

    hands[i].do_score();

    if (maxScore < hands[i].score) {
      maxScore = hands[i].score;
      split = false;
    } else if (maxScore == hands[i].score)
      split = true;
  }

  // Credit the winner 2 points, split result 1 point
  for (size_t i = 0; i < numPlayers; ++i) {
    if (hands[i].score == maxScore)
      results[i] += split ? 1 : 2;
  }
}

void enumerate(int missing, vector<uint64_t> &buf, vector<int> &set,
               unsigned commons, uint64_t all) {

  if (missing == 0) {
    if (commons) {
      uint64_t n = 0;
      for (size_t i = 0; i < commons; ++i)
        n = (n << 6) + set[i];
      buf.push_back(n);
    }

    if (set.size() > commons) {
      uint64_t n = 0;
      for (size_t i = commons; i < set.size(); ++i)
        n = (n << 6) + set[i];
      buf.push_back(n);
    }
    return;
  }

  for (int c = 0; c < 64; ++c) {

    uint64_t n = 1ULL << c;

    if (all & n)
      continue;

    set[set.size() - missing] = c;

    all |= n;
    enumerate(missing - 1, buf, set, commons, all);
    all ^= n;
  }
}

size_t Spot::set_enumerate_mode() {

  int given = popcount(allMask & ~FLAGS_AREA);
  int missing = 5 + 2 * numPlayers - given;
  int deck = 52 - given;
  vector<int> set(missing);

  if (missing > 4) {
      cout << "Missing too many cards (" << missing << "), max is 4" << endl;
      return 0;
  }

  int num = 1;
  for (int i = 0; i < missing; ++i)
    num *= deck--;

  enumerate(missing, enumBuf, set, 5 - commonsNum, allMask);
  return num;
}
