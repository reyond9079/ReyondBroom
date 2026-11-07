// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>

namespace ftebroom {
inline std::string readEditorVersion(const std::filesystem::path& metadata) {
  try {
    std::ifstream input(metadata);
    char line[128]{};
    if (!input.getline(line, sizeof line)) return "0.0.0";
    std::istringstream fields(line);
    std::string product, version;
    fields >> product >> version;
    if (product == "ReyondBroom" && (fields >> std::ws).eof() &&
        std::regex_match(version, std::regex(
          R"([0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z]+([.-][0-9A-Za-z]+)*)?)")))
      return version;
  } catch (const std::exception&) {}
  return "0.0.0";
}
}
