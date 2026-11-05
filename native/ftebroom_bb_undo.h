// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ftebroom_bb_model.h"
#include <deque>
namespace ftebroom::bb {
inline Json editDiff(const Json &from, const Json &to) {
  auto patch = Json::diff(from, to);
  auto applied = from.patch(patch);
  std::function<void(const Json &, const Json &, std::string)> types =
      [&](const Json &a, const Json &b, std::string path) {
        if (a.is_number() && b.is_number() && a.type() != b.type()) {
          patch.push_back({{"op", "replace"}, {"path", path}, {"value", b}});
          return;
        }
        if (b.is_array())
          for (size_t i = 0; i < b.size(); i++)
            types(a[i], b[i], path + "/" + std::to_string(i));
        else if (b.is_object())
          for (auto it = b.begin(); it != b.end(); ++it) {
            auto key = it.key();
            std::string escaped;
            for (char c : key)
              escaped += c == '~' ? "~0" : c == '/' ? "~1" : std::string(1, c);
            types(a[it.key()], it.value(), path + "/" + escaped);
          }
      };
  types(applied, to, "");
  return patch;
}
struct PixelDelta {
  size_t index;
  Texture before, after;
  int x = 0, y = 0, w = 0, h = 0;
  bool whole = false;
};
struct EditRecord {
  std::string label;
  Json before, after;
  Selection selectionBefore, selectionAfter;
  std::vector<PixelDelta> pixels;
  size_t bytes = 0;
  uint64_t beforeStamp = 0, afterStamp = 0;
};
class History {
  Json before;
  Selection selected;
  std::map<size_t, Texture> images;
  size_t imageCount = 0;
  bool editing = false;
  std::deque<EditRecord> undoStack, redoStack;
  size_t memory = 0;
  uint64_t currentStamp = 0, nextStamp = 0;
  static void applyPixels(Project &p, const EditRecord &e, bool after) {
    for (auto &d : e.pixels) {
      auto t = after ? d.after : d.before;
      if (d.whole) {
        uint64_t prior = d.index < p.textures.size() ? p.textures[d.index].revision : 0;
        t.revision = std::max(prior, t.revision) + 1;
        if (d.index < p.textures.size())
          p.textures[d.index] = t;
        else
          p.textures.push_back(t);
      } else if (d.index < p.textures.size()) {
        auto &dest = p.textures[d.index];
        for (int y = 0; y < d.h; y++)
          std::copy_n(t.rgba.data() + size_t(y) * d.w * 4, size_t(d.w) * 4,
                      dest.rgba.data() + (size_t(y + d.y) * dest.width + d.x) * 4);
        dest.saved = false;
        dest.revision++;
      }
    }
    p.textures.resize(p.root["textures"].size());
  }

public:
  size_t steps = 128, budget = 256 * 1024 * 1024;
  void begin(const Project &p, const Selection &s, std::vector<size_t> affected = {}) {
    if (editing)
      return;
    before = p.root;
    selected = s;
    imageCount = p.textures.size();
    images.clear();
    for (auto i : affected)
      if (i < p.textures.size())
        images.emplace(i, p.textures[i]);
    editing = true;
  }
  uint64_t state() const {
    return currentStamp;
  }
  bool active() const {
    return editing;
  }
  size_t size() const {
    return undoStack.size();
  }
  void cancel(Project &p, Selection &s) {
    if (!editing)
      return;
    p.root = before;
    for (auto &pair : images) {
      if (pair.first < p.textures.size())
        p.textures[pair.first] = pair.second;
      else
        p.textures.push_back(pair.second);
    }
    p.textures.resize(imageCount);
    s = selected;
    p.revision++;
    editing = false;
    images.clear();
  }
  bool finish(Project &p, const Selection &s, std::string label) {
    if (!editing)
      return false;
    try {
      validate(p);
    } catch (...) {
      p.root = before;
      for (auto &pair : images) {
        if (pair.first < p.textures.size())
          p.textures[pair.first] = pair.second;
        else
          p.textures.push_back(pair.second);
      }
      p.textures.resize(imageCount);
      p.revision++;
      editing = false;
      images.clear();
      throw;
    }
    EditRecord e;
    e.label = label;
    e.before = editDiff(p.root, before);
    e.after = editDiff(before, p.root);
    e.selectionBefore = selected;
    e.selectionAfter = s;
    for (size_t i = 0; i < std::max(imageCount, p.textures.size()); i++) {
      if (i < imageCount && !images.count(i))
        continue;
      Texture a = images.count(i) ? images.at(i) : Texture{},
              b = i < p.textures.size() ? p.textures[i] : Texture{};
      PixelDelta d;
      d.index = i;
      if (i >= imageCount || i >= p.textures.size() || a.width != b.width || a.height != b.height) {
        d.whole = true;
        d.before = std::move(a);
        d.after = std::move(b);
      } else {
        int x0 = a.width, y0 = a.height, x1 = -1, y1 = -1;
        for (int y = 0; y < a.height; y++)
          for (int x = 0; x < a.width; x++) {
            size_t n = (size_t(y) * a.width + x) * 4;
            if (!std::equal(a.rgba.begin() + n, a.rgba.begin() + n + 4, b.rgba.begin() + n)) {
              x0 = std::min(x0, x);
              x1 = std::max(x1, x);
              y0 = std::min(y0, y);
              y1 = std::max(y1, y);
            }
          }
        if (x1 < 0)
          continue;
        d.x = x0;
        d.y = y0;
        d.w = x1 - x0 + 1;
        d.h = y1 - y0 + 1;
        d.before = a;
        d.after = b;
        d.before.rgba.clear();
        d.after.rgba.clear();
        for (int y = y0; y <= y1; y++) {
          size_t n = (size_t(y) * a.width + x0) * 4;
          d.before.rgba.insert(d.before.rgba.end(), a.rgba.begin() + n,
                               a.rgba.begin() + n + d.w * 4);
          d.after.rgba.insert(d.after.rgba.end(), b.rgba.begin() + n, b.rgba.begin() + n + d.w * 4);
        }
      }
      e.bytes += d.before.rgba.size() + d.after.rgba.size();
      e.pixels.push_back(std::move(d));
    }
    e.bytes += e.before.dump().size() + e.after.dump().size();
    editing = false;
    images.clear();
    if (e.after.empty() && e.pixels.empty())
      return false;
    e.beforeStamp = currentStamp;
    e.afterStamp = currentStamp = ++nextStamp;
    redoStack.clear();
    memory += e.bytes;
    undoStack.push_back(std::move(e));
    while (undoStack.size() > steps || memory > budget) {
      memory -= undoStack.front().bytes;
      undoStack.pop_front();
    }
    p.revision++;
    return true;
  }
  bool undo(Project &p, Selection &s) {
    if (editing || undoStack.empty())
      return false;
    auto e = std::move(undoStack.back());
    undoStack.pop_back();
    memory -= e.bytes;
    p.root = p.root.patch(e.before);
    currentStamp = e.beforeStamp;
    applyPixels(p, e, false);
    s = e.selectionBefore;
    p.revision++;
    redoStack.push_back(std::move(e));
    return true;
  }
  bool redo(Project &p, Selection &s) {
    if (editing || redoStack.empty())
      return false;
    auto e = std::move(redoStack.back());
    redoStack.pop_back();
    p.root = p.root.patch(e.after);
    currentStamp = e.afterStamp;
    applyPixels(p, e, true);
    s = e.selectionAfter;
    p.revision++;
    memory += e.bytes;
    undoStack.push_back(std::move(e));
    return true;
  }
  std::string undoLabel() const {
    return undoStack.empty() ? "Undo" : ("Undo " + undoStack.back().label);
  }
  void clear() {
    undoStack.clear();
    redoStack.clear();
    images.clear();
    editing = false;
    memory = 0;
    currentStamp = nextStamp = 0;
  }
};
} // namespace ftebroom::bb
