/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2015 Marco Costalba, Joona Kiiski, Tord Romstad
  Copyright (C) 2015-2018 Marco Costalba, Joona Kiiski, Gary Linscott, Tord Romstad

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef THREAD_H_INCLUDED
#define THREAD_H_INCLUDED

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include "poker.h"
#include "thread_win32.h"


/// Thread class keeps together all the thread-related stuff. We use
/// per-thread pawn and material hash tables so that once we get a
/// pointer to an entry its life time is unlimited and we don't have
/// to care about someone changing the entry under our feet.

class Thread {

  Mutex mutex;
  ConditionVariable cv;
  bool exit = false, searching = true; // Set before starting std::thread
  std::thread stdThread;

  Spot spot;
  unsigned results[10];
  size_t gamesNum;


public:
  Thread();
  ~Thread();
  void idle_loop();
  void start_searching();
  void wait_for_search_finished();

  void run();
  unsigned result(size_t p) const { return results[p]; }
  void set_spot(const Spot& s, size_t n) {
    spot = s;
    gamesNum = n;
  }
};

/// ThreadPool struct handles all the threads-related stuff like init, starting,
/// parking and, most importantly, launching a thread. All the access to threads
/// is done through this class.

struct ThreadPool : public std::vector<Thread*> {

  void set(size_t);
  void run(const Spot& s, size_t, unsigned results[]);
};

extern ThreadPool Threads;

#endif // #ifndef THREAD_H_INCLUDED
