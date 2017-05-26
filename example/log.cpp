#include "log.h"
#include <iostream>
#include <string.h>

namespace logging {

const int MAX_CHARS = 256;

char kStdout[] = "stdout";

char kWriteEvent[] = "write";

cubez::Channel* std_out;

void initialize() {
  // Create a separate program/pipeline to handle stdout.
  cubez::create_program(kStdout);
  cubez::Pipeline* out = cubez::add_pipeline(kStdout, nullptr, nullptr);
  out->transform = +[](cubez::Pipeline*, cubez::Frame* f) {
    std::cout << (char*)(f->message.data);
  };
  out->callback = nullptr;
  cubez::ExecutionPolicy policy;
  policy.priority = cubez::MAX_PRIORITY;
  policy.trigger = cubez::Trigger::EVENT;
  cubez::enable_pipeline(out, policy);
  
  // Create an event to write to stdout with max buffer of 256 chars.
  cubez::EventPolicy event_policy;
  event_policy.size = MAX_CHARS;
  cubez::create_event(kStdout, kWriteEvent, event_policy);

  // Subcsribe pipeline handler to stdout.
  cubez::subscribe_to(kStdout, kWriteEvent, out);

  // Open channel to start writing.
  std_out = cubez::open_channel(kStdout, kWriteEvent);
}

void out(const std::string& s) {
  // Write a simple "hi".
  int num_msgs = 1 + (s.size() / MAX_CHARS);

  for (int i = 0; i < num_msgs; ++i) {
    cubez::Message* m = new_message(std_out);
    std::string to_send = s.substr(i * MAX_CHARS, MAX_CHARS);
    // Add 1 for null terminated character.
    memcpy(m->data, to_send.c_str(), to_send.size() + 1);

    cubez::send_message(m);
  }
}

}  // namespace log
