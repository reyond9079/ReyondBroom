// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <map>
#include <stdexcept>
#include <string>

namespace ftebroom {
// TrenchBroom's common spare-core expression is evaluated from the current
// machine at run time. This is a data-only allowlist, not a shell/EL evaluator.
inline bool runCpuCountMinusOne(const std::string &expression) {
  std::string compact;
  for (char c : expression)
    if (c != ' ' && c != '\t')
      compact += c;
  return compact == "CPU_COUNT-1";
}
inline std::string
expandRunVariables(const std::string &text,
                   const std::map<std::string, std::string> &variables) {
  std::string out;
  size_t cursor = 0;
  while (cursor < text.size()) {
    auto begin = text.find("${", cursor);
    if (begin == std::string::npos) {
      out += text.substr(cursor);
      break;
    }
    out += text.substr(cursor, begin - cursor);
    auto end = text.find('}', begin + 2);
    if (end == std::string::npos)
      throw std::runtime_error("Unclosed profile variable.");
    auto name = text.substr(begin + 2, end - begin - 2);
    bool spareCore = runCpuCountMinusOne(name);
    auto found = variables.find(spareCore ? "CPU_COUNT" : name);
    if (found == variables.end())
      throw std::runtime_error("Unknown profile variable: " + name);
    if (found->second.empty() &&
        (name == "TB_GAME_ROOT" || name == "TB_ACTIVE_MOD"))
      throw std::runtime_error(
          "Configure the active game or mod folder in Game / resources.");
    if (spareCore) {
      if (found->second.empty() ||
          found->second.find_first_not_of("0123456789") != std::string::npos)
        throw std::runtime_error(
            "Invalid CPU_COUNT for the thread expression.");
      auto count = std::stoull(found->second);
      out += std::to_string(count > 1 ? count - 1 : 1);
    } else
      out += found->second;
    cursor = end + 1;
  }
  return out;
}
} // namespace ftebroom
