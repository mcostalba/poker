#include <algorithm>
#include <cstdint>
#include <cstring>
#include <ctype.h>
#include <iostream>
#include <sstream>
#include <string>

#include "poker.h"

using namespace std;

// Parse a string token with one or more consecutive cards into a Hand
static bool parse_cards(const string& token, Hand& h, Hand& all, unsigned max)
{
    const string Values = "23456789TJQKA";
    const string Suites = "dhcs";
    size_t v, c;

    // Should be even number of chars (2 per card) and not exceeding the max
    if (token.length() % 2 || popcount(h.cards) + token.length() / 2 > max)
        return false;

    for (size_t i = 0; i < token.length(); i += 2) {

        if (   (v = Values.find(token[i])) == string::npos
            || (c = Suites.find(token[i + 1])) == string::npos)
            return false;

        Card card = Card(16 * c + v);

        if (!all.add(card, 0)) // Double card
            return false;

        if (!h.add(card, 0)) {
            assert(false);
            return false;
        }
    }
    return true;
}

/// Initialize a Spot from a given string like:
///
///  4P AcTc TdTh - 5h 6h 9c
///  1P Ac Tc Td Th 5h 6h 9c
///
/// First is 4 players, first 2 with AcTc and TdTh and with board cards 5h 6h 9c.
/// Second is a 7-card hand evaluation.
Spot::Spot(const std::string& pos)
{
    Hand all = Hand();
    string token;
    stringstream ss(pos);

    memset(missingHolesId, 0, sizeof(missingHolesId));
    memset(givenHoles, 0, sizeof(givenHoles));
    memset(hands, 0, sizeof(hands));

    givenCommon = Hand();
    givenCommon.suits = SuitInit; // Only givenCommon is set with SuitInit
    prng = nullptr;
    enumMask = 0;
    ready = false;

    ss >> skipws >> token;

    // Parse number of players
    if (token.length() != 2 || tolower(token[1]) != 'p')
        return;

    numPlayers = token[0] - '0';
    if (numPlayers < 1 || numPlayers > 9)
        return;

    // In case of a 7 cards fixed hand evaluation players are set to 1
    bool fixedHand = (numPlayers == 1);

    // First hole cards. One token per player, up to 2 cards per token
    if (!fixedHand) {
        int n = -1, *mi = missingHolesId;
        while (ss >> token && token != "-") {
            if (!parse_cards(token, givenHoles[++n], all, 2))
                return;

            assert(givenHoles[n].cards);

            // Add to missingHolesId[] the hole's index for the missing card
            // and update enumMask setting this hole's group boundary.
            if (popcount(givenHoles[n].cards) < 2) {
                *mi++ = n;
                enumMask = (enumMask << 1) | 1;
            }
        }
        // Populate missingHolesId[] and enumMask for the missing hole card
        // pairs up to the number of given players.
        for (int i = n + 1; i < int(numPlayers); ++i) {
            *mi++ = i, *mi++ = i;
            enumMask = (enumMask << 2) | 2;
        }
        *mi = -1; // Set EOF
    }

    // Then remaining common cards up to 5 (or 7 in case of a fixed hand)
    while (ss >> token)
        if (!parse_cards(token, givenCommon, all, fixedHand ? 7 : 5))
            return;

    if (fixedHand) {
        if (popcount(givenCommon.cards) != 7)
            return;

        missingCommons = enumMask = 0;
        givenCommon.do_score(); // Single position evaluation
    } else {
        missingCommons = 5 - popcount(givenCommon.cards);
        if (missingCommons) {
            unsigned v = 1 << (missingCommons - 1);
            enumMask = (enumMask << missingCommons) | v;
        }
    }
    allMask = all.cards | FlagsArea;
    ready = true;
}

/// Run a single spot and update results vector. First generate common cards,
/// then hole cards. Finally score the hands and find the max among them.
void Spot::run(Result results[])
{
    unsigned maxId = 0, split = 0;
    uint64_t maxScore = 0;
    Hand common = givenCommon;

    unsigned cnt = missingCommons;
    while (cnt) {
        uint64_t n = prng->next();
        for (unsigned i = 0; i <= 64 - 6; i += 6)
            if (common.add(Card((n >> i) & 0x3F), allMask) && --cnt == 0)
                break;
    }

    for (unsigned i = 0; i < numPlayers; ++i) {
        hands[i] = common;
        hands[i].merge(givenHoles[i]);
    }

    const int* mi = missingHolesId;
    while (*mi != -1) {
        uint64_t n = prng->next();
        for (unsigned i = 0; i <= 64 - 6; i += 6)
            if (hands[*mi].add(Card((n >> i) & 0x3F), allMask) && *(++mi) == -1)
                break;
    }

    for (unsigned i = 0; i < numPlayers; ++i) {
        hands[i].do_score();
        if (maxScore < hands[i].score) {
            maxScore = hands[i].score;
            maxId = i;
            split = 0;
        } else if (maxScore == hands[i].score)
            split++;
    }

    if (!split)
        results[maxId].first++;
    else
        for (unsigned i = 0; i < numPlayers; ++i) {
            if (hands[i].score == maxScore)
                results[i].second += KTie / (split + 1);
        }
}

/// Recursively compute all possible combinations (not permutations) of missing
/// cards for each hole group and common cards. Then add the cards to enumBuf
/// from where Spot::run() will fetch instead of using the PRNG. We push one
/// uint64_t (that can pack up to 10 cards) for the missing commons cards and
/// one for the missing hole cards.
void Spot::enumerate(unsigned missing, uint64_t cards, int limit,
    unsigned missingHoles)
{
    // At group boundaries enumMask is 1. We reset to 64 in this case
    unsigned end = (enumMask & (1 << (missing - 1))) ? 64 : limit;
    cards <<= 6;

    for (unsigned c = 0; c < end; ++c) {

        uint64_t n = 1ULL << c;

        if (allMask & n)
            continue;

        cards += c; // Append the new card

        if (missing == 1) {
            if (missingCommons) {
                unsigned mask = (1 << (6 * missingCommons)) - 1;
                enumBuf.push_back(cards & mask);
            }
            if (missingHoles)
                enumBuf.push_back(cards >> (6 * missingCommons));
        } else {
            allMask |= n;
            enumerate(missing - 1, cards, c, missingHoles);
            allMask ^= n;
        }
        cards -= c;
    }
}

/// Setup to run a full enumeration instead of the Monte Carlo simulation. This
/// is possible when the number of missing cards is limited. Full enumeration is
/// implemented in 2 steps: first all the combinations for the missing cards are
/// computed and saved in enumBuf, then Spot::run() is called as usual, but
/// instead of fetching cards from the PRNG, it will fetch from enumBuf. Here
/// we implement the first step: computation of all the possible combinations.
size_t Spot::set_enumerate_mode()
{
    unsigned given = popcount(allMask & ~FlagsArea);
    unsigned missing = 5 + 2 * numPlayers - given;
    unsigned missingHoles = missing - missingCommons;

    if (missing == 0)
        return 0;

    if (missing > 5) {
        cout << "Missing too many cards (" << missing << "), max is 5" << endl;
        return 0;
    }
    enumBuf.clear();
    enumerate(missing, 0, 64, missingHoles);
    size_t gamesNum = enumBuf.size();

    // We have 2 entries (instead of 1) for a single game in enumBuf in case
    // both some common and hole cards are missing.
    if (missingCommons && missingHoles)
        gamesNum /= 2;

    cout << "Evaluating " << gamesNum << " combinations..." << endl;
    return gamesNum;
}
