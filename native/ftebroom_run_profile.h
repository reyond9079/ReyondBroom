// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "third_party/json.hpp"
#include <string>
#include <vector>
enum RunTaskFlags { RunCopyToDirectory=1, RunAllowNonzero=2, RunLaunchEngine=4, RunAllowLaunchFailure=8 };
struct RunTask {
  int kind=0;
  std::string source,destination;
  bool enabled=true;
  int flags=0;
  bool stripTbProperties=false;
  std::string stripEntityPattern,engineProfileId;
  nlohmann::json entityToAdd=nullptr;
};
struct RunProfile {
  std::string name="New profile",work="${MAP_DIR_PATH}";
  bool launch=false;
  std::vector<RunTask> tasks;
  // Legacy bundled presets retain the preview/resource-preparation adapter.
  bool trenchBroom=false;
};
struct RunEngineProfile {std::string id,name,path,parameters;};
