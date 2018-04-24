#include <algorithm>
#include <cstdint>
#include <cstring>
#include <ctype.h>
#include <iostream>
#include <set>
#include <sstream>
#include <string>

#include "poker.h"

using namespace std;

// Needed by std::set, not to compare scores!
static bool operator<(const Hand& h1, const Hand& h2)
{
    return h1.cards < h2.cards;
}

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

// Expand tokens like T6s+, 88+, 52o+, AA, AK, QQ-99, T7s-T3s, J8-52 in the
// group of corresponding pair of cards (combos).
static bool expand(const string& token, set<Hand>& ranges)
{
    enum SuitFilter { AnySuit, Suited, Offsuited };

    const string Values = "23456789TJQKA";
    const string Suites = "dhcs";
    const string SO = "so";

    size_t v1, v2, v3 = string::npos, v4 = string::npos;
    SuitFilter f = AnySuit, f2 = AnySuit;
    bool plus, plus2, range;
    size_t next = 0;

    if (   token.length() < 2
        || (v1 = Values.find(token[next++])) == string::npos
        || (v2 = Values.find(token[next++])) == string::npos
        || v1 < v2)
        return false;

    size_t s = token.length() > next ? SO.find(token[next]) : string::npos;
    if (s != string::npos) {
        f = (SO[s] == 's' ? Suited : Offsuited);
        next++;
    }

    plus = token.length() > next && token[next] == '+';
    if (plus)
        next++;

    range = token.length() > next && token[next] == '-';
    if (range)
        next++;

    if ((v1 == v2 && f != AnySuit) || (plus && range))
        return false;

    if (range) {
        if (   token.length() < next + 2
            || (v3 = Values.find(token[next++])) == string::npos
            || (v4 = Values.find(token[next++])) == string::npos
            || v3 < v4 || v1 < v3 || v2 < v4)
            return false;

        if (v1 != v3 && (v1 - v2) != (v3 - v4))
            return false;

        s = token.length() > next ? SO.find(token[next]) : string::npos;
        if (s != string::npos) {
            f2 = (SO[s] == 's' ? Suited : Offsuited);
            next++;
        }
        plus2 = token.length() > next && token[next] == '+';

        if (plus != plus2 || (f != f2))
            return false;
    }

    while (true) {
        for (auto c1 : Suites)
            for (auto c2 : Suites) {
                if (v1 == v2 && c2 >= c1)
                    continue;
                if (   (f == Suited && c1 != c2)
                    || (f == Offsuited && c1 == c2))
                    continue;

                Hand h = Hand(), all = Hand();
                string card = Values[v1] + string(1, c1) + Values[v2] + string(1, c2);
                if (!parse_cards(card, h, all, 2))
                    return false;
                ranges.insert(h); // Insert if not already exsisting
            }

        if (range && v2 > v4) {
            if (v1 != v3)
                v1--, v2--;
            else
                v2--;
        } else if (!plus)
            break;
        else if (v1 == v2 && Values[v1] != 'A')
            v1++, v2++;
        else if (v2 + 1 < v1)
            v2++;
        else
            break;
    }
    return true;
}

// Parse a string token with a list of ranges like '[AK,88+,76s+]' or a single
// one like 'QQ+' into a set of hands, each one of 2 hole cards.
bool Spot::parse_range(const string& token, int player)
{
    bool hasBrackets = (token.front() == '[' && token.back() == ']');
    bool isList = (token.find(",") != string::npos);
    if (!hasBrackets && isList)
        return false;

    string combo;
    set<Hand> handSet; // Use a set to avoid duplicates
    size_t cnt = 0;
    stringstream ss(hasBrackets ? token.substr(1, token.size() - 2) : token);

    while (std::getline(ss, combo, ','))
        if (!expand(combo, handSet))
            return false;

    if (handSet.empty() || handSet.size() > MAX_RANGE)
        return false;

    // Duplicate the handset into multiple full copies in combos[]. We will pick
    // randomly from there at simulation time.
    for (size_t i = 0; i < MAX_RANGE / handSet.size(); ++i)
        for (const Hand& h : handSet)
            combos[player][cnt++] = h;

    // Fill remaining with an invalid hand so that (invalid & allMask) is true
    Hand invalid;
    invalid.cards = ~uint64_t(0);
    while (cnt < MAX_RANGE)
        combos[player][cnt++] = invalid;

    cout << "Set range " << token << " for player " << player + 1
         << " of size: " << handSet.size() << endl;

    return true;
}

/// Initialize a Spot from a given string like:
///
///  4P AcTc TdTh - 5h 6h 9c
///  3P [AA,QQ-99,AKs,T7s-T3s,AKo] [88+,T6s+,52o+] TT+
///
Spot::Spot(const std::string& pos)
{
    Hand all = Hand();
    string token;
    stringstream ss(pos);

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
    if (numPlayers < 2 || numPlayers > 9)
        return;

    // First hole cards. One token per player, up to 2 cards per token
    int n = -1, *mi = missingHolesId, *ci = combosId;
    while (ss >> token && token != "-") {
        if (!parse_cards(token, givenHoles[++n], all, 2)
            && !parse_range(token, n))
            return;

        // In case of a range givenHoles[n] remains empty
        if (!givenHoles[n].cards)
            *ci++ = n;

        // Add to missingHolesId[] the hole's index for the missing card
        // and update enumMask setting this hole's group boundary.
        if (givenHoles[n].cards && popcount(givenHoles[n].cards) < 2) {
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
    *mi = -1, *ci = -1; // Set EOF

    // Then remaining common cards up to 5 (or 7 in case of a fixed hand)
    while (ss >> token)
        if (!parse_cards(token, givenCommon, all, 5))
            return;

    missingCommons = 5 - popcount(givenCommon.cards);
    if (missingCommons) {
        unsigned v = 1 << (missingCommons - 1);
        enumMask = (enumMask << missingCommons) | v;
    }
    givenAllMask = all.cards | FlagsArea;
    ready = true;
}

/// Run a single spot and update results vector. First generate hole cards for
/// given ranges, then common cards, then free hole cards. Finally score the
/// hands and find the max among them.
void Spot::run(Result results[])
{
    unsigned maxId = 0, split = 0;
    uint64_t maxScore = 0;
    Hand common = givenCommon;
    uint64_t allMask = givenAllMask;

    // First generate givenHoles instances out of the given ranges, if any
    const int* ci = combosId;
    while (*ci != -1) {
        uint64_t n = prng->next();
        for (unsigned i = 0; i <= 64 - 9; i += 9) {
            givenHoles[*ci] = combos[*ci][(n >> i) & 0x1FF];
            if (givenHoles[*ci].cards & allMask)
                continue;
            allMask |= givenHoles[*ci].cards;
            if (*(++ci) == -1)
                break;
        }
    }

    // Then build the common board
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

    // Finally fill the missing hole cards (single or double)
    const int* mi = missingHolesId;
    while (*mi != -1) {
        uint64_t n = prng->next();
        for (unsigned i = 0; i <= 64 - 6; i += 6)
            if (hands[*mi].add(Card((n >> i) & 0x3F), allMask) && *(++mi) == -1)
                break;
    }

    // Now we are ready to score hands and find the winner
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
void Spot::enumerate(std::vector<uint64_t>& buf, unsigned missing,
                     uint64_t cards, int limit, unsigned missingHoles,
                     size_t idx, size_t threadsNum)
{
    // At group boundaries enumMask is 1. We reset to 64 in this case
    unsigned end = (enumMask & (1 << (missing - 1))) ? 64 : limit;
    cards <<= 6;

    for (unsigned c = 0; c < end; ++c) {

        // Split the cards among the threads. Only at root level
        if (threadsNum && idx != (c % threadsNum))
            continue;

        uint64_t n = 1ULL << c;

        if (givenAllMask & n) // FIXME not compatible with ranges
            continue;

        cards += c; // Append the new card

        if (missing == 1) {
            if (missingCommons) {
                unsigned mask = (1 << (6 * missingCommons)) - 1;
                buf.push_back(cards & mask);
            }
            if (missingHoles)
                buf.push_back(cards >> (6 * missingCommons));
        } else {
            givenAllMask |= n;
            enumerate(buf, missing - 1, cards, c, missingHoles, idx, 0);
            givenAllMask ^= n;
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
size_t Spot::set_enumerate(std::vector<uint64_t>& enumBuf,
                           size_t idx, size_t threadsNum)
{
    unsigned given = popcount(givenAllMask & ~FlagsArea);
    unsigned missing = 5 + 2 * numPlayers - given;
    unsigned missingHoles = missing - missingCommons;

    if (missing == 0)
        return 0;

    if (missing > 5) {
        cout << "Missing too many cards (" << missing << "), max is 5" << endl;
        return 0;
    }
    enumBuf.clear();
    enumerate(enumBuf, missing, 0, 64, missingHoles, idx, threadsNum);
    size_t gamesNum = enumBuf.size();

    // We have 2 entries (instead of 1) for a single game in enumBuf in case
    // both some common and hole cards are missing.
    if (missingCommons && missingHoles)
        gamesNum /= 2;

    cout << "Evaluating " << gamesNum << " combinations..." << endl;
    return gamesNum;
}
