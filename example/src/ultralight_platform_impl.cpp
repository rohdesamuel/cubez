#include <cubez/cubez.h>

#include <Framework/Overlay.cpp>
#include <Framework/JSHelpers.cpp>
#ifdef __COMPILE_AS_WINDOWS__
#define FRAMEWORK_PLATFORM_WIN
#undef max
#include <Framework/platform/win/FileSystemWin.cpp>
#include <Framework/platform/win/FontLoaderWin.cpp>
#elif defined(__COMPILE_AS_LINUX__)
#define FRAMEWORK_PLATFORM_LINUX
#include <Framework/platform/common/FileSystemBasic.cpp>
#include <Framework/platform/common/FontLoaderRoboto.cpp>
#endif
