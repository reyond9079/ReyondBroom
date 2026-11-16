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
