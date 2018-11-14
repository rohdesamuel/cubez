#include "inc/common.h"

#include <Framework/Overlay.cpp>
#ifdef __COMPILE_AS_WINDOWS__
#define FRAMEWORK_PLATFORM_WIN
#undef max
#include <Framework/platform/win/FileSystemWin.cpp>
#include <Framework/platform/win/FontLoaderWin.cpp>
#elif defined(__COMPILE_AS_LINUX__)
#define FRAMEWORK_PLATFORM_LINUX
#include <Framework/platform/linux/FileSystemLinux.cpp>
#include <Framework/platform/linux/FontLoaderLinux.cpp>
#endif