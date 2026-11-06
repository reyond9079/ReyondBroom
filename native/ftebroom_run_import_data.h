// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "ftebroom_run_profile.h"
#include "ftebroom_run_variables.h"
#include "third_party/json.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>

struct RunImport {
  std::vector<RunProfile> profiles;
  std::vector<std::string> notices;
  std::map<std::string, std::string> missingVariables;
};

static nlohmann::json runImportRead(const std::filesystem::path &path) {
  if (!std::filesystem::is_regular_file(path) ||
      std::filesystem::file_size(path) > 4 * 1024 * 1024)
    throw std::runtime_error(
        "Choose a readable TrenchBroom profile file up to 4 MiB.");
  std::ifstream input(path, std::ios::binary);
  if (!input)
    throw std::runtime_error("Could not read " + path.filename().u8string());
  auto config = nlohmann::json::parse(input, nullptr, true, true);
  const auto &version = config.at("version");
  if (version != 1 && version != 2)
    throw std::runtime_error("Unsupported TrenchBroom profile version.");
  if (!config.at("profiles").is_array())
    throw std::runtime_error("Expected a list of profiles.");
  return config;
}

static std::string
runImportText(std::string value, std::map<std::string, std::string> &required,
              const std::map<std::string, std::string> &existingVariables) {
  if (value.find_first_of("\r\n") != value.npos ||
      value.find('\0') != value.npos)
    throw std::runtime_error("Profile fields must be single-line text.");
  for (size_t start = 0; (start = value.find("${", start)) != value.npos;) {
    const auto end = value.find('}', start + 2);
    if (end == value.npos)
      throw std::runtime_error("Unclosed TrenchBroom variable.");
    auto name = value.substr(start + 2, end - start - 2);
    if (name == "GAME_DIR_PATH")
      name = "TB_GAME_ROOT";
    else if (name == "MODS" || name == "MODS[-1]" || name == "MODS[0]")
      name = "TB_ACTIVE_MOD";
    else if (!ftebroom::runCpuCountMinusOne(name) && name != "MAP_BASE_NAME" &&
             name != "MAP_FULL_NAME" && name != "MAP_DIR_PATH" &&
             name != "CPU_COUNT" && name != "WORK_DIR_PATH" &&
             name != "APP_DIR_PATH") {
      if (name.empty() ||
          name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRS"
                                 "TUVWXYZ0123456789_") != name.npos)
        throw std::runtime_error("Unsupported TrenchBroom expression: " + name);
      auto existing = existingVariables.find(name);
      if (existing == existingVariables.end() || existing->second.empty())
        required.try_emplace(name);
    }
    value.replace(start + 2, end - start - 2, name);
    start += name.size() + 3;
  }
  return value;
}

static RunImport runReadTrenchBroom(
    const std::filesystem::path &path,
    const std::map<std::string, std::string> &existingVariables) {
  auto config = runImportRead(path);
  RunImport result;
  nlohmann::json engines = nlohmann::json::array();
  const auto engineFile = path.parent_path() / "GameEngineProfiles.cfg";
  if (std::filesystem::exists(engineFile)) {
    try {
      engines = runImportRead(engineFile).at("profiles");
    } catch (const std::exception &error) {
      result.notices.push_back("GameEngineProfiles.cfg: " +
                               std::string(error.what()));
    }
  }
  for (const auto &entry : config.at("profiles")) {
    std::string name = "Unnamed profile";
    try {
      auto savedName = entry.at("name").get<std::string>();
      if (savedName.find_first_of("\r\n") != savedName.npos ||
          savedName.find('\0') != savedName.npos)
        throw std::runtime_error("Profile name must be single-line text.");
      if (!savedName.empty())
        name = std::move(savedName);
      RunProfile profile;
      profile.name = name;
      std::map<std::string, std::string> required;
      auto translate = [&](const std::string &value) {
        return runImportText(value, required, existingVariables);
      };
      profile.work = translate(entry.value("workdir", std::string{}));
      if (profile.work.empty())
        profile.work = "${MAP_DIR_PATH}";
      const auto &tasks = entry.at("tasks");
      if (!tasks.is_array())
        throw std::runtime_error("Expected a list of compile steps.");
      std::vector<std::string> notices;
      for (const auto &step : tasks) {
        RunTask task;
        task.enabled = step.value("enabled", true);
        const auto type = step.at("type").get<std::string>();
        auto field = [&](const char *key) {
          return translate(step.at(key).get<std::string>());
        };
        if (type == "export") {
          if (step.value("stripTbProperties", false) ||
              !step.value("stripEntityPattern", std::string{}).empty() ||
              step.contains("entityToAdd"))
            throw std::runtime_error(
                "Export options that change entity data require manual setup.");
          task.source = field("target");
        } else if (type == "tool") {
          task.kind = 1;
          task.source = field("tool");
          task.destination = field("parameters");
          if (!step.value("treatNonZeroResultCodeAsError", false))
            task.flags |= RunAllowNonzero;
        } else if (type == "copy" || type == "rename") {
          task.kind = type == "copy" ? 2 : 3;
          if (type == "copy")
            task.flags |= RunCopyToDirectory;
          task.source = field("source");
          task.destination = field("target");
        } else if (type == "delete") {
          task.kind = 4;
          task.source = field("target");
        } else if (type == "launchEngine") {
          task.kind = 1;
          task.flags = RunLaunchEngine;
          if (!step.value("treatLaunchFailureAsError", false))
            task.flags |= RunAllowLaunchFailure;
          const auto id = step.at("engineProfileId").get<std::string>();
          auto engine = std::find_if(
              engines.begin(), engines.end(), [&](const auto &candidate) {
                return candidate.is_object() && candidate.contains("id") &&
                       candidate["id"] == id;
              });
          if (engine != engines.end()) {
            task.source = translate(engine->at("path").get<std::string>());
            task.destination =
                translate(engine->at("parameters").get<std::string>());
          }
          if (task.source.empty()) {
            task.enabled = false;
            notices.push_back(
                "Launch Engine step disabled. Set its executable and "
                "arguments, or place the matching GameEngineProfiles.cfg "
                "beside the compile file.");
          }
        } else
          throw std::runtime_error("Unsupported TrenchBroom task: " + type);
        profile.tasks.push_back(std::move(task));
      }
      result.profiles.push_back(std::move(profile));
      result.missingVariables.merge(required);
      for (const auto &message : notices)
        result.notices.push_back(name + ": " + message);
    } catch (const std::exception &error) {
      result.notices.push_back(name + ": skipped. " + error.what());
    }
  }
  return result;
}
