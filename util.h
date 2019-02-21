
#ifndef UTIL_H_INCLUDED
#define UTIL_H_INCLUDED

#include <cassert>
#include <cstdint>
#include <string>

typedef std::pair<unsigned, unsigned> Result;

/// A constant divisible by 2,3,4,5,6 used to score split results
constexpr unsigned KTie = 60;

/// Our PRNG class is a wrapper around Xoroshiro128+. Used for Monte Carlo
class PRNG {

    uint64_t s[2];
    uint64_t* buf;

    void jump(void);

public:
    PRNG(size_t idx, uint64_t seed = 0);
    uint64_t next();
    void set_enum_buffer(uint64_t* b) { buf = b; }
};

/// popcount() counts the number of non-zero bits in a uint64_t
inline int popcount(uint64_t b)
{

#ifdef USE_POPCNT

    extern uint8_t PopCnt16[1 << 16];
    union {
        uint64_t bb;
        uint16_t u[4];
    } v = { b };
    return PopCnt16[v.u[0]] + PopCnt16[v.u[1]] + PopCnt16[v.u[2]] + PopCnt16[v.u[3]];

#elif defined(_MSC_VER) || defined(__INTEL_COMPILER)

    return (int)_mm_popcnt_u64(b);

#else // Assumed gcc or compatible compiler

    return __builtin_popcountll(b);

#endif
}

/// lsb() and msb() return the least/most significant bit in a non-zero uint64_t
#if defined(__GNUC__) // GCC, Clang, ICC

inline int lsb(uint64_t b)
{
    assert(b);
    return __builtin_ctzll(b);
}

inline int msb(uint64_t b)
{
    assert(b);
    return 63 ^ __builtin_clzll(b);
}

#elif defined(_MSC_VER) // MSVC

#ifdef _WIN64 // MSVC, WIN64

inline unsigned long lsb(uint64_t b)
{
    assert(b);
    unsigned long idx;
    _BitScanForward64(&idx, b);
    return idx;
}

inline unsigned long msb(uint64_t b)
{
    assert(b);
    unsigned long idx;
    _BitScanReverse64(&idx, b);
    return idx;
}

#else // MSVC, WIN32

inline unsigned long lsb(uint64_t b)
{
    assert(b);
    unsigned long idx;

    if (b & 0xffffffff) {
        _BitScanForward(&idx, int32_t(b));
        return idx;
    } else {
        _BitScanForward(&idx, int32_t(b >> 32));
        return idx + 32;
    }
}

inline unsigned long msb(uint64_t b)
{
    assert(b);
    unsigned long idx;

    if (b >> 32) {
        _BitScanReverse(&idx, int32_t(b >> 32));
        return idx + 32;
    } else {
        _BitScanReverse(&idx, int32_t(b));
        return idx;
    }
}

#endif

#else // Compiler is neither GCC nor MSVC compatible

#error "Compiler not supported."

#endif

/// pop_lsb() finds and clears the least significant bit in a non-zero bitboard
inline unsigned pop_lsb(uint64_t* b)
{
    const unsigned s = lsb(*b);
    *b &= *b - 1;
    return s;
}

/// pop_msb() finds and clears the most significant bit in a non-zero bitboard
inline unsigned pop_msb(uint64_t* b)
{
    const unsigned s = msb(*b);
    *b ^= 1ULL << s;
    return s;
}

/// msb_bb() returns as a bitboard the most significant bit in a non-zero bitboard
inline uint64_t msb_bb(uint64_t b)
{
    return 1ULL << msb(b);
}

/// Pretty printers of a uint64_t in "table of bits" format and of equity results
extern const std::string pretty64(uint64_t b, bool headers = false);
extern void pretty_results(Result* results, size_t players);

#endif // #ifndef UTIL_H_INCLUDED
