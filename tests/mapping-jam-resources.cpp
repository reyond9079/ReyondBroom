// SPDX-License-Identifier: GPL-2.0-or-later
#include "third_party/json.hpp"
#include "ftebroom_pak.h"
#include "ftebroom_jam_client.h"
#include <iostream>
#ifdef __linux__
#include <sys/resource.h>
#endif

int main(int argc,char** argv)
{
  if (argc!=6) return 2;
  namespace jam = ftebroom::jam;
  try {
    const auto manifest = jam::Json::parse(jam::fileBytes(jam::fs::u8path(argv[2]),64*1024));
    const auto target = jam::fs::u8path(argv[3]);
    const jam::Client client{{static_cast<unsigned>(std::stoul(argv[1])),{}},{},"/v1/"};
    const bool base = std::string_view(argv[4])=="base";
    jam::ResourceTransfer transfer;
    const std::string_view mode = argv[5];
    if (mode=="draft-file") {
      const auto name=jam::fs::u8path(u8"Tracé тест.map");
      const auto draft=target/name;
      jam::writeNewDraft(draft,"original draft");
      jam::writeNewDraft(draft,"must not replace the original");
      if (!ftebroom::pak::exists(draft) || ftebroom::pak::read(draft)!="original draft" ||
          jam::fileBytes(draft,4096)!="original draft")
        throw std::runtime_error("The draft could not be read or was overwritten.");
#ifdef _WIN32
      bool listed=false;
      jam::windowsDirectoryEntries(target,"*.map",[&](const auto& leaf,DWORD attributes) {
        if (leaf==name && !(attributes&FILE_ATTRIBUTE_DIRECTORY)) listed=true;
        return true;
      });
      if (!listed) throw std::runtime_error("The saved draft was missing from the Windows directory listing.");
#endif
      return 0;
    }
    if (mode=="watchdog") {
      transfer.localPhase(jam::ResourcePhase::Finalizing);
      const auto starting = transfer.lastLocalProgress.load();
      if (transfer.localStalled(starting + std::chrono::seconds(44)) ||
          !transfer.localStalled(starting + jam::ResourceTransfer::localIdleLimit))
        throw std::runtime_error("The local install timeout has the wrong threshold.");
      transfer.lastLocalProgress = starting - std::chrono::minutes(1);
      transfer.wrote(1);
      if (transfer.localStalled() || transfer.bytes!=1)
        throw std::runtime_error("File writes must renew the local install timeout.");
      transfer.timedOut = true;
      transfer.check();
      throw std::runtime_error("The worker ignored an installation timed out by the UI.");
    }
    if (mode=="cancel") transfer.canceled = true;
    jam::fs::path retired;
    if (mode=="versioned" || mode=="versioned-probe") {
      const auto version = manifest.at("packVersion").get<std::string>();
      const auto pack = jam::prepareBasePack(client,manifest,target.parent_path(),target,version,
                                            mode=="versioned",false,transfer,&retired);
      for (const auto& event : transfer.takeEvents()) {
        if (event.step=="activate_directory") throw std::runtime_error("A versioned pack moved a directory.");
        if (mode=="versioned-probe" && event.step=="cache_hash")
          throw std::runtime_error("A fresh cache receipt should avoid hashing the entire pack.");
      }
      if (pack.empty()) std::cout << "cache-miss\n";
      else {
        if (!jam::installedPack(pack,manifest)) throw std::runtime_error("The pack could not be reopened.");
        std::cout << "pack=" << pack.filename().u8string() << '\n';
      }
      return 0;
    }
#ifdef _WIN32
    if (mode=="inventory-failure") {
      const auto root = ftebroom::filesystemPath(target);
      const auto firstName = manifest.at("files").front().at("path").get<std::string>();
      const auto file = root/jam::fs::u8path(firstName).make_preferred();
      jam::writeBytes(file,"short copy");
      transfer.localPhase(jam::ResourcePhase::Finalizing);
      try { jam::packReceipt(root,manifest,&transfer); }
      catch (const jam::fs::filesystem_error& error) {
        if (transfer.member==1 && error.code().value()==ERROR_BAD_LENGTH &&
            error.path1().generic_u8string()==firstName) return 0;
        throw;
      }
      throw std::runtime_error("A mismatched installed file was not reported.");
    }
    if (mode=="blocked-activation") {
      const auto root = ftebroom::filesystemPath(target);
      const auto marker = root/"old-pack-marker";
      const auto handle = CreateFileW(marker.c_str(),GENERIC_READ,FILE_SHARE_READ,
                                      nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
      if (handle==INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot lock a file in the old pack directory.");
      try { jam::download(client,manifest,target,base,"jam_map",true,&transfer,&retired); }
      catch (const jam::fs::filesystem_error& error) {
        CloseHandle(handle);
        bool activationFailed=false;
        for (const auto& event : transfer.takeEvents()) {
          if (event.step=="operation_failed" && event.detail==error.code().value()) activationFailed=true;
        }
        if (error.code().value()!=0 && activationFailed &&
            jam::fs::is_regular_file(root/"old-pack-marker") &&
            jam::fs::is_regular_file(retired/".verified-pack.json")) return 0;
        throw;
      }
      CloseHandle(handle);
      throw std::runtime_error("A locked old pack was unexpectedly replaced.");
    }
#endif
    jam::download(client,manifest,target,base,"jam_map",true,&transfer,mode=="replace" ? &retired : nullptr);
    if (base && !jam::installedPack(target,manifest))
      throw std::runtime_error("The installed resource pack could not be reopened.");
    if (mode=="replace" &&
        (transfer.phase!=jam::ResourcePhase::Finalizing || !jam::fs::is_regular_file(retired/"old-pack-marker")))
      throw std::runtime_error("Installing a replacement must leave old pack cleanup to the caller.");
#ifdef __linux__
    rusage usage{};
    if (getrusage(RUSAGE_SELF,&usage)==0) std::cout << "peakKiB=" << usage.ru_maxrss << '\n';
#endif
    return 0;
  } catch (const jam::DownloadCanceled&) {
    std::cerr << "Canceled\n";
    return 3;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
