#ifndef UTILS__H
#define UTILS__H

#include <string>

#define SET_ENTITY_TABLE_ID(TABLE_ID, ENTITY) (ENTITY | ((uint64_t)(TABLE_ID) << 48))
#define ENTITY_TABLE_ID(ENTITY) ((0xFFFF000000000000ull & ENTITY) >> 48)
#define ENTITY_ID(ENTITY) (0x0000FFFFFFFFFFFFull & ENTITY)

std::wstring string_to_wstring(const std::string& str);
std::string wstring_to_string(const std::wstring& str);

#endif  // UTILS__H