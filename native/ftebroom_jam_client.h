// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#ifdef _WIN32
#include <dpapi.h>
#else
#include <fcntl.h>
#endif
#include <chrono>
#include <memory>
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
    if (transfer && transfer->phase==ResourcePhase::Checking) transfer->record("manifest_http",response.status);
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
  return ftebroom::pak::readFile(file,limit);
}

inline void createArtifactDirectories(const fs::path& directory)
{
#ifdef _WIN32
  // std::filesystem's recursive directory walk may misread the drive root
  // of an extended-length Win32 path as a directory that needs creation.
  std::vector<fs::path> missing;
  for (auto parent=directory; !parent.empty(); parent=parent.parent_path()) {
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
  fs::create_directories(directory);
#endif
}

inline void writeBytes(const fs::path& path, std::string_view bytes, ResourceTransfer* transfer = nullptr)
{
  if (transfer) transfer->check();
  const auto file=ftebroom::filesystemPath(path);
  if (transfer) transfer->record("file_directories");
  createArtifactDirectories(file.parent_path());
  if (transfer) transfer->record("file_open");
  std::ofstream output(file, std::ios::binary | std::ios::trunc);
  if (!output) throw fs::filesystem_error("Cannot open the Mapping Jam working file", file,
                                          std::error_code(errno,std::generic_category()));
  if (transfer) transfer->record("file_write");
  std::size_t finalChunk = 0;
  if (!transfer) {
    output.write(bytes.data(),bytes.size());
  } else {
    while (!bytes.empty()) {
      transfer->check();
      const auto count = std::min<std::size_t>(bytes.size(),256*1024);
      if (!output.write(bytes.data(),count))
        throw fs::filesystem_error("Cannot write the Mapping Jam working file",file,
                                   std::error_code(errno,std::generic_category()));
      bytes.remove_prefix(count);
      if (bytes.empty()) finalChunk = count;
      else transfer->wrote(count);
    }
  }
  if (transfer) transfer->record("file_close");
  output.close();
  if (!output) throw fs::filesystem_error("Cannot close the Mapping Jam working file",file,
                                          std::error_code(errno,std::generic_category()));
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

inline void writeNewDraft(const fs::path& path, const std::string& bytes)
{
  const auto physical=ftebroom::filesystemPath(path);
  createArtifactDirectories(physical.parent_path());
#ifdef _WIN32
  const auto handle=CreateFileW(physical.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,
                                FILE_ATTRIBUTE_NORMAL,nullptr);
  if (handle==INVALID_HANDLE_VALUE) {
    const auto error=GetLastError();
    if (error==ERROR_FILE_EXISTS || error==ERROR_ALREADY_EXISTS) return;
    throw fs::filesystem_error("Cannot create the Jam draft",path,std::error_code(error,std::system_category()));
  }
  DWORD written=0;
  const auto success=WriteFile(handle,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr);
  const auto error=success ? ERROR_WRITE_FAULT : GetLastError();
  const auto closed=CloseHandle(handle);
  if (success && closed && written==bytes.size()) return;
  DeleteFileW(physical.c_str());
  throw fs::filesystem_error("Cannot write the Jam draft",path,std::error_code(error,std::system_category()));
#else
  const auto fd=open(physical.c_str(),O_WRONLY|O_CREAT|O_EXCL,0666);
  if (fd<0) {
    if (errno==EEXIST) return;
    throw fs::filesystem_error("Cannot create the Jam draft",path,std::error_code(errno,std::generic_category()));
  }
  const auto count=write(fd,bytes.data(),bytes.size());
  const auto error=count<0 ? errno : EIO;
  const auto closed=close(fd);
  if (count==static_cast<ssize_t>(bytes.size()) && closed==0) return;
  std::error_code ignored;
  fs::remove(physical,ignored);
  throw fs::filesystem_error("Cannot write the Jam draft",path,std::error_code(error,std::generic_category()));
#endif
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

inline std::int64_t packFileStamp(const fs::path& root, const std::string& name, std::uint64_t size)
{
  const auto relative = fs::u8path(name).make_preferred();
  const auto file = root/relative;
#ifdef _WIN32
  WIN32_FILE_ATTRIBUTE_DATA data{};
  if (!GetFileAttributesExW(file.c_str(),GetFileExInfoStandard,&data))
    throw fs::filesystem_error("Cannot inspect the installed Jam resource",relative,
                               std::error_code(GetLastError(),std::system_category()));
  if (data.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))
    throw fs::filesystem_error("The installed Jam resource is not a regular file",relative,
                               std::error_code(ERROR_FILE_INVALID,std::system_category()));
  if ((std::uint64_t(data.nFileSizeHigh)<<32 | data.nFileSizeLow) != size)
    throw fs::filesystem_error("The installed Jam resource size does not match",relative,
                               std::error_code(ERROR_BAD_LENGTH,std::system_category()));
  return std::int64_t(data.ftLastWriteTime.dwHighDateTime)<<32 | data.ftLastWriteTime.dwLowDateTime;
#else
  if (!fs::is_regular_file(fs::symlink_status(file)))
    throw fs::filesystem_error("The installed Jam resource is missing or is not a regular file",relative,
                               std::make_error_code(std::errc::invalid_argument));
  if (fs::file_size(file)!=size)
    throw fs::filesystem_error("The installed Jam resource size does not match",relative,
                               std::make_error_code(std::errc::message_size));
  return fs::last_write_time(file).time_since_epoch().count();
#endif
}

inline Json packReceipt(const fs::path& root, const Json& manifest, ResourceTransfer* transfer=nullptr)
{
  const auto& files = manifest.at("files");
  if (!files.is_array() || files.empty() || files.size()>64) return nullptr;
  Json inventory = Json::object();
  if (transfer) transfer->member = 0;
  for (const auto& item : files) {
    if (transfer) { transfer->check(); ++transfer->member; }
    const auto name = item.at("path").get<std::string>();
    if (!packEntry(name) || inventory.contains(name)) return nullptr;
    inventory[name] = packFileStamp(root,name,item.at("sizeBytes").get<std::uint64_t>());
    if (transfer) transfer->wrote(0);
  }
  if (!inventory.contains("lq1/pak0.pak") || !inventory.contains("plants/builtin/catalog.json")) return nullptr;
  Json receipt = {{"manifest",manifest.at("sha256")},{"files",std::move(inventory)}};
#ifdef _WIN32
  // Older clients stored the C++ filesystem clock; check their files once before reusing the cache.
  receipt["fileTimeFormat"] = "win32";
#endif
  return receipt;
}

inline void publishPackReceipt(const fs::path& root, const Json& receipt, ResourceTransfer* transfer=nullptr)
{
  const auto marker=root/".verified-pack.json";
  const auto temporary=root/(".verified-"+randomHex(8)+".tmp");
  try {
    writeBytes(temporary,receipt.dump(),transfer);
    if (transfer) transfer->check();
#ifdef _WIN32
    if(!MoveFileExW(temporary.c_str(),marker.c_str(),MOVEFILE_REPLACE_EXISTING))
      throw fs::filesystem_error("Cannot publish the Jam pack receipt",marker.filename(),
                                 std::error_code(GetLastError(),std::system_category()));
#else
    fs::rename(temporary,marker);
#endif
  } catch (...) {
    // A failed new installation retires the whole directory. When updating a
    // legacy receipt, only the temporary marker belongs to this operation.
    if (!transfer) {
      std::error_code ignored;
      fs::remove(temporary,ignored);
    }
    throw;
  }
}

inline void refreshPackReceipt(const fs::path& root, const Json& receipt, ResourceTransfer* transfer)
{
  try {
    publishPackReceipt(root,receipt);
  } catch (const std::exception& error) {
    const auto* fsError=dynamic_cast<const fs::filesystem_error*>(&error);
    if (transfer) transfer->record("receipt_error",fsError ? fsError->code().value() : 0);
  }
}

inline bool installedPack(const fs::path& destination, const Json& manifest, ResourceTransfer* transfer = nullptr)
{
  try {
    const auto root = ftebroom::filesystemPath(destination);
    if (transfer) transfer->record("cache_inventory");
    const auto receipt = packReceipt(root,manifest,transfer);
    if (receipt.is_null()) return false;
    const auto marker = root/".verified-pack.json";
    if (fs::is_regular_file(marker) && !fs::is_symlink(marker) && fs::file_size(marker)<16384 &&
        Json::parse(fileBytes(marker,16384),nullptr,false)==receipt) return true;
    if (transfer) transfer->record("cache_hash");
    for (const auto& item : manifest.at("files")) {
      if (transfer) transfer->check();
      const auto file = root/fs::u8path(item.at("path").get<std::string>()).make_preferred();
      if (sha256(fileBytes(file,64*1024*1024))!=item.at("sha256").get<std::string>()) return false;
      if (transfer) transfer->wrote(0);
    }
    if (transfer) transfer->check();
    refreshPackReceipt(root,receipt,transfer);
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
  if (transfer) transfer->record("manifest_validate");
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
  if (base && !privatePack) {
    if (transfer) transfer->localPhase(ResourcePhase::CheckingCache);
    if (installedPack(destination, manifest,transfer)) {
      if (transfer) transfer->record("cache_hit");
      return;
    }
    if (transfer) transfer->record("cache_miss");
  }
  if (!base && reuse) {
    const bool present = std::all_of(members.begin(),members.end(),[&](const auto& item) {
      try {
        packFileStamp(target,item.first,item.second.at("sizeBytes").template get<std::uint64_t>());
        return sha256(fileBytes(target/fs::u8path(item.first).make_preferred(),maxSize))==
          item.second.at("sha256").template get<std::string>();
      } catch (const std::exception&) { return false; }
    });
    const bool staleLit = !members.count(mapPath+".lit") &&
      ftebroom::pak::regularFile(target/fs::u8path(mapPath+".lit").make_preferred());
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
      transfer->record("download_request",attempt+1);
    }
    try {
      response = request(client.endpoint,"GET",path,client.token,{},{},length+4096,transfer);
      if (transfer) transfer->record("download_http",response.status);
      if (response.status==200 && response.body.size()<length)
        throw DownloadInterrupted("The Mapping Jam download was incomplete.");
      if (response.status!=408 && response.status!=429 && response.status!=500 &&
          response.status!=502 && response.status!=503 && response.status!=504) break;
      if (response.status==429 && transfer)
        throw Error("The Mapping Jam server asked the client to wait.",true,"RATE_LIMITED");
      if (attempt==2) throw Error("The Jam download service is temporarily unavailable. Try again.",true);
    } catch (const DownloadInterrupted&) {
      if (transfer) transfer->record("download_interrupted");
      if (attempt==2) throw;
    }
    if (transfer) {transfer->phase = ResourcePhase::Retrying;transfer->record("download_retry",attempt+1);}
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
    transfer->member = 0;
    transfer->total = 0;
    transfer->bytes = 0;
    transfer->localPhase(ResourcePhase::Verifying);
    transfer->record("archive_hash");
  }
  if (response.body.size() != length ||
      sha256(response.body) != manifest.at("sha256").get<std::string>())
    throw std::runtime_error("The Jam download did not match its manifest.");

  auto staging = target;
  if (transfer) transfer->record("staging_create");
  if (privatePack) {
#ifdef _WIN32
    if (!CreateDirectoryW(staging.c_str(),nullptr))
      throw fs::filesystem_error("Cannot create the Mapping Jam installation directory",staging,
                                 std::error_code(GetLastError(),std::system_category()));
#else
    if (!fs::create_directory(staging))
      throw std::runtime_error("The Mapping Jam installation directory is already in use.");
#endif
  } else staging += ".incoming-" + randomHex(8);
  try {
    const std::string_view archive = response.body;
    ResourceArchive buffer(archive);
    std::istream input(&buffer);
    if (transfer) transfer->record("zip_directory");
    const auto entries = ftebroom::pak::zipDirectory(input,archive.size());
    if (entries.size() != members.size()) throw std::runtime_error("Unexpected files in Jam download.");
    if (transfer) {
      transfer->check();
      transfer->total = total;
      transfer->localPhase(ResourcePhase::Installing);
      transfer->record("install_start",static_cast<int>(entries.size()));
    }
    for (const auto& entry : entries) {
      if (transfer) {transfer->check();++transfer->member;transfer->record("member_inflate");}
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
      if (transfer) transfer->record("member_hash");
      if (sha256(bytes) != i->second.at("sha256").get<std::string>())
        throw std::runtime_error("An extracted Jam file did not match its manifest.");
      writeBytes(staging / fs::u8path(entry.name).make_preferred(),bytes,transfer);
      if (transfer) transfer->record("member_done");
    }
    if (transfer) {
      transfer->check();
      transfer->localPhase(ResourcePhase::Finalizing);
      transfer->member = 0;
      transfer->record("install_finalize");
    }
    if (base) {
      const auto receipt = packReceipt(staging,manifest,transfer);
      if (receipt.is_null())
        throw fs::filesystem_error("The installed Jam resource inventory is invalid",
                                   std::make_error_code(std::errc::invalid_argument));
      if (transfer) { transfer->member = 0; transfer->record("receipt_write"); }
      if (privatePack) {
        // Publishing the receipt commits this directory. A concurrent editor
        // may mount it immediately, so never delete it after this point.
        publishPackReceipt(staging,receipt,transfer);
        return;
      }
      writeBytes(staging/".verified-pack.json",receipt.dump(),transfer);
    }
    if (transfer) transfer->check();
    if (transfer) transfer->record("activate_directory");
#ifdef _WIN32
    if (GetFileAttributesW(target.c_str())!=INVALID_FILE_ATTRIBUTES) {
#else
    if (fs::exists(target)) {
#endif
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
  } catch (const std::exception& error) {
    const auto* fsError=dynamic_cast<const fs::filesystem_error*>(&error);
    if (transfer) transfer->record("operation_failed",fsError ? fsError->code().value() : 0);
    if (transfer) transfer->record("cleanup_failed_install");
    if (retired) {
      // Return the original error even when removing the unpublished files would block.
      *retired = std::move(staging);
    } else {
      std::error_code ignored;
      fs::remove_all(staging, ignored);
      if (transfer && ignored) transfer->record("cleanup_error",ignored.value());
    }
    throw;
  }
}

inline bool hexId(const std::string& value, std::size_t size)
{
  return value.size()==size && value.find_first_not_of("0123456789abcdef")==value.npos;
}

inline std::string basePackPrefix(const std::string& version, const Json& manifest)
{
  if (manifest.at("packVersion")!=version) throw std::runtime_error("The base pack version changed.");
  const auto digest = manifest.at("sha256").get<std::string>();
  if (!hexId(digest,64)) throw std::runtime_error("Invalid Mapping Jam pack checksum.");
  return version+"-"+digest.substr(0,16)+"-";
}

#ifdef _WIN32
// MinGW's directory iterator can lose entries under an extended-length path.
template<class Visitor>
inline void windowsDirectoryEntries(const fs::path& directory, const std::string& pattern, Visitor visit)
{
  const auto glob = ftebroom::filesystemPath(directory)/fs::u8path(pattern);
  WIN32_FIND_DATAW data{};
  const auto search = FindFirstFileW(glob.c_str(),&data);
  if (search==INVALID_HANDLE_VALUE) return;
  const auto close = [](HANDLE handle) { FindClose(handle); };
  const std::unique_ptr<void,decltype(close)> handle(search,close);
  do {
    if (!visit(fs::path(data.cFileName),data.dwFileAttributes)) return;
  } while (FindNextFileW(search,&data));
}
#endif

inline fs::path findBasePack(const fs::path& root, const fs::path& legacy, const std::string& prefix,
                             const Json& manifest, ResourceTransfer* transfer=nullptr)
{
  const auto candidate = [&](const fs::path& leaf) -> fs::path {
    if (transfer) transfer->check();
    const auto name = leaf.u8string();
    if (name.size()!=prefix.size()+16 || name.compare(0,prefix.size(),prefix)!=0 ||
        !hexId(name.substr(prefix.size()),16)) return {};
    const auto pack = root/leaf;
    const auto marker = ftebroom::filesystemPath(pack)/".verified-pack.json";
#ifdef _WIN32
    const auto attributes = GetFileAttributesW(marker.c_str());
    if (attributes==INVALID_FILE_ATTRIBUTES ||
        (attributes&(FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) return {};
#else
    std::error_code error;
    if (!fs::is_regular_file(marker,error)) return {};
#endif
    return installedPack(pack,manifest,transfer) ? pack : fs::path{};
  };
#ifdef _WIN32
  fs::path pack;
  windowsDirectoryEntries(root,prefix+"*",[&](const fs::path& leaf,DWORD attributes) {
    if ((attributes&(FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))==FILE_ATTRIBUTE_DIRECTORY)
      pack = candidate(leaf);
    return pack.empty();
  });
  if (!pack.empty()) return pack;
#else
  std::error_code iterationError;
  for (fs::directory_iterator it(root,iterationError), end; !iterationError && it!=end; it.increment(iterationError)) {
    if (transfer) transfer->check();
    std::error_code entryError;
    if (it->is_symlink(entryError) || entryError || !it->is_directory(entryError)) continue;
    const auto pack = candidate(it->path().filename());
    if (!pack.empty()) return pack;
  }
#endif
  if (!legacy.empty() && installedPack(legacy,manifest,transfer)) return legacy;
  return {};
}

inline fs::path prepareBasePack(const Client& client, const Json& manifest, const fs::path& root,
                                const fs::path& legacy, const std::string& version, bool install, bool fresh,
                                ResourceTransfer& transfer, fs::path* failedPack=nullptr)
{
  const auto prefix = basePackPrefix(version,manifest);
  transfer.localPhase(ResourcePhase::CheckingCache);
  transfer.record("cache_scan");
  if (!fresh) {
    const auto cached = findBasePack(root,legacy,prefix,manifest,&transfer);
    if (!cached.empty()) { transfer.record("cache_hit"); return cached; }
  }
  transfer.record("cache_miss");
  if (!install) return {};
  transfer.record("cache_create");
  createArtifactDirectories(ftebroom::filesystemPath(root));
  const auto destination = root/(prefix+randomHex(8));
  download(client,manifest,destination,true,"jam_map",false,&transfer,failedPack,true);
  return destination;
}

inline Json sendMap(const Client& client, const std::string& plot, const Json& rules,
                    const Json& baseRevision, const std::string& title, const fs::path& snapshot)
{
  const auto limit=rules.value("maxSourceBytes",2*1024*1024);
  if (limit<=0 || limit>2*1024*1024) throw std::runtime_error("Invalid Mapping Jam upload limit.");
  const auto bytes = fileBytes(snapshot,limit);
  if (bytes.empty()) throw std::runtime_error("The exported map is empty.");
  const auto manifest=Json{{"schemaVersion",1},{"sha256",sha256(bytes)}}.dump();
  std::ostringstream archive;
  ftebroom::pak::zipStore(archive,{{"manifest.json",manifest},{"map/source.map",bytes}});
  const auto payload=archive.str();
  if (payload.size()>static_cast<std::size_t>(limit))
    throw std::runtime_error("The Mapping Jam upload archive exceeds the allowed size.");
  const auto upload = client.json("POST", client.prefix + "plots/" + plot + "/uploads",
    {{"title", title}, {"baseRevisionId", baseRevision}, {"rulesVersion", rules.at("rulesVersion")},
     {"packVersion", rules.at("packVersion")}, {"termsVersion", rules.at("termsVersion")},
     {"sourceSha256", sha256(payload)}, {"sourceSizeBytes", payload.size()}}, randomHex(16));
  client.json("PUT", upload.at("uploadPath"), nullptr, {}, payload);
  return client.json("POST", client.prefix + "uploads/" + upload.at("uploadId").get<std::string>() + "/finalize",
                     Json::object(), randomHex(16));
}
} // namespace ftebroom::jam
