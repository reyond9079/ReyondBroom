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

  // Grouped moves are atomic with respect to locks, and clamps preserve
  // offsets.
  p.track(t)->locked = false;
  p.track(t2)->muted = false;
  p.clips.clear();
  a = p.addClip(sid, t, 100);
  b = p.addClip(sid, t2, 300);
  auto group = p.next++;
  p.clip(a)->group = p.clip(b)->group = group;
  move(p, {a}, -200);
  assert(p.clip(a)->at == 0 && p.clip(b)->at == 200);
  p.track(t2)->locked = true;
  auto lockedState = serialize(p);
  rejected = false;
  try {
    move(p, {a}, 20);
  } catch (...) {
    rejected = true;
  }
  assert(rejected && serialize(p) == lockedState);
  p.track(t2)->locked = false;
  p.linkage = false;
  move(p, {a}, 20);
  assert(p.clip(b)->at == 200);
  // Ripple closes the union of overlapping deleted spans only on their track.
  p.clips.clear();
  p.linkage = true;
  a = p.addClip(sid, t, 0);
  p.clip(a)->out = 100;
  b = p.addClip(sid, t, 50);
  p.clip(b)->out = 100;
  auto later = p.addClip(sid, t, 200), other = p.addClip(sid, t2, 200);
  erase(p, {a, b}, true);
  assert(p.clip(later)->at == 50 && p.clip(other)->at == 200);
  p.magnet = true;
  magnet(p);
  assert(p.clip(later)->at == 0 && p.clip(other)->at == 200);
  p.magnet = false;
  p.clip(later)->keys = {{0, -12}, {100, 0}, {1000, -6}};
  p.clip(later)->out = 1000;
  auto automation = p.clip(later)->automation(50);
  assert(std::abs(automation + 6) < 1e-6);
  trim(p, {later}, 100, true);
  assert(p.clip(later)->in == 100 && p.clip(later)->at == 100 &&
         p.clip(later)->keys.front().at == 0);
  // Visibility never removes audio; clipping is shared by preview/export and
  // leaves all in-range samples unchanged at unity gain.
  p.clips.clear();
  a = p.addClip(sid, t);
  p.sources[0].pcm = std::make_shared<PCM>(2000, .95f);
  p.track(t)->visible = false;
  m.project = p;
  m.render(0, 1000, after);
  assert(after[100] == .95f);
  b = p.addClip(sid, t2);
  m.project = p;
  float peak = 0;
  m.render(0, 1000, after, &peak);
  assert(after[100] == 1 && peak > 1.8f);
  std::cout << "PASS grouped lock/linkage, ripple interval union, magnet, dB "
               "automation, trim, visibility and output overload protection\n";
  std::cout << "PASS split identity, move, history, JSON validation, "
               "mute/solo, fades and locked-track protection\n";
}
