// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/SystemPaths.h"
#include <QString>
#include <filesystem>
std::filesystem::path reyondCompileResources;
namespace tb::ui::SystemPaths {
std::filesystem::path findResourceFile(const std::filesystem::path& p){auto path=reyondCompileResources/p;return std::filesystem::is_regular_file(path)?path:std::filesystem::path{};}
}
