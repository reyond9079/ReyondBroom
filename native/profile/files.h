// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <filesystem>
#include <string>
#include <vector>
namespace ftebroom::profile {
struct FilePlan {int kind;std::vector<std::filesystem::path> sources;std::filesystem::path target;};
std::filesystem::path parsePath(std::string text);
FilePlan planFiles(int kind,const std::string& source,const std::string& target,const std::filesystem::path& working);
void executeFiles(const FilePlan&);
}
