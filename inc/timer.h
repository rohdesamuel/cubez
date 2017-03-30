#ifndef TIMER__H
#define TIMER__H

#include <boost/container/vector.hpp>

#include "common.h"

#ifdef __COMPILE_AS_WINDOWS__
#include <Windows.h>
#undef min
#undef max
#elif defined (__COMPILE_AS_LINUX__)
#include <time.h>
#endif

class Timer {
public:
  Timer();

  static double now();

  // Starts the timer.
  void start();

  // Stops the timer.
  void stop();

  // Stops and resets the timer.
  void reset();

  // Gets the elapsed time in [ns] since start.
  double get_elapsed_ns();

private:
#ifdef __COMPILE_AS_WINDOWS__
  LARGE_INTEGER start_;
  LARGE_INTEGER end_;
  LARGE_INTEGER freq_;

  // Gets the clock frequency of the CPU.
  int64_t get_clock_frequency();

#elif defined (__COMPILE_AS_LINUX__)
  timespec start_;
  timespec end_;
  
  timespec diff(timespec start, timespec end);  
#endif

};

class WindowTimer {
public:
  WindowTimer(uint8_t window_size = 0);

  // Starts the timer.
  void start();

  // Adds to the window average.
  void step();

  // Stops the timer.
  void stop();

  // Stops and resets the timer.
  void reset();

  // Gets the average elapsed time in [ns] since start.
  double get_elapsed_ns();

  // Gets the average elapsed time in [ns] since start.
  double get_avg_elapsed_ns();

private:
  Timer timer_;

  uint8_t iterator_;
  uint8_t window_size_;
  boost::container::vector<int64_t> window_;
};

#endif
