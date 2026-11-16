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
    if (std::string_view(argv[5])=="cancel") transfer.canceled = true;
    jam::download(client,manifest,target,base,"jam_map",true,&transfer);
    if (base && !jam::installedPack(target,manifest))
      throw std::runtime_error("The installed resource pack could not be reopened.");
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
