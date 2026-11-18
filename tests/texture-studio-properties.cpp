// SPDX-License-Identifier: GPL-3.0-or-later
#include "../native/texture/presets.h"

#include <cmath>
#include <iostream>
#include <random>

namespace studio = ftebroom::textureStudio;

int main() try {
  constexpr uint32_t seed = 7305082;
  constexpr unsigned cases = 10000;
  std::mt19937 random(seed);
  std::string current;
  for (unsigned example = 0; example < cases; ++example) {
    current = "property-" + std::to_string(example);
    auto document = studio::runningBrick(current, random());
    auto& graph = document.source["graph"];
    auto& nodes = graph["nodes"];
    auto& edges = graph["edges"];
    float expected = float(random() % 1025u) / 1024.f;
    nodes = studio::Json::array({
      studio::node("field", "Constant", {{"value", expected}}, 0),
      studio::node("color", "ConstantColor", {{"rgba", {.125f,.25f,.375f,1}}}, 1),
    });
    edges = studio::Json::array();
    std::string previous = "field";
    const unsigned depth = 1 + random() % 9;
    for (unsigned step = 0; step < depth; ++step) {
      const auto id = "step-" + std::to_string(step);
      if (random() % 2 == 0) {
        nodes.push_back(studio::node(id.c_str(), "Invert", studio::Json::object(), step + 2));
        expected = 1.f - expected;
      } else {
        const float low = float(random() % 161u) / 1024.f;
        const float high = float(864u + random() % 161u) / 1024.f;
        nodes.push_back(studio::node(id.c_str(), "Clamp", {{"low", low}, {"high", high}}, step + 2));
        expected = std::min(std::max(expected, low), high);
      }
      edges.push_back(studio::wire(previous.c_str(), "value", id.c_str(), "value"));
      previous = id;
    }
    document.source["outputs"] = {{"height", {{"node", previous}, {"port", "value"}}},
      {"baseColor", {{"node", "color"}, {"port", "color"}}}};
    document.source["exposedParameters"] = studio::Json::array();
    const auto saved = document.save();
    const auto reopened = studio::open(saved);
    studio::require(reopened.save() == saved, current, "A generated graph did not round-trip.");
    const auto size = studio::Resolution{2u + unsigned(random() % 7u), 2u + unsigned(random() % 7u)};
    const auto actual = studio::evaluate(studio::compile(reopened), size);
    const auto height = actual.outputs.at("height");
    const auto color = actual.outputs.at("baseColor");
    studio::require(std::all_of(height->pixels.begin(), height->pixels.end(), [&](const auto& pixel) {
      return std::abs(pixel[0] - expected) <= 1e-7f;
    }), current, "The scalar graph differs from its analytic oracle.");
    studio::require(std::all_of(color->pixels.begin(), color->pixels.end(), [](const auto& pixel) {
      return pixel == studio::Pixel{.125f,.25f,.375f,1};
    }), current, "A scalar edit changed the independent color branch.");

    const char* reason = nullptr;
    switch (example % 4) {
      case 0:
        nodes.push_back(nodes.front());
        reason = "Duplicate node";
        break;
      case 1:
        edges[0]["fromNode"] = previous;
        reason = "cycle";
        break;
      case 2:
        edges[0]["fromPort"] = "color";
        reason = "port";
        break;
      case 3:
        edges[0]["fromNode"] = "color";
        edges[0]["fromPort"] = "color";
        reason = "incompatible";
        break;
    }
    bool rejected = false;
    try { studio::compile(document); }
    catch (const studio::Error& error) { rejected = std::string_view(error.what()).find(reason) != std::string_view::npos; }
    studio::require(rejected, current, "A malformed edge or node lacks the expected diagnostic.");
  }
  std::cout << "PASS " << cases << " valid graph round trips and " << cases << " malformed graphs with seed " << seed << '\n';
  return 0;
} catch (const std::exception& error) {
  std::cerr << error.what() << '\n';
  return 1;
}
