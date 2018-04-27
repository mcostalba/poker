#include <chrono>
#include <cstring>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

#include "poker.h"
#include "util.h"

using namespace std;

namespace {

// Positions used by bench
const vector<string> BenchPos = {
    "-p 2 3d 22+",
    "-p 3 KhKs 76s - Ac As 7c Ts Qs",
    "-p 4 -e AcTc TdTh JT - 5h 6h 9c 9d",
    "-p 5 2c3d KsTc AhTd - 4d 5d 9c 9d",
    "-p 6 Ac Ad KsKd 3c - 2c 2h 7c 7h 8c",
    "-p 7 Ad Kc QhJh 3s4s - 2c 2h 7c 5h 8c",
    "-p 8 - Ac Ah 3d 7h 8c",
    "-p 9 [AA,QQ-99,AKs,T7s-T3s,AKo] [88+,T6s+,52o+] TT+",
    "-p 4 -e AhAd Ac 7c6s [66,T8s] - 2c 3c 4c",
    "-p 4 AhAd AcTh 7c6s 2h3h",
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

// Stores parsed args out of the position string
struct Args {
    Result results[PLAYERS_NB];
    string pos;
    size_t gamesNum, threadsNum;
    int players;
    bool enumerate;
};

void parse_args(istringstream& is, Args& parsed)
{
    enum States { Option, Hole, Common };

    map<string, string> args;
    string token, value, sep = " ";
    int holesCnt = 0;
    States st = Option;

    // Parse arguments
    while (is >> token) {
        if (st == Option) {
            if (token == "-p" || token == "-t" || token == "-g") {
                if (is >> value)
                    args[token.substr(1, 1)] = value;
                continue;
            } else if (token == "-e") {
                args["e"] = "true";
                continue;
            } else if (token == "-") {
                st = Common;
                continue;
            } else
                st = Hole;
        }
        if (st == Hole) {
            if (token == "-") {
                st = Common;
                continue;
            }
            if (token.front() == '[')
                sep = "";
            if (token.back() == ']')
                sep = " ";
            args["holes"] += token + sep;
            holesCnt++;
        }
        if (st == Common)
            args["commons"] += token;
    }

    // Process options
    parsed.enumerate  = (args["e"] == "true");
    parsed.threadsNum = (args["t"].size() ? stoi(args["t"]) : 1);
    parsed.players    = (args["p"].size() ? stoi(args["p"]) : holesCnt);

    if (args["g"].size()) {
        string g = args["g"];
        parsed.gamesNum = 1;
        if (tolower(g.back()) == 'm')
            parsed.gamesNum = 1000 * 1000, g.pop_back();
        else if (tolower(g.back()) == 'k')
            parsed.gamesNum = 1000, g.pop_back();
        parsed.gamesNum *= stoi(g);
    } else
        parsed.gamesNum = 1000 * 1000;

    parsed.pos = args["holes"] + "- " + args["commons"];
}

void go(istringstream& is, Args& args)
{
    parse_args(is, args);

    Spot s(args.players, args.pos);
    if (!s.valid()) {
        cerr << "Error in: " << args.pos << endl;
        return;
    }
    memset(args.results, 0, sizeof(args.results));
    run(s, args.gamesNum, args.threadsNum, args.enumerate, args.results);
    pretty_results(args.results, args.players);
}

// bench() runs a benchmark for speed and signature
void bench(istringstream& is)
{
    constexpr uint64_t GoodSig = 12568965866008609937ULL;

    Args args;
    string token;
    Hash sig;
    uint64_t cards = 0, spots = 0, cnt = 0;
    string threads = (is >> token) ? "-t " + token + " " : "-t 1 ";

    TimePoint elapsed = now();

    for (const string& pos : BenchPos) {
        cerr << "\nPosition " << ++cnt << ": " << pos << endl;
        istringstream ss(threads + pos);
        go(ss, args);

        for (int p = 0; p < args.players; ++p)
            sig << args.results[p].first + args.results[p].second;

        cards += args.gamesNum * (args.players * 2 + 5);
        spots += args.gamesNum;
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
    else if (threads == "-t 1 ")
        cerr << " (FAIL)";

    cerr << endl;
}

} // namespace

int main(int argc, char* argv[])
{
    init_score_mask();

    Args args;
    string token, cmd;

    for (int i = 1; i < argc; ++i)
        cmd += std::string(argv[i]) + " ";

    do {
        if (argc == 1 && !getline(cin, cmd)) // Block here waiting for input or EOF
            cmd = "quit";

        istringstream is(cmd);

        token.clear(); // Avoid a stale if getline() returns empty or blank line
        is >> skipws >> token;

        if (token == "quit")
            break;
        else if (token == "go")
            go(is, args);
        else if (token == "bench")
            bench(is);
        else
            cout << "Unknown command: " << cmd << endl;

    } while (token != "quit" && argc == 1); // Command line args are one-shot

    return 0;
}
