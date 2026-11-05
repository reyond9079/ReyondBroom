// SPDX-License-Identifier: MIT
// Algorithm design after AutoLevel by Al-Asl (MIT).
#pragma once
#include "al_model.h"
#include <sstream>
#include <iomanip>
namespace al {
std::string serialize(const Repo&);std::string serialize(const Builder&);std::string serialize(const EditorSettings&);
Repo readRepo(const std::string&);Builder readBuilder(const std::string&);EditorSettings readSettings(const std::string&);
Repo importLevelgen(const std::string&);
}
