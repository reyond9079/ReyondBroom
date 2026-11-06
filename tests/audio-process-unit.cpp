// SPDX-License-Identifier: GPL-2.0-or-later
#include "../native/ftebroom_audio_process.h"
#include <cassert>
#include <iostream>
#include <random>
using namespace ftebroom::audio;
static double frequency(const PCM &pcm, int rate) {
  size_t begin = rate / 5, end = pcm.size() / 2 - rate / 5;
  int crossings = 0;
  for (size_t i = begin + 1; i < end; i++)
    crossings += pcm[(i - 1) * 2] <= 0 && pcm[i * 2] > 0;
  return double(crossings) * rate / (end - begin);
}
static double rms(const PCM &pcm) {
  double sum = 0;
  for (float v : pcm)
    sum += v * v;
  return std::sqrt(sum / pcm.size());
}
int main(int argc, char **argv) {
  fs::path exe = argc > 1 ? fs::u8path(argv[1]) : fs::path("/usr/bin/ffmpeg");
  Workspace w;
  Project p;
  auto data = std::make_shared<PCM>(44100 * 8);
  for (size_t i = 0; i < data->size() / 2; i++)
    (*data)[i * 2] = (*data)[i * 2 + 1] =
        .15f * std::sin(i * 440 * 6.283185307 / 44100);
  Source s;
  s.name = "tone.wav";
  s.uri = "tone.wav";
  s.fingerprint = "test";
  s.pcm = data;
  auto sid = p.addSource(s), tid = p.addTrack("Tone"), id = p.addClip(sid, tid);
  auto m = prepare(exe, p);
  ExportOptions options;
  options.end = p.duration();
  auto target = w.path / fs::u8path("mix 'quoted' звук.wav");
  exportAudio(exe, m, target, options);
  auto decoded = decode(exe, target.u8string(), readBytes(target), 44100);
  assert(decoded.length() == 176400 && decoded.rate == 44100 &&
         decoded.channels == 2);
  PCM preview(data->size());
  m.render(0, data->size() / 2, preview.data());
  double maxError = 0;
  for (size_t i = 0; i < preview.size(); i++)
    maxError =
        std::max(maxError, double(std::abs(preview[i] - (*decoded.pcm)[i])));
  assert(maxError <
         1.f / 30000); // Same master graph, only 16-bit quantization differs.
  p.clip(id)->speed = 2;
  p.clip(id)->pitchLinked = true;
  m = prepare(exe, p);
  assert(m.prepared.at(id).pcm->size() == 176400 &&
         std::abs(frequency(*m.prepared.at(id).pcm, 44100) - 880) < 3);
  p.clip(id)->pitchLinked = false;
  m = prepare(exe, p);
  assert(std::abs(frequency(*m.prepared.at(id).pcm, 44100) - 440) < 3);
  p.clip(id)->speed = 1;
  p.clip(id)->fx.normalize = true;
  m = prepare(exe, p);
  assert(!m.prepared.at(id).measuredLUFS.empty());
  writePCM(w.path / "normalized.pcm", *m.prepared.at(id).pcm);
  auto args = ffargs();
  args[4] = "info";
  args.insert(args.end(),
              {"-f", "f32le", "-ar", "44100", "-ac", "2", "-i",
               (w.path / "normalized.pcm").u8string(), "-af",
               "loudnorm=I=-16:TP=-1:print_format=json", "-f", "null", "-"});
  auto log = process(exe, args, w.path / "measure.log");
  auto j = Json::parse(
      log.substr(log.rfind('{'), log.rfind('}') - log.rfind('{') + 1));
  assert(std::abs(std::stof(j.at("input_i").get<std::string>()) + 16) < .5 &&
         std::stof(j.at("input_tp").get<std::string>()) <= -.9);
  std::cout << "PASS 440/880 Hz pitch modes, shared preview/export (<1 LSB), "
               "normalization "
            << j.at("input_i") << " LUFS / " << j.at("input_tp") << " dBTP\n";
  // Each encoder is decoded again and checked for duration and original
  // metadata.
  for (auto ext : {".wav", ".ogg", ".mp3", ".flac"}) {
    options.rate = 48000;
    options.channels = 1;
    options.begin = 44100;
    options.end = 3 * 44100;
    auto output = w.path / (std::string("range") + ext);
    exportAudio(exe, m, output, options);
    auto roundtrip = decode(exe, output.u8string(), readBytes(output), 44100);
    assert(roundtrip.rate == 48000 && roundtrip.channels == 1 &&
           std::abs(double(roundtrip.length()) / 44100 - 2) < .03);
  }
  auto cancel = std::make_shared<JobControl>();
  cancel->cancel = true;
  auto original = readBytes(target);
  bool rejected = false;
  try {
    exportAudio(exe, m, target, options, cancel);
  } catch (...) {
    rejected = true;
  }
  assert(rejected && readBytes(target) == original);
  // Cancel a child process after launch, rather than only testing the preflight
  // flag.
  cancel = std::make_shared<JobControl>();
  std::thread stop([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    cancel->cancel = true;
  });
  rejected = false;
  auto slow = ffargs();
  slow.insert(slow.end(),
              {"-re", "-f", "f32le", "-ar", "44100", "-ac", "2", "-stream_loop",
               "-1", "-i", (w.path / "normalized.pcm").u8string(), "-f", "null",
               "-"});
  try {
    process(exe, slow, w.path / "cancel.log", cancel);
  } catch (...) {
    rejected = true;
  }
  stop.join();
  assert(rejected);
  auto silence = std::make_shared<PCM>(44100 * 4, 0.f);
  p.sources[0].pcm = silence;
  p.sources[0].fingerprint = "silence";
  p.clip(id)->out = 88200;
  m = prepare(exe, p);
  assert(rms(*m.prepared.at(id).pcm) == 0);
  std::mt19937 random(42);
  std::normal_distribution<float> noise(0, .015f);
  auto noisy = std::make_shared<PCM>(44100 * 8);
  for (auto &v : *noisy)
    v = noise(random);
  p.sources[0].pcm = noisy;
  p.sources[0].fingerprint = "noise";
  p.clip(id)->out = 176400;
  p.clip(id)->fx.normalize = false;
  p.clip(id)->fx.denoise = true;
  m = prepare(exe, p);
  assert(rms(*m.prepared.at(id).pcm) < rms(*noisy) * .9);
  for (int effect = 1; effect <= 5; effect++) {
    p.clip(id)->fx.voice = effect;
    p.clip(id)->fx.enhance = true;
    m = prepare(exe, p);
    assert(m.prepared.at(id).pcm->size() == noisy->size());
  }
  p.clip(id)->fx.bypass = true;
  m = prepare(exe, p);
  assert(m.prepared.at(id).pcm == noisy);
  std::cout << "PASS four codecs, 48k mono range, cancellation preserves "
               "target, running-child cancellation, silent normalization, "
               "noise reduction, five voice filters and bypass\n";
}
