#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

#include "poker.h"
#include "util.h"

using namespace std;

void go(istringstream &is) {

  string token, pos;

  while (is >> token)
    pos += token + " ";

  Spot s(pos);
  if (!s.valid()) {
    cerr << "Error in: " << pos << endl;
    return;
  }

  unsigned results[10];
  memset(results, 0, sizeof(results));

  for (int i = 0; i < 1 * 1000 * 1000; i++)
  {
      s.run(results);
      //cout << s << "\n\n\n*******************\n" << endl;
  }

  print_results(results, s.numPlayers);
}

int main(int argc, char *argv[]) {

  PRNG::init();

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
    else if (token == "bench")
      bench();
    else
      cout << "Unknown command: " << cmd << endl;

  } while (token != "quit" && argc == 1); // Command line args are one-shot

  cout << endl; // FXIME

  return 0;
}
