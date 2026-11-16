// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#ifdef _WIN32
#include <dpapi.h>
#else
#include <fcntl.h>
#endif
#include <chrono>
#include <streambuf>
#include <thread>

#include "ftebroom_filesystem_path.h"
#include "ftebroom_jam_http.h"

namespace ftebroom::jam {
namespace fs = std::filesystem;
using Json = nlohmann::json;

struct Error : std::runtime_error {
  bool retryable;
  std::string code;
  Error(const std::string& message, bool retry, std::string errorCode = {})
    : std::runtime_error(message), retryable(retry), code(std::move(errorCode)) {}
};

struct Client {
  Endpoint endpoint;
  std::string token;
  std::string prefix;

  Json json(const std::string& method, const std::string& path, const Json& body = nullptr,
            const std::string& key = {}, const std::string& bytes = {}, ResourceTransfer* transfer = nullptr) const
  {
    const auto response = request(endpoint, method, path, token, key,
      method == "PUT" || path == "/v1/me/avatar" ? bytes : body.is_null() ? std::string{} : body.dump(), 512 * 1024, transfer);
    auto value = response.body.empty() ? Json::object() : Json::parse(response.body, nullptr, false);
    if (value.is_discarded() || !value.is_object()) throw Error("Invalid Mapping Jam API response.", true);
    if (response.status < 200 || response.status >= 300) {
      auto title = value.value("title", std::string("Mapping Jam request failed."));
      const auto code = value.value("code", std::string());
      if (code == "PLOT_ALREADY_OWNED") title = "This plot was just purchased. Refresh the board.";
      if (code == "AUTH_REQUIRED") title = "Sign out and sign in again.";
      throw Error(title, response.status >= 500 || response.status == 429, code);
    }
    return value;
  }

  Json board(ResourceTransfer* transfer = nullptr) const
  {
    return {{"board", json("GET", prefix + "board", nullptr, {}, {}, transfer)},
            {"wallet", json("GET", prefix + "me/wallet", nullptr, {}, {}, transfer)}};
  }
};

inline std::string fileBytes(const fs::path& file, std::size_t limit)
{
  const auto nativeFile = ftebroom::filesystemPath(file);
  const auto size = fs::file_size(nativeFile);
  if (size > limit) throw std::runtime_error("The local file exceeds its size limit.");
  std::string result(static_cast<std::size_t>(size), '\0');
  std::ifstream in(nativeFile, std::ios::binary);
  if (!in.read(result.data(), result.size())) throw std::runtime_error("Cannot read the local file.");
  return result;
}

inline void writeBytes(const fs::path& path, std::string_view bytes, ResourceTransfer* transfer = nullptr)
{
  if (transfer) transfer->check();
  const auto file=ftebroom::filesystemPath(path);
#ifdef _WIN32
  // std::filesystem's recursive directory walk may misread the drive root
  // of an extended-length Win32 path as a directory that needs creation.
  std::vector<fs::path> missing;
  for (auto parent=file.parent_path(); !parent.empty(); parent=parent.parent_path()) {
    const auto attributes=GetFileAttributesW(parent.c_str());
    if (attributes!=INVALID_FILE_ATTRIBUTES) {
      if (!(attributes&FILE_ATTRIBUTE_DIRECTORY))
        throw fs::filesystem_error("The Jam path is not a directory",parent,std::make_error_code(std::errc::not_a_directory));
      break;
    }
    const auto error=GetLastError();
    if ((error!=ERROR_FILE_NOT_FOUND && error!=ERROR_PATH_NOT_FOUND) || parent==parent.parent_path())
      throw fs::filesystem_error("Cannot access the Jam directory",parent,std::error_code(error,std::system_category()));
    missing.push_back(parent);
  }
  for (auto entry=missing.rbegin();entry!=missing.rend();++entry) {
    if (CreateDirectoryW(entry->c_str(),nullptr)) continue;
    const auto error=GetLastError();
    const auto attributes=GetFileAttributesW(entry->c_str());
    if (error!=ERROR_ALREADY_EXISTS || attributes==INVALID_FILE_ATTRIBUTES || !(attributes&FILE_ATTRIBUTE_DIRECTORY))
      throw fs::filesystem_error("Cannot create the Jam directory",*entry,std::error_code(error,std::system_category()));
  }
#else
  fs::create_directories(file.parent_path());
#endif
  std::ofstream output(file, std::ios::binary | std::ios::trunc);
  std::size_t finalChunk = 0;
  if (!transfer) {
    output.write(bytes.data(),bytes.size());
  } else {
    while (!bytes.empty()) {
      transfer->check();
      const auto count = std::min<std::size_t>(bytes.size(),256*1024);
      if (!output.write(bytes.data(),count))
        throw std::runtime_error("Cannot write the Mapping Jam working file.");
      bytes.remove_prefix(count);
      if (bytes.empty()) finalChunk = count;
      else transfer->wrote(count);
    }
  }
  output.close();
  if (!output) throw std::runtime_error("Cannot write the Mapping Jam working file.");
  if (transfer) {
    transfer->check();
    transfer->wrote(finalChunk);
  }
}

inline void forgetSession(const fs::path& path)
{
  std::error_code ignored;
  fs::remove(path,ignored);
}

#ifdef _WIN32
inline std::string sessionCrypt(const std::string& bytes,bool encrypt)
{
  const auto library=LoadLibraryW(L"crypt32.dll");
  if(!library)return {};
  const auto protect=reinterpret_cast<decltype(&CryptProtectData)>(GetProcAddress(library,"CryptProtectData"));
  const auto unprotect=reinterpret_cast<decltype(&CryptUnprotectData)>(GetProcAddress(library,"CryptUnprotectData"));
  DATA_BLOB input{static_cast<DWORD>(bytes.size()),reinterpret_cast<BYTE*>(const_cast<char*>(bytes.data()))},output{};
  const bool ok=encrypt ? protect&&protect(&input,L"ReyondBroom Mapping Jams",nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&output)
                        : unprotect&&unprotect(&input,nullptr,nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&output);
  const std::string result=ok ? std::string(reinterpret_cast<const char*>(output.pbData),output.cbData) : "";
  if(output.pbData)LocalFree(output.pbData);
  FreeLibrary(library);
  return result;
}
#endif

inline bool rememberSession(const fs::path& path,const std::string& token)
{
  const auto temp=path.parent_path()/("session-"+randomHex(8)+".tmp");
  try {
    fs::create_directories(path.parent_path());
#ifdef _WIN32
    const auto encrypted=sessionCrypt(token,true);
    if(encrypted.empty())return false;
    writeBytes(temp,encrypted);
    if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING))throw std::runtime_error("Cannot replace the saved Jam session.");
#else
    const int fd=open(temp.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);
    if(fd<0)return false;
    const auto count=write(fd,token.data(),token.size());
    const int closed=close(fd);
    if(count!=static_cast<ssize_t>(token.size())||closed!=0)throw std::runtime_error("Cannot write the saved Jam session.");
    fs::rename(temp,path);
#endif
    return true;
  }catch(const std::exception&){forgetSession(temp);return false;}
}

inline std::string savedSession(const fs::path& path)
{
  try {
    if(fs::is_symlink(path)||!fs::is_regular_file(path))return {};
#ifndef _WIN32
    if((fs::status(path).permissions() & (fs::perms::group_all|fs::perms::others_all))!=fs::perms::none)return {};
#endif
    auto token=fileBytes(path,4096);
#ifdef _WIN32
    token=sessionCrypt(token,false);
#endif
    if(token.size()<20||token.size()>120||token.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")!=token.npos)return {};
    return token;
  }catch(const std::exception&){return {};}
}

inline bool packEntry(const std::string& path)
{
  if (path == "lq1/pak0.pak" || path == "lq1/pak1.pak" || path == "plants/builtin/catalog.json") return true;
  constexpr std::string_view prefix = "lq1/texture-wads/";
  if (path.rfind(prefix, 0) != 0 || path.size() < prefix.size() + 5 ||
      path.compare(path.size() - 4, 4, ".wad") != 0)
    return false;
  return path.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_", prefix.size()) == path.size()-4;
}

inline void renameArtifactDirectory(const fs::path& source, const fs::path& target)
{
#ifdef _WIN32
  for (int attempt=0;;++attempt) {
    if (MoveFileExW(source.c_str(),target.c_str(),0)) return;
    const auto error=GetLastError();
    if (attempt>=8 || (error!=ERROR_SHARING_VIOLATION && error!=ERROR_LOCK_VIOLATION && error!=ERROR_ACCESS_DENIED))
      throw fs::filesystem_error("Cannot install the Mapping Jam files",source,target,
                                 std::error_code(error,std::system_category()));
    // Windows may briefly hold a freshly extracted file open during scanning.
    std::this_thread::sleep_for(std::chrono::milliseconds(25*(attempt+1)));
  }
#else
  fs::rename(source,target);
#endif
}

inline bool validPack(const fs::path& destination, const Json& files, ResourceTransfer* transfer)
{
  const auto root = ftebroom::filesystemPath(destination);
  for (const auto& item : files) {
    if (transfer) transfer->check();
    const auto name = item.at("path").get<std::string>();
    const auto length = item.at("sizeBytes").get<std::size_t>();
    const auto file = root / fs::u8path(name).make_preferred();
    if (!packEntry(name) || fs::is_symlink(file) || !fs::is_regular_file(file) || fs::file_size(file) != length ||
        sha256(fileBytes(file, 64 * 1024 * 1024)) != item.at("sha256").get<std::string>())
      return false;
  }
  return fs::is_regular_file(root / "lq1" / "pak0.pak");
}

inline Json packReceipt(const fs::path& root, const Json& manifest)
{
  const auto& files = manifest.at("files");
  if (!files.is_array() || files.empty() || files.size()>64) return nullptr;
  Json inventory = Json::object();
  for (const auto& item : files) {
    const auto name = item.at("path").get<std::string>();
    if (!packEntry(name) || inventory.contains(name)) return nullptr;
    const auto file = root/fs::u8path(name).make_preferred();
    if (fs::is_symlink(file) || !fs::is_regular_file(file) ||
        fs::file_size(file)!=item.at("sizeBytes").get<std::size_t>()) return nullptr;
    inventory[name] = fs::last_write_time(file).time_since_epoch().count();
  }
  if (!inventory.contains("lq1/pak0.pak") || !inventory.contains("plants/builtin/catalog.json")) return nullptr;
  return {{"manifest",manifest.at("sha256")},{"files",std::move(inventory)}};
}

inline void writePackReceipt(const fs::path& root, const Json& receipt)
{
  const auto marker=root/".verified-pack.json";
  const auto temporary=root/(".verified-"+randomHex(8)+".tmp");
  try {
    writeBytes(temporary,receipt.dump());
#ifdef _WIN32
    if(!MoveFileExW(temporary.c_str(),marker.c_str(),MOVEFILE_REPLACE_EXISTING))throw std::runtime_error("Cannot replace the pack receipt.");
#else
    fs::rename(temporary,marker);
#endif
  } catch (const std::exception&) {
    std::error_code ignored;
    fs::remove(temporary,ignored);
  }
}

inline bool installedPack(const fs::path& destination, const Json& manifest, ResourceTransfer* transfer = nullptr)
{
  try {
    const auto root = ftebroom::filesystemPath(destination);
    const auto receipt = packReceipt(root,manifest);
    if (receipt.is_null()) return false;
    const auto marker = root/".verified-pack.json";
    if (fs::is_regular_file(marker) && !fs::is_symlink(marker) && fs::file_size(marker)<16384 &&
        Json::parse(fileBytes(marker,16384),nullptr,false)==receipt) return true;
    if (!validPack(destination,manifest.at("files"),transfer)) return false;
    if (transfer) transfer->check();
    writePackReceipt(root,receipt);
    return true;
  } catch (const DownloadCanceled&) { throw; }
    catch (const DownloadInterrupted&) { throw; }
    catch (const std::exception&) {return false;}
}

// ZIP headers use seeks. Read them from the HTTP response without another
// copy of the archive or a temporary ZIP on the user's filesystem.
class ResourceArchive : public std::streambuf {
  pos_type seekoff(off_type offset, std::ios_base::seekdir direction,
                   std::ios_base::openmode mode) override
  {
    if (!(mode & std::ios_base::in)) return pos_type(-1);
    const off_type size = egptr()-eback();
    const off_type base = direction==std::ios_base::cur ? gptr()-eback() :
                          direction==std::ios_base::end ? size : 0;
    if (offset < -base || offset > size-base) return pos_type(-1);
    setg(eback(),eback()+base+offset,egptr());
    return pos_type(base+offset);
  }
  pos_type seekpos(pos_type position, std::ios_base::openmode mode) override
  {
    return seekoff(off_type(position),std::ios_base::beg,mode);
  }
public:
  explicit ResourceArchive(std::string_view bytes)
  {
    auto* first = const_cast<char*>(bytes.data());
    setg(first,first,first+bytes.size());
  }
};

inline void download(const Client& client, const Json& manifest, const fs::path& destination, bool base,
                     const std::string& mapStem = "jam_map", bool reuse = false,
                     ResourceTransfer* transfer = nullptr, fs::path* retired = nullptr,
                     bool privatePack = false)
{
  if (privatePack && !base) throw std::logic_error("A private installation requires a base pack.");
  if (!base && (mapStem != "jam_map" && mapStem != "jam_hub"))
    throw std::runtime_error("Unknown Jam map name.");
  const std::string mapPath = "maps/" + mapStem;
  const std::size_t maxSize = base ? 128 * 1024 * 1024 : 32 * 1024 * 1024;
  const auto length = manifest.at("sizeBytes").get<std::size_t>();
  const auto& files = manifest.at("files");
  if (manifest.value("schemaVersion", 0) != 1 || !files.is_array() || files.empty() ||
      files.size() > 64 || !length || length > maxSize)
    throw std::runtime_error("The Jam manifest is invalid or exceeds the size limit.");
  std::map<std::string, Json> members;
  std::size_t total = 0;
  for (const auto& entry : files) {
    const auto name = entry.at("path").get<std::string>();
    const auto count = entry.at("sizeBytes").get<std::size_t>();
    if (!(base ? packEntry(name) : (name == mapPath+".bsp" || name == mapPath+".lit")) ||
        count > maxSize - total ||
        !members.emplace(name, entry).second)
      throw std::runtime_error("The Jam manifest contains an unexpected file.");
    total += count;
  }
  if (!members.count(base ? "lq1/pak0.pak" : mapPath+".bsp") ||
      (base && !members.count("plants/builtin/catalog.json")))
    throw std::runtime_error("The Jam manifest is incomplete.");
  const auto target = ftebroom::filesystemPath(destination);
  if (transfer) transfer->check();
  if (base && !privatePack && installedPack(destination, manifest,transfer)) return;
  if (!base && reuse) {
    const bool present = std::all_of(members.begin(),members.end(),[&](const auto& item) {
      const auto file = target/fs::u8path(item.first).make_preferred();
      return !fs::is_symlink(file) && fs::is_regular_file(file) &&
        fs::file_size(file)==item.second.at("sizeBytes").template get<std::size_t>() &&
        sha256(fileBytes(file,maxSize))==item.second.at("sha256").template get<std::string>();
    });
    const bool staleLit = !members.count(mapPath+".lit") && fs::exists(target/fs::u8path(mapPath+".lit").make_preferred());
    if (present && !staleLit) return;
  }

  const auto path = manifest.at("downloadPath").get<std::string>();
  HttpResponse response{};
  for (int attempt=0;;++attempt) {
    if (transfer) {
      transfer->check();
      transfer->total = length;
      transfer->bytes = 0;
      transfer->phase = ResourcePhase::Downloading;
    }
    try {
      response = request(client.endpoint,"GET",path,client.token,{},{},length+4096,transfer);
      if (response.status==200 && response.body.size()<length)
        throw DownloadInterrupted("The Mapping Jam download was incomplete.");
      if (response.status!=408 && response.status!=429 && response.status!=500 &&
          response.status!=502 && response.status!=503 && response.status!=504) break;
      if (response.status==429 && transfer)
        throw Error("The Mapping Jam server asked the client to wait.",true,"RATE_LIMITED");
      if (attempt==2) throw Error("The Jam download service is temporarily unavailable. Try again.",true);
    } catch (const DownloadInterrupted&) {
      if (attempt==2) throw;
    }
    if (transfer) transfer->phase = ResourcePhase::Retrying;
    std::this_thread::sleep_for(std::chrono::seconds(attempt+1));
  }
  if (response.status == 404) throw Error("The Jam publication changed during download.", true, "ARTIFACT_CHANGED");
  if (response.status == 401 || response.status == 403)
    throw Error("This session cannot download the Jam map. Sign in again and retry.",false,
                response.status==401 ? "AUTH_REQUIRED" : "");
  if (response.status != 200)
    throw Error("The Jam download failed (HTTP "+std::to_string(response.status)+").",false);
  if (transfer) {
    transfer->check();
    transfer->total = 0;
    transfer->bytes = 0;
    transfer->localPhase(ResourcePhase::Verifying);
  }
  if (response.body.size() != length ||
      sha256(response.body) != manifest.at("sha256").get<std::string>())
    throw std::runtime_error("The Jam download did not match its manifest.");

  auto staging = target;
  if (privatePack) {
    // The worker owns this fresh directory. Only a completed receipt makes
    // it discoverable, so installation needs no directory move or exchange.
    if (!fs::create_directory(staging))
      throw std::runtime_error("The Mapping Jam installation directory is already in use.");
  } else staging += ".incoming-" + randomHex(8);
  try {
    const std::string_view archive = response.body;
    ResourceArchive buffer(archive);
    std::istream input(&buffer);
    const auto entries = ftebroom::pak::zipDirectory(input,archive.size());
    if (entries.size() != members.size()) throw std::runtime_error("Unexpected files in Jam download.");
    if (transfer) {
      transfer->check();
      transfer->total = total;
      transfer->localPhase(ResourcePhase::Installing);
    }
    for (const auto& entry : entries) {
      if (transfer) transfer->check();
      const auto i = members.find(entry.name);
      if (i == members.end() || entry.size != i->second.at("sizeBytes").get<std::size_t>())
        throw std::runtime_error("Unexpected file or size in Jam download.");
      std::string inflated;
      std::string_view bytes;
      if (entry.method==0) {
        if (entry.compressed!=entry.size) throw std::runtime_error("Invalid stored Jam file size.");
        const auto offset = ftebroom::pak::zipData(input,entry,archive.size());
        bytes = archive.substr(offset,entry.size);
        if (crc32(0,reinterpret_cast<const Bytef*>(bytes.data()),entry.size)!=entry.crc)
          throw std::runtime_error("A Jam file failed its ZIP checksum.");
      } else {
        inflated = ftebroom::pak::zipRead(input,archive.size(),entry);
        bytes = inflated;
      }
      if (sha256(bytes) != i->second.at("sha256").get<std::string>())
        throw std::runtime_error("An extracted Jam file did not match its manifest.");
      writeBytes(staging / fs::u8path(entry.name).make_preferred(),bytes,transfer);
    }
    if (transfer) {
      transfer->check();
      transfer->localPhase(ResourcePhase::Finalizing);
    }
    if (base) {
      const auto receipt = packReceipt(staging,manifest);
      if (receipt.is_null()) throw std::runtime_error("The installed Jam resources failed verification.");
      if (privatePack) writeBytes(staging/".verified-pack.json",receipt.dump());
      else writePackReceipt(staging,receipt);
    }
    if (transfer) transfer->check();
    if (privatePack) return;
    if (fs::exists(target)) {
      auto previous = target;
      previous += ".previous-" + randomHex(8);
      renameArtifactDirectory(target,previous);
      try { renameArtifactDirectory(staging,target); }
      catch (...) { renameArtifactDirectory(previous,target); throw; }
      if (retired) *retired = std::move(previous);
      else {
        std::error_code ignored;
        fs::remove_all(previous, ignored);
      }
    } else {
      renameArtifactDirectory(staging,target);
    }
  } catch (...) {
    std::error_code ignored;
    fs::remove_all(staging, ignored);
    throw;
  }
}

inline bool hexId(const std::string& value, std::size_t size)
{
  return value.size()==size && value.find_first_not_of("0123456789abcdef")==value.npos;
}

inline Json sendMap(const Client& client, const std::string& plot, const Json& rules,
                    const Json& baseRevision, const std::string& title, const fs::path& snapshot)
{
  const auto bytes = fileBytes(snapshot, 2 * 1024 * 1024);
  if (bytes.empty()) throw std::runtime_error("The exported map is empty.");
  const auto archive = ftebroom::filesystemPath(snapshot.parent_path() / "source.zip");
  std::string empty(22, '\0');
  empty.replace(0, 4, "PK\x05\x06", 4);
  writeBytes(archive, empty);
  ftebroom::pak::zipRewrite(archive, {
    {"manifest.json", Json{{"schemaVersion", 1}, {"sha256", sha256(bytes)}}.dump()},
    {"map/source.map", bytes}
  }, {}, {}, {}, false);
  const auto payload = fileBytes(archive, 2 * 1024 * 1024);
  const auto upload = client.json("POST", client.prefix + "plots/" + plot + "/uploads",
    {{"title", title}, {"baseRevisionId", baseRevision}, {"rulesVersion", rules.at("rulesVersion")},
     {"packVersion", rules.at("packVersion")}, {"termsVersion", rules.at("termsVersion")},
     {"sourceSha256", sha256(payload)}, {"sourceSizeBytes", payload.size()}}, randomHex(16));
  client.json("PUT", upload.at("uploadPath"), nullptr, {}, payload);
  return client.json("POST", client.prefix + "uploads/" + upload.at("uploadId").get<std::string>() + "/finalize",
                     Json::object(), randomHex(16));
}
} // namespace ftebroom::jam
