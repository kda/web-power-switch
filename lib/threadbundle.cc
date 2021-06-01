#include "threadbundle.h"


ThreadBundle::ThreadBundle() {
}

ThreadBundle::~ThreadBundle() {
  join();
}

void ThreadBundle::join() {
  for (auto & t : threads) {
    t.join();
  }
  threads.clear();
}
