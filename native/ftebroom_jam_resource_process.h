// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#ifndef _WIN32
#include <cerrno>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>

extern char** environ;

namespace ftebroom::jam {
class ResourceProcess {
  int channel = -1;
  pid_t child = 0;
  std::vector<pid_t> stopping;
  std::string pending;
  ResourceTransfer::Clock::time_point lastReply{};

public:
  void reap()
  {
    stopping.erase(std::remove_if(stopping.begin(),stopping.end(),[](pid_t pid) {
      const auto result = waitpid(pid,nullptr,WNOHANG);
      return result == pid || (result == -1 && errno == ECHILD);
    }),stopping.end());
  }

  bool active() const { return child != 0; }

  void stop()
  {
    if (channel >= 0) close(std::exchange(channel,-1));
    if (child) {
      const auto pid = std::exchange(child,0);
      pid_t exited;
      do { exited = waitpid(pid,nullptr,WNOHANG); } while (exited<0 && errno==EINTR);
      if (exited==0) {
        kill(pid,SIGKILL);
        stopping.push_back(pid);
      }
    }
    pending.clear();
    reap();
  }

  void start(const fs::path& program, const Json& request)
  {
    reap();
    if (active()) throw std::logic_error("A Mapping Jam resource worker is already running.");
    if (stopping.size() >= 3)
      throw std::runtime_error("The operating system is still stopping previous resource jobs. Check the storage and try again.");
    const auto line = request.dump() + "\n";
    if (line.size()>16384) throw std::runtime_error("The Mapping Jam resource request is too large.");
    int sockets[2];
    if (socketpair(AF_UNIX,SOCK_STREAM | SOCK_CLOEXEC,0,sockets) != 0)
      throw std::system_error(errno,std::generic_category(),"Cannot create the resource worker channel");
    channel = sockets[0];
    const int peer = sockets[1];
    posix_spawn_file_actions_t actions;
    auto error = posix_spawn_file_actions_init(&actions);
    if (!error) {
      error = posix_spawn_file_actions_adddup2(&actions,peer,STDIN_FILENO);
      if (!error) {
        char* const arguments[] = {const_cast<char*>(program.c_str()),nullptr};
        error = posix_spawn(&child,program.c_str(),&actions,nullptr,arguments,environ);
      }
      posix_spawn_file_actions_destroy(&actions);
    }
    close(peer);
    if (error) {
      child = 0;
      stop();
      throw std::system_error(error,std::generic_category(),"Cannot start the Mapping Jam resource worker");
    }
    if (send(channel,line.data(),line.size(),MSG_NOSIGNAL | MSG_DONTWAIT)!=static_cast<ssize_t>(line.size())) {
      stop();
      throw std::runtime_error("Cannot contact the Mapping Jam resource worker.");
    }
    lastReply = ResourceTransfer::Clock::now();
  }

  std::optional<Json> poll(ResourceTransfer& transfer,
                           ResourceTransfer::Clock::time_point now = ResourceTransfer::Clock::now())
  {
    reap();
    if (!active()) return {};
    if (transfer.canceled) {
      stop();
      throw DownloadCanceled();
    }
    char chunk[4096];
    for (int i=0; i<16; ++i) {
      const auto received = recv(channel,chunk,sizeof chunk,MSG_DONTWAIT);
      if (received<0 && (errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR)) break;
      if (received<=0) throw std::runtime_error("The Mapping Jam resource worker stopped unexpectedly.");
      pending.append(chunk,static_cast<std::size_t>(received));
      if (pending.size()>16384) throw std::runtime_error("Invalid Mapping Jam resource worker response.");
      for (auto eol=pending.find('\n'); eol!=pending.npos; eol=pending.find('\n')) {
        auto packet = Json::parse(pending.substr(0,eol));
        pending.erase(0,eol+1);
        lastReply = now;
        if (packet.value("done",false)) {
          stop();
          return packet;
        }
        const auto phase = packet.at("phase").get<int>();
        if (phase<static_cast<int>(ResourcePhase::Checking) || phase>static_cast<int>(ResourcePhase::Finalizing))
          throw std::runtime_error("Invalid Mapping Jam resource worker phase.");
        transfer.phase = static_cast<ResourcePhase>(phase);
        transfer.total = packet.at("total").get<std::size_t>();
        transfer.bytes = packet.at("bytes").get<std::size_t>();
        if (packet.value("stalled",false)) return Json{{"timeout",true},{"local",true}};
      }
    }
    if (now-lastReply>=ResourceTransfer::localIdleLimit)
      return Json{{"timeout",true},{"local",transfer.local()}};
    return {};
  }
};
} // namespace ftebroom::jam
#endif
