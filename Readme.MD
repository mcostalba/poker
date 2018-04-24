## Poker bits

A simple and lightweight Texas Hold'em 7-card equity calculator.

Through a Monte Carlo simulation, this command-line tool computes equities for
any 7 cards hand, with any number of random cards.

A simulation of a 2 player table with *Qh Qs* vs *Ad Jc* runs at **8.6 Milions
games/sec** on a quite old Intel I5 2.6GHz, single core. Tool supports multi-threading,
using 4 cores speed is almost 4X.

It has been fully validated for correctness against
[SKPokerEval](https://github.com/kennethshackleton/SKPokerEval).


### Usage

Simply call with _go_ command and the given cards, like:

```
; 2 players table with given hole cards
$ ./poker go AcKd 7h7s

; 4 threads and 2 players, first one has a 3 of diamond, second has a range with
; Ace-Queen suited or better or pocket ten or better. Third has Ace-King.
$ ./poker go -t 4 -p 2 3d [AQs+,TT+] AK

; 3 players and 10M games, player one has pocket kings, board is given
$ ./poker go -p 3 -g 10M KK - 8c 4d 7c Ts Qs

; 3 players, each one with a given hole card, the other is random. Full enumerate
; instead of Monte Carlo.
$ ./poker go -e Ac Td 7h - 5h 6h 9c Qs

; Full enumeration with 2 threads and given hole cards
$ ./poker go -e -t 2 AcKd 7h7s
```


This is the option list:

```
  -p X  With X number of players. Default to number of holes

  -t X  With X number of threads. Default to 1

  -g X  With X number of games, like 10000, 150K, 8M. Default to 1M

  -e    Full enumerate instead of running a Monte Carlo
```

Range syntax is the usual one (from PokerStartegy's Equilab):

```
Specific hands
  AhAd : Specific hand 'ace of hearts and ace of diamonds'

Hand groups
  AA  : All 6 combos of a pocket pair type for example
  AKs : All 4 ace king suited combos
  AKo : All 12 ace king offsuit combos
  AK  : All 16 ace king combos = AKo, AKs

Hand group ranges
  QQ-99   : All pocket pairs from 99 to QQ, i.e. QQ, JJ, TT, 99
  T7s-T3s : All suited tens from T3s to T7s , i.e. T7s, T6s, T5s, T4s, T3s
  T7o-T3o : All offsuit tens from T3o to T7o, i.e. T7o, T6o, T5o, T4o, T3o
  T7-T3   : All suited tens from T3s to T7s and all offsuit tens from T3o to T7o,
            i.e. T7s, T6s, T5s, T4s, T3s, T7o, T6o, T5o, T4o, T3o
  KJs-86s : All suited one gapper from 86s to KJs, i.e. KJs, QTs...86s
  AJo-63o : All suited two gapper from 63o to AJo, i.e. AJo, KTo...63o
  J8-52   : All two gapper from 52 to J8, i.e. J8s, T7s... 52s, J8o...52o

Open-ended hand group ranges
  QQ+  : All pocket pairs of queens and better, i.e. QQ, KK, AA
  T6s+ : All suited tens from T6s to T9s , i.e. T9s, T8s, T7s, T6s
  T6o+ : All offsuit tens from T6o to T9o, i.e. T9o, T8o, T7o, T6o
  T6+  : All offsuit tens from T6o to T9o and all suited tens from T6s to T9s
```


### How it works?

Instead of the usual hashing scheme, used by most evaluators, this tool is based
on card bitboards. Any card in [0,52] range can be mapped into a bit of a 64bit
unsigned integer, we leverage on this to build a score through a sequence
of bitwise operations, one for each card.

Suppose we have this 7-card hand: **Ks Tc Th 8s 5h 4d 2d**

After striping the suit, cards are mapped into a 64bit _score_ like this:

```
    | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10| 11| 12| 13| 14| 15|
+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 48 |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 32 |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 16 |   |   |   |   |   |   |   |   | X |   |   |   |   |   |   |   |
+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
  0 | X |   | X | X |   |   | X |   | X |   |   | X |   |   |   |   |
+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
```

The inner code is very simple and fast (a card is 6 bits, highest 2 the suit
and lowest 4 the value):

```C++
    uint64_t n = 1 << (card & 0xF);

    while (score & n)
        n <<= 16;
    score |= n;
```

And this is *almost* our score: we can compare hands by a simple:

```C++
    uint64_t maxScore = 0;

    for (size_t i = 0; i < numPlayers; ++i) {
        if (maxScore < hands[i].score)
            maxScore = hands[i].score;
    }
```

Unfortunately the devil is in the details and there are subtle cases like hand
with 3 pairs or hand with 2 sets that break the above simple scheme. To fix this
we bitwise AND the score with a pre-computed mask, indexed by the first 2
most significant bits (msb) of the score, something like this:

```C++
    uint64_t v = score;
    int idx1 = pop_msb(&v), idx2 = msb(v);
    score &= ScoreMask[idx1][idx2]; // Set also special combination flags
```

ScoreMask takes care of setting flags for some combination (full house, double
pair, etc). Flags are set in the unused part of the score (bits 13-15)
in a way that makes our score comparison to work correctly in all the cases.


#### Flush and straight
There is some special code for flushes and straights. For flushes we use a 32 bit
integer split in 4 slots (4 bit  each), each one inited at 3, and we add 1 for
every card according to card's suit. If one slot reaches 8, then we have a
flush:

```C++
    constexpr uint32_t SuitInit  =   3 | (3 << 4) | (3 << 8) | (3 << 12);
    constexpr uint32_t SuitAdd[] = { 1 , (1 << 4) , (1 << 8) , (1 << 12) };
    constexpr uint32_t IsFlush   =   8 | (8 << 4) | (8 << 8) | (8 << 12);

    suits = SuitInit;

    for ( <all cards> )
        suits += SuitAdd[card >> 4];

    if (suits & IsFlush) {
       unsigned r = lsb(suits & IsFlush) / 4; // Suite value in [0..3]
       ...
    }
```

For straights we use the following:

```C++
    uint64_t v = score & Rank1BB;
    v = (v << 1) | (v >> 12); // Duplicate an ace into first position
    v &= v >> 1;
    v &= v >> 1;
    v &= v >> 2;
    if (v)
        score = v << 3; // We have a straight, value is (v << 3)
```
