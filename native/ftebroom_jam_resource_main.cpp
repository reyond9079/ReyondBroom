// SPDX-License-Identifier: GPL-2.0-or-later
#include "third_party/json.hpp"
#include "ftebroom_pak.h"
#include "ftebroom_jam_client.h"
#include <iostream>
#include <signal.h>
#include <sys/prctl.h>

namespace jam = ftebroom::jam;
using jam::Json;
namespace fs = jam::fs;

static void reply(const Json& message)
{
  const auto line = message.dump(-1,' ',false,Json::error_handler_t::replace) + "\n";
  // The channel carries small messages. Never let UI backpressure stop I/O.
  if (send(STDIN_FILENO,line.data(),line.size(),MSG_DONTWAIT | MSG_NOSIGNAL) !=
      static_cast<ssize_t>(line.size())) _exit(1);
}

static fs::path prepare(const Json& request, jam::ResourceTransfer& transfer)
{
  const std::string version = request.at("version");
  if (version != "librequake-0.09-plants-local1")
    throw std::runtime_error("Unsupported Mapping Jam pack version.");
  const jam::Client client{{request.at("port"),request.at("hostname")},request.at("token"),request.at("prefix")};
  transfer.record("manifest_request");
  const auto manifest = client.json("GET",client.prefix+"packs/"+version+"/manifest",nullptr,{},{},&transfer);
  transfer.record("manifest_received");
  const auto root = fs::u8path(request.at("root").get<std::string>());
  const auto legacy = fs::u8path(request.at("legacy").get<std::string>());
  return jam::prepareBasePack(client,manifest,root,legacy,version,request.at("install").get<bool>(),
                             request.at("fresh").get<bool>(),transfer);
}

int main()
{
  // This utility is spawned only by the editor and has no arguments with credentials.
  const auto parent = getppid();
  if (parent == 1 || prctl(PR_SET_PDEATHSIG,SIGKILL) != 0 || getppid() != parent) return 1;
  std::string line;
  if (!std::getline(std::cin,line) || line.size()>16384) return 1;
  jam::ResourceTransfer transfer;
  std::atomic<bool> done{false};
  std::thread progress([&] {
    for (;;) {
      const bool finished=done.load();
      Json events=Json::array();
      for (const auto& event : transfer.takeEvents()) {
        events.push_back({{"step",event.step},{"detail",event.detail},{"phase",static_cast<int>(event.phase)},
                          {"bytes",event.bytes},{"total",event.total},{"member",event.member},{"elapsed",event.elapsed}});
        if (events.size()==16) {reply({{"trace",std::move(events)}});events=Json::array();}
      }
      if (!events.empty()) reply({{"trace",std::move(events)}});
      if (finished) break;
      reply({{"phase",static_cast<int>(transfer.phase.load())},{"bytes",transfer.bytes.load()},
             {"total",transfer.total.load()},{"member",transfer.member.load()},{"stalled",transfer.localStalled()}});
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
  });
  Json result{{"done",true}};
  try {
    result["pack"] = prepare(Json::parse(line),transfer).u8string();
  } catch (const jam::DownloadInterrupted&) {
    result["timeout"] = true;
    result["local"] = transfer.local();
  } catch (const fs::filesystem_error& error) {
    result["error"] = std::string(error.what()).substr(0,1400);
    result["detail"] = error.code().value();
  } catch (const std::exception& error) {
    result["error"] = std::string(error.what()).substr(0,1400);
  }
  done = true;
  progress.join();
  reply(result);
}
