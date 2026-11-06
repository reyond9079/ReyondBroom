// SPDX-License-Identifier: GPL-2.0-or-later
#include "../native/ftebroom_audio_capture.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
int main() {
#ifndef _WIN32
  setenv("SDL_AUDIODRIVER", "dummy", 1);
#endif
  ftebroom::audio::Capture capture;
  auto devices = capture.devices();
  assert(!devices.empty());
  capture.begin(0);
  std::this_thread::sleep_for(std::chrono::milliseconds(160));
  capture.poll();
  capture.finish();
  assert(!capture.active() && capture.samples.size() > 1000);
  std::cout << "PASS capture API queue, device selection and stop, "
            << capture.samples.size() / 2
            << " frames (synthetic dummy device, not a physical microphone)\n";
}
