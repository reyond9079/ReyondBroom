// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#else
#include <dlfcn.h>
#endif

namespace ftebroom {

inline std::string sha256(std::string_view bytes)
{
  std::array<unsigned char, 32> hash{};
#ifdef _WIN32
  static const auto library = LoadLibraryExW(L"bcrypt.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (!library) throw std::runtime_error("The system SHA-256 implementation is unavailable.");
  if (bytes.size() > std::numeric_limits<ULONG>::max())
    throw std::runtime_error("The SHA-256 input exceeds the platform limit.");
  const auto open = reinterpret_cast<decltype(&BCryptOpenAlgorithmProvider)>(GetProcAddress(library, "BCryptOpenAlgorithmProvider"));
  const auto close = reinterpret_cast<decltype(&BCryptCloseAlgorithmProvider)>(GetProcAddress(library, "BCryptCloseAlgorithmProvider"));
  using Hash = NTSTATUS (WINAPI *)(BCRYPT_ALG_HANDLE, PUCHAR, ULONG, PUCHAR, ULONG, PUCHAR, ULONG);
  const auto calculate = reinterpret_cast<Hash>(GetProcAddress(library, "BCryptHash"));
  if (!open || !close || !calculate)
    throw std::runtime_error("The system SHA-256 implementation is unavailable.");
  BCRYPT_ALG_HANDLE handle = nullptr;
  if (open(&handle, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0)
    throw std::runtime_error("Cannot open SHA-256.");
  const auto status = calculate(handle, nullptr, 0,
    reinterpret_cast<PUCHAR>(const_cast<char*>(bytes.data())), static_cast<ULONG>(bytes.size()),
    hash.data(), static_cast<ULONG>(hash.size()));
  close(handle, 0);
  if (status != 0) throw std::runtime_error("Cannot calculate SHA-256.");
#else
  static const auto library = [] {
    auto* current = dlopen("libcrypto.so.3", RTLD_NOW | RTLD_LOCAL);
    return current ? current : dlopen("libcrypto.so", RTLD_NOW | RTLD_LOCAL);
  }();
  using Hash = unsigned char* (*)(const unsigned char*, std::size_t, unsigned char*);
  const auto calculate = library ? reinterpret_cast<Hash>(dlsym(library, "SHA256")) : nullptr;
  if (!calculate || !calculate(reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size(), hash.data()))
    throw std::runtime_error("The system SHA-256 implementation is unavailable.");
#endif
  constexpr char hex[] = "0123456789abcdef";
  std::string result;
  result.reserve(hash.size() * 2);
  for (const auto b : hash) {
    result += hex[b >> 4];
    result += hex[b & 15];
  }
  return result;
}

} // namespace ftebroom
