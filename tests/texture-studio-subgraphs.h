// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

static void checkSubgraphBoundaries(const ts::Document& grouped) {
  using namespace ts;
  const auto repeated = [](size_t count, const Json& item) {
    return Json(std::vector<Json>(count, item));
  };
  const auto numbered = [](unsigned count, const Json& item) {
    auto object = Json::object();
    for (unsigned i = 0; i < count; ++i) object["pin-" + std::to_string(i)] = item;
    return object;
  };
  const auto pin = [](const std::string& name, const char* target, const char* port) {
    return Json{{"name", name}, {"node", target}, {"port", port}};
  };
  const auto leaf = [](const Json& members, const Json& inputs) {
    return Json{{"nodes", members}, {"edges", Json::array()}, {"inputs", inputs}, {"outputs", Json::object()}};
  };
  const auto instance = [](const char* id, const std::string& group) {
    return node(id, "Subgraph", {{"group", group}}, 0);
  };
  const Json empty = Json::array();
  const auto emptyGroup = leaf(empty, empty);
  const std::vector<std::pair<const char*, Json>> malformedDefinitions{
    {"nodes", false}, {"nodes", repeated(maxNodes + 1, false)},
    {"edges", false}, {"edges", repeated(maxEdges + 1, false)},
    {"inputs", false}, {"inputs", repeated(65, false)},
    {"outputs", empty}, {"outputs", numbered(17, false)},
  };
  for (const auto& [key, value] : malformedDefinitions) {
    auto invalid = grouped;
    invalid.source["graph"]["groups"]["inner"][key] = value;
    reject([&] { compile(invalid); }, "definition is malformed or exceeds");
  }
  for (auto library : {empty, numbered(129, emptyGroup), Json{{"bad/name", emptyGroup}}, Json{{"inner", false}}}) {
    auto invalid = grouped;
    invalid.source["graph"]["groups"] = std::move(library);
    reject([&] { compile(invalid); }, "graph.groups");
  }
  for (const auto& [key, value] : malformedDefinitions) {
    if (std::string_view(key) != "nodes" && std::string_view(key) != "edges") continue;
    auto invalid = grouped;
    invalid.source["graph"][key] = value;
    reject([&] { compile(invalid); }, "too many nodes or connections");
  }
  auto unknown = grouped;
  unknown.source["graph"]["nodes"][1]["version"] = 2;
  reject([&] { compile(unknown); }, "unknown node type or version");

  auto withSeeds = grouped;
  auto& outer = withSeeds.source["graph"]["groups"]["outer"]["nodes"][0];
  outer["parameters"]["keepSeeds"] = true;
  const auto independent = compile(withSeeds);
  withSeeds.source["graph"]["nodes"][1]["parameters"]["keepSeeds"] = true;
  withSeeds.source["graph"]["groups"]["inner"]["nodes"][0]["seedIdentity"] = "preserved-seed";
  const auto preserved = compile(withSeeds);
  const auto identityPresent = [](const Plan& plan, std::string_view identity) {
    return std::any_of(plan.nodes.begin(), plan.nodes.end(),
      [&](const auto& item) { return item.seedIdentity == identity; });
  };
  check(identityPresent(preserved, "preserved-seed") && identityPresent(preserved, "product") &&
    !identityPresent(independent, "product"), "Nested instances preserve seeds only through enabled ancestors");

  auto excessive = grouped;
  auto& graph = excessive.source["graph"];
  auto& library = graph["groups"];
  library = Json::object();
  graph["nodes"] = Json::array({instance("exterior", "depth-0")});
  graph["edges"] = empty;
  for (unsigned i = 0; i <= 16; ++i) {
    auto members = i == 16 ? empty : Json::array({instance("next", "depth-" + std::to_string(i + 1))});
    library["depth-" + std::to_string(i)] = leaf(members, empty);
  }
  reject([&] { compile(excessive); }, "nesting beyond 16 levels");
  library = {{"outer", leaf(repeated(maxNodes, node("n", "Constant", {{"value", 1}}, 0)), empty)}};
  for (unsigned i = 0; i < maxNodes; ++i)
    library["outer"]["nodes"][i]["id"] = "n-" + std::to_string(i);
  graph["nodes"] = Json::array({instance("exterior", "outer")});
  reject([&] { compile(excessive); }, "instance limit");

  library["inner"] = leaf(Json::array({node("sample", "Invert", Json::object(), 0)}),
    repeated(64, pin("amount", "sample", "value")));
  library["outer"] = leaf(Json::array({instance("inner", "inner")}), repeated(64, pin("amount", "inner", "amount")));
  library["top"] = leaf(Json::array({instance("outer", "outer")}), repeated(3, pin("amount", "outer", "amount")));
  graph["nodes"] = Json::array({instance("exterior", "top")});
  reject([&] { compile(excessive); }, "fans out too widely");
  library["top"]["inputs"] = empty;
  for (unsigned i = 0; i < 9; ++i)
    library["top"]["inputs"].push_back(pin("amount-" + std::to_string(i), "outer", "amount"));
  reject([&] { compile(excessive); }, "fans out too widely");
  graph["nodes"] = Json::array({node("source", "Constant", {{"value", 1}}, 0), instance("exterior", "outer")});
  graph["edges"] = repeated(3, wire("source", "value", "exterior", "amount"));
  reject([&] { compile(excessive); }, "expanded graph exceeds its edge limit");

  const auto checkRejectedGrouping = [](Document document, const std::set<std::string>& selected,
                                        const std::string& id, const char* diagnostic) {
    const auto before = document.save();
    reject([&] { groupNodes(document, selected, id, "Boundary test"); }, diagnostic);
    check(document.save() == before, "Rejected grouping must preserve the document");
  };
  const auto brick = runningBrick("group-regression");
  checkRejectedGrouping(brick, {}, "room", "Choose nodes");
  checkRejectedGrouping(brick, {"bond"}, "bad/id", "Choose nodes");
  checkRejectedGrouping(brick, {"bond"}, "bond", "Choose nodes");
  auto invalidGroups = brick;
  for (auto groups : {empty, numbered(128, emptyGroup), Json{{"room", emptyGroup}}}) {
    invalidGroups.source["graph"]["groups"] = std::move(groups);
    checkRejectedGrouping(invalidGroups, {"bond"}, "room", "Choose a new subgraph definition ID");
  }
  auto manyPorts = brick;
  manyPorts.source["outputs"] = Json::object();
  manyPorts.source["graph"]["edges"] = empty;
  checkRejectedGrouping(manyPorts, {"bond"}, "room", "needs an outgoing connection");
  for (unsigned i = 0; i < 17; ++i)
    manyPorts.source["outputs"]["output-" + std::to_string(i)] = {{"node", "bond"}, {"port", "pin-" + std::to_string(i)}};
  checkRejectedGrouping(manyPorts, {"bond"}, "room", "needs an outgoing connection");
  manyPorts.source["outputs"] = brick.source["outputs"];
  manyPorts.source["graph"]["edges"] = repeated(65, wire("wear", "value", "bond", "variation"));
  checkRejectedGrouping(manyPorts, {"bond"}, "room", "needs an outgoing connection");

  for (const auto& position : {Json(false), Json::array(), Json::array({"x", 0}), Json::array({0, "y"})}) {
    auto withPosition = brick;
    findNode(withPosition, "bond")["position"] = position;
    withPosition.source["exposedParameters"][0]["group"] = "other-room";
    groupNodes(withPosition, {"bond"}, "room", "Room");
    check(findNode(withPosition, "room")["position"] == Json::array({0, 0}), "Invalid positions cannot shift group placement");
    check(withPosition.source["exposedParameters"][0]["group"] == "other-room", "Existing bindings stay scoped");
    check(evaluate(compile(withPosition), {16, 16}).outputs.at("baseColor")->pixels ==
      evaluate(compile(brick), {16, 16}).outputs.at("baseColor")->pixels, "Grouping across an incoming link preserves output");
  }
}
