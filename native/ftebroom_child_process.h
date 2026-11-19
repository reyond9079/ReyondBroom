// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <atomic>
#include <cerrno>
#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif

namespace ftebroom::child {
namespace fs = std::filesystem;

#ifdef _WIN32
inline std::wstring quote(const std::wstring &value) {
  std::wstring out = L"\"";
  int slashes = 0;
  for (auto c : value) {
    if (c == L'\\') {
      slashes++;
      continue;
    }
    if (c == L'"')
      out.append(slashes * 2 + 1, L'\\');
    else
      out.append(slashes, L'\\');
    slashes = 0;
    out += c;
  }
  out.append(slashes * 2, L'\\');
  return out + L"\"";
}
#endif
inline int run(const fs::path &exe,
               const std::vector<std::string> &args,
               const fs::path &log,
               const std::atomic<bool>* cancel,
               std::chrono::seconds timeout,
               std::string_view name) {
  if (cancel && *cancel) return -1;
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  int code = 1;
  bool expired = false;
#ifdef _WIN32
  std::wstring line = quote(exe.wstring());
  for (const auto &a : args)
    line += L" " + quote(fs::u8path(a).wstring());
  SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
  HANDLE err = CreateFileW(log.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  HANDLE null = CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE,
                            FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                            OPEN_EXISTING, 0, nullptr);
  if (err == INVALID_HANDLE_VALUE || null == INVALID_HANDLE_VALUE) {
    if (err != INVALID_HANDLE_VALUE)
      CloseHandle(err);
    if (null != INVALID_HANDLE_VALUE)
      CloseHandle(null);
    throw std::runtime_error("Cannot create process streams for " + std::string(name) + ".");
  }
  STARTUPINFOEXW si{};
  si.StartupInfo.cb = sizeof si;
  si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
  si.StartupInfo.hStdInput = null;
  si.StartupInfo.hStdOutput = null;
  si.StartupInfo.hStdError = err;
  HANDLE inherited[] = {null, err};
  SIZE_T attributeBytes = 0;
  InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeBytes);
  const auto heap = GetProcessHeap();
  si.lpAttributeList = static_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(HeapAlloc(heap, 0, attributeBytes));
  const bool initialized = si.lpAttributeList &&
    InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &attributeBytes);
  PROCESS_INFORMATION pi{};
  const bool ok = initialized && UpdateProcThreadAttribute(si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
    inherited, sizeof inherited, nullptr, nullptr) &&
    CreateProcessW(exe.c_str(), line.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT,
      nullptr, nullptr, &si.StartupInfo, &pi);
  if (initialized) DeleteProcThreadAttributeList(si.lpAttributeList);
  if (si.lpAttributeList) HeapFree(heap, 0, si.lpAttributeList);
  CloseHandle(err);
  CloseHandle(null);
  if (!ok)
    throw std::runtime_error("Cannot start " + std::string(name) + ".");
  CloseHandle(pi.hThread);
  for (;;) {
    auto status = WaitForSingleObject(pi.hProcess, 20);
    if (status == WAIT_OBJECT_0) {
      DWORD result = 1;
      GetExitCodeProcess(pi.hProcess, &result);
      code = int(result);
      break;
    }
    if (status == WAIT_FAILED || (cancel && *cancel) ||
        std::chrono::steady_clock::now() > deadline) {
      expired = !(cancel && *cancel);
      TerminateProcess(pi.hProcess, 1);
      WaitForSingleObject(pi.hProcess, 5000);
      break;
    }
  }
  CloseHandle(pi.hProcess);
#else
  std::vector<std::string> values{exe.string()};
  values.insert(values.end(), args.begin(), args.end());
  std::vector<char*> argv;
  for (auto &v : values)
    argv.push_back(v.data());
  argv.push_back(nullptr);
  int err = open(log.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600),
      null = open("/dev/null", O_RDWR | O_CLOEXEC);
  if (err < 0 || null < 0) {
    if (err >= 0)
      close(err);
    if (null >= 0)
      close(null);
    throw std::runtime_error("Cannot create process streams for " + std::string(name) + ".");
  }
  posix_spawn_file_actions_t actions;
  pid_t pid = -1;
  int error = posix_spawn_file_actions_init(&actions);
  if (!error) {
    error = posix_spawn_file_actions_adddup2(&actions, null, STDIN_FILENO);
    if (!error) error = posix_spawn_file_actions_adddup2(&actions, null, STDOUT_FILENO);
    if (!error) error = posix_spawn_file_actions_adddup2(&actions, err, STDERR_FILENO);
    if (!error) error = posix_spawn_file_actions_addclose(&actions, err);
    if (!error) error = posix_spawn_file_actions_addclose(&actions, null);
    if (!error) error = posix_spawn(&pid, exe.c_str(), &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
  }
  close(err);
  close(null);
  if (error)
    throw std::runtime_error("Cannot start " + std::string(name) + ".");
  for (;;) {
    int status = 0;
    auto result = waitpid(pid, &status, WNOHANG);
    if (result == pid) {
      code = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
      break;
    }
    if (result < 0 && errno != EINTR)
      throw std::runtime_error("Cannot read exit status for " + std::string(name) + ".");
    if ((cancel && *cancel) ||
        std::chrono::steady_clock::now() > deadline) {
      expired = !(cancel && *cancel);
      kill(pid, SIGKILL);
      while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
      }
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
#endif
  if (expired)
    throw std::runtime_error(std::string(name) + " timed out.");
  return cancel && *cancel ? -1 : code;
}
} // namespace ftebroom::child
