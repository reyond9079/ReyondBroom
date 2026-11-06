// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "../third_party/json.hpp"
#include <string>
#include <vector>
namespace ftebroom::profile {
using Json = nlohmann::json;
// Implements the original TB 2026.2 EL semantics. Variables retain their types.
std::string interpolate(const std::string&, const Json& variables);
Json parseConfig(const std::string&);
std::vector<std::string> parseArguments(const std::string&);
std::vector<std::string> splitMods(const std::string&);
bool filenameMatches(const std::string& name, const std::string& pattern);
}
