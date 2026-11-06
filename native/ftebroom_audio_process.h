// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "ftebroom_audio_project.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <regex>
#include <sstream>
#include <thread>
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
namespace ftebroom::audio {
namespace fs = std::filesystem;
struct JobControl {
  std::atomic<bool> cancel{false};
  std::atomic<float> progress{0};
};
inline void cancelled(const std::shared_ptr<JobControl> &job) {
  if (job && job->cancel)
    throw std::runtime_error("Audio operation cancelled");
}
inline std::string number(double n) {
  if (!std::isfinite(n))
    throw std::runtime_error("Non-finite audio parameter");
  std::ostringstream s;
  s.imbue(std::locale::classic());
  s << std::setprecision(12) << n;
  return s.str();
}
inline std::string readBytes(const fs::path &path,
                             size_t limit = 512u * 1024 * 1024) {
  if (!fs::is_regular_file(path) || fs::file_size(path) > limit)
    throw std::runtime_error("File missing or exceeds audio size limit");
  std::ifstream f(path, std::ios::binary);
  std::string b((std::istreambuf_iterator<char>(f)), {});
  if (f.bad())
    throw std::runtime_error("Cannot read audio file");
  return b;
}
inline void writeBytes(const fs::path &path, const std::string &bytes) {
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f)
    throw std::runtime_error("Cannot create audio output");
  f.write(bytes.data(), std::streamsize(bytes.size()));
  f.close();
  if (!f)
    throw std::runtime_error(
        "Cannot write audio output (disk full or permission denied)");
}
struct Workspace {
  fs::path path;
  Workspace() {
    for (int n = 0; n < 10; n++) {
      path = fs::temp_directory_path() /
             ("reyond-audio-" +
              std::to_string(std::chrono::high_resolution_clock::now()
                                 .time_since_epoch()
                                 .count()) +
              "-" + std::to_string(n));
      if (fs::create_directory(path)) {
        fs::permissions(path, fs::perms::owner_all, fs::perm_options::replace);
        return;
      }
    }
    throw std::runtime_error("Cannot create private audio workspace");
  }
  ~Workspace() {
    std::error_code ec;
    fs::remove_all(path, ec);
  }
};
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
inline std::string process(const fs::path &exe,
                           const std::vector<std::string> &args,
                           const fs::path &log,
                           const std::shared_ptr<JobControl> &control = {}) {
  cancelled(control);
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(120);
  int code = 1;
  bool timeout = false;
#ifdef _WIN32
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
    throw std::runtime_error("Cannot create audio process streams");
  }
  std::wstring line = quote(exe.wstring());
  for (auto &a : args)
    line += L" " + quote(fs::u8path(a).wstring());
  STARTUPINFOW si{};
  si.cb = sizeof si;
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdInput = null;
  si.hStdOutput = null;
  si.hStdError = err;
  PROCESS_INFORMATION pi{};
  BOOL ok = CreateProcessW(exe.c_str(), line.data(), nullptr, nullptr, TRUE,
                           CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
  CloseHandle(err);
  CloseHandle(null);
  if (!ok)
    throw std::runtime_error(
        "Cannot start FFmpeg. Check the bundled audio converter.");
  CloseHandle(pi.hThread);
  for (;;) {
    auto status = WaitForSingleObject(pi.hProcess, 20);
    if (status == WAIT_OBJECT_0) {
      DWORD result = 1;
      GetExitCodeProcess(pi.hProcess, &result);
      code = int(result);
      break;
    }
    if (status == WAIT_FAILED || (control && control->cancel) ||
        std::chrono::steady_clock::now() > deadline) {
      timeout = !(control && control->cancel);
      TerminateProcess(pi.hProcess, 1);
      WaitForSingleObject(pi.hProcess, 5000);
      break;
    }
  }
  CloseHandle(pi.hProcess);
#else
  int err = open(log.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600),
      null = open("/dev/null", O_RDWR);
  if (err < 0 || null < 0) {
    if (err >= 0)
      close(err);
    if (null >= 0)
      close(null);
    throw std::runtime_error("Cannot create audio process streams");
  }
  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_adddup2(&actions, null, STDIN_FILENO);
  posix_spawn_file_actions_adddup2(&actions, null, STDOUT_FILENO);
  posix_spawn_file_actions_adddup2(&actions, err, STDERR_FILENO);
  posix_spawn_file_actions_addclose(&actions, err);
  posix_spawn_file_actions_addclose(&actions, null);
  std::vector<std::string> values{exe.string()};
  values.insert(values.end(), args.begin(), args.end());
  std::vector<char *> argv;
  for (auto &v : values)
    argv.push_back(v.data());
  argv.push_back(nullptr);
  pid_t pid = -1;
  int error =
      posix_spawn(&pid, exe.c_str(), &actions, nullptr, argv.data(), environ);
  posix_spawn_file_actions_destroy(&actions);
  close(err);
  close(null);
  if (error)
    throw std::runtime_error(
        "Cannot start FFmpeg. Install the system audio converter.");
  for (;;) {
    int status = 0;
    auto result = waitpid(pid, &status, WNOHANG);
    if (result == pid) {
      code = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
      break;
    }
    if (result < 0 && errno != EINTR)
      throw std::runtime_error("Cannot read audio converter exit status");
    if ((control && control->cancel) ||
        std::chrono::steady_clock::now() > deadline) {
      timeout = !(control && control->cancel);
      kill(pid, SIGKILL);
      while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
      }
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
#endif
  cancelled(control);
  auto messages =
      fs::exists(log) ? readBytes(log, 4u * 1024 * 1024) : std::string{};
  if (timeout)
    throw std::runtime_error("Audio conversion exceeded two minutes");
  if (code) {
    if (messages.size() > 1800)
      messages = messages.substr(messages.size() - 1800);
    throw std::runtime_error("FFmpeg failed: " + messages);
  }
  return messages;
}
inline std::vector<std::string> ffargs() {
  return {"-nostdin", "-hide_banner", "-nostats", "-loglevel", "error", "-y"};
}
inline std::shared_ptr<PCM> readPCM(const fs::path &p) {
  auto bytes = readBytes(p, maxDecodedBytes);
  if (bytes.empty() || bytes.size() % 8 || bytes.size() >= maxDecodedBytes)
    throw std::runtime_error(
        "Decoded audio is empty, invalid or exceeds 256 MiB");
  auto pcm = std::make_shared<PCM>(bytes.size() / 4);
  std::memcpy(pcm->data(), bytes.data(), bytes.size());
  for (auto &v : *pcm)
    if (!std::isfinite(v))
      v = 0;
  return pcm;
}
inline void writePCM(const fs::path &p, const PCM &pcm) {
  std::ofstream f(p, std::ios::binary);
  f.write((const char *)pcm.data(), std::streamsize(pcm.size() * 4));
  f.close();
  if (!f)
    throw std::runtime_error("Cannot write intermediate audio (disk full)");
}
inline Source decode(const fs::path &exe, std::string uri, std::string bytes,
                     int rate, const std::shared_ptr<JobControl> &job = {}) {
  Source s;
  s.uri = std::move(uri);
  s.name = fs::u8path(s.uri).filename().u8string();
  s.fingerprint = fingerprint(bytes);
  s.original = std::make_shared<const std::string>(std::move(bytes));
  s.rate = rate;
  s.channels = 2;
  auto &b = *s.original;
  if (b.size() >= 12 && b.substr(0, 4) == "RIFF" && b.substr(8, 4) == "WAVE") {
    auto word = [&](size_t n) {
      uint32_t x = 0;
      for (int i = 0; i < 4; i++)
        x |= uint32_t((unsigned char)b[n + i]) << (8 * i);
      return x;
    };
    for (size_t at = 12; at + 8 <= b.size();) {
      size_t n = word(at + 4);
      if (n > b.size() - at - 8)
        break;
      if (b.compare(at, 4, "fmt ") == 0 && n >= 16) {
        s.rate = int(word(at + 12));
        s.channels =
            int((unsigned char)b[at + 10] | ((unsigned char)b[at + 11] << 8));
        if (s.channels < 1 || s.channels > 2 || s.rate < 8000 ||
            s.rate > 192000)
          throw std::runtime_error(
              "Only mono/stereo WAV between 8 and 192 kHz is supported");
        break;
      }
      at += 8 + n + (n & 1);
    }
  }
  Workspace w;
  auto ext = fs::u8path(s.uri).extension().u8string();
  auto input = w.path / ("input" + ext), output = w.path / "decoded.pcm";
  writeBytes(input, *s.original);
  auto args = ffargs();
  args[4] = "info";
  args.insert(args.end(),
              {"-i", input.u8string(), "-vn", "-f", "f32le", "-c:a",
               "pcm_f32le", "-ar", std::to_string(rate), "-ac", "2", "-fs",
               std::to_string(maxDecodedBytes), output.u8string()});
  auto log = process(exe, args, w.path / "decode.log", job);
  std::smatch match;
  if (std::regex_search(
          log, match,
          std::regex("Audio: [^\\n]*?([0-9]+) Hz, (mono|stereo)"))) {
    s.rate = std::stoi(match[1]);
    s.channels = match[2] == "mono" ? 1 : 2;
  }

  s.pcm = readPCM(output);
  s.buildPeaks();
  return s;
}
inline bool needsProcessing(const Clip &c) {
  return c.speed != 1 || (!c.fx.bypass && (c.fx.normalize || c.fx.denoise ||
                                           c.fx.enhance || c.fx.voice != 0));
}
inline std::string processingKey(const Clip &c, const Source &s, int rate) {
  return s.fingerprint + ":" + std::to_string(c.in) + ":" +
         std::to_string(c.out) + ":" + number(c.speed) + ":" +
         std::to_string(c.pitchLinked) + ":" + std::to_string(c.fx.normalize) +
         std::to_string(c.fx.denoise) + std::to_string(c.fx.enhance) +
         std::to_string(c.fx.bypass) + std::to_string(c.fx.voice) + ":" +
         number(c.fx.amount) + ":" + std::to_string(rate);
}
inline Prepared prepareClip(const fs::path &exe, const Clip &c,
                            const Source &source, int rate,
                            const std::shared_ptr<JobControl> &job = {}) {
  if (!source.pcm)
    throw std::runtime_error("Missing audio source: " + source.name);
  if (c.in < 0 || c.out > source.length())
    throw std::runtime_error("Clip exceeds the current source duration: " +
                             source.name);
  if (!needsProcessing(c))
    return {c.id, source.pcm, c.in};
  Workspace w;
  auto input = w.path / "input.pcm", output = w.path / "processed.pcm";
  PCM slice(source.pcm->begin() + c.in * 2, source.pcm->begin() + c.out * 2);
  writePCM(input, slice);
  slice.clear();
  slice.shrink_to_fit();
  std::vector<std::string> filters;
  std::string measuredLUFS, measuredTruePeak;
  if (c.speed != 1) {
    if (c.pitchLinked)
      filters.push_back("asetrate=" + number(rate * c.speed) +
                        ",aresample=" + std::to_string(rate));
    else {
      double tempo = c.speed;
      while (tempo < .5) {
        filters.push_back("atempo=0.5");
        tempo *= 2;
      }
      while (tempo > 2) {
        filters.push_back("atempo=2");
        tempo /= 2;
      }
      filters.push_back("atempo=" + number(tempo));
    }
  }
  if (!c.fx.bypass) {
    if (c.fx.denoise)
      filters.push_back("afftdn=nr=" + number(5 + 25 * c.fx.amount) +
                        ":nf=-35");
    if (c.fx.enhance)
      filters.push_back("highpass=f=80,lowpass=f=12000,acompressor=threshold=0."
                        "125:ratio=3:attack=15:release=100:makeup=1.3");
    switch (c.fx.voice) {
    case 1:
      filters.push_back("asetrate=" + number(rate * .8) +
                        ",aresample=" + std::to_string(rate) + ",atempo=1.25");
      break;
    case 2:
      filters.push_back("asetrate=" + number(rate * 1.25) +
                        ",aresample=" + std::to_string(rate) + ",atempo=0.8");
      break;
    case 3:
      filters.push_back("highpass=f=300,lowpass=f=3400");
      break;
    case 4:
      filters.push_back("aecho=0.8:0.75:90:" + number(.1 + .4 * c.fx.amount));
      break;
    case 5:
      filters.push_back("tremolo=f=5:d=" + number(c.fx.amount));
      break;
    }
  }
  auto joined = [&] {
    std::string s;
    for (auto &f : filters) {
      if (!s.empty())
        s += ',';
      s += f;
    }
    return s;
  };
  auto base = ffargs();
  base.insert(base.end(), {"-f", "f32le", "-ar", std::to_string(rate), "-ac",
                           "2", "-i", input.u8string()});
  if (!c.fx.bypass && c.fx.normalize) {
    auto args = base;
    args[4] = "info";
    std::string before = joined();
    args.insert(args.end(),
                {"-af",
                 before + (before.empty() ? "" : ",") +
                     "loudnorm=I=-16:TP=-1:LRA=11:print_format=json",
                 "-f", "null", "-"});
    auto log = process(exe, args, w.path / "analysis.log", job);
    auto start = log.rfind('{'), end = log.rfind('}');
    if (start == std::string::npos || end == std::string::npos || end < start)
      throw std::runtime_error("FFmpeg did not return loudness analysis");
    auto j = Json::parse(log.substr(start, end - start + 1));
    measuredLUFS = j.at("input_i").get<std::string>();
    measuredTruePeak = j.at("input_tp").get<std::string>();
    auto finite = [&](const char *key) {
      auto v = j.at(key).get<std::string>();
      return v != "-inf" && v != "inf" && v != "nan";
    };
    if (finite("input_i") && finite("input_tp") && finite("input_lra") &&
        finite("input_thresh"))
      filters.push_back(
          "loudnorm=I=-16:TP=-1:LRA=11:measured_I=" +
          j.at("input_i").get<std::string>() +
          ":measured_TP=" + j.at("input_tp").get<std::string>() +
          ":measured_LRA=" + j.at("input_lra").get<std::string>() +
          ":measured_thresh=" + j.at("input_thresh").get<std::string>() +
          ":offset=" + j.at("target_offset").get<std::string>() +
          ":linear=true");
  }
  auto args = base;
  auto filter = joined();
  if (!filter.empty())
    args.insert(args.end(), {"-af", filter});
  args.insert(args.end(),
              {"-f", "f32le", "-ar", std::to_string(rate), "-ac", "2", "-fs",
               std::to_string(maxDecodedBytes), output.u8string()});
  process(exe, args, w.path / "process.log", job);
  auto pcm = readPCM(output);
  size_t expected = size_t(c.length()) * 2;
  if (expected * 4 >= maxDecodedBytes)
    throw std::runtime_error("Processed clip exceeds 256 MiB");
  pcm->resize(expected, 0.f);
  return {c.id, pcm, 0, measuredLUFS, measuredTruePeak};
}
inline Mixer prepare(const fs::path &exe, Project p,
                     const std::shared_ptr<JobControl> &job = {},
                     const Mixer *previous = nullptr) {
  p.validate();
  Mixer m;
  m.project = std::move(p);
  size_t done = 0, cacheBytes = 0;
  std::set<const PCM *> seen;
  for (auto &c : m.project.clips) {
    cancelled(job);
    auto s = m.project.source(c.source);
    if (!s || !s->pcm)
      throw std::runtime_error(
          "Relink missing sources before preview or export");
    bool reused = false;
    if (previous) {
      auto old = previous->project.clip(c.id);
      auto oldSource = old ? previous->project.source(old->source) : nullptr;
      auto prepared = previous->prepared.find(c.id);
      if (old && oldSource && prepared != previous->prepared.end() &&
          processingKey(c, *s, m.project.rate) ==
              processingKey(*old, *oldSource, previous->project.rate)) {
        m.prepared[c.id] = prepared->second;
        reused = true;
      }
    }
    if (!reused)
      m.prepared[c.id] = prepareClip(exe, c, *s, m.project.rate, job);
    auto buffer = m.prepared.at(c.id).pcm;
    if (buffer && seen.insert(buffer.get()).second)
      cacheBytes += buffer->size() * 4;
    if (cacheBytes > maxProjectBytes)
      throw std::runtime_error("Prepared audio exceeds the 512 MiB cache "
                               "limit; split into smaller projects");
    if (job)
      job->progress =
          float(++done) / std::max(size_t(1), m.project.clips.size());
  }
  return m;
}
struct ExportOptions {
  Frame begin = 0, end = 0;
  int rate = 44100, channels = 2, bits = 16, bitrate = 192;
};
inline void exportAudio(const fs::path &exe, const Mixer &mixer,
                        const fs::path &target, ExportOptions options,
                        const std::shared_ptr<JobControl> &job = {}) {
  if (options.begin < 0 || options.end <= options.begin ||
      options.end > mixer.project.duration() ||
      (options.channels != 1 && options.channels != 2) || options.rate < 8000 ||
      options.rate > 192000)
    throw std::runtime_error("Invalid audio export range/rate/channels");
  auto ext = target.extension().u8string();
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return char(std::tolower(c)); });
  if (ext != ".wav" && ext != ".ogg" && ext != ".mp3" && ext != ".flac")
    throw std::runtime_error("Export format must be WAV, OGG, MP3 or FLAC");
  Workspace w;
  auto raw = w.path / "mix.pcm", encoded = w.path / ("encoded" + ext);
  std::ofstream out(raw, std::ios::binary);
  std::array<float, 8192> block{};
  for (Frame at = options.begin; at < options.end;) {
    cancelled(job);
    size_t count = size_t(std::min(Frame(4096), options.end - at));
    mixer.render(at, count, block.data());
    out.write((char *)block.data(), std::streamsize(count * 2 * 4));
    if (!out)
      throw std::runtime_error("Cannot write mix workspace (disk full)");
    at += count;
    if (job)
      job->progress =
          float(at - options.begin) / float(options.end - options.begin) * .8f;
  }
  out.close();
  auto args = ffargs();
  args.insert(args.end(),
              {"-f", "f32le", "-ar", std::to_string(mixer.project.rate), "-ac",
               "2", "-i", raw.u8string(), "-ar", std::to_string(options.rate),
               "-ac", std::to_string(options.channels), "-c:a",
               ext == ".wav" ? (options.bits == 24 ? "pcm_s24le" : "pcm_s16le")
               : ext == ".flac" ? "flac"
               : ext == ".mp3"  ? "libmp3lame"
                                : "libvorbis"});
  if (ext == ".mp3" || ext == ".ogg")
    args.insert(args.end(), {"-b:a", std::to_string(options.bitrate) + "k"});
  args.push_back(encoded.u8string());
  process(exe, args, w.path / "export.log", job);
  cancelled(job);
  // Stage on the destination filesystem before replacement. An existing file is
  // never destroyed if encoding/cancellation/write fails.
  auto temp = target;
  temp += ".reyond-audio-" +
          std::to_string(std::chrono::high_resolution_clock::now()
                             .time_since_epoch()
                             .count()) +
          ".tmp";
  try {
    fs::copy_file(encoded, temp, fs::copy_options::none);
    cancelled(job);
#ifdef _WIN32
    if (!MoveFileExW(temp.c_str(), target.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
      throw std::runtime_error("Cannot replace audio export");
#else
    fs::rename(temp, target);
#endif
  } catch (...) {
    std::error_code ec;
    fs::remove(temp, ec);
    throw;
  }
  if (job)
    job->progress = 1;
}
} // namespace ftebroom::audio
