// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "../third_party/json.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ftebroom::textureStudio {

using Json = nlohmann::json;

constexpr unsigned schemaVersion = 1;
inline constexpr const char* generatorVersion = "1.0.0";
constexpr size_t maxSourceBytes = 16 * 1024 * 1024;
constexpr unsigned maxNodes = 2048, maxEdges = 8192, maxDimension = 8192;

struct Error : std::runtime_error {
  std::string location;
  Error(std::string where, std::string reason)
    : std::runtime_error(where + ": " + reason), location(std::move(where)) {}
};

inline void require(bool ok, const std::string& at, const char* reason) {
  if (!ok) throw Error(at, reason);
}

inline const Json& member(const Json& object, const char* key, std::string_view at) {
  require(object.is_object(), std::string(at), "Expected an object.");
  auto child = object.find(key);
  require(child != object.end(), std::string(at) + "." + key, "Missing required field.");
  return *child;
}

inline unsigned integer(const Json& value, unsigned low, unsigned high, const std::string& at) {
  require(value.is_number_integer(), at, "Expected an integer.");
  if (value.is_number_unsigned()) {
    const auto n = value.get<uint64_t>();
    require(n >= low && n <= high, at, "Integer is outside its supported range.");
    return unsigned(n);
  }
  const auto n = value.get<int64_t>();
  require(n >= int64_t(low) && n <= int64_t(high), at, "Integer is outside its supported range.");
  return unsigned(n);
}

inline float number(const Json& value, float low, float high, const std::string& at) {
  require(value.is_number(), at, "Expected a number.");
  const double n = value.get<double>();
  require(std::isfinite(n) && n >= low && n <= high, at, "Number is outside its supported range.");
  return static_cast<float>(n);
}

inline bool identifier(std::string_view id) {
  return !id.empty() && id.size() <= 64 &&
    std::all_of(id.begin(), id.end(), [](char c) {
      return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
             (c >= 'A' && c <= 'Z') || c == '_' || c == '-';
    });
}

inline std::string idField(const Json& obj, const char* key, const std::string& at) {
  const auto& value = member(obj, key, at);
  require(value.is_string() && identifier(value.get_ref<const std::string&>()),
    at + "." + key, "Use an identifier with 1–64 ASCII letters, digits, underscores or hyphens.");
  return value.get<std::string>();
}

inline Json parseJson(std::string_view bytes) {
  require(!bytes.empty() && bytes.size() <= maxSourceBytes, "document", "Source must be between 1 byte and 16 MiB.");
  std::vector<std::set<std::string>> keys;
  try {
    return Json::parse(bytes, [&keys](int depth, Json::parse_event_t event, Json& value) {
      require(depth <= 48, "document", "Source nesting is too deep.");
      if (event == Json::parse_event_t::object_start) keys.emplace_back();
      if (event == Json::parse_event_t::object_end) keys.pop_back();
      if (event == Json::parse_event_t::key)
        require(keys.back().insert(value.get<std::string>()).second, "document", "Duplicate object key.");
      return true;
    });
  } catch (const Json::exception& e) {
    throw Error("document", e.what());
  }
}

struct Resolution { unsigned width, height; };

struct Document {
  Json source;
  bool readOnly = false;
  std::string original;

  Resolution resolution() const {
    const auto& res = member(source, "resolution", "document");
    return {integer(member(res, "width", "resolution"), 1, maxDimension, "resolution.width"),
      integer(member(res, "height", "resolution"), 1, maxDimension, "resolution.height")};
  }
  uint32_t seed() const {
    return integer(member(source, "rootSeed", "document"), 0, std::numeric_limits<uint32_t>::max(), "rootSeed");
  }
  std::string save() const { return readOnly ? original : source.dump(2) + "\n"; }
};

inline Document open(std::string_view bytes) {
  Document doc{parseJson(bytes), false, {}};
  const auto version = integer(member(doc.source, "schemaVersion", "document"), 1,
    std::numeric_limits<unsigned>::max(), "schemaVersion");
  if (version > schemaVersion) return {std::move(doc.source), true, std::string(bytes)};
  idField(doc.source, "documentId", "document");
  const auto& name = member(doc.source, "name", "document");
  require(name.is_string() && !name.get_ref<const std::string&>().empty() &&
    name.get_ref<const std::string&>().size() <= 256, "name", "Use a nonempty name of at most 256 UTF-8 bytes.");
  require(member(doc.source, "generatorVersion", "document") == generatorVersion,
    "generatorVersion", "This graph requires a different generator version.");
  const auto& color = member(doc.source, "colorManagement", "document");
  require(member(color, "working", "colorManagement") == "linear-srgb" &&
    member(color, "display", "colorManagement") == "srgb",
    "colorManagement", "Supported working and display spaces are linear sRGB and sRGB.");
  doc.resolution();
  doc.seed();
  const auto& graph = member(doc.source, "graph", "document");
  const auto& nodes = member(graph, "nodes", "graph");
  const auto& edges = member(graph, "edges", "graph");
  require(nodes.is_array() && nodes.size() <= maxNodes, "graph.nodes", "An array of at most 2048 nodes is required.");
  require(edges.is_array() && edges.size() <= maxEdges, "graph.edges", "An array of at most 8192 edges is required.");
  require(member(doc.source, "outputs", "document").is_object(), "outputs", "Expected an output map.");
  for (const auto* key : {"exposedParameters", "layerBindings", "assetReferences", "exportProfiles"})
    require(member(doc.source, key, "document").is_array(), key, "Expected an array.");
  const auto& animation = member(doc.source, "animation", "document");
  integer(member(animation, "frames", "animation"), 1, 64, "animation.frames");
  integer(member(animation, "fps", "animation"), 1, 30, "animation.fps");
  require(member(animation, "loop", "animation").is_boolean(), "animation.loop", "Expected a Boolean.");
  require(member(doc.source, "provenance", "document").is_object(), "provenance", "Expected a provenance object.");
  return doc;
}

inline std::string renderIdentity(const Document& doc) {
  require(!doc.readOnly, "document", "A newer source version is read-only.");
  const auto& graph = doc.source.at("graph");
  auto nodes = graph.at("nodes");
  auto edges = graph.at("edges");
  const auto stripPresentation = [](Json& node) {
    node.erase("position"); node.erase("label"); node.erase("comment");
  };
  const auto canonicalGraph = [&](Json& objects, Json& links) {
    for (auto& node : objects) stripPresentation(node);
    std::sort(objects.begin(), objects.end(), [](const auto& a, const auto& b) { return a.at("id") < b.at("id"); });
    std::sort(links.begin(), links.end(), [](const auto& a, const auto& b) { return a.dump() < b.dump(); });
  };
  canonicalGraph(nodes, edges);
  Json identity{{"generatorVersion", doc.source.at("generatorVersion")},
    {"rootSeed", doc.seed()}, {"resolution", doc.source.at("resolution")},
    {"colorManagement", doc.source.at("colorManagement")}, {"nodes", nodes}, {"edges", edges},
    {"outputs", doc.source.at("outputs")}, {"animation", doc.source.at("animation")},
    {"assetReferences", doc.source.at("assetReferences")}};
  if (graph.contains("groups")) {
    auto groups = graph.at("groups");
    for (auto& group : groups) {
      stripPresentation(group);
      canonicalGraph(group.at("nodes"), group.at("edges"));
      auto& inputs = group.at("inputs");
      std::sort(inputs.begin(), inputs.end(), [](const auto& a, const auto& b) { return a.dump() < b.dump(); });
    }
    identity["groups"] = std::move(groups);
  }
  return identity.dump();
}

class History {
  std::vector<Document> past, future;
  Document current;
  static constexpr size_t limit = 32;

public:
  explicit History(Document start) : current(std::move(start)) {}
  const Document& get() const { return current; }
  void replace(Document next) {
    require(!current.readOnly, "document", "A newer source version is read-only.");
    if (next.source == current.source) return;
    if (past.size() == limit) past.erase(past.begin());
    past.push_back(std::move(current));
    current = std::move(next);
    future.clear();
  }
  bool canUndo() const { return !past.empty(); }
  bool canRedo() const { return !future.empty(); }
  bool undo() {
    if (past.empty()) return false;
    future.push_back(std::move(current));
    current = std::move(past.back());
    past.pop_back();
    return true;
  }
  bool redo() {
    if (future.empty()) return false;
    past.push_back(std::move(current));
    current = std::move(future.back());
    future.pop_back();
    return true;
  }
};

} // namespace ftebroom::textureStudio
