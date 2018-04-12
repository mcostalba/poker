#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

#include "poker.h"
#include "util.h"

using namespace std;

/// ScoreMask contains 1248 masks for each combination of 2 cards c1, c2 in range
/// [0..63] with c1 > c2 and with c1, c2 of different face value (2,3..K,A).
/// ScoreMask is indexed by (c1 << 6) + c2. ScoreMask bitwise AND the hand score
/// to "fix" it for some special cases.
uint64_t ScoreMask[4096];

namespace {

// Positions used by bench
const vector<string> Defaults = {
    "2P 3d",
    "3P KhKs - Ac Ad 7c Ts Qs",
    "4P AcTc TdTh - 5h 6h 9c",
    "5P 2c3d KsTc AhTd - 4d 5d 9c 9d",
    "6P Ac Ad KsKd 3c - 2c 2h 7c 7h 8c",
    "7P Ad Kc QhJh 3s4s - 2c 2h 7c 5h 8c",
    "8P - Ac Ah 3d 7h 8c",
    "9P",
    "4P AhAd AcTh 7c6s 2h3h - 2c 3c 4c",
    "4P AhAd AcTh 7c6s 2h3h",
};

typedef chrono::milliseconds::rep TimePoint; // A value in milliseconds

TimePoint now()
{
    return chrono::duration_cast<chrono::milliseconds>(
        chrono::steady_clock::now().time_since_epoch())
        .count();
}

// Quick hash, see https://stackoverflow.com/questions/13325125/
// lightweight-8-byte-hash-function-algorithm
struct Hash {

    static const uint64_t Mulp = 2654435789;
    uint64_t mix = 104395301;

    void operator<<(unsigned v) { mix += (v * Mulp) ^ (mix >> 23); }
    uint64_t get() { return mix ^ (mix << 37); }
};

class Thread {

    size_t idx;
    PRNG prng;
    Spot spot;
    size_t gamesNum;
    std::thread* th;
    Result results[PLAYERS_NB];
    std::vector<uint64_t> enumBuf;

public:
    Result result(size_t p) const { return results[p]; }

    Thread(size_t id, const Spot& s, size_t n, size_t threadsNum, bool enumerate)
        : idx(id)
        , prng(id)
        , spot(s)
        , gamesNum(n)
    {
        memset(results, 0, sizeof(results));
        spot.set_prng(&prng);

        // Launch a thread that will call immediately Thread::run()
        th = new std::thread(&Thread::run, this, enumerate, threadsNum);
    }

    void join()
    {
        th->join();
        delete th;
    }

    void run(bool enumerate, size_t threadsNum)
    {
        if (enumerate) {
            gamesNum = spot.set_enumerate(enumBuf, idx, threadsNum);
            if (!gamesNum)
                return;
            prng.set_enum_buffer(enumBuf.data());
        }
        for (size_t i = 0; i < gamesNum; i++)
            spot.run(results);
    }
};

// Helpers used by init_score_mask()
constexpr uint64_t set_counter(unsigned n)
{
    return n << 13;
}
uint64_t clear_below(uint64_t b)
{
    return ~((b >> 16) | (b >> 32) | (b >> 48));
}
uint64_t clear_before(uint64_t b)
{
    unsigned r = msb(b) / 16; // Rank in [0..3]
    return ~((b - 1) & RanksBB[r]);
}

} // namespace

/// Create, run and retire threads of execution, needed data is passed through
/// the wrapping Thread object. New threads are created every time run is called.
void run(const Spot& s, size_t gamesNum, size_t threadsNum,
    bool enumerate, Result results[])
{
    std::vector<Thread*> threads; // Pointers because std::vector reallocates

    if (gamesNum < threadsNum)
        threadsNum = 1;

    size_t n = gamesNum / threadsNum;

    for (size_t i = 0; i < threadsNum; ++i)
        threads.push_back(new Thread(i, s, n, threadsNum, enumerate));

    for (Thread* th : threads) {
        th->join(); // Wait here for thread finished
        for (size_t p = 0; p < s.players(); ++p) {
            results[p].first += th->result(p).first;
            results[p].second += th->result(p).second;
        }
        delete th;
    }
}

/// Populate ScoreMask[] at startup. Table is indexed by the 2 highest bits of
/// the score value that correspond to the hand's best combination (for instance
/// a set and a pair). Given these 2 keys, the table is built such that a bitwise
/// AND with the score produces the following:
///
/// - Clear all the bits below in the column of the 2 highest ones
///
/// - Preserve/clear some flag bit according to the combination
///
/// - Set the number of bits that should remain in score's first rank, so that
///   the score uses just the best 5 cards out of 7.
///
void init_score_mask()
{
    // Fixed mask to clear the 3-bit counter and some flags that eventually
    // will be re-added when neded on specific cases (like double pair).
    constexpr uint64_t Init = ~(FullHouseBB | DoublePairBB | set_counter(7));

    for (unsigned c1 = 0; c1 < 64; c1++) {

        if ((c1 & 0xF) >= INVALID)
            continue;

        for (unsigned c2 = 0; c2 < c1; c2++) {

            if ((c2 & 0xF) >= INVALID)
                continue;

            // When used in scoring, the 2 key bits always correspond to cards
            // of different face value, so skip this case.
            if ((c1 & 0xF) == (c2 & 0xF))
                continue;

            unsigned idx = (c1 << 6) + c2;

            uint64_t h = 1ULL << c1;
            uint64_t l = 1ULL << c2;

            // Init and clear the columns below the 2 most significant bits
            ScoreMask[idx] = Init & clear_below(h) & clear_below(l);

            // High card. Set counter to pick the 5 msb bits in score's first rank
            if (h & Rank1BB)
                ScoreMask[idx] |= set_counter(5);

            // Single pair, we just need highest 3 bit of score's first rank
            else if ((h & Rank2BB) && (l & Rank1BB))
                ScoreMask[idx] |= set_counter(3);

            // Double Pair. Use clear_before(l) to drop any possible third pair
            // that should not influence the score.
            else if ((h & Rank2BB) && (l & Rank2BB)) {
                ScoreMask[idx] &= clear_before(l);
                ScoreMask[idx] |= set_counter(1) | DoublePairBB;
            }
            // Single Set. Nothing fancy.
            else if ((h & Rank3BB) && (l & Rank1BB))
                ScoreMask[idx] |= set_counter(2);

            // Full house. Use clear_before(l) to drop any possible second pair
            // that should not influence the score.
            else if ((h & Rank3BB) && (l & Rank2BB)) {
                ScoreMask[idx] &= clear_before(l);
                ScoreMask[idx] |= set_counter(0) | FullHouseBB;
            }
            // Double set. It's a full house, second set is counted as a pair,
            // so use clear_before(h), not clear_before(l) as in double pair.
            else if ((h & Rank3BB) && (l & Rank3BB)) {
                ScoreMask[idx] &= clear_before(h);
                // Re-add the (shifted) bit dropped by clear_below(h, l)
                ScoreMask[idx] |= (l >> 16) | set_counter(0) | FullHouseBB;
            }
            // Quad. Drop anything but first rank. Re-add the bit on first
            // rank in the column of l, that was dropped by clear_below(h, l)
            else if ((h & Rank4BB)) {
                ScoreMask[idx] ^= ~clear_below(l); // Re-add bits below l
                ScoreMask[idx] &= ~(Rank3BB | Rank2BB);
                ScoreMask[idx] |= set_counter(1);
            } else
                assert(false);
        }
    }
}

const string pretty64(uint64_t b, bool headers)
{
    string s = "\n";
    string hstr = headers ? "\n" : "---+---+---+\n";

    if (headers)
        s += "    | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | T | J | Q | K | A \n";

    s += "    +---+---+---+---+---+---+---+---+---+---+---+---+---+" + hstr;

    for (int r = 3; r >= 0; --r) {
        s += headers ? string("   ") + "dhcs"[r] : string("    ");

        for (int f = 0; f < (headers ? 13 : 16); ++f)
            s += b & (1ULL << ((r * 16) + f)) ? "| X " : "|   ";

        s += "|\n    +---+---+---+---+---+---+---+---+---+---+---+---+---+" + hstr;
    }

    return s;
}

ostream& operator<<(ostream& os, Card c)
{
    if (c % 16 < INVALID)
        os << "23456789TJQKA"[c % 16] << "dhcs"[c / 16] << " ";
    else
        os << "-- ";
    return os;
}

ostream& operator<<(ostream& os, const Hand& h)
{
    vector<Card> cards;
    uint64_t v = h.cards;

    while (v)
        cards.push_back(Card(pop_lsb(&v)));

    // Sort the cards in descending face value
    auto comp = [](Card a, Card b) { return (a & 0xF) > (b & 0xF); };
    sort(cards.begin(), cards.end(), comp);

    os << "\n\nHand: ";
    for (Card c : cards)
        os << c;

    os << "\n" << pretty64(h.cards, true) << "\n";

    if (h.score)
        os << "\nScore:\n" << pretty64(h.score, false) << "\n";

    return os;
}

void pretty_results(Result* results, size_t players)
{
    size_t games = 0;
    for (size_t p = 0; p < players; p++)
        games += KTie * results[p].first + results[p].second;
    games /= KTie;

    cout << std::showpoint << std::noshowpos << std::fixed << std::setprecision(2)
         << "\n     Equity    Win     Tie   Pots won  Pots tied\n";

    for (size_t p = 0; p < players; p++) {
        cout << "P" << p + 1 << ": ";
        size_t equity = KTie * results[p].first + results[p].second;
        cout << std::setw(6) << equity * 100.0 / KTie / games << "% "
             << std::setw(6) << results[p].first * 100.0 / games << "% "
             << std::setw(6) << results[p].second * 100.0 / KTie / games << "% "
             << std::setw(9) << results[p].first << " "
             << std::setw(9) << double(results[p].second) / KTie << endl;
    }
}

void bench(istringstream& is)
{
    constexpr uint64_t GoodSig = 11714201772365687243ULL;
    constexpr int GamesNum = 1500 * 1000;

    Result results[PLAYERS_NB];
    string token;
    Hash sig;
    uint64_t cards = 0, spots = 0, cnt = 0;

    int threadsNum = (is >> token) ? stoi(token) : 1;

    TimePoint elapsed = now();

    for (const string& v : Defaults) {
        cerr << "\nPosition " << ++cnt << ": " << v << endl;
        memset(results, 0, sizeof(results));
        Spot s(v);
        run(s, GamesNum, threadsNum, false, results);

        for (size_t p = 0; p < s.players(); ++p)
            sig << results[p].first + results[p].second;

        pretty_results(results, s.players());

        cards += GamesNum * (s.players() * 2 + 5);
        spots += GamesNum;
    }

    elapsed = now() - elapsed + 1; // Ensure positivity to avoid a 'divide by zero'

    cerr << "\n==========================="
         << "\nTotal time   : " << elapsed << " msec"
         << "\nSpots played : " << spots / 1000000 << "M"
         << "\nCards/second : " << 1000 * cards / elapsed
         << "\nGames/second : " << 1000 * spots / elapsed
         << "\nSignature    : " << sig.get();

    if (sig.get() == GoodSig)
        cerr << " (OK)";
    else if (threadsNum == 1)
        cerr << " (FAIL)";

    cerr << endl;
}
