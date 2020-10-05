/**
* Author: Samuel Rohde (rohde.samuel@cubez.io)
*
* Copyright 2020 Samuel Rohde
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include <cubez/log.h>
#include <iostream>
#include <cstring>
#include <string.h>
#include <cstdarg>
#include <vector>
#include <mutex>
#include <condition_variable>

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
std::vector<std::string> log_buffers[2];

void log_initialize() {
  // Create a separate program/system to handle stdout.
  program_id = qb_program_create(kStdout);

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
          for (auto& s : log_buffers[read_log]) {
            std::cout << "[INFO] "; std::cout << s.c_str() << std::endl;
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

  qb_program_detach(program_id);
}

void qb_log(qbLogLevel level, const char* format, ...) {
  std::unique_lock<decltype(log_lock)> lock(log_lock);

  va_list args;
  va_start(args, format);

  char buf[256] = { 0 };

  vsprintf_s(buf, sizeof(buf), format, args);
  std::string s(buf);

  size_t num_msgs = 1 + (s.length() / MAX_CHARS);
  for (size_t i = 0; i < num_msgs; ++i) {
    log_buffers[write_log].push_back(s.substr(i * MAX_CHARS, MAX_CHARS));
  }

  va_end(args);
  flush = true;
  flush_logs.notify_all();
}
