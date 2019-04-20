#include "log.h"
#include <iostream>
#include <cstring>
#include <string.h>
#include <cstdarg>
#include <vector>
#include <mutex>
#include <condition_variable>

namespace logging {

const int MAX_CHARS = 256;

char kStdout[] = "stdout";
char msg_buffer[MAX_CHARS];

qbSystem system_out;
qbEvent std_out;

qbId program_id;

std::condition_variable flush_logs;
volatile bool flush = false;
std::mutex log_lock;
volatile uint32_t read_log;
volatile uint32_t write_log;
std::vector<std::string> log[2];

void initialize() {
  // Create a separate program/system to handle stdout.
  program_id = qb_create_program(kStdout);

  read_log = 0;
  write_log = 1;

  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_setprogram(attr, program_id);
    qb_systemattr_setcallback(attr,
        [](qbFrame* f) {
          std::unique_lock<decltype(log_lock)> lock(log_lock);
          flush_logs.wait(lock, [] {return flush; });

          std::swap(read_log, write_log);
          for (auto& s : log[read_log]) {
            std::cout << "[INFO] " << s << std::endl;
          }
          flush = false;
        });
    qb_system_create(&system_out, attr);
    qb_systemattr_destroy(&attr);
  }
  
  {
    qbEventAttr attr;
    qb_eventattr_create(&attr);
    qb_eventattr_setprogram(attr, program_id);
    qb_eventattr_setmessagesize(attr, MAX_CHARS);
    qb_event_create(&std_out, attr);
    qb_event_subscribe(std_out, system_out);
    qb_eventattr_destroy(&attr);
  }

  qb_detach_program(program_id);
}

void out(const char* format, ...) {
  std::unique_lock<decltype(log_lock)> lock(log_lock);

  va_list args;
  va_start(args, format);

  char buf[256] = { 0 };

  vsprintf_s(buf, sizeof(buf), format, args);
  std::string s(buf);

  size_t num_msgs = 1 + (s.length() / MAX_CHARS);
  for (size_t i = 0; i < num_msgs; ++i) {
    log[write_log].push_back(s.substr(i * MAX_CHARS, MAX_CHARS));
  }

  va_end(args);
  flush = true;
  flush_logs.notify_all();
}

}  // namespace log
