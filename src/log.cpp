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

#include <cubez/async.h>
#include <cubez/utils.h>
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
qbId program_id;

qbQueue log_queue;

struct LogEntry {
  const char* filename;
  uint64_t fileline;
  int64_t timestamp_ms;

  qbLogLevel level;
  std::string log_entry;
};

void log_initialize() {
  qb_queue_create(&log_queue);

  qb_task_async([](qbTask, qbVar) {
    while (qb_running()) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      qbVar log;
      while (qb_queue_tryread(log_queue, &log)) {
        LogEntry* entry = (LogEntry*)log.p;
        switch (entry->level) {
          case qbLogLevel::QB_DEBUG:
            std::cout << "[DEBUG] " << entry->log_entry << std::endl;
            break;
          case qbLogLevel::QB_INFO:
            std::cout << "[INFO] " << entry->log_entry << std::endl;
            break;
          case qbLogLevel::QB_WARN:
            std::cout << "[WARN] " << entry->log_entry << std::endl;
            break;
          case qbLogLevel::QB_ERR:
            std::cout << "[ERR] " << entry->log_entry << std::endl;
            break;
        }

        delete entry;
      }      
    }
    return qbNil;
  }, qbNil);
}

void qb_log_ex(qbLogLevel level, const char* filename, uint64_t fileline, const char* format, ...) {
  va_list args;
  va_start(args, format);

  char buf[1024] = { 0 };

  vsprintf_s(buf, sizeof(buf), format, args);
  buf[1023] = 0;
  std::string s(buf);

  LogEntry* entry = new LogEntry{
    .filename = filename,
    .fileline = fileline,
    .timestamp_ms = time(nullptr),
    .level = level,
    .log_entry = std::move(s)
  };

  qb_queue_write(log_queue, qbPtr(entry));
}
