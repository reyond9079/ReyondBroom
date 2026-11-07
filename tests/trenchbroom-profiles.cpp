// SPDX-License-Identifier: GPL-2.0-or-later
#include "../native/ftebroom_run_import_data.h"
#include <cassert>
#include <iostream>
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  auto imported = runReadTrenchBroom(std::filesystem::u8path(argv[1]), {});
  for (const auto &notice : imported.notices)
    std::cerr << notice << '\n';
  if (imported.profiles.size() != 5) {
    std::cerr << "Expected 5 profiles, imported " << imported.profiles.size()
              << '\n';
    return 1;
  }
  for (const auto &profile : imported.profiles)
    if (profile.tasks.size() != 14)
      return 1;
  assert(imported.notices.empty() && imported.missingVariables.empty());
  int expressions = 0;
  for (const auto &profile : imported.profiles) {
    assert(profile.work == "${MAP_DIR_PATH}");
    for (const auto &task : profile.tasks) {
      if (task.destination.find("${CPU_COUNT - 1}") != std::string::npos) {
        expressions++;
        assert(task.enabled && (task.flags & RunAllowNonzero));
        for (auto cpu : {1, 2, 8, 64}) {
          auto expanded = ftebroom::profile::interpolate(
              task.destination,
              {{"CPU_COUNT", cpu}, {"MAP_BASE_NAME", "test"}});
          assert(expanded.find("-threads " +
                               std::to_string(cpu - 1) + " ") ==
                 0);
          assert(expanded.find("${") == std::string::npos);
        }
      }
      if (task.kind == 2)
        assert((task.flags & RunCopyToDirectory) &&
               task.destination.find("${MODS[-1]}") != std::string::npos);
    }
  }
  assert(expressions == 10);
  for (auto expression :
       {"CPU_COUNT-1", "CPU_COUNT - 1", " CPU_COUNT \t- 1 "}) {
    assert(ftebroom::runCpuCountMinusOne(expression));
    assert(ftebroom::expandRunVariables("${" + std::string(expression) + "}",
                                        {{"CPU_COUNT", "8"}}) == "7");
  }
  std::map<std::string, std::string> required;
  assert(runImportText("${compiler}", required, {}) == "${compiler}" &&
         required.count("compiler"));
  assert(ftebroom::profile::interpolate("${CPU_COUNT - 2}", {{"CPU_COUNT", 8}})=="6");
  for(auto expression : {"CPU_COUNT;echo BAD", "CPU_COUNT - 1;echo BAD"}) {
    auto text="${"+std::string(expression)+"}";
    assert(runImportText(text,required,{})==text);
    bool rejected=false;try {ftebroom::profile::interpolate(text,{{"CPU_COUNT",8}});}catch(...){rejected=true;}
    assert(rejected);
  }
  for(const auto& p:imported.profiles)assert(runProfileToJson(runProfileFromJson(runProfileToJson(p)))==runProfileToJson(p));
  assert(imported.profiles[0].tasks[2].destination.find(".bspw")!=std::string::npos);
  assert(ftebroom::expandRunVariables(
             "${MAP_BASE_NAME} ${CPU_COUNT}",
             {{"MAP_BASE_NAME", "test"}, {"CPU_COUNT", "8"}}) == "test 8");
  std::cout << "PASS runtime CPU arithmetic, full EL, whitespace, "
               "preserved MODS expressions and runtime diagnostics\n";
  std::cout << "PASS customer file: 5 profiles and 70 tasks imported\n";
}
