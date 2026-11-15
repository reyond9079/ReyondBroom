// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <filesystem>

namespace ftebroom {
namespace fs = std::filesystem;

inline fs::path filesystemPath(const fs::path& path)
{
#ifdef _WIN32
  if (path.native().rfind(L"\\\\?\\",0)==0)
    return fs::path(path).make_preferred();
  const auto native = fs::absolute(path).lexically_normal().make_preferred().native();
  if (native.rfind(L"\\\\",0)==0) return L"\\\\?\\UNC\\" + native.substr(2);
  return L"\\\\?\\" + native;
#else
  return path;
#endif
}
} // namespace ftebroom
