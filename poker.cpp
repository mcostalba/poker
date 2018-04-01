#include <algorithm>
#include <cstdint>
#include <cstring>
#include <ctype.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "poker.h"

using namespace std;

typedef vector<Card> CardVec;

static bool parse_cards(const string &token, Card **ptr, Card *end, CardVec &all) {

  const string Values = "23456789TJQKA";
  const string Colors = "dhcs";
  size_t v, c;

  // Should be even number of chars (2 per card) and not too many
  if (token.length() % 2 || *ptr + token.length() / 2 > end)
    return false;

  for (size_t i = 0; i < token.length(); i += 2) {

    if ((v = Values.find(token[i])) == string::npos ||
        (c = Colors.find(token[i + 1])) == string::npos)
      return false;

    Card card = Card(16 * c + v);

    if (find(all.begin(), all.end(), card) != all.end())
      return false; // Duplicated card

    all.push_back(card);
    *(*ptr)++ = card;
  }
  return true;
}

// Initializes a spot from a given setup like:
//
//   go 4P AcTc TdTh - 5h 6h 9c
//
// That is 4 players, first 2 with AcTc and TdTh and
// with flopped common cards 5h 6h 9c.
Spot::Spot(const std::string &pos) {

  CardVec all;
  string token;
  stringstream ss(pos);

  memset(holes, 0, sizeof(holes));
  memset(commons, 0, sizeof(commons));

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
  int n = -1;
  Card *ptr, *end;
  while (ss >> token && token != "-") {
    ptr = holes[++n], end = ptr + 2;
    if (!parse_cards(token, &ptr, end, all))
      return;
  }

  // Then common cards up to 5, split or in a single token
  ptr = commons, end = ptr + 5;
  while (ss >> token)
    if (!parse_cards(token, &ptr, end, all))
      return;

  ready = true;
}
