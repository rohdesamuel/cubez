#include "utils.h"

#include <cubez/cubez.h>
#ifdef __COMPILE_AS_WINDOWS__
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

std::wstring string_to_wstring(const std::string& str) {
#ifdef __COMPILE_AS_WINDOWS__
  if (str.empty()) {
    return std::wstring();
  }

  if constexpr (sizeof(int) < sizeof(size_t)) {
    assert((str.size() < (1ull << 32)) && "Trying to convert a string that is too big.");
  }

  int buf_size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str.c_str(), (int)str.size(), nullptr, 0);
  std::wstring buf(buf_size, L'\0');

  assert(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str.c_str(), (int)str.size(), buf.data(), buf.size()) > 0);
  return buf;
#else
  return L"";
#endif  // __COMPILE_AS_WINDOWS__
}

std::string wstring_to_string(const std::wstring& str) {
#ifdef __COMPILE_AS_WINDOWS__
  if (str.empty()) {
    return std::string();
  }

  if constexpr (sizeof(int) < sizeof(size_t)) {
    assert((str.size() < (1ull << 32)) && "Trying to convert a string that is too big.");
  }

  int buf_size = WideCharToMultiByte(CP_UTF8, MB_ERR_INVALID_CHARS, str.c_str(), (int)str.size(), nullptr, 0, NULL, NULL);
  std::string buf(buf_size, '\0');

  assert(WideCharToMultiByte(CP_UTF8, MB_ERR_INVALID_CHARS, str.c_str(), (int)str.size(), buf.data(), buf.size(), NULL, NULL) > 0);
  return buf;
#else
  return "";
#endif  // __COMPILE_AS_WINDOWS__
}