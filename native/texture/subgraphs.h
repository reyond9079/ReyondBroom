// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "document.h"
#include "../ftebroom_sha256.h"

namespace ftebroom::textureStudio {

class SubgraphExpander {
  struct Endpoint { std::string node, port; };
  struct NodePorts {
    std::string id;
    std::map<std::string, std::vector<Endpoint>> inputs;
    std::map<std::string, Endpoint> outputs;
  };
  using Scope = std::map<std::string, NodePorts>;
  std::map<std::string, const Json*> library;
  std::set<std::string> stack;
  Json nodes = Json::array(), edges = Json::array();
  size_t instances = 0, projectedInputs = 0;

  static const NodePorts& lookup(const Scope& scope, const std::string& id, const std::string& where) {
    auto found = scope.find(id);
    require(found != scope.end(), where, "A subgraph connection refers to a missing node.");
    return found->second;
  }

  static Endpoint output(const Scope& scope, const std::string& id, const std::string& port, const std::string& where) {
    const auto& node = lookup(scope, id, where);
    if (!node.id.empty()) return {node.id, port};
    auto found = node.outputs.find(port);
    require(found != node.outputs.end(), where, "The subgraph has no such output.");
    return found->second;
  }

  static std::vector<Endpoint> input(const Scope& scope, const std::string& id, const std::string& port,
                                      const std::string& where) {
    const auto& node = lookup(scope, id, where);
    if (!node.id.empty()) return {{node.id, port}};
    auto found = node.inputs.find(port);
    require(found != node.inputs.end(), where, "The subgraph has no such input.");
    return found->second;
  }

  void addEdge(const Endpoint& from, const Endpoint& to, const std::string& where) {
    require(edges.size() < maxEdges, where, "The expanded graph exceeds its edge limit.");
    edges.push_back({{"fromNode", from.node}, {"fromPort", from.port}, {"toNode", to.node}, {"toPort", to.port}});
  }

  Scope expand(const Json& scope, const std::string& where, const std::string& identity, bool keepSeeds = false) {
    Scope result;
    for (const auto& entry : scope.at("nodes")) {
      const auto id = idField(entry, "id", where);
      const auto at = where + ".nodes." + id;
      const auto type = idField(entry, "type", at);
      const auto version = integer(member(entry, "version", at), 1, 65535, at + ".version");
      require(++instances <= maxNodes, at, "The expanded graph exceeds its instance limit.");
      const auto present = result.try_emplace(id);
      require(present.second, at, "Duplicate node ID.");
      auto& ports = present.first->second;
      const auto tier = identity + "/" + id;
      if (type != "Subgraph" || version != 1) {
        // Instance paths supply stable independent seeds without leaking UI labels into the hash.
        ports.id = identity.empty() ? id : "group-" + ftebroom::sha256(tier).substr(0, 57);
        auto node = entry;
        node["id"] = ports.id;
        if (keepSeeds && !node.contains("seedIdentity")) node["seedIdentity"] = id;
        nodes.push_back(std::move(node));
        continue;
      }
      const auto& properties = member(entry, "parameters", at);
      const auto definition = idField(properties, "group", at);
      const auto selected = library.find(definition);
      require(selected != library.end(), at, "The subgraph definition is missing.");
      require(stack.size() < 16 && stack.insert(definition).second, at, "Recursive subgraph or nesting beyond 16 levels.");
      const auto& graph = *selected->second;
      const bool keep = properties.value("keepSeeds", false);
      const auto inside = expand(graph, at + ".subgraph." + definition, tier, keep && (identity.empty() || keepSeeds));
      stack.erase(definition);
      for (const auto& connection : graph.at("inputs")) {
        const auto pin = idField(connection, "name", at);
        const auto destination = input(inside, idField(connection, "node", at), idField(connection, "port", at), at);
        auto& aliases = ports.inputs[pin];
        require(destination.size() <= maxEdges - aliases.size() && destination.size() <= 4 * maxEdges - projectedInputs,
          at, "A subgraph input fans out too widely.");
        projectedInputs += destination.size();
        aliases.insert(aliases.end(), destination.begin(), destination.end());
      }
      const auto& outputs = graph.at("outputs");
      for (auto item = outputs.begin(); item != outputs.end(); ++item) {
        require(identifier(item.key()), at, "The subgraph has an invalid output name.");
        ports.outputs.emplace(item.key(), output(inside, idField(*item, "node", at), idField(*item, "port", at), at));
      }
    }
    for (const auto& link : scope.at("edges")) {
      const auto from = output(result, idField(link, "fromNode", where), idField(link, "fromPort", where), where);
      for (const auto& to : input(result, idField(link, "toNode", where), idField(link, "toPort", where), where))
        addEdge(from, to, where);
    }
    return result;
  }

public:
  struct Expanded { Json nodes, edges, outputs; };

  explicit SubgraphExpander(const Json& groups) {
    require(groups.is_object() && groups.size() <= 128, "graph.groups", "A source can contain up to 128 subgraph definitions.");
    for (auto entry = groups.begin(); entry != groups.end(); ++entry) {
      require(identifier(entry.key()) && entry->is_object(), "graph.groups", "The subgraph definition has an invalid ID or body.");
      const auto path = "graph.groups." + entry.key();
      const auto& nodes = member(*entry, "nodes", path);
      const auto& edges = member(*entry, "edges", path);
      const auto& inputs = member(*entry, "inputs", path);
      const auto& outputs = member(*entry, "outputs", path);
      require(nodes.is_array() && nodes.size() <= maxNodes && edges.is_array() && edges.size() <= maxEdges &&
        inputs.is_array() && inputs.size() <= 64 && outputs.is_object() && outputs.size() <= 16,
        path, "The subgraph definition is malformed or exceeds its size limit.");
      for (const auto& node : nodes) idField(node, "id", path + ".nodes");
      library.emplace(entry.key(), &*entry);
    }
  }

  Expanded finish(const Json& graph, const Json& targets) {
    const auto& members = member(graph, "nodes", "graph");
    const auto& links = member(graph, "edges", "graph");
    require(members.is_array() && members.size() <= maxNodes && links.is_array() && links.size() <= maxEdges,
      "graph", "The graph has too many nodes or connections.");
    const auto scope = expand(graph, "graph", "");
    auto outputs = Json::object();
    require(targets.is_object(), "outputs", "Connect at least one output.");
    for (auto binding = targets.begin(); binding != targets.end(); ++binding) {
      const auto path = "outputs." + binding.key();
      const auto endpoint = output(scope, idField(*binding, "node", path), idField(*binding, "port", path), path);
      outputs[binding.key()] = {{"node", endpoint.node}, {"port", endpoint.port}};
    }
    return {std::move(nodes), std::move(edges), std::move(outputs)};
  }
};

inline void groupNodes(Document& doc, const std::set<std::string>& selected,
                       const std::string& id, std::string label) {
  require(!selected.empty() && identifier(id) && !selected.count(id), "graph", "Choose nodes for a new subgraph.");
  auto& graph = doc.source.at("graph");
  auto remaining = Json::array(), inside = Json::array();
  auto center = std::array{0.f, 0.f};
  for (const auto& node : graph.at("nodes")) {
    if (!selected.count(node.at("id").get<std::string>())) {
      require(node.at("id") != id, "graph", "The new subgraph ID is already used by a node.");
      remaining.push_back(node);
      continue;
    }
    require(node.at("type") != "Subgraph", "graph", "Open the definition to group its internal nodes.");
    const auto point = node.value("position", Json::array());
    if (point.is_array() && point.size() == 2 && point[0].is_number() && point[1].is_number()) {
      center[0] += point[0].get<float>();
      center[1] += point[1].get<float>();
    }
    inside.push_back(node);
  }
  require(inside.size() == selected.size(), "graph", "A selected node no longer exists.");
  for (float& coordinate : center) coordinate /= inside.size();
  Json internal = Json::array(), external = Json::array(), inputs = Json::array(), outputs = Json::object();
  std::map<std::pair<std::string, std::string>, std::string> usedOutputs;
  const auto expose = [&](std::string node, std::string port) {
    auto found = usedOutputs.find({node, port});
    if (found != usedOutputs.end()) return found->second;
    const auto name = "out-" + std::to_string(usedOutputs.size() + 1);
    outputs[name] = {{"node", node}, {"port", port}};
    usedOutputs.emplace(std::make_pair(std::move(node), std::move(port)), name);
    return name;
  };
  for (auto connection : graph.at("edges")) {
    const auto from = connection["fromNode"].get<std::string>(), to = connection["toNode"].get<std::string>();
    const bool source = selected.count(from) != 0, destination = selected.count(to) != 0;
    if (source && destination) {
      internal.push_back(std::move(connection));
      continue;
    }
    if (source) {
      connection["fromNode"] = id;
      connection["fromPort"] = expose(from, connection["fromPort"].get<std::string>());
    }
    if (destination) {
      const auto name = "in-" + std::to_string(inputs.size() + 1);
      inputs.push_back({{"name", name}, {"node", to}, {"port", connection["toPort"]}});
      connection["toNode"] = id;
      connection["toPort"] = name;
    }
    external.push_back(std::move(connection));
  }
  auto documentOutputs = doc.source.at("outputs");
  for (auto& output : documentOutputs) {
    const auto from = output["node"].get<std::string>();
    if (selected.count(from)) output = {{"node", id}, {"port", expose(from, output["port"].get<std::string>())}};
  }
  require(!outputs.empty() && outputs.size() <= 16 && inputs.size() <= 64, "graph",
    "A group needs an outgoing connection and can expose up to 16 outputs and 64 inputs.");
  const auto existingGroups = graph.find("groups");
  if (existingGroups != graph.end() && !existingGroups->is_null())
    require(existingGroups->is_object() && existingGroups->size() < 128 && !existingGroups->contains(id),
      "graph", "Choose a new subgraph definition ID.");
  auto& groups = graph["groups"];
  groups[id] = {{"label", label}, {"nodes", std::move(inside)}, {"edges", std::move(internal)},
    {"inputs", std::move(inputs)}, {"outputs", std::move(outputs)}};
  remaining.push_back({{"id", id}, {"type", "Subgraph"}, {"version", 1}, {"position", center},
    {"label", std::move(label)}, {"parameters", {{"group", id}, {"keepSeeds", true}}}});
  for (auto& binding : doc.source["exposedParameters"]) {
    if (!binding.contains("group") && selected.count(binding.value("node", ""))) binding["group"] = id;
  }
  graph["nodes"] = std::move(remaining);
  graph["edges"] = std::move(external);
  doc.source["outputs"] = std::move(documentOutputs);
}

} // namespace ftebroom::textureStudio
