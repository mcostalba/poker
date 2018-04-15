#include <algorithm>
#include <cstring>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

#include "poker.h"
#include "util.h"

using namespace std;

typedef map<size_t, vector<string>> RangeMap;

static bool parse_ranges(const string& holes, RangeMap& ranges) {

    string token, h = holes;

    while (h.find("[") != std::string::npos) {

        auto b = h.find("[");
        auto e = h.find("]");

        if (e == std::string::npos || b > e)
            return false;

        // Extract the range n (count spaces before '[' to deduce range id)
        auto n = std::count(h.begin(), h.begin() + b, ' ');
        stringstream ss(h.substr(b + 1, e - b - 1));

        // Extract each range's term
        while(std::getline(ss, token, ',')) {
            stringstream trim(token);
            trim >> token;
            ranges[n].push_back(token);
        }
        h.erase(b, e - b + 1);
    }
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
    RangeMap ranges;
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
    if (!parse_ranges(args["holes"], ranges))
        return args["holes"];

    if (!ranges.empty()) {
        for (auto const& l : ranges) {
            cout << "\n" << l.first << endl;
            for (auto const& r : l.second)
                std::cout << r << '\n';
        }
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
