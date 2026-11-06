// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "third_party/json.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
namespace ftebroom::audio {
using Frame = int64_t;
using Id = uint64_t;
using PCM = std::vector<float>;
using Json = nlohmann::json;
constexpr size_t maxDecodedBytes = 256u * 1024 * 1024;
constexpr size_t maxProjectBytes = 512u * 1024 * 1024;
inline float gain(float db) {
  return db <= -96 ? 0.f : std::pow(10.f, db / 20.f);
}
inline Frame frames(double seconds, int rate) {
  if (!std::isfinite(seconds) || seconds < 0 || seconds > 86400)
    throw std::runtime_error("Invalid audio time");
  return Frame(std::llround(seconds * rate));
}
inline std::string fingerprint(const std::string &data) {
  uint64_t hash = 1469598103934665603ull;
  for (unsigned char c : data) {
    hash ^= c;
    hash *= 1099511628211ull;
  }
  return std::to_string(hash) + ":" + std::to_string(data.size());
}
struct Peak {
  float lo = 0, hi = 0;
};
struct Source {
  Id id = 0;
  std::string uri, name, fingerprint, resourceUri, resourceFingerprint;
  int rate = 44100, channels = 2;
  std::shared_ptr<const PCM> pcm;
  std::shared_ptr<const std::string> original, targetOriginal;
  std::vector<Peak> peaks;
  Frame length() const { return pcm ? Frame(pcm->size() / 2) : 0; }
  void buildPeaks() {
    peaks.assign(1024, {});
    if (!pcm)
      return;
    for (size_t i = 0; i < pcm->size() / 2; i++) {
      auto &p = peaks[std::min(
          size_t(1023), i * 1024 / std::max(size_t(1), pcm->size() / 2))];
      for (int c = 0; c < 2; c++) {
        float v = (*pcm)[i * 2 + c];
        p.lo = std::min(p.lo, v);
        p.hi = std::max(p.hi, v);
      }
    }
  }
};
struct Key {
  Frame at = 0;
  float db = 0;
};
struct Effects {
  bool normalize = false, denoise = false, enhance = false, bypass = false;
  int voice = 0;
  float amount = .5f;
};
struct Clip {
  Id id = 0, source = 0, track = 0, group = 0;
  Frame at = 0, in = 0, out = 0, fadeIn = 0, fadeOut = 0;
  float db = 0, speed = 1, balance = 0;
  bool pitchLinked = false;
  Effects fx;
  std::vector<Key> keys;
  Frame length() const {
    return std::max(Frame(0), Frame(std::llround(double(out - in) /
                                                 std::max(.25f, speed))));
  }
  Frame end() const { return at + length(); }
  float automation(Frame local) const {
    if (keys.empty())
      return 0;
    auto next =
        std::upper_bound(keys.begin(), keys.end(), local,
                         [](Frame t, const Key &k) { return t < k.at; });
    if (next == keys.begin())
      return next->db;
    if (next == keys.end())
      return keys.back().db;
    auto prev = next - 1;
    double ratio =
        double(local - prev->at) / std::max(Frame(1), next->at - prev->at);
    return float(prev->db + (next->db - prev->db) * ratio);
  }
};
struct Track {
  Id id = 0;
  std::string name;
  bool locked = false, muted = false, solo = false, visible = true;
  float db = 0;
};
struct Marker {
  Frame at = 0;
  std::string name;
};
struct Project {
  int rate = 44100;
  Id next = 1;
  std::string name = "Untitled audio";
  std::vector<Source> sources;
  std::vector<Track> tracks;
  std::vector<Clip> clips;
  std::vector<Marker> markers;
  Frame rangeIn = 0, rangeOut = 0;
  bool range = false, magnet = false, snapping = true, linkage = true,
       previewAxis = false;
  float masterDb = 0;
  Source *source(Id id) {
    for (auto &v : sources)
      if (v.id == id)
        return &v;
    return nullptr;
  }
  const Source *source(Id id) const {
    for (auto &v : sources)
      if (v.id == id)
        return &v;
    return nullptr;
  }
  Track *track(Id id) {
    for (auto &v : tracks)
      if (v.id == id)
        return &v;
    return nullptr;
  }
  const Track *track(Id id) const {
    for (auto &v : tracks)
      if (v.id == id)
        return &v;
    return nullptr;
  }
  Clip *clip(Id id) {
    for (auto &v : clips)
      if (v.id == id)
        return &v;
    return nullptr;
  }
  const Clip *clip(Id id) const {
    for (auto &v : clips)
      if (v.id == id)
        return &v;
    return nullptr;
  }
  Frame duration() const {
    Frame n = 0;
    for (auto &c : clips)
      n = std::max(n, c.end());
    return n;
  }
  Id addTrack(std::string name = "Audio") {
    if (tracks.size() >= 64)
      throw std::runtime_error("Maximum 64 audio tracks");
    Track t;
    t.id = next++;
    t.name = std::move(name);
    tracks.push_back(t);
    return t.id;
  }
  Id addSource(Source s) {
    if (sources.size() >= 256)
      throw std::runtime_error("Maximum 256 audio sources");
    s.id = next++;
    s.buildPeaks();
    sources.push_back(std::move(s));
    return sources.back().id;
  }
  Id addClip(Id sid, Id tid, Frame at = 0) {
    auto s = source(sid);
    auto t = track(tid);
    if (!s || !t || t->locked || !s->pcm)
      throw std::runtime_error("Choose an available source and unlocked track");
    if (clips.size() >= 4096)
      throw std::runtime_error("Maximum 4096 audio clips");
    Clip c;
    c.id = next++;
    c.source = sid;
    c.track = tid;
    c.at = std::max(Frame(0), at);
    c.out = s->length();
    clips.push_back(c);
    return c.id;
  }
  bool editable(const Clip &c) const {
    auto t = track(c.track);
    return t && !t->locked;
  }
  void validate() const {
    if (!next || next > 1000000000ull)
      throw std::runtime_error("Invalid next audio ID");
    if (rate != 44100 && rate != 48000)
      throw std::runtime_error("Project sample rate must be 44100 or 48000");
    if (sources.size() > 256 || tracks.size() > 64 || clips.size() > 4096 ||
        markers.size() > 10000)
      throw std::runtime_error("Audio project exceeds object limits");
    if (name.size() > 256)
      throw std::runtime_error("Audio project name is too long");
    std::set<Id> ids;
    auto unique = [&](Id id) {
      if (!id || id >= next || !ids.insert(id).second)
        throw std::runtime_error("Invalid or duplicate audio object ID");
    };
    size_t totalBytes = 0;
    for (const auto &source : sources) {
      if (source.pcm)
        totalBytes += source.pcm->size() * sizeof(float);
      if (source.original)
        totalBytes += source.original->size();
      if (source.targetOriginal)
        totalBytes += source.targetOriginal->size();
    }
    if (totalBytes > maxProjectBytes)
      throw std::runtime_error("Project source buffers exceed 512 MiB; use "
                               "shorter sources or a separate project");
    for (auto &s : sources) {
      unique(s.id);
      if (s.uri.size() > 4096 || s.resourceUri.size() > 4096 ||
          s.resourceFingerprint.size() > 128 || s.name.size() > 256 ||
          s.fingerprint.size() > 128 || s.rate < 8000 || s.rate > 192000 ||
          s.channels < 1 || s.channels > 2)
        throw std::runtime_error("Invalid audio source metadata");
      if (s.pcm && s.pcm->size() * 4 > maxDecodedBytes)
        throw std::runtime_error("Decoded source exceeds 256 MiB");
    }
    for (auto &t : tracks) {
      unique(t.id);
      if (t.name.size() > 256 || !std::isfinite(t.db) || t.db < -96 ||
          t.db > 24)
        throw std::runtime_error("Invalid track parameters");
    }
    auto validTime = [&](Frame t) {
      return t >= 0 && t <= Frame(rate) * 86400;
    };
    for (auto &c : clips) {
      unique(c.id);
      if (!source(c.source) || !track(c.track) || c.group >= next ||
          !validTime(c.at) || !validTime(c.in) || !validTime(c.out) ||
          c.out <= c.in || !validTime(c.fadeIn) || !validTime(c.fadeOut) ||
          !std::isfinite(c.speed) || c.speed < .25 || c.speed > 4 ||
          !validTime(c.end()) || !std::isfinite(c.db) || c.db < -96 ||
          c.db > 24 || !std::isfinite(c.balance) || c.balance < -1 ||
          c.balance > 1 || c.fx.voice < 0 || c.fx.voice > 5 ||
          !std::isfinite(c.fx.amount) || c.fx.amount < 0 || c.fx.amount > 1 ||
          c.keys.size() > 10000)
        throw std::runtime_error("Invalid audio clip parameters");
      Frame last = -1;
      for (auto k : c.keys) {
        if (k.at < 0 || k.at <= last || k.at > c.length() ||
            !std::isfinite(k.db) || k.db < -96 || k.db > 24)
          throw std::runtime_error("Invalid audio automation");
        last = k.at;
      }
    }
    for (auto &m : markers)
      if (!validTime(m.at) || m.name.size() > 256)
        throw std::runtime_error("Invalid audio marker");
    if (!validTime(rangeIn) || !validTime(rangeOut) ||
        !std::isfinite(masterDb) || masterDb < -96 || masterDb > 12)
      throw std::runtime_error("Invalid project range or master gain");
  }
};
inline std::set<Id> expandSelection(const Project &p, std::set<Id> selected) {
  if (!p.linkage)
    return selected;
  std::set<Id> groups;
  for (auto c : p.clips)
    if (selected.count(c.id) && c.group)
      groups.insert(c.group);
  for (auto c : p.clips)
    if (c.group && groups.count(c.group))
      selected.insert(c.id);
  return selected;
}
inline void requireEditable(const Project &p, const std::set<Id> &selected) {
  for (auto c : p.clips)
    if (selected.count(c.id) && !p.editable(c))
      throw std::runtime_error("Selection includes a locked track");
}
inline void shiftKeys(Clip &c, Frame begin, Frame end) {
  std::vector<Key> keys;
  float first = c.automation(begin), last = c.automation(end);
  if (!c.keys.empty()) {
    keys.push_back({0, first});
    for (auto k : c.keys)
      if (k.at > begin && k.at < end)
        keys.push_back({k.at - begin, k.db});
    if (end > begin)
      keys.push_back({end - begin, last});
  }
  c.keys = std::move(keys);
}
inline bool split(Project &p, const std::set<Id> &selection, Frame position,
                  bool all = false) {
  if (!all)
    requireEditable(p, selection);
  std::vector<Clip> tail;
  bool changed = false;
  for (auto &c : p.clips) {
    if ((!all && !selection.count(c.id)) || !p.editable(c) ||
        position <= c.at || position >= c.end())
      continue;
    auto old = c;
    Frame local = position - c.at,
          cut = c.in + Frame(std::llround(double(local) * c.speed));
    if (cut <= c.in || cut >= c.out)
      continue;
    c.out = cut;
    c.fadeOut = 0;
    shiftKeys(c, 0, local);
    Clip right = old;
    right.id = p.next++;
    right.at = position;
    right.in = cut;
    right.fadeIn = 0;
    shiftKeys(right, local, old.length());
    tail.push_back(std::move(right));
    changed = true;
  }
  p.clips.insert(p.clips.end(), tail.begin(), tail.end());
  return changed;
}
inline void move(Project &p, const std::set<Id> &selected, Frame delta,
                 Id target = 0) {
  auto ids = expandSelection(p, selected);
  requireEditable(p, ids);
  Frame min = std::numeric_limits<Frame>::max();
  std::set<Id> oldTracks;
  for (auto c : p.clips)
    if (ids.count(c.id)) {
      min = std::min(min, c.at);
      oldTracks.insert(c.track);
    }
  if (min == std::numeric_limits<Frame>::max())
    return;
  delta = std::max(delta, -min);
  if (target) {
    auto t = p.track(target);
    if (!t || t->locked || oldTracks.size() != 1)
      throw std::runtime_error(
          "Move clips from one track to an unlocked destination");
  }
  for (auto &c : p.clips)
    if (ids.count(c.id)) {
      c.at += delta;
      if (target)
        c.track = target;
    }
}
inline void erase(Project &p, const std::set<Id> &selected,
                  bool ripple = false) {
  auto ids = expandSelection(p, selected);
  requireEditable(p, ids);
  std::map<Id, std::vector<std::pair<Frame, Frame>>> intervals;
  for (auto c : p.clips)
    if (ids.count(c.id))
      intervals[c.track].push_back({c.at, c.end()});
  p.clips.erase(std::remove_if(p.clips.begin(), p.clips.end(),
                               [&](auto &c) { return ids.count(c.id); }),
                p.clips.end());
  if (ripple)
    for (auto &item : intervals) {
      auto v = item.second;
      std::sort(v.begin(), v.end());
      std::vector<std::pair<Frame, Frame>> merged;
      for (auto span : v) {
        if (!merged.empty() && span.first <= merged.back().second)
          merged.back().second = std::max(span.second, merged.back().second);
        else
          merged.push_back(span);
      }
      for (auto &c : p.clips)
        if (c.track == item.first && p.editable(c)) {
          Frame shift = 0;
          for (auto span : merged)
            if (c.at >= span.second)
              shift += span.second - span.first;
          c.at -= shift;
        }
    }
}
inline void trim(Project &p, const std::set<Id> &ids, Frame position,
                 bool left) {
  requireEditable(p, ids);
  for (auto &c : p.clips)
    if (ids.count(c.id) && position > c.at && position < c.end()) {
      Frame local = position - c.at;
      auto old = c;
      if (left) {
        c.in += Frame(std::llround(local * c.speed));
        c.at = position;
        c.fadeIn = 0;
        shiftKeys(c, local, old.length());
      } else {
        c.out = c.in + Frame(std::llround(local * c.speed));
        c.fadeOut = 0;
        shiftKeys(c, 0, local);
      }
    }
}
inline void magnet(Project &p) {
  if (!p.magnet || p.tracks.empty() || p.tracks.front().locked)
    return;
  auto tid = p.tracks.front().id;
  std::vector<Clip *> ordered;
  for (auto &c : p.clips)
    if (c.track == tid)
      ordered.push_back(&c);
  std::sort(ordered.begin(), ordered.end(),
            [](auto a, auto b) { return a->at < b->at; });
  Frame end = 0;
  for (auto c : ordered) {
    if (c->at > end)
      move(p, {c->id}, end - c->at);
    end = std::max(end, c->end());
  }
}
struct History {
  struct State {
    Project project;
    std::string label;
  };
  std::vector<State> undo, redo;
  size_t limit = 100;
  void remember(const Project &p, std::string label) {
    undo.push_back({p, std::move(label)});
    redo.clear();
    auto bytes = [&]() {
      std::set<const void *> seen;
      size_t size = 0;
      for (const auto &state : undo)
        for (const auto &source : state.project.sources) {
          if (source.pcm && seen.insert(source.pcm.get()).second)
            size += source.pcm->size() * 4;
          if (source.original && seen.insert(source.original.get()).second)
            size += source.original->size();
          if (source.targetOriginal &&
              seen.insert(source.targetOriginal.get()).second)
            size += source.targetOriginal->size();
        }
      return size;
    };
    while (undo.size() > 1 &&
           (undo.size() > limit || bytes() > maxProjectBytes))
      undo.erase(undo.begin());
  }
  bool back(Project &p) {
    if (undo.empty())
      return false;
    redo.push_back({p, undo.back().label});
    p = std::move(undo.back().project);
    undo.pop_back();
    return true;
  }
  bool forward(Project &p) {
    if (redo.empty())
      return false;
    undo.push_back({p, redo.back().label});
    p = std::move(redo.back().project);
    redo.pop_back();
    return true;
  }
  void clear() {
    undo.clear();
    redo.clear();
  }
};
inline Json serialize(const Project &p) {
  p.validate();
  Json j = {{"kind", "reyondbroom-audio"},
            {"schemaVersion", 1},
            {"rate", p.rate},
            {"nextId", p.next},
            {"name", p.name},
            {"range", {p.rangeIn, p.rangeOut, p.range}},
            {"masterDb", p.masterDb},
            {"magnet", p.magnet},
            {"snapping", p.snapping},
            {"linkage", p.linkage},
            {"previewAxis", p.previewAxis}};
  j["sources"] = Json::array();
  j["tracks"] = Json::array();
  j["clips"] = Json::array();
  j["markers"] = Json::array();
  for (auto &s : p.sources)
    j["sources"].push_back({{"id", s.id},
                            {"uri", s.uri},
                            {"name", s.name},
                            {"fingerprint", s.fingerprint},
                            {"resourceUri", s.resourceUri},
                            {"resourceFingerprint", s.resourceFingerprint},
                            {"originalRate", s.rate},
                            {"originalChannels", s.channels}});
  for (auto &t : p.tracks)
    j["tracks"].push_back({{"id", t.id},
                           {"name", t.name},
                           {"locked", t.locked},
                           {"muted", t.muted},
                           {"solo", t.solo},
                           {"visible", t.visible},
                           {"db", t.db}});
  for (auto &c : p.clips) {
    Json keys = Json::array();
    for (auto k : c.keys)
      keys.push_back({k.at, k.db});
    j["clips"].push_back({{"id", c.id},
                          {"source", c.source},
                          {"track", c.track},
                          {"group", c.group},
                          {"at", c.at},
                          {"in", c.in},
                          {"out", c.out},
                          {"fadeIn", c.fadeIn},
                          {"fadeOut", c.fadeOut},
                          {"db", c.db},
                          {"speed", c.speed},
                          {"balance", c.balance},
                          {"pitchLinked", c.pitchLinked},
                          {"keys", keys},
                          {"fx",
                           {{"normalize", c.fx.normalize},
                            {"denoise", c.fx.denoise},
                            {"enhance", c.fx.enhance},
                            {"bypass", c.fx.bypass},
                            {"voice", c.fx.voice},
                            {"amount", c.fx.amount}}}});
  }
  for (auto m : p.markers)
    j["markers"].push_back({m.at, m.name});
  return j;
}
inline Project deserialize(const Json &j) {
  if (j.at("kind") != "reyondbroom-audio" || j.at("schemaVersion") != 1)
    throw std::runtime_error("Unsupported audio project version");
  Project p;
  p.rate = j.at("rate");
  p.next = j.at("nextId");
  p.name = j.at("name");
  p.masterDb = j.value("masterDb", 0.f);
  p.magnet = j.value("magnet", false);
  p.snapping = j.value("snapping", true);
  p.linkage = j.value("linkage", true);
  p.previewAxis = j.value("previewAxis", false);
  p.rangeIn = j.at("range").at(0);
  p.rangeOut = j.at("range").at(1);
  p.range = j.at("range").at(2);
  if (j.at("sources").size() > 256 || j.at("tracks").size() > 64 ||
      j.at("clips").size() > 4096)
    throw std::runtime_error("Audio project exceeds limits");
  for (auto &v : j.at("sources")) {
    Source s;
    s.id = v.at("id");
    s.uri = v.at("uri");
    s.name = v.at("name");
    s.fingerprint = v.at("fingerprint");
    s.resourceUri = v.value("resourceUri", std::string{});
    s.resourceFingerprint = v.value("resourceFingerprint", std::string{});
    s.rate = v.at("originalRate");
    s.channels = v.at("originalChannels");
    p.sources.push_back(std::move(s));
  }
  for (auto &v : j.at("tracks")) {
    Track t;
    t.id = v.at("id");
    t.name = v.at("name");
    t.locked = v.at("locked");
    t.muted = v.at("muted");
    t.solo = v.at("solo");
    t.visible = v.at("visible");
    t.db = v.at("db");
    p.tracks.push_back(t);
  }
  for (auto &v : j.at("clips")) {
    Clip c;
    c.id = v.at("id");
    c.source = v.at("source");
    c.track = v.at("track");
    c.group = v.value("group", Id(0));
    c.at = v.at("at");
    c.in = v.at("in");
    c.out = v.at("out");
    c.fadeIn = v.at("fadeIn");
    c.fadeOut = v.at("fadeOut");
    c.db = v.at("db");
    c.speed = v.at("speed");
    c.balance = v.at("balance");
    c.pitchLinked = v.at("pitchLinked");
    if (v.at("keys").size() > 10000)
      throw std::runtime_error("Too many automation keys");
    for (auto &k : v.at("keys"))
      c.keys.push_back({k.at(0), k.at(1)});
    auto f = v.at("fx");
    c.fx = {f.at("normalize"), f.at("denoise"), f.at("enhance"),
            f.at("bypass"),    f.at("voice"),   f.at("amount")};
    p.clips.push_back(std::move(c));
  }
  if (j.at("markers").size() > 10000)
    throw std::runtime_error("Too many markers");
  for (auto &m : j.at("markers"))
    p.markers.push_back({m.at(0), m.at(1)});
  p.validate();
  return p;
}
struct Prepared {
  Id clip = 0;
  std::shared_ptr<const PCM> pcm;
  Frame sourceStart = 0;
  std::string measuredLUFS, measuredTruePeak;
};
// A snapshot owns immutable source/DSP buffers. Preview and offline export call
// the same block renderer; no allocation or I/O occurs inside render().
struct Mixer {
  Project project;
  std::map<Id, Prepared> prepared;
  void render(Frame at, size_t count, float *output,
              float *rawPeak = nullptr) const {
    std::fill(output, output + count * 2, 0.f);
    bool solo = false;
    for (auto &t : project.tracks)
      solo |= t.solo && !t.muted;
    for (auto &c : project.clips) {
      auto t = project.track(c.track);
      if (!t || t->muted || (solo && !t->solo))
        continue;
      auto s = project.source(c.source);
      auto found = prepared.find(c.id);
      const PCM *pcm = nullptr;
      Frame start = c.in;
      if (found != prepared.end()) {
        pcm = found->second.pcm.get();
        start = found->second.sourceStart;
      } else if (s)
        pcm = s->pcm.get();
      if (!pcm)
        continue;
      Frame begin = std::max(at, c.at),
            end = std::min(at + Frame(count), c.end());
      if (begin >= end)
        continue;
      float base = gain(c.db + t->db + project.masterDb);
      for (Frame f = begin; f < end; f++) {
        Frame local = f - c.at, pos = local + start;
        if (pos < 0 || pos >= Frame(pcm->size() / 2))
          continue;
        float volume = base * gain(c.automation(local));
        if (c.fadeIn > 0)
          volume *= float(std::min(1., double(local) / c.fadeIn));
        if (c.fadeOut > 0)
          volume *=
              float(std::min(1., double(c.length() - 1 - local) / c.fadeOut));
        for (int ch = 0; ch < 2; ch++) {
          float balance = ch == 0 ? 1.f - std::max(0.f, c.balance)
                                  : 1.f + std::min(0.f, c.balance);
          output[(f - at) * 2 + ch] += (*pcm)[pos * 2 + ch] * volume * balance;
        }
      }
    }
    if (rawPeak)
      *rawPeak = 0;
    for (size_t i = 0; i < count * 2; i++) {
      if (rawPeak)
        *rawPeak = std::max(*rawPeak, std::abs(output[i]));
      output[i] = std::clamp(output[i], -1.f, 1.f);
    }
  }
};
} // namespace ftebroom::audio
