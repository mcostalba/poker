
#ifndef POKER_H_INCLUDED
#define POKER_H_INCLUDED

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "util.h"

extern uint64_t ScoreMask[4096];
extern void init_score_mask();

enum Card : unsigned { INVALID = 13 };

constexpr int PLAYERS_NB = 9;
constexpr int HOLE_NB    = 2;
constexpr int MAX_RANGE  = 1 << 9;

// Bitboards representing ranks/rows
constexpr uint64_t Rank1BB = 0xFFFFULL << (16 * 0);
constexpr uint64_t Rank2BB = 0xFFFFULL << (16 * 1);
constexpr uint64_t Rank3BB = 0xFFFFULL << (16 * 2);
constexpr uint64_t Rank4BB = 0xFFFFULL << (16 * 3);

constexpr uint64_t RanksBB[] = { Rank1BB, Rank2BB, Rank3BB, Rank4BB };

// Bitboard representing the area of the 'score' reserved for flags
constexpr uint64_t Last3 = 0xE000;
constexpr uint64_t FlagsArea = Last3 | (Last3 << 16) | (Last3 << 32) | (Last3 << 48);

// Alter the score according to combination type. Needed only for few cases,
// when native score value is not enough. We use the the score's flags area.
constexpr uint64_t StraightFlushBB = 1ULL << (16 * 3 + 15);
constexpr uint64_t FullHouseBB     = 1ULL << (16 * 2 + 15);
constexpr uint64_t FlushBB         = 1ULL << (16 * 2 + 14);
constexpr uint64_t StraightBB      = 1ULL << (16 * 2 + 13);
constexpr uint64_t DoublePairBB    = 1ULL << (16 * 1 + 13);

// Flush detector: a 32 bit integer split in 4 slots (4 bit  each), each one
// inited at 3, and we add 1 for every card according to card's suit. If one
// slot reaches 8, then we have a flush.
constexpr uint32_t SuitInit  =   3 | (3 << 4) | (3 << 8) | (3 << 12);
constexpr uint32_t SuitAdd[] = { 1 , (1 << 4) , (1 << 8) , (1 << 12) };
constexpr uint32_t IsFlush   =   8 | (8 << 4) | (8 << 8) | (8 << 12);

struct Hand {

    uint64_t score;
    uint64_t cards;
    uint32_t suits;

    friend std::ostream& operator<<(std::ostream&, const Hand&);

    bool add(Card c, uint64_t allMask)
    {
        uint64_t n = 1ULL << c;

        if ((cards | allMask) & n) // Double card or invalid
            return false;

        cards |= n;
        n = 1 << (c & 0xF);

        suits += SuitAdd[(c & 0x30) >> 4];

        while (score & n)
            n <<= 16;

        score |= n;
        return true;
    }

    void merge(const Hand& holes)
    {
        if ((score & holes.score) == 0) { // Common case
            score |= holes.score;
            cards |= holes.cards;
            suits += holes.suits;
            return;
        }
        // We are unlucky: add one by one
        uint64_t v = holes.cards;
        while (v)
            add(Card(pop_lsb(&v)), 0);
    }

    void do_score()
    {
        if (suits & IsFlush) {
            unsigned r = lsb(suits & IsFlush) / 4;
            score = FlushBB | ((cards & RanksBB[r]) >> (16 * r));
        }

        // Check for a straight
        uint64_t v = score & Rank1BB;
        v = (v << 1) | (v >> 12); // Duplicate an ace into first position
        v &= v >> 1;
        v &= v >> 1;
        v &= v >> 2;
        if (v) {
            auto f = (score & FlushBB) ? StraightFlushBB : StraightBB;
            v = 1ULL << msb(v); // Could be more than 1 in case of straight > 5
            score = f | (v << 3) | (v << 2); // At least 2 bits needed by ScoreMask
        }

        // Drop all bits below the highest ones so that when calling 2 times
        // msb() we get bits on different files/columns.
        v = (score ^ (score >> 16)) & ~FlagsArea;

        // Mask out the score and get the final one
        unsigned cnt = pop_msb(&v) << 6;
        v = ScoreMask[cnt + msb(v)];
        score = (score | FullHouseBB | DoublePairBB) & v;

        // Drop the lowest cards so that only 5 remains
        cnt = (unsigned(v) >> 13) & 0x7;
        unsigned p = popcount(score & Rank1BB);
        while (p-- > cnt)
            score &= score - 1;
    }
};

class Spot {

    Hand combos[PLAYERS_NB][MAX_RANGE];
    int combosId[PLAYERS_NB + 1];
    int missingHolesId[PLAYERS_NB * HOLE_NB + 1];
    Hand givenHoles[PLAYERS_NB];
    Hand givenCommon;

    PRNG* prng;
    unsigned numPlayers;
    unsigned missingCommons;
    uint32_t enumMask;
    uint64_t givenAllMask;
    bool ready;

    void enumerate(std::vector<uint64_t>& enumBuf, unsigned missing,
                   uint64_t cards, int limit, unsigned missingHoles,
                   size_t idx, size_t threadsNum);
    bool parse_range(const std::string& token, int player);

public:
    Spot() = default;
    explicit Spot(const std::string& pos);
    void run(Result results[]);
    size_t set_enumerate(std::vector<uint64_t>&, size_t, size_t);

    bool valid() const { return ready; }
    uint64_t eval() const { return givenCommon.score; }
    size_t players() const { return numPlayers; }
    void set_prng(PRNG* p) { prng = p; }
};

extern void run(const Spot& s, size_t games, size_t threads, bool enumerate, Result results[]);

#endif // #ifndef POKER_H_INCLUDED
