// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ftebroom_bb_mesh_ops.h"
#include "ftebroom_bb_undo.h"
namespace ftebroom::bb {
inline std::set<std::string> descendants(const Project &p, std::set<std::string> ids) {
  auto par = parents(p);
  bool more = true;
  while (more) {
    more = false;
    for (auto &[id, parent] : par)
      if (ids.count(parent) && !ids.count(id)) {
        ids.insert(id);
        more = true;
      }
  }
  return ids;
}
inline std::set<std::string> selectedTop(const Project &p, const Selection &s) {
  auto ids = s.elements;
  ids.insert(s.groups.begin(), s.groups.end());
  auto par = parents(p);
  auto copy = ids;
  for (auto &id : copy) {
    auto parent = par[id];
    int guard = 0;
    while (!parent.empty() && guard++ < 512) {
      if (ids.count(parent)) {
        ids.erase(id);
        break;
      }
      parent = par[parent];
    }
  }
  return ids;
}
inline void eraseNodes(Json &nodes, const std::set<std::string> &ids) {
  for (size_t i = nodes.size(); i-- > 0;) {
    auto &n = nodes[i];
    auto id = n.is_string() ? n.get<std::string>() : n.value("uuid", "");
    if (ids.count(id))
      nodes.erase(i);
    else if (n.is_object())
      eraseNodes(n["children"], ids);
  }
}
inline void eraseSelection(Project &p, Selection &s) {
  auto selected = s.elements;
  selected.insert(s.groups.begin(), s.groups.end());
  auto ids = descendants(p, selected);
  for (auto key : {"elements", "groups"})
    for (size_t i = p.root[key].size(); i-- > 0;)
      if (ids.count(p.root[key][i].value("uuid", "")))
        p.root[key].erase(i);
  eraseNodes(p.root["outliner"], ids);
  for (auto &a : p.root["animations"])
    for (auto &id : ids)
      a["animators"].erase(id);
  s = {};
}
inline void translateObject(Json &e, Vec3 delta) {
  auto type = e.value("type", "");
  if (type == "cube") {
    e["from"] = add(getv(e, "from"), delta);
    e["to"] = add(getv(e, "to"), delta);
    e["origin"] = add(getv(e, "origin"), delta);
  } else if (type == "locator")
    e["position"] = add(getv(e, "position"), delta);
  else
    e["origin"] = add(getv(e, "origin"), delta);
}
inline void moveSelection(Project &p, const Selection &s, Vec3 delta) {
  auto top = selectedTop(p, s), all = descendants(p, top);
  for (auto &id : all)
    if (auto e = p.find(id)) {
      if (e->value("locked", false))
        continue;
      if (s.vertices.count(id) && !s.vertices.at(id).empty() && e->value("type", "") == "mesh") {
        auto inv = inverse(rotation(getv(*e, "rotation")));
        for (auto &k : s.vertices.at(id))
          (*e)["vertices"][k] = add(vec((*e)["vertices"][k]), inv.vector(delta));
      } else
        translateObject(*e, delta);
    }
}
inline void pivotObject(Project &p, std::string id, Vec3 origin) {
  auto e = p.find(id);
  if (!e)
    return;
  auto old = getv(*e, "origin");
  auto r = rotation(getv(*e, "rotation"));
  auto delta = add(inverse(r).vector(sub(old, origin)), sub(origin, old));
  if (e->value("type", "") == "cube") {
    (*e)["from"] = add(getv(*e, "from"), delta);
    (*e)["to"] = add(getv(*e, "to"), delta);
  } else if (e->value("type", "") == "mesh") {
    auto localDelta = inverse(r).vector(sub(old, origin));
    for (auto &v : (*e)["vertices"])
      v = add(vec(v), localDelta);
  } else {
    auto all = descendants(p, {id});
    all.erase(id);
    for (auto &child : all)
      if (auto c = p.find(child))
        translateObject(*c, delta);
  }
  (*e)["origin"] = origin;
}
inline void duplicate(Project &p, Selection &s) {
  auto top = selectedTop(p, s), all = descendants(p, top);
  auto par = parents(p);
  std::map<std::string, std::string> ids;
  for (auto &id : all)
    ids[id] = uuid();
  for (auto key : {"elements", "groups"}) {
    auto old = p.root[key];
    for (auto e : old)
      if (all.count(e.value("uuid", ""))) {
        e["uuid"] = ids.at(e["uuid"].get<std::string>());
        e["name"] = e.value("name", "object") + " copy";
        p.root[key].push_back(e);
      }
  }
  std::function<Json(const std::string &)> node = [&](const std::string &id) {
    bool isGroup = false;
    for (auto &g : p.root["groups"])
      if (g.value("uuid", "") == ids[id])
        isGroup = true;
    if (!isGroup)
      return Json(ids[id]);
    Json n = {{"uuid", ids[id]}, {"isOpen", true}, {"children", Json::array()}};
    for (auto &[child, parent] : par)
      if (parent == id)
        n["children"].push_back(node(child));
    return n;
  };
  for (auto &id : top) {
    auto n = node(id);
    std::function<bool(Json &)> insert = [&](Json &list) {
      for (auto &q : list)
        if (q.is_object()) {
          if (q.value("uuid", "") == par[id]) {
            q["children"].push_back(n);
            return true;
          }
          if (insert(q["children"]))
            return true;
        }
      return false;
    };
    if (par[id].empty() || !insert(p.root["outliner"]))
      p.root["outliner"].push_back(n);
  }
  for (auto &a : p.root["animations"]) {
    auto old = a["animators"];
    for (auto it = old.begin(); it != old.end(); ++it)
      if (ids.count(it.key())) {
        auto track = it.value();
        for (auto &k : track["keyframes"])
          k["uuid"] = uuid();
        a["animators"][ids[it.key()]] = track;
      }
  }
  s = {};
  for (auto &id : top) {
    auto e = p.find(ids[id]);
    if (e && e->value("type", "").empty())
      s.groups.insert(ids[id]);
    else
      s.elements.insert(ids[id]);
  }
  p.revision++;
}
inline void reparent(Project &p, const std::set<std::string> &ids, const std::string &parent) {
  auto all = descendants(p, ids);
  if (all.count(parent))
    throw std::runtime_error("A group cannot be parented to itself or its descendants");
  if (!parent.empty()) {
    auto e = p.find(parent);
    if (!e || e->contains("type"))
      throw std::runtime_error("Drop inside a group");
  }
  auto oldParents = parents(p);
  auto oldMatrices = groupMatrices(p);
  auto target = parent.empty() ? Mat4::identity() : oldMatrices.at(parent);
  Selection selected;
  for (auto id : ids) {
    auto e = p.find(id);
    if (e && e->contains("type"))
      selected.elements.insert(id);
    else
      selected.groups.insert(id);
  }
  for (auto top : selectedTop(p, selected)) {
    auto e = p.find(top);
    if (!e)
      continue;
    auto oldParent = oldParents[top].empty() ? Mat4::identity() : oldMatrices.at(oldParents[top]);
    auto relative = inverse(target) * oldParent;
    bool groupTop = !e->contains("type");
    for (auto id : groupTop ? descendants(p, {top}) : std::set<std::string>{top})
      if (auto item = p.find(id)) {
        auto type = item->value("type", "");
        if (type.empty()) {
          (*item)["origin"] = relative.point(getv(*item, "origin"));
          (*item)["rotation"] =
              eulerAngles(relative * rotation(getv(*item, "rotation")) * inverse(relative));
        } else if (type == "locator") {
          (*item)["position"] = relative.point(getv(*item, "position"));
          (*item)["rotation"] = eulerAngles(relative * rotation(getv(*item, "rotation")));
        } else {
          auto origin = getv(*item, "origin"), transformed = relative.point(origin);
          if (type == "cube") {
            (*item)["from"] = add(getv(*item, "from"), sub(transformed, origin));
            (*item)["to"] = add(getv(*item, "to"), sub(transformed, origin));
          }
          (*item)["origin"] = transformed;
          (*item)["rotation"] = eulerAngles(relative * rotation(getv(*item, "rotation")));
        }
      }
  }
  std::vector<Json> moving;
  std::function<void(Json &)> take = [&](Json &list) {
    for (size_t i = list.size(); i-- > 0;) {
      auto &n = list[i];
      auto id = n.is_string() ? n.get<std::string>() : n.value("uuid", "");
      if (ids.count(id)) {
        moving.push_back(n);
        list.erase(i);
      } else if (n.is_object())
        take(n["children"]);
    }
  };
  take(p.root["outliner"]);
  std::reverse(moving.begin(), moving.end());
  Json *dest = &p.root["outliner"];
  std::function<void(Json &)> find = [&](Json &list) {
    for (auto &n : list)
      if (n.is_object()) {
        if (n.value("uuid", "") == parent)
          dest = &n["children"];
        find(n["children"]);
      }
  };
  if (!parent.empty())
    find(p.root["outliner"]);
  for (auto &n : moving)
    dest->push_back(n);
}
inline void groupSelection(Project &p, Selection &s) {
  auto top = selectedTop(p, s);
  auto g = group();
  std::string id = g["uuid"];
  p.root["groups"].push_back(g);
  appendNode(p, id, "", true);
  reparent(p, top, id);
  s = {};
  s.groups.insert(id);
  s.activeGroup = id;
}
inline void createFace(Json &m, const std::set<std::string> &selected) {
  if (selected.size() < 3 || selected.size() > 4)
    throw std::runtime_error("Select three or four vertices");
  std::vector<std::string> keys(selected.begin(), selected.end());
  Vec3 c{};
  for (auto k : keys)
    c = add(c, vec(m["vertices"][k]));
  c = mul(c, 1. / keys.size());
  auto axis = unit(sub(vec(m["vertices"][keys[0]]), c)),
       n = unit(cross(sub(vec(m["vertices"][keys[1]]), c), axis)), y = cross(n, axis);
  std::sort(keys.begin(), keys.end(), [&](auto a, auto b) {
    auto pa = sub(vec(m["vertices"][a]), c), pb = sub(vec(m["vertices"][b]), c);
    return std::atan2(dot(pa, y), dot(pa, axis)) < std::atan2(dot(pb, y), dot(pb, axis));
  });
  face(m, keys);
}
inline void dissolveEdges(Json &m, const std::set<std::string> &selected) {
  bool merged = false;
  auto old = m["faces"];
  for (auto a = old.begin(); a != old.end() && !merged; ++a)
    for (auto b = std::next(a); b != old.end() && !merged; ++b) {
      auto av = a.value()["vertices"].get<std::vector<std::string>>(),
           bv = b.value()["vertices"].get<std::vector<std::string>>();
      if (av.size() != 3 || bv.size() != 3)
        continue;
      std::set<std::string> keys(av.begin(), av.end());
      keys.insert(bv.begin(), bv.end());
      if (keys.size() != 4)
        continue;
      std::vector<std::string> shared;
      for (auto k : av)
        if (std::find(bv.begin(), bv.end(), k) != bv.end())
          shared.push_back(k);
      if (shared.size() != 2)
        continue;
      if (!selected.empty() && !selected.count(shared[0] + "|" + shared[1]) &&
          !selected.count(shared[1] + "|" + shared[0]))
        continue;
      auto pa = vec(m["vertices"][av[0]]),
           n = unit(cross(sub(vec(m["vertices"][av[1]]), pa), sub(vec(m["vertices"][av[2]]), pa)));
      bool planar = true;
      for (auto k : keys)
        if (std::abs(dot(n, sub(vec(m["vertices"][k]), pa))) > .001)
          planar = false;
      if (!planar)
        continue;
      size_t count = m["faces"].size();
      createFace(m, keys);
      auto key = m["faces"].end();
      --key;
      for (auto it = m["faces"].begin(); it != m["faces"].end(); ++it)
        if (!old.contains(it.key())) {
          it.value()["texture"] = a.value()["texture"];
          for (auto k : keys)
            it.value()["uv"][k] =
                a.value()["uv"].contains(k) ? a.value()["uv"][k] : b.value()["uv"][k];
          break;
        }
      m["faces"].erase(a.key());
      m["faces"].erase(b.key());
      merged = true;
    }
  if (!merged)
    throw std::runtime_error("Select the shared edge of two coplanar triangles");
}
} // namespace ftebroom::bb
