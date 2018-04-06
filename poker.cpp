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
  if (numPlayers < 2 || numPlayers > 9)
    return;

  // Parse spot setup like AcTc TdTh - 5h 6h 9c
  //
  // First hole cards. One token per player, up to 2 cards per token
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

  // Then common cards up to 5, split or in a single token
  while (ss >> token)
    if (!parse_cards(token, givenCommon, all, 5))
      return;

  commonsNum = popcount(givenCommon.cards);
  allMask = all.cards | FLAGS_AREA;
  ready = true;
}

// Run a single spot and update results vector
void Spot::run(unsigned results[]) {

  uint64_t maxScore = 0;
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

    if (maxScore < hands[i].score)
      maxScore = hands[i].score;
  }

  // Credit the winner, considering split results
  for (size_t i = 0; i < numPlayers; ++i) {
    if (hands[i].score == maxScore)
      results[i]++;
  }
}

template <int N>
void enumerate(vector<uint64_t> &buf, vector<int> &set, unsigned commons,
               uint64_t all) {

  for (int c = 0; c < 64; ++c) {

    uint64_t n = 1ULL << c;

    if (all & n)
      continue;

    set[set.size() - N] = c;

    all |= n;
    enumerate<N - 1>(buf, set, commons, all);
    all ^= n;
  }
}

template <>
void enumerate<0>(vector<uint64_t> &buf, vector<int> &set, unsigned commons,
                  uint64_t) {

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
}

size_t Spot::set_enumerate_mode() {

  int given = popcount(allMask & ~FLAGS_AREA);
  int missing = 5 + 2 * numPlayers - given;
  int deck = 52 - given;
  vector<int> set(missing);

  int num = 1;
  for (int i = 0; i < missing; ++i)
    num *= deck--;

  cout << "Generating " << num << " combinations for " << missing
       << " missing cards...";

  switch (missing) {
  case 0:
    break;
  case 1:
    enumerate<1>(enumBuf, set, 5 - commonsNum, allMask);
    break;
  case 2:
    enumerate<2>(enumBuf, set, 5 - commonsNum, allMask);
    break;
  case 3:
    enumerate<3>(enumBuf, set, 5 - commonsNum, allMask);
    break;
  case 4:
    enumerate<4>(enumBuf, set, 5 - commonsNum, allMask);
    break;
  default:
    assert(false);
  }

  cout << "done!" << endl;
  return num;
}
