// SPDX-License-Identifier: GPL-3.0-or-later
#include "../native/texture/presets.h"

#include <chrono>
#include <iostream>
#include <thread>
#ifndef _WIN32
#include <sys/resource.h>
#endif

namespace ts = ftebroom::textureStudio;

ts::Plan representative(unsigned count) {
  auto doc = ts::runningBrick("performance");
  auto& nodes = doc.source["graph"]["nodes"];
  auto& edges = doc.source["graph"]["edges"];
  std::string input = "bond";
  for (unsigned i = unsigned(nodes.size()); i < count; ++i) {
    const auto id = "filter-" + std::to_string(i);
    nodes.push_back(ts::node(id.c_str(), "Clamp", {{"low", 0.01}, {"high", 0.99}}, i % 5, i / 5));
    edges.push_back(ts::wire(input.c_str(), "value", id.c_str(), "value"));
    input = id;
  }
  for (auto& edge : edges)
    if (edge["toNode"] == "ao" || edge["toNode"] == "normal") edge["fromNode"] = input;
  doc.source["outputs"].erase("height");
  const auto plan = ts::compile(doc);
  ts::require(plan.order.size() == count && plan.outputs.size() == 4, "benchmark", "The fixture graph is incomplete.");
  return plan;
}

int cancellationBenchmark() {
  using Clock = std::chrono::steady_clock;
  const auto plan = representative(100);
  std::vector<double> elapsed;
  for (int run = -5; run < 30; ++run) {
    std::atomic<bool> stop{false};
    std::promise<void> ready;
    auto begun = ready.get_future();
    auto task = std::async(std::launch::async, [&] {
      ready.set_value();
      try {
        ts::evaluate(plan, {2048, 2048}, &stop, ts::memoryBudget, 4);
      } catch (const ts::Error& e) {
        return std::string_view(e.what()).find("cancelled") != std::string_view::npos;
      }
      return false;
    });
    begun.wait();
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    const auto start = Clock::now();
    stop.store(true, std::memory_order_relaxed);
    ts::require(task.get(), "cancel", "A cancelled render did not terminate with cancellation.");
    if (run >= 0)
      elapsed.push_back(std::chrono::duration<double, std::milli>(Clock::now() - start).count());
  }
  auto sorted = elapsed;
  std::sort(sorted.begin(), sorted.end());
  std::cout << ts::Json{{"backend", "cpu"}, {"phase", "cancel signal to completed worker"},
    {"nodes", 100}, {"size", 2048}, {"threads", 4}, {"warmupsDiscarded", 5},
    {"measurementsMs", elapsed}, {"medianMs", (sorted[14] + sorted[15]) * .5},
    {"p95Ms", sorted[28]}, {"maximumMs", sorted.back()}}.dump(2) << '\n';
  return sorted.back() <= 500 ? 0 : 1;
}

int main(int argc, char** argv) try {
  if (argc == 2 && std::string_view(argv[1]) == "--cancel") return cancellationBenchmark();
  if (argc != 4) return 2;
  const unsigned nodes = std::stoul(argv[1]), edge = std::stoul(argv[2]), threads = std::stoul(argv[3]);
  if ((nodes != 40 && nodes != 100) || (edge != 256 && edge != 512 && edge != 2048)) return 2;
  using Clock = std::chrono::steady_clock;
  const auto plan = representative(nodes);
  std::vector<double> elapsed;
  for (int i = -5; i < 30; ++i) {
    auto start = Clock::now();
    auto result = ts::evaluate(plan, {edge, edge}, nullptr, ts::memoryBudget, threads);
    const auto stop = Clock::now();
    ts::require(!result.outputs.at("baseColor")->pixels.empty(), "benchmark", "The render produced no output.");
    if (i >= 0) elapsed.push_back(std::chrono::duration<double, std::milli>(stop - start).count());
  }
  auto sorted = elapsed;
  std::sort(sorted.begin(), sorted.end());
  ts::Json report{{"backend", "cpu"}, {"phase", "evaluation only"}, {"nodes", nodes},
    {"size", edge}, {"threads", threads}, {"warmupsDiscarded", 5}, {"measurementsMs", elapsed},
    {"medianMs", (sorted[14] + sorted[15]) * .5}, {"p95Ms", sorted[28]}, {"maximumMs", sorted.back()}};
#ifndef _WIN32
  rusage peak{};
  if (!getrusage(RUSAGE_SELF, &peak)) report["peakProcessRssKiB"] = peak.ru_maxrss;
#endif
  std::cout << report.dump(2) << '\n';
  return 0;
} catch (const std::exception& e) {
  std::cerr << e.what() << '\n';
  return 1;
}
