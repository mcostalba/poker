#include <cstring>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

#include "poker.h"
#include "thread.h"
#include "util.h"

using namespace std;

static string parse_args(istringstream& is, size_t& players,
    size_t& gamesNum, bool& enumerate)
{
    enum States {
        Option,
        Hole,
        Common
    };

    map<string, string> args;
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

    // Process options
    enumerate = (args["e"] == "true");

    Threads.set(args["t"].size() ? stoi(args["t"]) : 1);

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
    size_t players = 0, gamesNum;
    bool enumerate = false;
    string pos = parse_args(is, players, gamesNum, enumerate);

    Spot s(pos);
    if (!s.valid()) {
        cerr << "Error in: " << pos << endl;
        return;
    }

    if (enumerate && (gamesNum = s.set_enumerate_mode()) == 0)
        return;

    unsigned results[10];
    memset(results, 0, sizeof(results));
    Threads.run(s, gamesNum, results);
    print_results(results, players);
}

void eval(istringstream& is)
{
    size_t players = 1, gamesNum;
    bool enumerate;
    string pos = parse_args(is, players, gamesNum, enumerate);

    Spot s(pos);
    if (players != 1 || !s.valid()) {
        cerr << "Error in: " << pos << endl;
        return;
    }

    cout << "Score is: " << s.eval() << "\n"
         << pretty_hand(s.eval(), false) << endl;
}

int main(int argc, char* argv[])
{
    init_score_mask();
    Threads.set(1);

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
