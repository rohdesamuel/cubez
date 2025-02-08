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
#include <cubez/time.h>
#include <cubez/log.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <string.h>
#include <cstdarg>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <stdlib.h>

#include "log_internal.h"

const int MAX_CHARS = 256;

char kStdout[] = "stdout";
qbId program_id;

qbQueue log_queue;

struct LogEntry {
  std::filesystem::path filename;
  uint64_t fileline;
  int64_t timestamp_us;

  qbLogLevel level;
  std::string log_entry;

  void print(std::ostream& stream) const {
    switch (level) {
      case qbLogLevel::QB_DEBUG:
        stream << "[DEBUG] ";
        break;
      case qbLogLevel::QB_INFO:
        stream << "[INFO] ";
        break;
      case qbLogLevel::QB_WARN:
        stream << "[WARN] ";
        break;
      case qbLogLevel::QB_ERR:
        stream << "[ERR] ";
        break;
    }
    stream << "[" << timestamp_us << "] [" << filename.filename().string() << ":" << fileline << "]: " << log_entry;
  }
};

namespace {

std::ostream& operator<<(std::ostream& stream, const LogEntry& log_entry) {
  log_entry.print(stream);
  return stream;
}

std::filesystem::path logs_dir;
std::ostream* log_output;
std::unique_ptr<std::ifstream> log_fstream;

std::mutex flush_mu;
}

void log_initialize(qbLoggingAttr_ log_attr) {
  qb_queue_create(&log_queue);

  log_output = &std::cout;

  qb_task_async([](qbTask, qbVar) {
    while (qb_running()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      qb_log_flush();
    }
    return qbNil;
  }, qbNil);
}

void qb_log_ex(qbLogLevel level, const char* filename, uint64_t fileline, const char* format, ...) {
  va_list args;
  va_start(args, format);
  va_list copy;
  va_copy(copy, args);
  int len = vsnprintf(nullptr, 0, format, copy) + 1;
  std::string buf(len, '\0');
  vsnprintf(buf.data(), len, format, args);

  LogEntry* entry = new LogEntry{
    .filename = filename,
    .fileline = fileline,
    .timestamp_us = qb_time() / 1000,
    .level = level,
    .log_entry = std::move(buf)
  };

  qb_queue_write(log_queue, qbPtr(entry));

  if (level == QB_ERR) {
    qb_log_flush();
  }
}

void qb_log_flush() {
  std::lock_guard<decltype(flush_mu)> l(flush_mu);

  qbVar log;
  while (qb_queue_tryread(log_queue, &log)) {
    LogEntry* entry = (LogEntry*)log.p;
    std::cout << *entry << std::endl;
    delete entry;
  }
}