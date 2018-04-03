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
  if (token.length() % 2 || popcount(h.colors) + token.length() / 2 > max)
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
    for (int i = 0; i < 2 - popcount(givenHoles[n].colors); ++i)
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

  commonsNum = popcount(givenCommon.colors);
  allMask = all.colors;
  ready = true;
}

// Run a single spot and update results vector
void Spot::run(unsigned results[]) {

  uint64_t maxScore = 0;
  Hand common = givenCommon;

  unsigned cnt = 5 - commonsNum;
  while (cnt) {
    uint64_t n = PRNG::next();
    for (unsigned i = 0; i <= 64 - 6; i += 6) {
      if (common.add(Card((n >> i) & 0x3F), allMask) && --cnt == 0)
        break;
    }
  }

  for (size_t i = 0; i < numPlayers; ++i) {
    hands[i] = common;
    hands[i].merge(givenHoles[i]);
  }

  const int *f = fill;
  while (*f != -1) {
    uint64_t n = PRNG::next();
    for (unsigned i = 0; i <= 64 - 6; i += 6) {
      if (hands[*f].add(Card((n >> i) & 0x3F), allMask) && *(++f) == -1)
        break;
    }
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
