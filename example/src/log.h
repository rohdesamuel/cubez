#ifndef LOG__H
#define LOG__H

#include <cubez/cubez.h>
#include <string>

namespace logging {

void initialize();

void out(const char* format, ...);

void err(const std::string& s);

}  // namespace log


#endif  // LOG__H
