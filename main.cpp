#include <algorithm>
#include <cstring>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

#include "poker.h"
#include "util.h"

using namespace std;

typedef vector<Hand> Ranges;
typedef vector<Ranges> AllRanges;

// Expand tokens like T6s+, 88+, 52o+, AA, AK, QQ-99, T7s-T3s, J8-52 in the
// group of corresponding pair of cards (combos).
static bool expand(const string& token, Ranges& ranges)
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

    cout << "\nExpand:" << endl;

    while (true) {
        for (auto c1 : Suites)
            for (auto c2 : Suites) {
                if (v1 == v2 && c2 >= c1)
                    continue;
                if (   (f == Suited && c1 != c2)
                    || (f == Offsuited && c1 == c2))
                    continue;

                Card card1 = Card(16 * Suites.find(c1) + v1);
                Card card2 = Card(16 * Suites.find(c2) + v2);

                Hand h = Hand();
                h.add(card1, 0);
                h.add(card2, 0);
                ranges.push_back(h);

                cout << Values[v1] + string(1, c1) + Values[v2] + string(1, c2) << endl;
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

static bool parse_ranges(string& holes, AllRanges& allRanges)
{
    string token, h = holes;

    while (h.find("[") != std::string::npos) {

        auto b = h.find("[");
        auto e = h.find("]");

        if (e == std::string::npos || b > e)
            return false;

        allRanges.push_back(Ranges());

        // Extract each range's term
        stringstream ss(h.substr(b + 1, e - b - 1));

        while (std::getline(ss, token, ',')) {
            stringstream trim(token);
            trim >> token;
            if (!expand(token, allRanges.back()))
                return false;
        }
        h.replace(b, e - b + 1, "RR");
    }
    holes = h;
    return true;
}

static string parse_args(istringstream& is, size_t& players,
    size_t& gamesNum, size_t& threadsNum, bool& enumerate)
{
    enum States {
        Option,
        Hole,
        Common
    };

    map<string, string> args;
    AllRanges allRanges;
    string token, value;
    size_t holesCnt = 0;
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
            args["holes"] += token + " ";
            holesCnt++;
        }
        if (st == Common)
            args["commons"] += token;
    }

    // Parse ranges, if any
    if (!parse_ranges(args["holes"], allRanges))
        return args["holes"];

    if (!allRanges.empty()) {
        cout << "\n" << args["holes"] << "\n" << endl;
        for (auto& r : allRanges)
            cout << "Ranges size " << r.size() << endl;
        exit(0);
    }

    // Process options
    enumerate = (args["e"] == "true");
    threadsNum = args["t"].size() ? stoi(args["t"]) : 1;

    if (args["p"].size())
        players = stoi(args["p"]);
    else if (!players)
        players = holesCnt;

    if (args["g"].size()) {
        string g = args["g"];
        gamesNum = 1;
        if (tolower(g.back()) == 'm')
            gamesNum = 1000 * 1000, g.pop_back();
        else if (tolower(g.back()) == 'k')
            gamesNum = 1000, g.pop_back();
        gamesNum *= stoi(g);
    } else
        gamesNum = 1000 * 1000;

    string sep = (players == 1 ? "" : "- ");
    return to_string(players) + "P " + args["holes"] + sep + args["commons"];
}

void go(istringstream& is)
{
    size_t players = 0, gamesNum, threadsNum;
    bool enumerate = false;
    string pos = parse_args(is, players, gamesNum, threadsNum, enumerate);

    Spot s(pos);
    if (!s.valid()) {
        cerr << "Error in: " << pos << endl;
        return;
    }

    Result results[PLAYERS_NB];
    memset(results, 0, sizeof(results));
    run(s, gamesNum, threadsNum, enumerate, results);
    pretty_results(results, players);
}

void eval(istringstream& is)
{
    size_t players = 1, gamesNum, threadsNum;
    bool enumerate;
    string pos = parse_args(is, players, gamesNum, threadsNum, enumerate);

    Spot s(pos);
    if (players != 1 || !s.valid()) {
        cerr << "Error in: " << pos << endl;
        return;
    }

    cout << "Score is: " << s.eval() << "\n"
         << pretty64(s.eval(), false) << endl;
}

int main(int argc, char* argv[])
{
    init_score_mask();

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
            go(is);
        else if (token == "eval")
            eval(is);
        else if (token == "bench")
            bench(is);
        else
            cout << "Unknown command: " << cmd << endl;

    } while (token != "quit" && argc == 1); // Command line args are one-shot

    return 0;
}
