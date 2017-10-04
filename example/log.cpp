#include "log.h"
#include <iostream>
#include <string.h>

namespace logging {

const int MAX_CHARS = 256;

char kStdout[] = "stdout";

char kWriteEvent[] = "write";

qbSystem system_out;
qbEvent std_out;

qbId program_id;

void initialize() {
  // Create a separate program/system to handle stdout.
  program_id = qb_create_program(kStdout);

  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_setprogram(&attr, kStdout);
    qb_systemattr_setcallback(&attr,
        [](qbSystem, void* message, qbCollectionInterface*) {
        std::cout << (char*)(message);
        });
    qb_system_create(&system_out, attr);
    qb_systemattr_destroy(&attr);
  }
  
  {
    qbEventAttr attr;
    qb_eventattr_create(&attr);
    qb_eventattr_setprogram(&attr, kStdout);
    qb_eventattr_setmessagesize(&attr, MAX_CHARS);
    qb_event_create(&std_out, attr);
    qb_event_subscribe(std_out, system_out);
    qb_eventattr_destroy(&attr);
  }

  qb_detach_program(program_id);
}

void out(const std::string& s) {
  int num_msgs = 1 + (s.size() / MAX_CHARS);
  for (int i = 0; i < num_msgs; ++i) {
    std::string to_send = s.substr(i * MAX_CHARS, MAX_CHARS);
    qb_event_send(std_out, (void*)to_send.c_str());
  }
}

}  // namespace log
