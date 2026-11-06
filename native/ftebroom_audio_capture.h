// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "ftebroom_audio_project.h"
#include <array>
#include <cstring>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <mmsystem.h>
#else
#include <dlfcn.h>
#endif
namespace ftebroom::audio {
// Native capture is independent of FTE's output-only Audio plugin interface.
// Windows uses the system WaveIn API, Linux the system SDL2 capture queue.
// Device buffers are polled without blocking; FFmpeg/files never run here.
class Capture {
#ifdef _WIN32
  HMODULE library = nullptr;
  HWAVEIN device = nullptr;
  using Open = MMRESULT(WINAPI *)(LPHWAVEIN, UINT, LPCWAVEFORMATEX, DWORD_PTR,
                                  DWORD_PTR, DWORD);
  using Header = MMRESULT(WINAPI *)(HWAVEIN, LPWAVEHDR, UINT);
  using Simple = MMRESULT(WINAPI *)(HWAVEIN);
  Open open = nullptr;
  Header prepare = nullptr, unprepare = nullptr, add = nullptr;
  Simple start = nullptr, stop = nullptr, reset = nullptr, close = nullptr;
  std::array<WAVEHDR, 8> headers{};
  std::array<std::array<int16_t, 8192>, 8> buffers{};
  template <class T> T symbol(const char *name) {
    auto p = (T)GetProcAddress(library, name);
    if (!p)
      throw std::runtime_error("Windows audio capture API is unavailable");
    return p;
  }
#else
  void *library = nullptr;
  uint32_t device = 0;
  struct Spec {
    int freq;
    uint16_t format;
    uint8_t channels, silence;
    uint16_t samples, padding;
    uint32_t size;
    void (*callback)(void *, uint8_t *, int);
    void *userdata;
  };
  int (*init)(uint32_t) = nullptr;
  void (*quit)(uint32_t) = nullptr;
  int (*count)(int) = nullptr;
  const char *(*name)(int, int) = nullptr;
  const char *(*error)() = nullptr;
  uint32_t (*open)(const char *, int, const Spec *, Spec *, int) = nullptr;
  void (*pause)(uint32_t, int) = nullptr;
  void (*close)(uint32_t) = nullptr;
  uint32_t (*queued)(uint32_t) = nullptr;
  uint32_t (*dequeue)(uint32_t, void *, uint32_t) = nullptr;
  bool initialized = false;
  template <class T> T symbol(const char *name) {
    auto p = (T)dlsym(library, name);
    if (!p)
      throw std::runtime_error("SDL2 capture API is unavailable");
    return p;
  }
#endif
  void load() {
    if (library)
      return;
    try {
#ifdef _WIN32
      library = LoadLibraryW(L"winmm.dll");
      if (!library)
        throw std::runtime_error("Cannot load Windows audio capture");
      open = symbol<Open>("waveInOpen");
      prepare = symbol<Header>("waveInPrepareHeader");
      unprepare = symbol<Header>("waveInUnprepareHeader");
      add = symbol<Header>("waveInAddBuffer");
      start = symbol<Simple>("waveInStart");
      stop = symbol<Simple>("waveInStop");
      reset = symbol<Simple>("waveInReset");
      close = symbol<Simple>("waveInClose");
#else
      library = dlopen("libSDL2-2.0.so.0", RTLD_NOW | RTLD_LOCAL);
      if (!library)
        throw std::runtime_error(
            "Recording requires the system SDL2 audio library");
      init = symbol<decltype(init)>("SDL_InitSubSystem");
      quit = symbol<decltype(quit)>("SDL_QuitSubSystem");
      count = symbol<decltype(count)>("SDL_GetNumAudioDevices");
      name = symbol<decltype(name)>("SDL_GetAudioDeviceName");
      error = symbol<decltype(error)>("SDL_GetError");
      open = symbol<decltype(open)>("SDL_OpenAudioDevice");
      pause = symbol<decltype(pause)>("SDL_PauseAudioDevice");
      close = symbol<decltype(close)>("SDL_CloseAudioDevice");
      queued = symbol<decltype(queued)>("SDL_GetQueuedAudioSize");
      dequeue = symbol<decltype(dequeue)>("SDL_DequeueAudio");
      if (init(0x10))
        throw std::runtime_error(std::string("Audio input unavailable: ") +
                                 error());
      initialized = true;
#endif
    } catch (...) {
#ifdef _WIN32
      if (library)
        FreeLibrary(library);
#else
      if (library)
        dlclose(library);
      initialized = false;
#endif
      library = nullptr;
      throw;
    }
  }
  void append(const int16_t *p, size_t size, float volume) {
    if (samples.size() + size >= maxDecodedBytes / sizeof(float))
      throw std::runtime_error(
          "Recording reached the decoded size limit. Insert the captured take "
          "before starting another");
    for (size_t i = 0; i < size; i++) {
      auto value = int16_t(std::clamp(float(p[i]) * volume, -32768.f, 32767.f));
      samples.push_back(value);
      peak = std::max(peak, std::abs(float(value) / 32768));
    }
  }

public:
  std::vector<int16_t> samples;
  float peak = 0;
  int rate = 44100;
  std::string deviceName;
  bool active() const { return device != 0; }
  std::vector<std::string> devices() {
    load();
    std::vector<std::string> result{"Default microphone"};
#ifdef _WIN32
    auto n = symbol<decltype(&waveInGetNumDevs)>("waveInGetNumDevs")();
    auto caps = symbol<decltype(&waveInGetDevCapsW)>("waveInGetDevCapsW");
    for (UINT i = 0; i < n; i++) {
      WAVEINCAPSW c{};
      if (!caps(i, &c, sizeof c)) {
        int size = WideCharToMultiByte(CP_UTF8, 0, c.szPname, -1, nullptr, 0,
                                       nullptr, nullptr);
        std::string s(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, c.szPname, -1, s.data(), size, nullptr,
                            nullptr);
        if (!s.empty() && s.back() == 0)
          s.pop_back();
        result.push_back(s);
      }
    }
#else
    for (int i = 0; i < count(1); i++)
      if (auto n = name(i, 1))
        result.emplace_back(n);
#endif
    return result;
  }
  void begin(int index = 0) {
    end();
    load();
    samples.clear();
    samples.reserve(44100 * 4);
    peak = 0;
#ifdef _WIN32
    WAVEFORMATEX fmt{};
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 2;
    fmt.nSamplesPerSec = rate;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = 4;
    fmt.nAvgBytesPerSec = rate * 4;
    if (open(&device, index ? UINT(index - 1) : WAVE_MAPPER, &fmt, 0, 0,
             CALLBACK_NULL))
      throw std::runtime_error(
          "Cannot open the selected microphone at 44.1 kHz stereo");
    for (size_t i = 0; i < headers.size(); i++) {
      auto &h = headers[i];
      h = {};
      h.lpData = (LPSTR)buffers[i].data();
      h.dwBufferLength = DWORD(buffers[i].size() * 2);
      if (prepare(device, &h, sizeof h) || add(device, &h, sizeof h)) {
        end();
        throw std::runtime_error("Cannot allocate microphone buffers");
      }
    }
    if (start(device)) {
      end();
      throw std::runtime_error("Cannot start microphone capture");
    }
#else
    if (index < 0 || index > count(1))
      throw std::runtime_error("Microphone is no longer available");
    Spec want{}, got{};
    want.freq = rate;
    want.format = 0x8010;
    want.channels = 2;
    want.samples = 1024;
    device = open(index ? name(index - 1, 1) : nullptr, 1, &want, &got, 0);
    if (!device)
      throw std::runtime_error(std::string("Cannot open microphone: ") +
                               error());
    if (got.freq != rate || got.channels != 2 || got.format != want.format) {
      end();
      throw std::runtime_error(
          "Microphone did not provide 44.1 kHz stereo PCM");
    }
    pause(device, 0);
#endif
  }
  void poll(float volume = 1) {
    if (!active())
      return;
    peak = 0;
#ifdef _WIN32
    for (auto &h : headers)
      if (h.dwFlags & WHDR_DONE) {
        append((const int16_t *)h.lpData, h.dwBytesRecorded / 2, volume);
        h.dwBytesRecorded = 0;
        if (add(device, &h, sizeof h))
          throw std::runtime_error("Microphone disconnected");
      }
#else
    if (queued(device) > 4u * 44100 * 4)
      throw std::runtime_error("Microphone queue overrun; recording stopped");
    std::array<int16_t, 8192> buffer{};
    while (queued(device)) {
      auto bytes = dequeue(device, buffer.data(), uint32_t(buffer.size() * 2));
      if (!bytes)
        break;
      append(buffer.data(), bytes / 2, volume);
    }
#endif
  }
  void finish(float volume = 1) {
    if (!active())
      return;
    try {
      poll(volume);
#ifdef _WIN32
      stop(device);
      reset(device);
      for (auto &header : headers)
        if ((header.dwFlags & WHDR_DONE) && header.dwBytesRecorded)
          append((const int16_t *)header.lpData, header.dwBytesRecorded / 2,
                 volume);
#else
      pause(device, 1);
      poll(volume);
#endif
      end();
    } catch (...) {
      end();
      throw;
    }
  }
  void end() {
    if (!active())
      return;
#ifdef _WIN32
    stop(device);
    reset(device);
    for (auto &h : headers)
      if (h.dwFlags & WHDR_PREPARED)
        unprepare(device, &h, sizeof h);
    close(device);
    device = nullptr;
#else
    pause(device, 1);
    close(device);
    device = 0;
#endif
  }
  ~Capture() {
    end();
#ifdef _WIN32
    if (library)
      FreeLibrary(library);
#else
    if (initialized && quit)
      quit(0x10);
    if (library)
      dlclose(library);
#endif
  }
};
} // namespace ftebroom::audio
