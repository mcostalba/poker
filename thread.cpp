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

#include <algorithm> // For std::count
#include <cassert>
#include <cstring>

#include "thread.h"
#include "util.h"

ThreadPool Threads; // Global object


/// Thread constructor launches the thread and waits until it goes to sleep
/// in idle_loop(). Note that 'searching' and 'exit' should be alredy set.

Thread::Thread(size_t n) : stdThread(&Thread::idle_loop, this), prng(n) {

  wait_for_search_finished();
}


/// Thread destructor wakes up the thread in idle_loop() and waits
/// for its termination. Thread should be already waiting.

Thread::~Thread() {

  assert(!searching);

  exit = true;
  start_searching();
  stdThread.join();
}


/// Thread::start_searching() wakes up the thread that will start the search

void Thread::start_searching() {

  std::lock_guard<Mutex> lk(mutex);
  searching = true;
  cv.notify_one(); // Wake up the thread in idle_loop()
}


/// Thread::wait_for_search_finished() blocks on the condition variable
/// until the thread has finished searching.

void Thread::wait_for_search_finished() {

  std::unique_lock<Mutex> lk(mutex);
  cv.wait(lk, [&]{ return !searching; });
}


/// Tstartop() is where the thread is parked, blocked on the
/// condition variable, when it has no work to do.

void Thread::idle_loop() {

  while (true)
  {
      std::unique_lock<Mutex> lk(mutex);
      searching = false;
      cv.notify_one(); // Wake up anyone waiting for search finished
      cv.wait(lk, [&]{ return searching; });

      if (exit)
          return;

      lk.unlock();

      run();
  }
}

void Thread::run() {

  memset(results, 0, sizeof(results));
  for (size_t i = 0; i < gamesNum; i++)
      spot.run(results);
}

/// ThreadPool::set() creates/destroys threads to match the requested number.
/// Created and launced threads wil go immediately to sleep in idle_loop.

void ThreadPool::set(size_t requested) {

  while (size() > requested)
      delete back(), pop_back();

  while (size() < requested)
      push_back(new Thread(size()));
}

void ThreadPool::run(const Spot& s, size_t gamesNum, unsigned results[]) {

  size_t n = gamesNum < size() ? 1 : gamesNum / size();

  for (Thread* th : *this) {
      th->set_spot(s, n);
      th->start_searching();
  }

  for (Thread* th : *this)
      th->wait_for_search_finished();

  for (Thread* th : *this)
      for (size_t p = 0; p < s.players(); p++)
          results[p] += th->result(p);
}
