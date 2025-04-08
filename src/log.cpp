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

#ifdef __COMPILE_AS_WINDOWS__
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Stringapiset.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#endif

#include "log_internal.h"

const int MAX_CHARS = 256;

char kStdout[] = "stdout";
qbId program_id;

qbQueue log_queue;

#ifdef __COMPILE_AS_WINDOWS__
namespace {
std::wstring string_to_wstring(const std::string& str) {
  if constexpr (sizeof(int) < sizeof(size_t)) {
    assert((str.size() < (1ull << 32)) && "Trying to convert a string that is too big.");
  }

  int buf_size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str.c_str(), (int)str.size(), nullptr, 0);
  std::wstring buf(buf_size, L'\0');

  assert(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str.c_str(), (int)str.size(), buf.data(), buf.size()) > 0);
  return buf;
}
}  // namespace
#endif  // __COMPILE_AS_WINDOWS__

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

#ifdef __COMPILE_AS_WINDOWS__
  void print(std::wostream& stream) const {
    switch (level) {
      case qbLogLevel::QB_DEBUG:
        stream << L"[DEBUG] ";
        break;
      case qbLogLevel::QB_INFO:
        stream << L"[INFO] ";
        break;
      case qbLogLevel::QB_WARN:
        stream << L"[WARN] ";
        break;
      case qbLogLevel::QB_ERR:
        stream << L"[ERR] ";
        break;
    }
    stream << L"[" << timestamp_us << L"] [" << filename.filename().wstring() << L":" << fileline << L"]: " << string_to_wstring(log_entry);
  }
#endif  // __COMPILE_AS_WINDOWS__

};

namespace {

std::ostream& operator<<(std::ostream& stream, const LogEntry& log_entry) {
  log_entry.print(stream);
  return stream;
}

#ifdef __COMPILE_AS_WINDOWS__
std::wostream& operator<<(std::wostream& stream, const LogEntry& log_entry) {
  log_entry.print(stream);
  return stream;
}
#endif  // __COMPILE_AS_WINDOWS__

std::filesystem::path logs_dir;
std::ostream* log_output;
std::unique_ptr<std::ifstream> log_fstream;

std::mutex flush_mu;
}

void log_initialize(qbLoggingAttr_ log_attr) {
  qb_queue_create(&log_queue);

  log_output = &std::cout;

  qb_task_async([](qbTask, qbVar) {
#ifdef __COMPILE_AS_WINDOWS__
    // Set printing to the console in this thread to utf16. Windows doesn't
    // output in utf8 by default.
    _setmode(_fileno(stdout), _O_U16TEXT);
#endif  // __COMPILE_AS_WINDOWS__

    while (qb_running()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      qb_log_flush();
    }
    return qbNil;
  }, qbNil);
}

void qb_log_ex(qbLogLevel level, const utf8_t* filename, uint64_t fileline, const utf8_t* format, ...) {
  va_list args;
  va_start(args, format);
  va_list copy;
  va_copy(copy, args);
  int len = vsnprintf(nullptr, 0, (char*)format, copy) + 1;
  std::string buf(len, '\0');
  vsnprintf(buf.data(), len, (char*)format, args);

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

    // Windows incorrectly chose utf16, so special case it here for unicode support.
#ifdef __COMPILE_AS_WINDOWS__
    std::wcout << *entry << std::endl;
#else
    std::cout << *entry << std::endl;
#endif
    delete entry;
  }
}