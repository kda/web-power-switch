#ifndef __THREADBUNDLE_H__INCLUDED__
#define __THREADBUNDLE_H__INCLUDED__

#include <iostream>
#include <thread>
#include <vector>


class ThreadBundle {
public:
  ThreadBundle();
  ~ThreadBundle();

  void setMaxThreads(uint max_threads) {
    max_threads_ = max_threads;
  }

  template<typename _Callable, typename ... _Args>
  void add(_Callable&& f, _Args&& ... args) {
    if (max_threads_ > 0) {
      if (threads.size() >= max_threads_) {
        waitForThreadsToFinish();
      }
    }
    while (true) {
      try {
        std::thread t(std::forward<_Callable>(f), std::forward<_Args>(args)...);
        threads.push_back(std::move(t));
        break;
      } catch (...) {
        waitForThreadsToFinish();
#ifdef kda_COMMENTED_OUT
        auto threadCount = threads.size();
        int threadsToJoin = 1;
        if (threadCount >= 20) {
          threadsToJoin = (threadCount * 10) / 100;
        }
        std::cout << "joining: count: " << threadCount << " threadsToJoin: " << threadsToJoin << std::endl;
        auto itThread = threads.begin();
        while (threadsToJoin > 0 && itThread != threads.end()) {
          itThread->join();
          itThread = threads.erase(itThread);
          threadsToJoin--;
        }
        std::cout << "joining done" << std::endl;
#endif /* kda_COMMENTED_OUT */
      }
    }
  }

  void join();

private:
  uint max_threads_ = 0;
  std::vector<std::thread> threads;

  void waitForThreadsToFinish() {
    auto threadCount = threads.size();
    int threadsToJoin = 1;
    if (threadCount >= 20) {
      threadsToJoin = (threadCount * 10) / 100;
    }
    //std::cout << "joining: count: " << threadCount << " threadsToJoin: " << threadsToJoin << std::endl;
    auto itThread = threads.begin();
    while (threadsToJoin > 0 && itThread != threads.end()) {
      itThread->join();
      itThread = threads.erase(itThread);
      threadsToJoin--;
    }
    //std::cout << "joining done" << std::endl;
  }
};

#endif  /*  __THREADBUNDLE_H__INCLUDED__  */
