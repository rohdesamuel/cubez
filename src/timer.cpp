#include <algorithm>

#include "timer.h"

double NS_TO_SEC = 1e9;

#ifdef __COMPILE_AS_LINUX__
#define CLOCK_TYPE CLOCK_MONOTONIC //CLOCK_PROCESS_CPUTIME_ID
#endif  // ifdef __COMPILE_AS_LINUX__

Timer::Timer() {
  reset();
#ifdef __COMPILE_AS_WINDOWS__
  get_clock_frequency();
#endif
}

double Timer::now() {
  double ret = 0.0;
#ifdef __COMPILE_AS_WINDOWS__
  LARGE_INTEGER freq;
  LARGE_INTEGER now;
  if (!freq.QuadPart) {
    QueryPerformanceFrequency(&freq);
  }
  QueryPerformanceCounter(&now);
  ret = (now.QuadPart * NS_TO_SEC) / (double)freq.QuadPart;
#elif defined (__COMPILE_AS_LINUX__)
  timespec now;
  clock_gettime(CLOCK_TYPE, &now);
  ret = (NS_TO_SEC * now.tv_sec) + now.tv_nsec;
#endif
  return ret;
}

void Timer::start() {
#ifdef __COMPILE_AS_WINDOWS__
  get_clock_frequency();
  QueryPerformanceCounter(&start_);
#elif defined (__COMPILE_AS_LINUX__)
  clock_gettime(CLOCK_TYPE, &start_);
#endif
}

void Timer::stop() {
#ifdef __COMPILE_AS_WINDOWS__
  QueryPerformanceCounter(&end_);
#elif defined (__COMPILE_AS_LINUX__)
  clock_gettime(CLOCK_TYPE, &end_);
#endif
}

double Timer::get_elapsed_ns() {
#ifdef __COMPILE_AS_WINDOWS__
  return ((end_.QuadPart - start_.QuadPart) * NS_TO_SEC) / (double)freq_.QuadPart;
#elif defined (__COMPILE_AS_LINUX__)
  timespec tmp = diff(start_, end_);
  return (NS_TO_SEC * tmp.tv_sec) + tmp.tv_nsec;
#endif
}

#ifdef __COMPILE_AS_LINUX__
timespec Timer::diff(timespec start, timespec end) {
  timespec temp;
  if ((end.tv_nsec-start.tv_nsec)<0) {
    temp.tv_sec = end.tv_sec - start.tv_sec - 1;
    temp.tv_nsec = NS_TO_SEC + end.tv_nsec - start.tv_nsec;
  } else {
    temp.tv_sec = end.tv_sec - start.tv_sec;
    temp.tv_nsec = end.tv_nsec - start.tv_nsec;
  }
  return temp;
}
#endif

#ifdef __COMPILE_AS_WINDOWS__
int64_t Timer::get_clock_frequency() {
  if (!freq_.QuadPart) {
    QueryPerformanceFrequency(&freq_);
  }
  return freq_.QuadPart;
}
#endif

void Timer::reset() {
#ifdef __COMPILE_AS_WINDOWS__
  freq_ = start_ = end_ = LARGE_INTEGER();
#elif defined (__COMPILE_AS_LINUX__)
  start_.tv_sec = end_.tv_sec = 0;
  start_.tv_nsec = end_.tv_nsec = 0;
#endif
}

WindowTimer::WindowTimer(uint8_t window_size) : iterator_(0) {
  window_size_ = std::max(window_size, (uint8_t)1);
  window_.reserve(window_size_);
  for (uint8_t i = 0; i < window_size_; ++i) {
    window_.push_back(0);
  }
  reset();
}

// Starts the timer.
void WindowTimer::start() {
  timer_.start();
}

// Starts the timer.
void WindowTimer::step() {
  window_[iterator_++] = get_elapsed_ns();
  iterator_ = iterator_ % window_size_;
}

// Stops the timer.
void WindowTimer::stop() {
  timer_.stop();
}

// Stops and resets the timer.
void WindowTimer::reset() {
  timer_.reset();
  for (uint8_t i = 0; i < window_size_; ++i) {
    window_[i] = 0;
  }
}

// Gets the average elapsed time in [ns] since start.
double WindowTimer::get_elapsed_ns() {
  return timer_.get_elapsed_ns();
}

// Gets the average elapsed time in [ns] since start.
double WindowTimer::get_avg_elapsed_ns() {
  double accum = 0;
  for (auto& n : window_) {
    accum += n;
  }
  return accum / window_size_;
}
