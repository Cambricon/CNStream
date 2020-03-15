#ifndef __NANO_TIMER_H_
#define __NANO_TIMER_H_

#include <sys/time.h>
#include <time.h>
class NanoTimer {
 private:
  struct timespec time1, time2;

 public:
  NanoTimer() {}
  inline void Start() { clock_gettime(CLOCK_MONOTONIC, &time1); }
  inline double GetElapsed_ms() { return GetElapsed_ns() / 1000000.0; }

  inline double GetElapsed_us() { return GetElapsed_ns() / 1000.0; }

  double GetElapsed_ns() {
    clock_gettime(CLOCK_MONOTONIC, &time2);
    return ((1000000000.0 * static_cast<double>(time2.tv_sec - time1.tv_sec)) +
            static_cast<double>(time2.tv_nsec - time1.tv_nsec));
  }
};

#endif
