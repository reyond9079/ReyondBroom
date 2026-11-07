// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "el.h"
#include <string>
namespace ftebroom::profile {
// Applies export-only entity options to the editor snapshot, retaining geometry.
std::string exportEntities(const std::string& map,bool stripTb,const std::string& stripPattern,const Json& entityToAdd);
}
