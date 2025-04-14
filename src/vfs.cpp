#include "vfs.h"

#include "utils.h"

#include <cubez/cubez.h>
#include <cubez/filesystem.h>

#include <filesystem>
#include <limits>

namespace {

qbResult errno_to_result(errno_t err) {
  // Full list of error codes are here: https://learn.microsoft.com/en-us/cpp/c-runtime-library/errno-constants?view=msvc-170.
  // This will take the most used ones.
  switch (err) {
    case 0: return QB_OK;
    case EACCES: return QB_ERROR_FILE_PERMISSION_DENIED;
    case ENOENT: return QB_ERROR_FILE_NOT_FOUND;
    case EISDIR: return QB_ERROR_FILE_IS_A_DIRECTORY;
    case ENFILE: return QB_ERROR_FILE_TOO_MANY_OPEN_FILES;
    case ESPIPE: return QB_ERROR_FILE_INVALID_SEEK;
    case EEXIST: return QB_ERROR_FILE_ALREADY_EXISTS;
    default: return QB_UNKNOWN;
  }
}

}

void vfs_initialize(qbVfsAttr attr) {
  std::filesystem::path cwd;

  if (attr->argv) {
    std::u8string arg(attr->argv[0], attr->argv[0] + strlen(attr->argv[0]));
    cwd = arg;
  } else {
    std::wstring warg(attr->wargv[0]);
    cwd = warg;
  }
  cwd = cwd.remove_filename();
  std::filesystem::current_path(cwd);
}

qbResult qb_fopen(qbFile* file, const utf8_t* path, const char* mode) {
#ifdef __COMPILE_AS_WINDOWS__
  std::wstring wpath = string_to_wstring(std::string((char*)path));
  std::wstring wmode = string_to_wstring(std::string(mode));
  FILE* fp = NULL;

  errno_t err = _wfopen_s(&fp, wpath.c_str(), wmode.c_str());
  qbResult result = errno_to_result(err);
  if (result == QB_OK) {
    *file = fp;
  } else {
    *file = nullptr;
  }
#else
  static_assert(false, "unimplemented");
#endif  // __COMPILE_AS_WINDOWS__

  return result;
}

qbResult qb_fclose(qbFile file) {
#ifdef __COMPILE_AS_WINDOWS__
  return errno_to_result(fclose((FILE*)file));
#else
  static_assert(false, "unimplemented");
#endif  // __COMPILE_AS_WINDOWS__
}

qbBool qb_fexists(const utf8_t* path) {
  qbBool ret = QB_FALSE;
  qbFile fp;
  if (qb_fopen(&fp, path, "r") == QB_OK) {
    ret = QB_TRUE;
  }
  qb_fclose(fp);
  return ret;
}

qbResult qb_fread(qbFile file, qbBuffer buf, size_t size, size_t count, size_t* nread) {
  QB_ASSERT(file && buf && buf->bytes && nread);

  if (size == 0 || count == 0) {
    return QB_OK;
  }

  size_t total = size * count;
  QB_ASSERT(size != 0 && total / size == count);
  QB_ASSERT(buf->capacity >= total);

#ifdef __COMPILE_AS_WINDOWS__
  ptrdiff_t pos = 0;
  *nread = fread_s(buf->bytes, buf->capacity, size, count, (FILE*)file);

  if (*nread > 0) {
    return QB_OK;
  } else if (feof((FILE*)file)) {
    return QB_EOF;
  } else {
    return errno_to_result(ferror((FILE*)file));
  }

#else
  static_assert(false, "unimplemented");
#endif  // __COMPILE_AS_WINDOWS__
}

QB_API qbResult qb_fseek(qbFile file, int32_t pos, qbOrigin seek_mode) {
  int origin = 0;
  switch (seek_mode) {
    case QB_ORIGIN_SET: origin = SEEK_SET; break;
    case QB_ORIGIN_CUR: origin = SEEK_CUR; break;
    case QB_ORIGIN_END: origin = SEEK_END; break;
  }

  if (fseek((FILE*)file, pos, origin) == 0) {
    return QB_OK;
  } else {
    return QB_UNKNOWN;
  }
}

qbResult qb_ftell(qbFile file, int32_t* pos) {
  QB_ASSERT(pos);
  *pos = ftell((FILE*)file);
  return QB_OK;
}

qbBufferOrResult_ qb_fload(const utf8_t* path) {
  qbFile fp;
  int32_t file_size;

  QB_ASSERT(path);

  qbResult res = qb_fopen(&fp, (const utf8_t*)path, "rb");
  if (res != QB_OK) {
    qb_warn("Could not open file: %s", path);
    return { QB_FALSE, res };
  }

  qb_fseek(fp, 0, QB_ORIGIN_END);
  res = qb_ftell(fp, &file_size);
  if (res != QB_OK || file_size < 0) {
    qb_fclose(fp);
    return { QB_FALSE, res };
  }

  qb_fseek(fp, 0, QB_ORIGIN_SET);
  qbBuffer_ buf = {
    .capacity = (size_t)file_size,
    .bytes = (uint8_t*)malloc(file_size),
  };

  if (!buf.bytes) {
    qb_fclose(fp);
    return { false, QB_ERROR_OUT_OF_MEMORY };
  }

  size_t nread;
  res = qb_fread(fp, &buf, 1, file_size, &nread);
  qb_fclose(fp);

  if (file_size != nread) {
    free(buf.bytes);
    qb_warn("Could not read file for loading: %s", path);
    return { false, res };
  }

  return qbBufferOrResult_{ .has_val = true, .val = buf };
}

void qb_ffree(qbBuffer buf) {
  free(buf->bytes);
}
