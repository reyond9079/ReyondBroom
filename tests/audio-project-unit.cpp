// SPDX-License-Identifier: GPL-2.0-or-later
#include "../native/ftebroom_audio_project.h"
#include <cassert>
#include <iostream>
using namespace ftebroom::audio;
int main() {
  Project p;
  Source s;
  s.name = "tone";
  s.uri = "tone.wav";
  s.pcm = std::make_shared<PCM>(2000, .25f);
  auto sid = p.addSource(s), t = p.addTrack("One"), t2 = p.addTrack("Two"),
       id = p.addClip(sid, t);
  p.validate();
  Mixer m;
  m.project = p;
  float before[2000];
  m.render(0, 1000, before);
  assert(split(p, {id}, 500));
  m.project = p;
  float after[2000];
  m.render(0, 1000, after);
  assert(std::equal(before, before + 2000, after));
  move(p, {id}, 100, t2);
  assert(p.clip(id)->at == 100 && p.clip(id)->track == t2);
  History h;
  h.remember(p, "Delete");
  erase(p, {id});
  assert(p.clips.size() == 1);
  assert(h.back(p) && p.clips.size() == 2);
  assert(h.forward(p) && p.clips.size() == 1);
  p = deserialize(serialize(p));
  p.validate();
  assert(p.tracks.size() == 2);
  auto bad = serialize(p);
  bad["clips"][0]["speed"] = 0;
  bool rejected = false;
  try {
    deserialize(bad);
  } catch (...) {
    rejected = true;
  }
  assert(rejected);
  p.sources.front().pcm = std::make_shared<PCM>(2000, .25f);
  p.clips.clear();
  auto a = p.addClip(sid, t), b = p.addClip(sid, t2);
  p.track(t)->muted = true;
  m.project = p;
  m.render(0, 1000, after);
  assert(std::abs(after[100] - .25f) < 1e-6);
  p.track(t)->muted = false;
  p.track(t2)->solo = true;
  m.project = p;
  m.render(0, 1000, after);
  assert(std::abs(after[100] - .25f) < 1e-6);
  p.track(t2)->solo = false;
  p.clip(a)->fadeIn = 100;
  p.clip(a)->fadeOut = 100;
  p.track(t2)->muted = true;
  m.project = p;
  m.render(0, 1000, after);
  assert(after[0] == 0 && after[1998] == 0);
  p.track(t)->locked = true;
  rejected = false;
  try {
    move(p, {a}, 10);
  } catch (...) {
    rejected = true;
  }
  assert(rejected);
  std::cout << "PASS split identity, move, history, JSON validation, "
               "mute/solo, fades and locked-track protection\n";
}
