// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#else
#include <arpa/inet.h>
#include <curl/curl.h>
#include <dlfcn.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace ftebroom::jam {
struct HttpResponse {
  int status;
  std::string body;
};

struct Endpoint {
  unsigned port = 0;
  std::string hostname;
};

inline Endpoint httpsEndpoint(std::string_view url)
{
  constexpr std::string_view scheme = "https://";
  if (url.substr(0, scheme.size()) != scheme) return {};
  url.remove_prefix(scheme.size());
  if (!url.empty() && url.back() == '/') url.remove_suffix(1);
  if (url.empty() || url.size() > 253 || url.find('.') == url.npos) return {};
  std::string host;
  for (char c : url) {
    if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.')) return {};
    host += c;
  }
  for (std::size_t begin = 0;;) {
    const auto end = host.find('.', begin);
    const auto length = (end == host.npos ? host.size() : end) - begin;
    if (!length || length > 63 || host[begin] == '-' || host[begin+length-1] == '-') return {};
    if (end == host.npos) break;
    begin = end + 1;
  }
  return {443, std::move(host)};
}

inline bool safePath(std::string_view path)
{
  return path.rfind("/v1/", 0) == 0 && path.find("..") == path.npos
         && path.find_first_not_of("/-_.0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ") == path.npos;
}

inline std::string randomHex(std::size_t bytes)
{
  static constexpr char hex[] = "0123456789abcdef";
  std::random_device source;
  std::uniform_int_distribution<unsigned> byte(0, 255);
  std::string result;
  for (std::size_t i = 0; i < bytes; ++i) {
    const auto n = byte(source);
    result += hex[n >> 4];
    result += hex[n & 15];
  }
  return result;
}

#ifdef _WIN32
template<class T> T loadProc(HMODULE library, const char* name)
{
  const auto fn = library ? GetProcAddress(library, name) : nullptr;
  if (!fn) throw std::runtime_error("The system network or cryptography component is unavailable.");
  return reinterpret_cast<T>(fn);
}
#endif

inline std::string sha256(std::string_view bytes)
{
  std::array<unsigned char, 32> hash{};
#ifdef _WIN32
  static const auto library = LoadLibraryExW(L"bcrypt.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
  const auto open = loadProc<decltype(&BCryptOpenAlgorithmProvider)>(library, "BCryptOpenAlgorithmProvider");
  const auto close = loadProc<decltype(&BCryptCloseAlgorithmProvider)>(library, "BCryptCloseAlgorithmProvider");
  using Hash = NTSTATUS (WINAPI *)(BCRYPT_ALG_HANDLE, PUCHAR, ULONG, PUCHAR, ULONG, PUCHAR, ULONG);
  const auto calculate = loadProc<Hash>(library, "BCryptHash");
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

#ifndef _WIN32
template<class T> inline T curlSymbol(void* library, const char* name)
{
  const auto proc = library ? dlsym(library, name) : nullptr;
  if (!proc) throw std::runtime_error("HTTPS for Mapping Jams requires libcurl.so.4.");
  return reinterpret_cast<T>(proc);
}

inline HttpResponse requestHttps(const Endpoint& endpoint, const std::string& method,
                                 const std::string& path, const std::string& headers,
                                 const std::string& body, std::size_t limit)
{
  static auto* const library = dlopen("libcurl.so.4", RTLD_NOW | RTLD_LOCAL);
  static const auto globalInit = curlSymbol<decltype(&curl_global_init)>(library,"curl_global_init");
  static const auto initialized = globalInit(CURL_GLOBAL_DEFAULT);
  if (initialized != CURLE_OK) throw std::runtime_error("Cannot initialize HTTPS for Mapping Jams.");
  static const auto init = curlSymbol<decltype(&curl_easy_init)>(library,"curl_easy_init");
  static const auto cleanup = curlSymbol<decltype(&curl_easy_cleanup)>(library,"curl_easy_cleanup");
  static const auto option = curlSymbol<decltype(&curl_easy_setopt)>(library,"curl_easy_setopt");
  static const auto perform = curlSymbol<decltype(&curl_easy_perform)>(library,"curl_easy_perform");
  static const auto info = curlSymbol<decltype(&curl_easy_getinfo)>(library,"curl_easy_getinfo");
  static const auto append = curlSymbol<decltype(&curl_slist_append)>(library,"curl_slist_append");
  static const auto freeList = curlSymbol<decltype(&curl_slist_free_all)>(library,"curl_slist_free_all");
  struct Handle { CURL* value; ~Handle() { if (value) cleanup(value); } } handle{init()};
  struct Headers { curl_slist* value = nullptr; ~Headers() { freeList(value); } } list;
  if (!handle.value) throw std::runtime_error("Cannot start the Mapping Jam HTTPS client.");
  for (std::size_t pos = 0; pos < headers.size();) {
    const auto next = headers.find("\r\n",pos);
    const auto line = headers.substr(pos,next-pos);
    auto* entry = append(list.value,line.c_str());
    if (!entry) throw std::runtime_error("Cannot prepare Mapping Jam request headers.");
    list.value = entry;
    pos = next == headers.npos ? headers.size() : next+2;
  }
  const auto configure = [&](CURLoption name, auto value) {
    if (option(handle.value,name,value) != CURLE_OK)
      throw std::runtime_error("Cannot configure the Mapping Jam HTTPS connection.");
  };
  struct Download { HttpResponse result{}; std::size_t limit; bool oversized = false; } download{{},limit};
  const auto write = +[](char* bytes, std::size_t size, std::size_t count, void* context) -> std::size_t {
    auto& target = *static_cast<Download*>(context);
    const auto length = size*count;
    if (length > target.limit-target.result.body.size()) {
      target.oversized = true;
      return 0;
    }
    try { target.result.body.append(bytes,length); }
    catch (...) { return 0; }
    return length;
  };
  const auto url = "https://"+endpoint.hostname+path;
  configure(CURLOPT_URL,url.c_str());
  configure(CURLOPT_FOLLOWLOCATION,0L);
  configure(CURLOPT_SSL_VERIFYPEER,1L);
  configure(CURLOPT_SSL_VERIFYHOST,2L);
  configure(CURLOPT_CONNECTTIMEOUT,6L);
  configure(CURLOPT_TIMEOUT,90L);
  configure(CURLOPT_LOW_SPEED_LIMIT,1L);
  configure(CURLOPT_LOW_SPEED_TIME,15L);
  configure(CURLOPT_NOSIGNAL,1L);
  configure(CURLOPT_HTTPHEADER,list.value);
  configure(CURLOPT_USERAGENT,"ReyondBroom Mapping Jams");
  configure(CURLOPT_WRITEFUNCTION,write);
  configure(CURLOPT_WRITEDATA,&download);
  if (method != "GET") {
    configure(CURLOPT_POSTFIELDS,body.c_str());
    configure(CURLOPT_POSTFIELDSIZE_LARGE,static_cast<curl_off_t>(body.size()));
    configure(CURLOPT_CUSTOMREQUEST,method.c_str());
  }
  const auto code = perform(handle.value);
  if (download.oversized) throw std::runtime_error("The Jam response exceeds its size limit.");
  if (code == CURLE_PEER_FAILED_VERIFICATION)
    throw std::runtime_error("The Mapping Jam server's TLS certificate could not be verified.");
  long status = 0;
  if (code != CURLE_OK || info(handle.value,CURLINFO_RESPONSE_CODE,&status) != CURLE_OK || !status)
    throw std::runtime_error("Cannot reach the Mapping Jam server over HTTPS.");
  download.result.status = static_cast<int>(status);
  return std::move(download.result);
}
#endif

inline HttpResponse request(const Endpoint& endpoint, const std::string& method, const std::string& path,
                            const std::string& token, const std::string& key, const std::string& body,
                            std::size_t limit)
{
  const unsigned port = endpoint.port;
  const bool secure = !endpoint.hostname.empty();
  if (!safePath(path) || (secure ? port != 443 : (port < 1024 || port > 65535)))
    throw std::runtime_error("Invalid Mapping Jam endpoint.");
  std::string headers = "Connection: close\r\n";
  if (!token.empty()) headers += "Authorization: Bearer " + token + "\r\n";
  if (!key.empty()) headers += "Idempotency-Key: " + key + "\r\n";
  if (method != "GET") headers += std::string("Content-Type: ") +
    (method == "PUT" || path == "/v1/me/avatar" ? "application/octet-stream\r\n" : "application/json\r\n");
  HttpResponse result{};
  std::array<char, 65536> buffer{};
#ifdef _WIN32
  static const auto library = LoadLibraryExW(L"winhttp.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
  const auto open = loadProc<decltype(&WinHttpOpen)>(library, "WinHttpOpen");
  const auto connect = loadProc<decltype(&WinHttpConnect)>(library, "WinHttpConnect");
  const auto openRequest = loadProc<decltype(&WinHttpOpenRequest)>(library, "WinHttpOpenRequest");
  const auto send = loadProc<decltype(&WinHttpSendRequest)>(library, "WinHttpSendRequest");
  const auto receive = loadProc<decltype(&WinHttpReceiveResponse)>(library, "WinHttpReceiveResponse");
  const auto query = loadProc<decltype(&WinHttpQueryHeaders)>(library, "WinHttpQueryHeaders");
  const auto read = loadProc<decltype(&WinHttpReadData)>(library, "WinHttpReadData");
  const auto close = loadProc<decltype(&WinHttpCloseHandle)>(library, "WinHttpCloseHandle");
  const auto timeout = loadProc<decltype(&WinHttpSetTimeouts)>(library, "WinHttpSetTimeouts");
  const auto setOption = loadProc<decltype(&WinHttpSetOption)>(library, "WinHttpSetOption");
  struct Handle {
    HINTERNET value;
    decltype(close) release;
    ~Handle() { if (value) release(value); }
  };
  Handle session{open(L"ReyondBroom Mapping Jams", WINHTTP_ACCESS_TYPE_NO_PROXY, nullptr, nullptr, 0), close};
  if (!session.value || !timeout(session.value, 3000, 3000, 12000, 12000))
    throw std::runtime_error("Cannot start the local HTTP client.");
  const std::wstring hostname = secure ? std::wstring(endpoint.hostname.begin(),endpoint.hostname.end()) : L"127.0.0.1";
  Handle host{connect(session.value, hostname.c_str(), static_cast<INTERNET_PORT>(port), 0), close};
  if (!host.value) throw std::runtime_error("Cannot open the Mapping Jam server.");
  const std::wstring verb(method.begin(), method.end()), route(path.begin(), path.end());
  Handle req{openRequest(host.value, verb.c_str(), route.c_str(), nullptr, nullptr, nullptr,
                         secure ? WINHTTP_FLAG_SECURE : 0), close};
  DWORD disabled = WINHTTP_DISABLE_REDIRECTS | WINHTTP_DISABLE_COOKIES | WINHTTP_DISABLE_AUTHENTICATION;
  const std::wstring wideHeaders(headers.begin(), headers.end());
  if (!req.value || !setOption(req.value, WINHTTP_OPTION_DISABLE_FEATURE, &disabled, sizeof disabled) ||
      !send(req.value, wideHeaders.c_str(), static_cast<DWORD>(wideHeaders.size()),
        body.empty() ? nullptr : const_cast<char*>(body.data()), static_cast<DWORD>(body.size()),
        static_cast<DWORD>(body.size()), 0) || !receive(req.value, nullptr))
    throw std::runtime_error("Cannot reach the Mapping Jam server.");
  DWORD status = 0, size = sizeof status;
  if (!query(req.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
             WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX))
    throw std::runtime_error("The local server returned an invalid HTTP response.");
  result.status = static_cast<int>(status);
  for (;;) {
    DWORD n = 0;
    if (!read(req.value, buffer.data(), static_cast<DWORD>(buffer.size()), &n))
      throw std::runtime_error("The local download was interrupted.");
    if (!n) break;
    if (n > limit - result.body.size()) throw std::runtime_error("The local response exceeds its size limit.");
    result.body.append(buffer.data(), n);
  }
#else
  if (secure) return requestHttps(endpoint,method,path,headers,body,limit);
  struct Socket { int value; ~Socket() { if (value >= 0) close(value); } };
  Socket client{socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0)};
  sockaddr_in host{};
  host.sin_family = AF_INET;
  host.sin_port = htons(static_cast<uint16_t>(port));
  host.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (client.value < 0 || connect(client.value, reinterpret_cast<sockaddr*>(&host), sizeof host) != 0)
    throw std::runtime_error("Cannot reach the local Mapping Jam server.");
  timeval timeout{12, 0};
  if (setsockopt(client.value, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout) ||
      setsockopt(client.value, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof timeout))
    throw std::runtime_error("Cannot configure the local HTTP connection.");
  std::string message = method + " " + path + " HTTP/1.1\r\nHost: 127.0.0.1:" + std::to_string(port) +
    "\r\n" + headers + "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
  for (std::size_t sent = 0; sent < message.size();) {
    const auto n = send(client.value, message.data() + sent, message.size() - sent, MSG_NOSIGNAL);
    if (n <= 0) throw std::runtime_error("The local request was interrupted.");
    sent += static_cast<std::size_t>(n);
  }
  std::string received;
  std::size_t offset = std::string::npos, expected = 0;
  for (;;) {
    const auto n = recv(client.value, buffer.data(), buffer.size(), 0);
    if (n < 0) throw std::runtime_error("The local download was interrupted.");
    if (!n) break;
    received.append(buffer.data(), static_cast<std::size_t>(n));
    if (offset == received.npos) {
      const auto end = received.find("\r\n\r\n");
      if (end == received.npos) {
        if (received.size() > 16384) throw std::runtime_error("Invalid local HTTP headers.");
        continue;
      }
      if (received.rfind("HTTP/1.", 0) != 0 || end > 16384)
        throw std::runtime_error("Invalid local HTTP response.");
      const auto length = received.find("\r\nContent-Length: ");
      if (length == received.npos || length >= end || received.find("\r\nTransfer-Encoding:") < end)
        throw std::runtime_error("Invalid local HTTP length.");
      result.status = std::stoi(received.substr(9, 3));
      expected = std::stoull(received.substr(length + 18));
      offset = end + 4;
      if (expected > limit) throw std::runtime_error("The local response exceeds its size limit.");
    }
    if (received.size() - offset >= expected) break;
  }
  if (offset == received.npos || received.size() - offset != expected)
    throw std::runtime_error("Incomplete local HTTP response.");
  result.body.assign(received, offset, expected);
#endif
  return result;
}

inline HttpResponse request(unsigned port, const std::string& method, const std::string& path,
                            const std::string& token, const std::string& key, const std::string& body,
                            std::size_t limit)
{
  return request(Endpoint{port,{}},method,path,token,key,body,limit);
}
} // namespace ftebroom::jam
