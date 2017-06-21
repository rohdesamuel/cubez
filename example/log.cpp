#include "log.h"
#include <iostream>
#include <string.h>

namespace logging {

const int MAX_CHARS = 256;

char kStdout[] = "stdout";

char kWriteEvent[] = "write";

qbChannel* std_out;

void initialize() {
  // Create a separate program/system to handle stdout.
  qb_create_program(kStdout);
  qbSystem* out = qb_alloc_system(kStdout, nullptr, nullptr);
  out->transform = +[](qbSystem*, qbFrame* f) {
    std::cout << (char*)(f->message.data);
  };
  out->callback = nullptr;
  qbExecutionPolicy policy;
  policy.priority = QB_MAX_PRIORITY;
  policy.trigger = qbTrigger::EVENT;
  qb_enable_system(out, policy);
  
  // Create an event to write to stdout with max buffer of 256 chars.
  qbEventPolicy event_policy;
  event_policy.size = MAX_CHARS;
  qb_create_event(kStdout, kWriteEvent, event_policy);

  // Subcsribe system handler to stdout.
  qb_subscribe_to(kStdout, kWriteEvent, out);

  // Open channel to start writing.
  std_out = qb_open_channel(kStdout, kWriteEvent);
}

void out(const std::string& s) {
  // Write a simple "hi".
  int num_msgs = 1 + (s.size() / MAX_CHARS);

  for (int i = 0; i < num_msgs; ++i) {
    qbMessage* m = qb_alloc_message(std_out);
    std::string to_send = s.substr(i * MAX_CHARS, MAX_CHARS);
    // Add 1 for null terminated character.
    memcpy(m->data, to_send.c_str(), to_send.size() + 1);

    qb_send_message(m);
  }
}

}  // namespace log
