#ifndef LOG__H
#define LOG__H

#include "inc/cubez.h"
#include "inc/common.h"
#include <string>

namespace logging {

void initialize();

void out(const std::string& s);

void err(const std::string& s);

}  // namespace log


#endif  // LOG__H
