// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "presets.h"

namespace ftebroom::textureStudio {

struct LayerChain {
  std::string anchor, port;
  std::vector<std::string> order;
};

inline LayerChain layers(Document& doc) {
  auto output = doc.source.at("outputs").at("baseColor");
  LayerChain stack;
  const auto& edges = doc.source.at("graph").at("edges");
  for (unsigned depth = 0; depth < maxNodes; ++depth) {
    const auto id = output.at("node").get<std::string>();
    const auto& layer = findNode(doc, id);
    if (layer.at("type") != "Blend" || layer.at("version") != 1) {
      stack.anchor = id;
      stack.port = output.at("port").get<std::string>();
      std::reverse(stack.order.begin(), stack.order.end());
      return stack;
    }
    stack.order.push_back(id);
    const auto base = std::find_if(edges.begin(), edges.end(), [&](const auto& edge) {
      return edge.at("toNode") == id && edge.at("toPort") == "base";
    });
    require(base != edges.end(), "layers", "Connect the layer's base input.");
    output = {{"node", base->at("fromNode")}, {"port", base->at("fromPort")}};
  }
  throw Error("layers", "The layer chain exceeds the graph depth limit.");
}

inline void bindLayers(Document& doc, const LayerChain& stack) {
  auto source = stack.anchor, port = stack.port;
  auto& edges = doc.source["graph"]["edges"];
  for (const auto& id : stack.order) {
    const auto existing = std::find_if(edges.begin(), edges.end(), [&](const auto& edge) {
      return edge.at("toNode") == id && edge.at("toPort") == "base";
    });
    auto replacement = wire(source.c_str(), port.c_str(), id.c_str(), "base");
    if (existing == edges.end()) edges.push_back(std::move(replacement));
    else *existing = std::move(replacement);
    source = id;
    port = "color";
  }
  doc.source["outputs"]["baseColor"] = {{"node", source}, {"port", port}};
}

inline void appendSolidLayer(Document& doc, LayerChain stack, const std::string& id, const Pixel& tint) {
  auto& nodes = doc.source["graph"]["nodes"];
  auto& edges = doc.source["graph"]["edges"];
  const auto colorId = id + "-color", maskId = id + "-mask";
  const int row = 3 + int(stack.order.size());
  nodes.push_back(node(colorId.c_str(), "ConstantColor", {{"rgba", tint}}, 3, row));
  nodes.push_back(node(maskId.c_str(), "Constant", {{"value", 1}}, 3, row + 1));
  auto blend = node(id.c_str(), "Blend", defaultParameters(Kernel::blend), 4, row);
  blend["label"] = "Color layer";
  nodes.push_back(std::move(blend));
  edges.push_back(wire(colorId.c_str(), "color", id.c_str(), "top"));
  edges.push_back(wire(maskId.c_str(), "value", id.c_str(), "mask"));
  stack.order.push_back(id);
  bindLayers(doc, stack);
}

} // namespace ftebroom::textureStudio
