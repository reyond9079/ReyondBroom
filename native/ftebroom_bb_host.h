// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ftebroom_bb_render.h"
static void modelViewerLoad(const std::string &path);
static bool bbActive = false;
static void bbConvertNative(const std::string &source);
static void bbPanel();
static std::string modelDropText(const std::filesystem::path &path, float x, float y);
static void bbReleaseImages();
static bool bbCameraVisible();
static void bbNewDialog();
static void bbImportTick();
static void bbOpen(const std::string &path);
static void bbNew(const std::filesystem::path &folder = {});
static void bbLeave(std::function<void()> next);
static ftebroom::bb::DynamicMeshAPI *bbAPI() {
  return static_cast<ftebroom::bb::DynamicMeshAPI *>(
      core->GetEngineInterface("ReyondBroomDynamicMeshV1", sizeof(ftebroom::bb::DynamicMeshAPI)));
}
static ftebroom::bb::DynamicImageAPI *bbImageAPI() {
  return static_cast<ftebroom::bb::DynamicImageAPI *>(
      core->GetEngineInterface("ReyondBroomDynamicImageV1", sizeof(ftebroom::bb::DynamicImageAPI)));
}
static bool bbAuthorTiming = false;
static unsigned bbAuthorFrames = 0;
static double bbAuthorBegan = 0, bbAuthorCpu = 0, bbAuthorSubmit = 0;
static int bbAuthorTriangleCount = 0;
static std::string bbBenchBuffer;
static int bbBenchFrames;
static double bbBenchBegan, bbBenchDuration;
static void bbBenchStart(int count, bool slow) {
  input->SetMenuFocus(qtrue, "", 0, 0, 1);
  using namespace ftebroom::bb;
  auto api = bbAPI();
  if (!api)
    throw std::runtime_error("Dynamic mesh engine interface is unavailable");
  Project p;
  std::vector<Triangle> ts;
  for (int i = 0; i < count; i++) {
    double x = (i % 100) * 2, y = (i / 100) * 2;
    Triangle t;
    t.uv = {};
    t.p = {Vec3{x, 0, y}, Vec3{x + 1.9, 0, y}, Vec3{x, 0, y + 1.9}};
    t.n.fill({0, -1, 0});
    ts.push_back(t);
  }
  bbBenchBuffer = renderBuffer(p, ts);
  if (!api->upload(bbBenchBuffer.data(), bbBenchBuffer.size()))
    throw std::runtime_error("Synthetic buffer rejected");
  for (auto n : {"ftebroom_bb_active", "ftebroom_bb_benchmark_b", "ftebroom_bb_benchmark_vertices",
                 "ftebroom_bb_low", "ftebroom_bb_high", "ftebroom_bb_ortho"})
    cvars->GetNVFDG(n, "0", 0, "Model authoring preview", "ReyondBroom");
  bbBenchFrames = 0;
  bbBenchBegan = core->GetMilliseconds();
  bbBenchDuration = 0;
  modelViewerLoad("models/qa/test.md3");
  cvars->SetFloat("ftebroom_bb_active", 1);
  cvars->SetFloat("ftebroom_bb_benchmark_b", slow);
  cvars->SetFloat("ftebroom_bb_benchmark_vertices", count * 3);
  cvars->SetString("ftebroom_bb_low", count == 5000 ? "-100 -200 0" : "-400 -200 0");
  cvars->SetString("ftebroom_bb_high", "0 0 0");
}
static void bbBenchTick() {
  if (bbAuthorTiming) {
    bbAuthorFrames++;
    bbAuthorSubmit += imguiRenderMs;
  }
  if (bbBenchBuffer.empty())
    return;
  auto api = bbAPI();
  if (!api)
    return;
  float x;
  std::memcpy(&x, bbBenchBuffer.data() + 144, 4);
  x += .001f;
  std::memcpy(bbBenchBuffer.data() + 144, &x, 4);
  auto start = std::chrono::steady_clock::now();
  api->upload(bbBenchBuffer.data(), bbBenchBuffer.size());
  bbBenchDuration +=
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
  bbBenchFrames++;
}
static void bbBenchStop() {
  if (bbBenchBuffer.empty())
    return;
  double stats[4]{};
  bbAPI()->stats(stats);
  double seconds = (core->GetMilliseconds() - bbBenchBegan) / 1000.;
  std::ostringstream s;
  s << "BB_RENDER_BENCH triangles=" << (bbBenchBuffer.size() - 144) / 120
    << " mode=" << cvars->GetFloat("ftebroom_bb_benchmark_b") << " frames=" << bbBenchFrames
    << " seconds=" << seconds << " fps=" << bbBenchFrames / std::max(.001, seconds)
    << " upload_mean_ms=" << bbBenchDuration / std::max(1, bbBenchFrames)
    << " native_draw_ms=" << stats[1] << "\n";
  core->Print(s.str().c_str());
  bbBenchBuffer.clear();
  cvars->SetFloat("ftebroom_bb_active", 0);
  cvars->SetFloat("ftebroom_bb_benchmark_b", 0);
}
