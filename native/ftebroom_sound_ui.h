#include "ftebroom_sound_widgets.h"
// SPDX-License-Identifier: GPL-2.0-or-later
static bool soundButton(const char *label, const char *help = nullptr,
                        ImVec2 size = {}) {
  bool b = ImGui::Button(label, size);
  if (help && ImGui::IsItemHovered())
    ImGui::SetTooltip("%s", help);
  return b;
}
struct SoundDisabled {
  explicit SoundDisabled(bool disabled = true) {
    ImGui::BeginDisabled(disabled);
  }
  ~SoundDisabled() { ImGui::EndDisabled(); }
};
static void soundDialog(const std::string &name) {
  soundState.pendingDialog = name;
}
static void soundPathDialog(const std::string &purpose) {
  auto &s = soundState;
  s.pickerPurpose = purpose;
  s.browseEntries.clear();
  auto roots = soundRoots();
  s.browseFolder = roots.empty() ? settingsPath().parent_path() : roots.front();
  std::snprintf(s.path, sizeof s.path, "%s", s.browseFolder.u8string().c_str());
  soundDialog("Audio file browser");
}
static void soundRequest(const std::string &action) {
  auto &s = soundState;
  if (soundBusy() || s.capture.active() || s.recordAt >= 0) {
    soundMessage = "Finish or cancel the audio operation first.";
    return;
  }
  if (s.dirty) {
    s.closing = action == "close";
    s.pendingDialog = "Unsaved audio project";
    s.pickerPurpose = action;
  } else if (action == "new") {
    soundStop();
    s.project = {};
    s.history.clear();
    s.selected.clear();
    s.clipboard.clear();
    s.source = s.track = 0;
    s.cursor = 0;
    s.projectPath.clear();
    s.revision++;
    soundRefreshMixer();
  } else if (action == "close") {
    soundStop();
    soundEditorOpen = false;
  } else
    soundPathDialog("open");
}
static void soundExecute(const std::string &id) {
  auto &s = soundState;
  auto &p = s.project;
  try {
    if (id == "transport.play") {
      if (soundPlaying)
        soundStop();
      else {
        auto start = p.range ? std::min(p.rangeIn, p.rangeOut) : 0;
        auto end = p.range ? std::max(p.rangeIn, p.rangeOut) : p.duration();
        if (s.cursor < start || s.cursor >= end)
          s.cursor = start;
        soundSeek(double(s.cursor) / p.rate, true);
      }
      return;
    }
    if (id == "transport.stop") {
      soundStop();
      s.cursor = p.range ? std::min(p.rangeIn, p.rangeOut) : 0;
      return;
    }
    if (id == "transport.left" || id == "transport.right" ||
        id == "transport.leftLarge" || id == "transport.rightLarge") {
      auto step =
          rbAudio::Frame(p.rate * (id.find("Large") != id.npos ? .1 : .01));
      auto sign = id.find("right") != id.npos ? 1 : -1;
      soundSeek(double(std::clamp(s.cursor + sign * step, rbAudio::Frame(0),
                                  p.duration())) /
                    p.rate,
                false);
      return;
    }
    if (id == "transport.home" || id == "transport.end") {
      soundSeek(id == "transport.home" ? 0 : double(p.duration()) / p.rate,
                false);
      return;
    }
    if (id == "transport.previousCut" || id == "transport.nextCut") {
      std::set<rbAudio::Frame> cuts{0, p.duration()};
      for (auto &c : p.clips) {
        cuts.insert(c.at);
        cuts.insert(c.end());
      }
      auto next = cuts.upper_bound(s.cursor);
      rbAudio::Frame at = p.duration();
      if (id == "transport.previousCut") {
        next = cuts.lower_bound(s.cursor);
        at = next == cuts.begin() ? 0 : *--next;
      } else if (next != cuts.end())
        at = *next;
      soundSeek(double(at) / p.rate, false);
      return;
    }
    if (id == "tool.select" || id == "tool.split") {
      soundTool = id == "tool.split" ? 2 : 0;
      return;
    }
    if (id == "edit.undo" || id == "edit.redo") {
      soundHistory(id == "edit.redo");
      return;
    }
    if (id == "edit.cancel") {
      s.selected.clear();
      return;
    }
    if (id == "edit.selectAll") {
      s.selected.clear();
      for (auto c : p.clips)
        if (p.editable(c))
          s.selected.insert(c.id);
      return;
    }
    if (id == "edit.copy" || id == "edit.cut" || id == "edit.duplicate") {
      s.clipboard.clear();
      auto copied = rbAudio::expandSelection(p, s.selected);
      for (auto c : p.clips)
        if (copied.count(c.id))
          s.clipboard.push_back(c);
      if (id == "edit.copy")
        return;
      if (id == "edit.cut") {
        soundApply("Cut clips", [&] {
          rbAudio::erase(p, s.selected);
          rbAudio::magnet(p);
        });
        s.selected.clear();
        return;
      }
    }
    if (id == "edit.paste" || id == "edit.duplicate") {
      if (s.clipboard.empty())
        return;
      auto selected = rbAudio::expandSelection(p, s.selected);
      if (id == "edit.duplicate")
        rbAudio::requireEditable(p, selected);
      soundApply("Paste clips", [&] {
        rbAudio::Frame origin = s.clipboard.front().at, end = 0;
        std::set<rbAudio::Id> tracks;
        for (auto c : s.clipboard) {
          origin = std::min(origin, c.at);
          end = std::max(end, c.end());
          tracks.insert(c.track);
        }
        auto target = p.track(s.track);
        if (id == "edit.paste" && tracks.size() == 1 &&
            (!target || target->locked))
          throw std::runtime_error("Choose an unlocked destination track");
        rbAudio::Frame at = id == "edit.duplicate" ? end : s.cursor;
        std::map<rbAudio::Id, rbAudio::Id> groups;
        s.selected.clear();
        for (auto c : s.clipboard) {
          if (p.clips.size() >= 4096)
            throw std::runtime_error("Maximum 4096 clips");
          if (id == "edit.paste" && tracks.size() == 1)
            c.track = s.track;
          auto t = p.track(c.track);
          if (!t || t->locked)
            throw std::runtime_error("Paste destination is locked or missing");
          c.id = p.next++;
          c.at = at + c.at - origin;
          if (c.group) {
            if (!groups.count(c.group))
              groups[c.group] = p.next++;
            c.group = groups[c.group];
          }
          p.clips.push_back(c);
          s.selected.insert(c.id);
        }
      });
      return;
    }
    if (id == "clip.split" || id == "clip.splitAll") {
      soundApply("Split clips", [&] {
        rbAudio::split(p, s.selected, s.cursor, id == "clip.splitAll");
      });
      return;
    }
    if (id == "clip.trimLeft" || id == "clip.trimRight") {
      soundApply("Trim clips", [&] {
        rbAudio::trim(p, s.selected, s.cursor, id == "clip.trimLeft");
        rbAudio::magnet(p);
      });
      return;
    }
    if (id == "clip.delete" || id == "clip.rippleDelete") {
      soundApply("Delete clips", [&] {
        rbAudio::erase(p, s.selected, id == "clip.rippleDelete");
        rbAudio::magnet(p);
      });
      s.selected.clear();
      return;
    }
    if (id == "clip.group" || id == "clip.ungroup") {
      soundApply("Group clips", [&] {
        rbAudio::requireEditable(p, s.selected);
        auto group = id == "clip.group" ? p.next++ : 0;
        for (auto &c : p.clips)
          if (s.selected.count(c.id))
            c.group = group;
      });
      return;
    }
    if (id == "range.in" || id == "range.out" || id == "range.clear" ||
        id == "range.select") {
      soundApply("Export range", [&] {
        if (id == "range.clear")
          p.range = false;
        else {
          if (id == "range.in")
            p.rangeIn = s.cursor;
          else if (id == "range.out")
            p.rangeOut = s.cursor;
          else {
            p.rangeIn = p.duration();
            p.rangeOut = 0;
            for (auto c : p.clips)
              if (s.selected.count(c.id)) {
                p.rangeIn = std::min(p.rangeIn, c.at);
                p.rangeOut = std::max(p.rangeOut, c.end());
              }
            if (p.rangeOut <= p.rangeIn) {
              p.rangeIn = 0;
              p.rangeOut = p.duration();
            }
          }
          p.range = true;
        }
      });
      return;
    }
    if (id == "marker.add") {
      soundApply("Add marker", [&] {
        p.markers.push_back(
            {s.cursor, "Marker " + std::to_string(p.markers.size() + 1)});
      });
      return;
    }
    if (id == "timeline.magnet" || id == "timeline.snap" ||
        id == "timeline.linkage" || id == "timeline.previewAxis") {
      soundApply("Timeline option", [&] {
        if (id == "timeline.magnet")
          p.magnet = !p.magnet;
        if (id == "timeline.snap")
          p.snapping = !p.snapping;
        if (id == "timeline.linkage")
          p.linkage = !p.linkage;
        if (id == "timeline.previewAxis")
          p.previewAxis = !p.previewAxis;
      });
      return;
    }
    if (id == "timeline.zoomIn" || id == "timeline.zoomOut" ||
        id == "timeline.fit") {
      if (id == "timeline.fit") {
        s.zoom = p.duration() > 0
                     ? std::clamp(
                           float(std::max(30., double(p.duration()) / p.rate) /
                                 std::max(.03, double(p.duration()) / p.rate)),
                           .25f, 1000.f)
                     : 1;
        s.viewStart = 0;
      } else
        s.zoom = std::clamp(s.zoom * (id == "timeline.zoomIn" ? 1.25f : .8f),
                            .25f, 1000.f);
      return;
    }
    if (id == "project.new" || id == "project.open") {
      soundRequest(id == "project.new" ? "new" : "open");
      return;
    }
    if (id == "project.save" || id == "project.saveAs") {
      if (id == "project.save" && !s.projectPath.empty())
        soundProjectSave(s.projectPath);
      else
        soundPathDialog("save");
      return;
    }
    if (id == "project.import") {
      soundPathDialog("import");
      return;
    }
    if (id == "export.open") {
      s.exportRange = p.range;
      soundDialog("Export audio");
      return;
    }
    if (id == "resource.update") {
      soundDialog("Update source with backup");
      return;
    }
    if (id == "record.open") {
      s.devices = s.capture.devices();
      s.recordDevice = 0;
      soundDialog("Record audio");
      return;
    }
    if (id == "shortcuts.open") {
      soundBindings();
      soundDialog("Keyboard shortcuts");
      return;
    }
    if (id == "workspace.layout") {
      soundDialog("Audio workspace");
      return;
    }
    if (id == "track.add") {
      soundApply("Add track", [&] {
        s.track = p.addTrack("Audio " + std::to_string(p.tracks.size() + 1));
      });
      return;
    }
    if (id == "track.delete") {
      soundDialog("Delete audio track");
      return;
    }
    if (id == "track.rename") {
      if (auto t = p.track(s.track)) {
        std::snprintf(s.rename, sizeof s.rename, "%s", t->name.c_str());
        soundDialog("Rename audio track");
      }
      return;
    }
    if (id == "track.lock" || id == "track.mute" || id == "track.solo" ||
        id == "track.visibility") {
      soundApply("Track state", [&] {
        if (auto t = p.track(s.track)) {
          if (id == "track.lock")
            t->locked = !t->locked;
          if (id == "track.mute")
            t->muted = !t->muted;
          if (id == "track.solo")
            t->solo = !t->solo;
          if (id == "track.visibility")
            t->visible = !t->visible;
        }
      });
      return;
    }
    if (id == "source.relink") {
      soundPathDialog("relink");
      return;
    }
    if (id == "clip.keyframe") {
      soundApply("Gain keyframe", [&] {
        rbAudio::requireEditable(p, s.selected);
        for (auto &c : p.clips)
          if (s.selected.count(c.id)) {
            auto at =
                std::clamp(s.cursor - c.at, rbAudio::Frame(0), c.length());
            auto k = std::find_if(c.keys.begin(), c.keys.end(),
                                  [&](auto k) { return k.at == at; });
            if (k != c.keys.end())
              c.keys.erase(k);
            else
              c.keys.push_back({at, 0});
            std::sort(c.keys.begin(), c.keys.end(),
                      [](auto a, auto b) { return a.at < b.at; });
          }
      });
      return;
    }
    if (id == "clip.reset") {
      soundApply("Reset clip", [&] {
        rbAudio::requireEditable(p, s.selected);
        for (auto &c : p.clips)
          if (s.selected.count(c.id)) {
            c.db = c.balance = 0;
            c.speed = 1;
            c.fadeIn = c.fadeOut = 0;
            c.keys.clear();
            c.fx = {};
          }
      });
      return;
    }
    if (id == "clip.normalize" || id == "clip.denoise" ||
        id == "clip.enhance" || id == "clip.pitchLink") {
      soundApply("Toggle clip processing", [&] {
        rbAudio::requireEditable(p, s.selected);
        auto selected = soundSelected();
        if (!selected)
          return;
        bool value = id == "clip.normalize" ? !selected->fx.normalize
                     : id == "clip.denoise" ? !selected->fx.denoise
                     : id == "clip.enhance" ? !selected->fx.enhance
                                            : !selected->pitchLinked;
        for (auto &clip : p.clips)
          if (s.selected.count(clip.id)) {
            if (id == "clip.normalize")
              clip.fx.normalize = value;
            else if (id == "clip.denoise")
              clip.fx.denoise = value;
            else if (id == "clip.enhance")
              clip.fx.enhance = value;
            else
              clip.pitchLinked = value;
          }
      });
      return;
    }
    if (id == "clip.speed" || id == "clip.voice" || id == "clip.gain" ||
        id == "clip.fadeIn" || id == "clip.fadeOut" || id == "clip.normalize" ||
        id == "clip.denoise" || id == "clip.enhance" || id == "clip.balance" ||
        id == "clip.pitchLink" || id == "master.gain") {
      s.focusField = id;
      s.tab = id == "clip.speed" || id == "clip.pitchLink" ? 2
              : id == "clip.voice"                         ? 1
                                                           : 0;
      return;
    }
  } catch (const std::exception &e) {
    soundMessage = e.what();
  }
}
static ImU32 soundColor(ImVec4 color) { return ImGui::GetColorU32(color); }
static void soundWaveDraw(ImDrawList *draw, const ImRect &bounds,
                          const rbAudio::Source *source,
                          const rbAudio::Clip *clip = nullptr) {
  if (!source || source->peaks.empty())
    return;
  float start = clip ? float(clip->in) /
                           std::max(rbAudio::Frame(1), source->length())
                     : 0,
        end = clip ? float(clip->out) /
                         std::max(rbAudio::Frame(1), source->length())
                   : 1;
  int bars = std::max(1, std::min(1000, int(bounds.GetWidth() / 2)));
  float amp = clip ? std::min(3.f, rbAudio::gain(clip->db)) : 1;
  draw->PushClipRect(bounds.Min, bounds.Max, true);
  for (int i = 0; i < bars; i++) {
    float ratio = float(i) / bars;
    size_t index =
        std::clamp(size_t(std::max(0.f, start + (end - start) * ratio) *
                          source->peaks.size()),
                   size_t(0), source->peaks.size() - 1);
    auto peak = source->peaks[index];
    float x = bounds.Min.x + ratio * bounds.GetWidth(),
          mid = (bounds.Min.y + bounds.Max.y) * .5f;
    draw->AddLine({x, mid - peak.hi * amp * bounds.GetHeight() * .46f},
                  {x, mid - peak.lo * amp * bounds.GetHeight() * .46f},
                  clip ? IM_COL32(120, 224, 224, 255)
                       : ImGui::GetColorU32(ImGuiCol_CheckMark));
  }
  draw->PopClipRect();
}
static void soundDirtyField(const rbAudio::Project &before, bool changed,
                            const char *label) {
  auto &s = soundState;
  if (ImGui::IsItemActivated())
    s.history.remember(before, label);
  if (changed) {
    try {
      s.project.validate();
      s.dirty = true;
      s.revision++;
      soundRefreshMixer();
      if (!s.ready && soundPlaying) {
        soundStop();
        s.autoplay = true;
      }
    } catch (const std::exception &e) {
      s.project = before;
      soundMessage = e.what();
    }
  }
}
static void soundFocusField(const char *id) {
  auto &focus = soundState.focusField;
  if (focus == id) {
    ImGui::SetKeyboardFocusHere();
    focus.clear();
  }
}
static void soundInspector() {
  auto &s = soundState;
  auto &p = s.project;
  ImGui::BeginChild("Audio inspector", {0, 0}, ImGuiChildFlags_Borders);
  for (int i = 0; i < 3; i++) {
    if (i)
      ImGui::SameLine();
    if (ImGui::Selectable(
            i == 0   ? "Basic"
            : i == 1 ? "Voice changer"
                     : "Speed",
            s.tab == i, 0,
            {(ImGui::GetContentRegionAvail().x) / float(3 - i) - 5,
             ImGui::GetFrameHeight()}))
      s.tab = i;
  }
  ImGui::Separator();
  ImGui::BeginChild("Inspector scroll");
  auto c = soundSelected();
  if (!c) {
    ImGui::TextWrapped("Select a clip on the timeline to edit its audio.");
    ImGui::EndChild();
    ImGui::EndChild();
    return;
  }
  auto source = p.source(c->source);
  ImGui::TextWrapped("%s", source ? source->name.c_str() : "Missing source");
  if (s.selected.size() > 1)
    ImGui::Text("%zu selected clips", s.selected.size());
  ImGui::Spacing();
  bool locked = false;
  for (auto id : s.selected)
    if (auto clip = p.clip(id))
      locked |= !p.editable(*clip);
  {
    SoundDisabled disabled(locked || soundBusy());
    if (s.tab == 0) {
      auto before = p;
      float value = c->db;
      soundFocusField("clip.gain");
      bool changed = soundFloatRow("Volume", value, .1f, -96, 24, "%.1f dB");
      if (changed)
        for (auto &v : p.clips)
          if (s.selected.count(v.id))
            v.db = value;
      soundDirtyField(before, changed, "Clip volume");
      before = p;
      value = c->db;
      ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x -
                              38 * uiScale());
      changed = ImGui::SliderFloat("##clipgain", &value, -96, 24, "");
      soundVolumeMin = ImGui::GetItemRectMin();
      soundVolumeMax = ImGui::GetItemRectMax();
      soundVolume = rbAudio::gain(value);
      if (changed)
        for (auto &clip : p.clips)
          if (s.selected.count(clip.id))
            clip.db = value;
      soundDirtyField(before, changed, "Clip volume");
      ImGui::SameLine();
      if (soundIcon("clip.keyframe", "Add volume keyframe", false, 24))
        soundExecute("clip.keyframe");
      for (bool out : {false, true}) {
        before = p;
        float sec = float(out ? c->fadeOut : c->fadeIn) / p.rate;
        soundFocusField(out ? "clip.fadeOut" : "clip.fadeIn");
        changed = soundFloatRow(out ? "Fade out" : "Fade in", sec, .01f, 0,
                                float(c->length()) / p.rate, "%.2f s");
        if (changed)
          for (auto &v : p.clips)
            if (s.selected.count(v.id))
              (out ? v.fadeOut : v.fadeIn) = std::min(
                  v.length(), rbAudio::frames(std::max(0.f, sec), p.rate));
        soundDirtyField(before, changed, "Fade");
      }
      ImGui::Separator();
      before = p;
      bool normalize = c->fx.normalize;
      changed = ImGui::Checkbox("Normalize loudness", &normalize);
      if (changed)
        for (auto &v : p.clips)
          if (s.selected.count(v.id))
            v.fx.normalize = normalize;
      soundDirtyField(before, changed, "Normalize loudness");
      ImGui::TextDisabled("Target -16 LUFS / -1 dBTP");
      auto processed = s.mixer.prepared.find(c->id);
      if (c->fx.normalize && processed != s.mixer.prepared.end() &&
          !processed->second.measuredLUFS.empty())
        ImGui::TextDisabled("Input %s LUFS / %s dBTP",
                            processed->second.measuredLUFS.c_str(),
                            processed->second.measuredTruePeak.c_str());
      before = p;
      bool enhance = c->fx.enhance;
      changed = ImGui::Checkbox("Enhance voice", &enhance);
      if (changed)
        for (auto &v : p.clips)
          if (s.selected.count(v.id))
            v.fx.enhance = enhance;
      soundDirtyField(before, changed, "Enhance voice");
      before = p;
      bool denoise = c->fx.denoise;
      changed = ImGui::Checkbox("Reduce noise", &denoise);
      if (changed)
        for (auto &v : p.clips)
          if (s.selected.count(v.id))
            v.fx.denoise = denoise;
      soundDirtyField(before, changed, "Reduce noise");
      ImGui::Separator();
      before = p;
      float balance = c->balance;
      soundFocusField("clip.balance");
      changed = soundFloatRow("Channel balance", balance, .01f, -1, 1, "%.2f");
      if (changed)
        for (auto &v : p.clips)
          if (s.selected.count(v.id))
            v.balance = balance;
      soundDirtyField(before, changed, "Channel balance");
      if (!c->keys.empty()) {
        ImGui::SeparatorText("Volume keyframes");
        ImGui::TextDisabled("Linear interpolation in dB");
        for (size_t i = 0; i < c->keys.size(); i++) {
          ImGui::PushID(int(i));
          before = p;
          float at = float(c->keys[i].at) / p.rate, db = c->keys[i].db;
          ImGui::SetNextItemWidth(80 * uiScale());
          changed =
              ImGui::DragFloat("s", &at, .01f, 0, float(c->length()) / p.rate,
                               "%.3f", ImGuiSliderFlags_AlwaysClamp);
          ImGui::SameLine();
          ImGui::SetNextItemWidth(80 * uiScale());
          changed |= ImGui::DragFloat("dB", &db, .1f, -96, 24, "%.1f",
                                      ImGuiSliderFlags_AlwaysClamp);
          if (changed) {
            at = std::isfinite(at)
                     ? std::clamp(at, 0.f, float(c->length()) / p.rate)
                     : 0.f;
            db = std::isfinite(db) ? std::clamp(db, -96.f, 24.f) : 0.f;
            c->keys[i] = {rbAudio::frames(at, p.rate), db};
            std::sort(c->keys.begin(), c->keys.end(),
                      [](auto a, auto b) { return a.at < b.at; });
            c->keys.erase(
                std::unique(c->keys.begin(), c->keys.end(),
                            [](auto a, auto b) { return a.at == b.at; }),
                c->keys.end());
            soundChanged(before, "Edit keyframe");
          }
          ImGui::SameLine();
          if (ImGui::SmallButton("X")) {
            auto old = p;
            c->keys.erase(c->keys.begin() + i);
            soundChanged(old, "Remove keyframe");
            ImGui::PopID();
            break;
          }
          ImGui::PopID();
        }
      }
    } else if (s.tab == 1) {
      ImGui::TextUnformatted("Voice filters (local DSP)");
      const char *presets[] = {"Original", "Low pitch", "High pitch",
                               "Radio",    "Echo",      "Tremolo"};
      for (int i = 0; i < 6; i++) {
        ImGui::PushID(i);
        if (i % 3)
          ImGui::SameLine();
        if (ImGui::Selectable(
                presets[i], c->fx.voice == i, 0,
                {(ImGui::GetContentRegionAvail().x) / float(3 - i % 3) - 5,
                 50 * uiScale()}))
          soundApply("Voice filter", [&] {
            for (auto &v : p.clips)
              if (s.selected.count(v.id))
                v.fx.voice = i;
          });
        ImGui::PopID();
      }
      auto before = p;
      float amount = c->fx.amount;
      bool changed = false, adjustable = false;
      for (const auto &clip : p.clips)
        if (s.selected.count(clip.id))
          adjustable |=
              clip.fx.denoise || clip.fx.voice == 4 || clip.fx.voice == 5;
      {
        SoundDisabled amountDisabled(!adjustable);
        changed = ImGui::SliderFloat("Intensity", &amount, 0, 1);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
          ImGui::SetTooltip(
              "Intensity applies to Reduce noise, Echo and Tremolo.");
        if (changed)
          for (auto &clip : p.clips)
            if (s.selected.count(clip.id))
              clip.fx.amount = amount;
        soundDirtyField(before, changed, "Effect intensity");
      }
      before = p;
      bool bypass = c->fx.bypass;
      changed = ImGui::Checkbox("Bypass effects", &bypass);
      if (changed)
        for (auto &v : p.clips)
          if (s.selected.count(v.id))
            v.fx.bypass = bypass;
      soundDirtyField(before, changed, "Bypass effects");
    } else {
      auto before = p;
      float speed = c->speed;
      soundFocusField("clip.speed");
      bool changed = soundFloatRow("Times", speed, .01f, .25f, 4, "%.2fx");
      if (changed)
        for (auto &v : p.clips)
          if (s.selected.count(v.id)) {
            auto previousLength = v.length();
            v.speed = speed;
            for (auto &k : v.keys)
              k.at =
                  std::clamp(rbAudio::Frame(std::llround(
                                 double(k.at) * v.length() /
                                 std::max(rbAudio::Frame(1), previousLength))),
                             rbAudio::Frame(0), v.length());
            v.fadeIn =
                rbAudio::Frame(double(v.fadeIn) * v.length() /
                               std::max(rbAudio::Frame(1), previousLength));
            v.fadeOut =
                rbAudio::Frame(double(v.fadeOut) * v.length() /
                               std::max(rbAudio::Frame(1), previousLength));
            v.keys.erase(
                std::unique(v.keys.begin(), v.keys.end(),
                            [](auto a, auto b) { return a.at == b.at; }),
                v.keys.end());
          }
      soundDirtyField(before, changed, "Clip speed");
      ImGui::Text("Duration %.3f s", double(c->length()) / p.rate);
      before = p;
      bool linked = c->pitchLinked;
      changed = ImGui::Checkbox("Voice tone change", &linked);
      if (changed)
        for (auto &v : p.clips)
          if (s.selected.count(v.id))
            v.pitchLinked = linked;
      soundDirtyField(before, changed, "Pitch mode");
      ImGui::TextWrapped(
          "Off preserves pitch. On links pitch and speed (legacy mode).");
    }
    ImGui::Spacing();
    if (ImGui::Button("Reset clip"))
      soundExecute("clip.reset");
  }
  if (locked)
    ImGui::TextWrapped("This selection includes a locked track.");
  if (source && !source->pcm) {
    ImGui::TextWrapped("Source missing or changed.");
    if (ImGui::Button("Relink source...")) {
      s.source = source->id;
      soundExecute("source.relink");
    }
  }
  ImGui::EndChild();
  ImGui::EndChild();
}
static void soundTimeline() {
  auto &s = soundState;
  auto &p = s.project;
  const float scale = uiScale();
  ImGui::BeginChild("Audio timeline", {0, 0}, ImGuiChildFlags_Borders);
  s.timelineFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
  const std::pair<const char *, const char *> tools[] = {
      {"Select", "tool.select"},      {"Undo", "edit.undo"},
      {"Redo", "edit.redo"},          {"Split", "clip.split"},
      {"Trim left", "clip.trimLeft"}, {"Trim right", "clip.trimRight"},
      {"Delete", "clip.delete"},      {"Marker", "marker.add"}};
  for (int i = 0; i < 8; i++) {
    if (i)
      ImGui::SameLine(0, 5 * scale);
    if (soundIcon(tools[i].second, tools[i].first, i == 0 && soundTool == 0))
      soundExecute(tools[i].second);
    if (i == 0 && ImGui::BeginPopupContextItem("Audio tool")) {
      if (ImGui::MenuItem("Select", "A", soundTool == 0))
        soundExecute("tool.select");
      if (ImGui::MenuItem("Split", "B", soundTool == 2))
        soundExecute("tool.split");
      ImGui::EndPopup();
    }
  }
  ImGui::SameLine();
  float rightWidth = 330 * scale;
  if (ImGui::GetContentRegionAvail().x > rightWidth)
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                         ImGui::GetContentRegionAvail().x - rightWidth);
  if (soundIcon("record.open", "Record audio"))
    soundExecute("record.open");
  int option = 0;
  for (auto a : {std::make_pair("Magnet", "timeline.magnet"),
                 {"Snapping", "timeline.snap"},
                 {"Linkage", "timeline.linkage"},
                 {"Preview axis", "timeline.previewAxis"}}) {
    ImGui::SameLine(0, 5 * scale);
    bool selected = option == 0   ? p.magnet
                    : option == 1 ? p.snapping
                    : option == 2 ? p.linkage
                                  : p.previewAxis;
    if (soundIcon(a.second, a.first, selected))
      soundExecute(a.second);
    option++;
  }
  ImGui::SameLine(0, 10 * scale);
  if (soundIcon("timeline.fit", "Fit project"))
    soundExecute("timeline.fit");
  ImGui::SameLine(0, 5 * scale);
  if (soundIcon("timeline.zoomOut", "Zoom out"))
    soundExecute("timeline.zoomOut");
  ImGui::SameLine(0, 5 * scale);
  ImGui::SetNextItemWidth(65 * scale);
  ImGui::SliderFloat("##timelinezoom", &s.zoom, .25f, 20.f, "",
                     ImGuiSliderFlags_Logarithmic);
  ImGui::SameLine(0, 5 * scale);
  if (soundIcon("timeline.zoomIn", "Zoom in"))
    soundExecute("timeline.zoomIn");
  ImGui::Separator();
  ImGui::BeginChild("Timeline scroll", {0, -25 * scale}, ImGuiChildFlags_None,
                    ImGuiWindowFlags_HorizontalScrollbar);
  s.timelineFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) ||
                      ImGui::IsWindowFocused();
  const float header = 180 * scale, row = 88 * scale;
  auto start = ImGui::GetCursorScreenPos();
  float width = std::max(200.f, ImGui::GetContentRegionAvail().x - header);
  double seconds = std::max(30., double(p.duration()) / p.rate);
  s.pixelsPerSecond = float(width / seconds) * s.zoom;
  float canvas = std::max(width, float(seconds) * s.pixelsPerSecond);
  s.timelineMin = {start.x + header, start.y};
  s.timelineMax = {s.timelineMin.x + canvas,
                   start.y + 35 * scale + row * p.tracks.size()};
  auto draw = ImGui::GetWindowDrawList();
  auto toX = [&](rbAudio::Frame at) {
    return s.timelineMin.x + float(at) / p.rate * s.pixelsPerSecond;
  };
  auto toFrame = [&](float x) {
    return rbAudio::frames(
        std::max(0., std::min(86400.,
                              double(x - s.timelineMin.x) / s.pixelsPerSecond)),
        p.rate);
  };
  ImGui::Dummy({header, 30 * scale});
  draw->AddText({start.x + 5, start.y + 5},
                ImGui::GetColorU32(ImGuiCol_TextDisabled), "TRACKS");
  int step = std::max(1, int(std::ceil(70 * scale / s.pixelsPerSecond)));
  for (int second = 0; second <= seconds; second += step) {
    float x = toX(rbAudio::Frame(second) * p.rate);
    if (x < ImGui::GetWindowPos().x ||
        x > ImGui::GetWindowPos().x + ImGui::GetWindowSize().x)
      continue;
    char label[32];
    std::snprintf(label, sizeof label, "%02d:%02d", second / 60, second % 60);
    draw->AddText({x + 3, start.y + 2},
                  ImGui::GetColorU32(ImGuiCol_TextDisabled), label);
    draw->AddLine({x, start.y + 22 * scale}, {x, s.timelineMax.y},
                  ImGui::GetColorU32(ImGuiCol_Border));
  }
  ImGui::SetCursorScreenPos({s.timelineMin.x, start.y});
  ImGui::InvisibleButton("Ruler", {canvas, 30 * scale});
  if (ImGui::IsItemActivated()) {
    s.rangeDrag = 0;
    if (p.range) {
      float mx = ImGui::GetIO().MousePos.x;
      if (std::abs(mx - toX(p.rangeIn)) < 8 * scale)
        s.rangeDrag = 1;
      else if (std::abs(mx - toX(p.rangeOut)) < 8 * scale)
        s.rangeDrag = 2;
    }
    if (!s.rangeDrag)
      soundSeek(double(toFrame(ImGui::GetIO().MousePos.x)) / p.rate, false);
    else
      s.history.remember(p, "Move range handle");
  }
  if (ImGui::IsItemActive() && s.rangeDrag) {
    auto at = std::min(p.duration(), toFrame(ImGui::GetIO().MousePos.x));
    (s.rangeDrag == 1 ? p.rangeIn : p.rangeOut) = at;
    s.dirty = true;
    s.revision++;
  }

  if (ImGui::IsItemClicked(1))
    s.contextAt = toFrame(ImGui::GetIO().MousePos.x);
  if (ImGui::BeginPopupContextItem("Range menu")) {
    if (ImGui::MenuItem("Set In here")) {
      s.cursor = s.contextAt;
      soundExecute("range.in");
    }
    if (ImGui::MenuItem("Set Out here")) {
      s.cursor = s.contextAt;
      soundExecute("range.out");
    }
    if (ImGui::MenuItem("Clear range"))
      soundExecute("range.clear");
    ImGui::EndPopup();
  }
  if (p.range) {
    float a = toX(std::min(p.rangeIn, p.rangeOut)),
          b = toX(std::max(p.rangeIn, p.rangeOut));
    draw->AddRectFilled({a, start.y + 26 * scale}, {b, start.y + 30 * scale},
                        IM_COL32(197, 171, 98, 255));
  }
  if (p.range)
    for (auto at : {p.rangeIn, p.rangeOut}) {
      float x = toX(at);
      draw->AddRectFilled({x - 3 * scale, start.y + 19 * scale},
                          {x + 3 * scale, start.y + 31 * scale},
                          IM_COL32(197, 171, 98, 255));
    }
  for (auto m : p.markers) {
    float x = toX(m.at);
    draw->AddTriangleFilled(
        {x, start.y + 18 * scale}, {x - 4 * scale, start.y + 10 * scale},
        {x + 4 * scale, start.y + 10 * scale}, IM_COL32(197, 171, 98, 255));
  }
  std::string deferredAction;
  rbAudio::Id removeKeyClip = 0;
  int removeKey = -1;
  rbAudio::Id deferredClip = 0;
  rbAudio::Frame splitAt = -1;
  for (size_t ti = 0; ti < p.tracks.size(); ti++) {
    auto &t = p.tracks[ti];
    float y = start.y + 32 * scale + ti * row;
    ImGui::SetCursorScreenPos({start.x, y});
    ImGui::PushID(int(t.id));
    if (ImGui::Selectable(t.name.c_str(), s.track == t.id,
                          ImGuiSelectableFlags_AllowOverlap,
                          {header - 8 * scale, 23 * scale}))
      s.track = t.id;
    if (ImGui::BeginPopupContextItem("Track menu")) {
      s.track = t.id;
      for (auto item : {std::make_pair("Rename...", "track.rename"),
                        {"Delete track...", "track.delete"},
                        {"Add track", "track.add"}})
        if (ImGui::MenuItem(item.first))
          deferredAction = item.second;
      if (ImGui::MenuItem("Move up", nullptr, false, ti > 0 && !t.locked)) {
        auto old = p;
        std::swap(p.tracks[ti], p.tracks[ti - 1]);
        soundChanged(old, "Reorder track");
      }
      if (ImGui::MenuItem("Move down", nullptr, false,
                          ti + 1 < p.tracks.size() && !t.locked)) {
        auto old = p;
        std::swap(p.tracks[ti], p.tracks[ti + 1]);
        soundChanged(old, "Reorder track");
      }
      ImGui::EndPopup();
    }
    ImGui::SetCursorScreenPos({start.x + 4 * scale, y + 29 * scale});
    int trackOption = 0;
    for (auto item : {std::make_pair("Lock", "track.lock"),
                      {"Visibility", "track.visibility"},
                      {"Mute", "track.mute"},
                      {"Solo", "track.solo"}}) {
      bool selected = trackOption == 0   ? t.locked
                      : trackOption == 1 ? !t.visible
                      : trackOption == 2 ? t.muted
                                         : t.solo;
      if (soundIcon(item.second, item.first, selected, 23)) {
        s.track = t.id;
        soundExecute(item.second);
      }
      ImGui::SameLine(0, 3 * scale);
      trackOption++;
    }
    ImGui::PopID();
    for (auto &c : p.clips) {
      if (c.track != t.id)
        continue;
      float x = toX(c.at), end = toX(c.end());
      ImRect r{{x, y + 4 * scale}, {end, y + row - 7 * scale}};
      if (r.Max.x < ImGui::GetWindowPos().x ||
          r.Min.x > ImGui::GetWindowPos().x + ImGui::GetWindowSize().x ||
          r.Max.y < ImGui::GetWindowPos().y ||
          r.Min.y > ImGui::GetWindowPos().y + ImGui::GetWindowSize().y)
        continue;
      ImGui::PushID(int(c.id));
      ImGui::SetCursorScreenPos(r.Min);
      ImGui::InvisibleButton("Clip",
                             {std::max(4.f, r.GetWidth()), r.GetHeight()});
      auto source = p.source(c.source);
      bool selected = s.selected.count(c.id);
      auto col = ti % 3 == 0   ? IM_COL32(32, 89, 98, t.muted ? 80 : 255)
                 : ti % 3 == 1 ? IM_COL32(54, 93, 105, t.muted ? 80 : 255)
                               : IM_COL32(74, 64, 102, t.muted ? 80 : 255);
      draw->AddRectFilled(r.Min, r.Max, col, 4 * scale);
      if (selected)
        draw->AddRect(r.Min, r.Max, IM_COL32(235, 239, 239, 255), 4 * scale, 0,
                      2 * scale);
      draw->PushClipRect(r.Min, r.Max, true);
      draw->AddText({r.Min.x + 7 * scale, r.Min.y + 4 * scale},
                    IM_COL32(235, 239, 239, 255),
                    source ? source->name.c_str() : "Missing source");
      if (t.visible)
        soundWaveDraw(
            draw,
            {{r.Min.x + 4, r.Min.y + 24 * scale}, {r.Max.x - 4, r.Max.y - 5}},
            source, &c);
      if (!c.keys.empty()) {
        for (size_t k = 0; k < c.keys.size(); k++) {
          float keyX = toX(c.at + c.keys[k].at),
                keyY = r.Max.y - 8 * scale -
                       (c.keys[k].db + 96) / 120 * (r.GetHeight() - 30 * scale);
          draw->AddCircleFilled({keyX, keyY}, 3 * scale,
                                IM_COL32(240, 235, 219, 255));
          if (k) {
            auto prev = c.keys[k - 1];
            draw->AddLine(
                {toX(c.at + prev.at),
                 r.Max.y - 8 * scale -
                     (prev.db + 96) / 120 * (r.GetHeight() - 30 * scale)},
                {keyX, keyY}, IM_COL32(240, 235, 219, 255));
          }
        }
      }
      float fadeInX = toX(c.at + c.fadeIn), fadeOutX = toX(c.end() - c.fadeOut);
      if (selected) {
        draw->AddLine({r.Min.x, r.Max.y}, {fadeInX, r.Min.y + 20 * scale},
                      IM_COL32(240, 235, 219, 255));
        draw->AddLine({fadeOutX, r.Min.y + 20 * scale}, {r.Max.x, r.Max.y},
                      IM_COL32(240, 235, 219, 255));
        for (float fx : {fadeInX, fadeOutX})
          draw->AddCircleFilled({fx, r.Min.y + 20 * scale}, 4 * scale,
                                IM_COL32(240, 235, 219, 255));
      }
      int hoveredKey = -1;
      auto mouse = ImGui::GetIO().MousePos;
      if (selected)
        for (size_t ki = 0; ki < c.keys.size(); ki++) {
          float kx = toX(c.at + c.keys[ki].at),
                ky = r.Max.y - 8 * scale -
                     (c.keys[ki].db + 96) / 120 * (r.GetHeight() - 30 * scale);
          if (std::abs(kx - mouse.x) < 7 * scale &&
              std::abs(ky - mouse.y) < 7 * scale)
            hoveredKey = int(ki);
        }
      draw->PopClipRect();
      if (ImGui::IsItemActivated()) {
        s.track = t.id;
        s.source = c.source;
        if (soundTool == 2 && !t.locked) {
          deferredClip = c.id;
          splitAt = toFrame(ImGui::GetIO().MousePos.x);
          ImGui::PopID();
          break;
        }
        if (ImGui::GetIO().KeyCtrl) {
          if (selected)
            s.selected.erase(c.id);
          else
            s.selected.insert(c.id);
        } else if (!selected)
          s.selected = {c.id};
        if (!t.locked && s.selected.count(c.id)) {
          soundStop();
          s.dragging = true;
          s.dragClip = c.id;
          s.dragAt = toFrame(ImGui::GetIO().MousePos.x);
          s.dragMode = ImGui::GetIO().MousePos.x < r.Min.x + 7 * scale   ? 1
                       : ImGui::GetIO().MousePos.x > r.Max.x - 7 * scale ? 2
                                                                         : 0;
          s.keyIndex = hoveredKey;
          s.keyClip = c.id;
          if (hoveredKey >= 0)
            s.dragMode = 3;
          else if (selected &&
                   std::abs(mouse.y - (r.Min.y + 20 * scale)) < 7 * scale) {
            if (std::abs(mouse.x - fadeInX) < 7 * scale)
              s.dragMode = 4;
            else if (std::abs(mouse.x - fadeOutX) < 7 * scale)
              s.dragMode = 5;
          }
          s.beforeDrag = p;
        }
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s\n%.3f s / %.3f s%s",
                          source ? source->uri.c_str() : "Missing",
                          double(c.at) / p.rate, double(c.length()) / p.rate,
                          t.locked ? "\nTrack locked" : "");
        if (p.previewAxis) {
          float hover = ImGui::GetIO().MousePos.x;
          draw->AddLine({hover, start.y + 25 * scale}, {hover, s.timelineMax.y},
                        IM_COL32(150, 150, 160, 160));
        }
      }
      if (ImGui::BeginPopupContextItem("Clip menu")) {
        s.source = c.source;
        s.track = t.id;
        if (!s.selected.count(c.id))
          s.selected = {c.id};
        if (hoveredKey >= 0 && ImGui::MenuItem("Delete volume keyframe")) {
          removeKeyClip = c.id;
          removeKey = hoveredKey;
        }
        for (auto item : {std::make_pair("Copy", "edit.copy"),
                          {"Cut", "edit.cut"},
                          {"Paste", "edit.paste"},
                          {"Duplicate", "edit.duplicate"},
                          {"Split", "clip.split"},
                          {"Delete", "clip.delete"},
                          {"Ripple delete", "clip.rippleDelete"},
                          {"Group", "clip.group"},
                          {"Ungroup", "clip.ungroup"},
                          {"Gain keyframe", "clip.keyframe"},
                          {"Relink source...", "source.relink"},
                          {"Update source with backup...", "resource.update"}})
          if (ImGui::MenuItem(item.first))
            deferredAction = item.second;
        ImGui::EndPopup();
      }
      ImGui::PopID();
    }
    draw->AddLine({start.x, y + row - 2}, {s.timelineMax.x, y + row - 2},
                  ImGui::GetColorU32(ImGuiCol_Border));
  }
  if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0) &&
      !ImGui::IsAnyItemHovered() &&
      ImGui::GetIO().MousePos.x >= s.timelineMin.x) {
    s.selected.clear();
    soundSeek(double(toFrame(ImGui::GetIO().MousePos.x)) / p.rate, false);
  }
  if (s.dragging) {
    if (ImGui::IsMouseDown(0)) {
      auto original = s.beforeDrag.clip(s.dragClip);
      if (original) {
        auto changed = s.beforeDrag;
        auto delta = toFrame(ImGui::GetIO().MousePos.x) - s.dragAt;
        try {
          if (!s.dragMode) {
            rbAudio::Id target = 0;
            int index =
                int((ImGui::GetIO().MousePos.y - start.y - 32 * scale) / row);
            std::set<rbAudio::Id> tracks;
            for (auto c : changed.clips)
              if (s.selected.count(c.id))
                tracks.insert(c.track);
            if (index >= 0 && index < int(changed.tracks.size()) &&
                tracks.size() == 1)
              target = changed.tracks[index].id;
            if (changed.snapping) {
              auto proposed = original->at + delta;
              auto tolerance =
                  rbAudio::Frame(6 * scale / s.pixelsPerSecond * p.rate);
              for (auto c : changed.clips)
                if (!s.selected.count(c.id))
                  for (auto edge : {c.at, c.end()})
                    if (std::abs(proposed - edge) < tolerance)
                      delta = edge - original->at;
              if (std::abs(proposed - s.cursor) < tolerance)
                delta = s.cursor - original->at;
            }
            rbAudio::move(changed, s.selected, delta, target);
          } else {
            rbAudio::requireEditable(changed, {s.dragClip});
            auto clip = changed.clip(s.dragClip);
            if (s.dragMode == 3 && s.keyIndex >= 0 &&
                s.keyIndex < int(clip->keys.size())) {
              auto &key = clip->keys[s.keyIndex];
              auto lo = s.keyIndex ? clip->keys[s.keyIndex - 1].at + 1 : 0;
              auto hi = s.keyIndex + 1 < int(clip->keys.size())
                            ? clip->keys[s.keyIndex + 1].at - 1
                            : clip->length();
              key.at = std::clamp(key.at + delta, lo, hi);
              int trackIndex = 0;
              for (size_t i = 0; i < p.tracks.size(); i++)
                if (p.tracks[i].id == clip->track)
                  trackIndex = int(i);
              float bottom =
                  start.y + 32 * scale + trackIndex * row + row - 15 * scale;
              key.db = std::clamp((bottom - ImGui::GetIO().MousePos.y) /
                                          (row - 41 * scale) * 120 -
                                      96,
                                  -96.f, 24.f);
            } else if (s.dragMode == 4)
              clip->fadeIn = std::clamp(clip->fadeIn + delta, rbAudio::Frame(0),
                                        clip->length());
            else if (s.dragMode == 5)
              clip->fadeOut = std::clamp(clip->fadeOut - delta,
                                         rbAudio::Frame(0), clip->length());
            else if (s.dragMode == 1) {
              auto sourceDelta =
                  rbAudio::Frame(std::llround(delta * clip->speed));
              sourceDelta =
                  std::clamp(sourceDelta, -clip->in, clip->out - clip->in - 1);
              clip->in += sourceDelta;
              clip->at = std::max(
                  rbAudio::Frame(0),
                  clip->at + rbAudio::Frame(sourceDelta / clip->speed));
              clip->fadeIn = 0;
              rbAudio::shiftKeys(*clip,
                                 rbAudio::Frame(sourceDelta / clip->speed),
                                 original->length());
            } else {
              auto source = changed.source(clip->source);
              clip->out = std::clamp(
                  clip->out + rbAudio::Frame(std::llround(delta * clip->speed)),
                  clip->in + 1, source ? source->length() : clip->out);
              clip->fadeOut = 0;
              rbAudio::shiftKeys(*clip, 0, clip->length());
            }
          }
          s.project = std::move(changed);
        } catch (const std::exception &e) {
          soundMessage = e.what();
        }
      }
    } else {
      auto before = s.beforeDrag;
      s.dragging = false;
      rbAudio::magnet(p);
      soundChanged(before, "Move/trim clips");
    }
  }
  float cursorX = toX(s.cursor);
  draw->AddLine({cursorX, start.y + 20 * scale}, {cursorX, s.timelineMax.y},
                ImGui::GetColorU32(ImGuiCol_Text), scale);
  draw->AddTriangleFilled({cursorX - 4 * scale, start.y + 19 * scale},
                          {cursorX + 4 * scale, start.y + 19 * scale},
                          {cursorX, start.y + 26 * scale},
                          ImGui::GetColorU32(ImGuiCol_Text));
  ImGui::SetCursorScreenPos(
      {start.x, start.y + 35 * scale + row * p.tracks.size()});
  if (ImGui::Button("+ Add audio track"))
    soundExecute("track.add");
  ImGui::Dummy({canvas + header, 1});
  if (ImGui::BeginDragDropTargetCustom(
          ImRect{{start.x, start.y},
                 {s.timelineMax.x, std::max(start.y + 200 * scale,
                                            s.timelineMax.y + 40 * scale)}},
          ImGui::GetID("Audio canvas drop"))) {
    if (auto payload = ImGui::AcceptDragDropPayload("FTEBROOM_MOD_FILE")) {
      auto path = std::filesystem::u8path((const char *)payload->Data);
      s.cursor = toFrame(ImGui::GetIO().MousePos.x);
      int index = int((ImGui::GetIO().MousePos.y - start.y - 32 * scale) / row);
      s.importAddTrack = index < 0 || index >= int(p.tracks.size());
      if (!s.importAddTrack)
        s.track = p.tracks[index].id;
      soundOpen(path);
    }
    if (auto payload = ImGui::AcceptDragDropPayload("REYOND_AUDIO_SOURCE")) {
      auto id = *(const rbAudio::Id *)payload->Data;
      soundApply("Insert audio clip", [&] {
        int index =
            int((ImGui::GetIO().MousePos.y - start.y - 32 * scale) / row);
        s.track = index < 0 || index >= int(p.tracks.size())
                      ? p.addTrack("Audio")
                      : p.tracks[index].id;
        auto track = p.track(s.track);
        if (track->locked)
          throw std::runtime_error("Destination track is locked");
        auto cid = p.addClip(id, s.track, toFrame(ImGui::GetIO().MousePos.x));
        s.selected = {cid};
      });
    }
    ImGui::EndDragDropTarget();
  }
  ImGui::EndChild();
  if (removeKey >= 0)
    soundApply("Delete volume keyframe", [&] {
      if (auto c = p.clip(removeKeyClip)) {
        rbAudio::requireEditable(p, {c->id});
        c->keys.erase(c->keys.begin() + removeKey);
      }
    });
  if (splitAt >= 0)
    soundApply("Split at pointer",
               [&] { rbAudio::split(p, {deferredClip}, splitAt); });
  if (!deferredAction.empty())
    soundExecute(deferredAction);
  ImGui::TextDisabled(
      "%zu tracks / %zu clips    In %.3f  Out %.3f    %d Hz stereo",
      p.tracks.size(), p.clips.size(), double(p.rangeIn) / p.rate,
      double(p.rangeOut) / p.rate, p.rate);
  ImGui::EndChild();
}
static void soundBrowserAccept() {
  auto &s = soundState;
  auto path = std::filesystem::u8path(s.path);
  auto purpose = s.pickerPurpose;
  if (purpose == "import") {
    s.importAddTrack = false;
    soundOpen(path);
  } else if (purpose == "open")
    soundProjectLoad(path);
  else if (purpose == "save") {
    if (path.extension() != ".json")
      path += ".rbaudio.json";
    soundProjectSave(path);
    auto next = s.afterSave;
    s.afterSave.clear();
    if (!next.empty()) {
      s.dialog.clear();
      ImGui::CloseCurrentPopup();
      soundRequest(next);
      return;
    }
  } else if (purpose == "export") {
    auto parent =
        std::filesystem::is_directory(path) ? path : path.parent_path();
    if (!std::filesystem::is_directory(parent))
      throw std::runtime_error("Choose an existing export directory");
    std::snprintf(s.path, sizeof s.path, "%s", parent.u8string().c_str());
    soundDialog("Export audio");
    return;
  } else if (purpose == "relink") {
    auto roots = soundRoots();
    soundValidate(path, roots);
    auto exe = soundFFmpeg();
    auto sourceId = s.source;
    int rate = s.project.rate;
    rbAudio::Frame required = 0;
    for (const auto &clip : s.project.clips)
      if (clip.source == sourceId)
        required = std::max(required, clip.out);
    soundJobStart(SoundWorkResult::Relink, [=](auto &r, auto control) {
      r.sourceId = sourceId;
      r.source = rbAudio::decode(exe, path.u8string(),
                                 soundReadResource(path, roots), rate, control);
      if (r.source.length() < required)
        throw std::runtime_error(
            "Replacement source is shorter than existing clip boundaries. "
            "Choose a longer source or trim those clips first.");
    });
  }
  s.dialog.clear();
  ImGui::CloseCurrentPopup();
}
static void soundRecordFinish() {
  auto &s = soundState;
  s.capture.finish(s.recordGain);
  s.recordAt = -1;
  if (s.capture.samples.empty())
    throw std::runtime_error("Microphone returned no audio");
  auto roots = soundRoots();
  if (roots.empty())
    throw std::runtime_error(
        "Configure a writable mod directory before recording");
  auto folder = roots.front() / ".reyond-recordings";
  std::filesystem::create_directories(folder);
  auto target =
      folder / ("take-" +
                std::to_string(std::chrono::high_resolution_clock::now()
                                   .time_since_epoch()
                                   .count()) +
                ".wav");
  soundValidate(target, roots);
  auto captured = std::make_shared<const std::vector<int16_t>>(
      std::move(s.capture.samples));
  auto rate = s.project.rate;
  auto exe = soundFFmpeg();
  auto at = s.recordPosition;
  auto track = s.recordTrack;
  soundJobStart(SoundWorkResult::Import, [=](auto &r, auto control) {
    const auto &samples = *captured;
    std::string wav(44 + samples.size() * 2, '\0');
    auto put = [&](size_t pos, uint32_t v, int n) {
      for (int i = 0; i < n; i++)
        wav[pos + i] = char(v >> (8 * i));
    };
    wav.replace(0, 4, "RIFF");
    put(4, uint32_t(wav.size() - 8), 4);
    wav.replace(8, 8, "WAVEfmt ");
    put(16, 16, 4);
    put(20, 1, 2);
    put(22, 2, 2);
    put(24, 44100, 4);
    put(28, 176400, 4);
    put(32, 4, 2);
    put(34, 16, 2);
    wav.replace(36, 4, "data");
    put(40, uint32_t(samples.size() * 2), 4);
    std::memcpy(wav.data() + 44, samples.data(), samples.size() * 2);
    rbAudio::cancelled(control);
    rbAudio::writeBytes(target, wav);
    try {
      r.source = rbAudio::decode(exe, target.u8string(), std::move(wav), rate,
                                 control);
      r.at = at;
      r.track = track;
    } catch (...) {
      std::error_code ec;
      std::filesystem::remove(target, ec);
      throw;
    }
  });
  soundMessage = "Importing microphone recording...";
}
static void soundDialogs() {
  auto &s = soundState;
  auto &p = s.project;
  const float scale = uiScale();
  if (!s.pendingDialog.empty()) {
    s.dialog = s.pendingDialog;
    s.pendingDialog.clear();
    ImGui::OpenPopup(s.dialog.c_str());
  }
  if (s.dialog.empty())
    return;
  ImGui::SetNextWindowSize(
      {std::min(ImGui::GetIO().DisplaySize.x - 30, 700 * scale), 0},
      ImGuiCond_Appearing);
  ImGui::SetNextWindowSizeConstraints(
      {350 * scale, 100 * scale},
      {ImGui::GetIO().DisplaySize.x - 20, ImGui::GetIO().DisplaySize.y - 40});
  bool open = true;
  if (ImGui::BeginPopupModal(s.dialog.c_str(), &open,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    try {
      if (s.dialog == "Audio file browser") {
        ImGui::TextWrapped(
            "%s", s.pickerPurpose == "save"     ? "Save project (.rbaudio.json)"
                  : s.pickerPurpose == "open"   ? "Open audio project"
                  : s.pickerPurpose == "relink" ? "Relink selected source"
                  : s.pickerPurpose == "export"
                      ? "Choose export location"
                      : "Import WAV / OGG / MP3 / FLAC");
        ImGui::InputText("Path", s.path, sizeof s.path);
        if (ImGui::Button("Up")) {
          s.browseFolder = s.browseFolder.parent_path();
          s.browseEntries.clear();
        }
        ImGui::SameLine();
        if (ImGui::Button("Refresh"))
          s.browseEntries.clear();
        for (auto root : soundRoots()) {
          ImGui::SameLine();
          if (ImGui::SmallButton(root.filename().u8string().c_str())) {
            s.browseFolder = root;
            s.browseEntries.clear();
            std::snprintf(s.path, sizeof s.path, "%s", root.u8string().c_str());
          }
        }
        ImGui::TextWrapped("Folder: %s", s.browseFolder.u8string().c_str());
        if (s.browseEntries.empty()) {
          s.browseDirectories.clear();
          for (auto &e : modFileList(s.browseFolder)) {
            if (s.browseEntries.size() >= 1024)
              break;
            s.browseEntries.push_back(e.second);
            if (e.first || ((e.second.extension() == ".pak" ||
                             e.second.extension() == ".pk3") &&
                            !modFileMember(e.second)))
              s.browseDirectories.insert(e.second);
          }
          std::sort(s.browseEntries.begin(), s.browseEntries.end(),
                    [&](const auto &a, const auto &b) {
                      bool ad = s.browseDirectories.count(a),
                           bd = s.browseDirectories.count(b);
                      return ad != bd ? ad > bd : a.filename() < b.filename();
                    });
        }
        ImGui::BeginChild("Audio files", {0, 250 * scale},
                          ImGuiChildFlags_Borders);
        std::filesystem::path folder;
        for (auto path : s.browseEntries) {
          auto label = (s.browseDirectories.count(path) ? "[+] " : "") +
                       path.filename().u8string();
          if (ImGui::Selectable(label.c_str(), false,
                                ImGuiSelectableFlags_DontClosePopups)) {
            std::snprintf(s.path, sizeof s.path, "%s", path.u8string().c_str());
            if (s.browseDirectories.count(path))
              folder = path;
          }
        }
        ImGui::EndChild();
        if (!folder.empty()) {
          s.browseFolder = folder;
          s.browseEntries.clear();
        }
        {
          SoundDisabled disabled(soundBusy());
          if (ImGui::Button(s.pickerPurpose == "save" ? "Save project"
                                                      : "Choose"))
            soundBrowserAccept();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
          s.dialog.clear();
          ImGui::CloseCurrentPopup();
        }
      } else if (s.dialog == "Export audio") {
        ImGui::InputText("File name", s.fileName, sizeof s.fileName);
        ImGui::InputText("Directory", s.path, sizeof s.path);
        if (!*s.path) {
          auto roots = soundRoots();
          std::snprintf(
              s.path, sizeof s.path, "%s",
              (roots.empty() ? settingsPath().parent_path() : roots.front())
                  .u8string()
                  .c_str());
        }
        if (ImGui::Button("Browse...")) {
          ImGui::CloseCurrentPopup();
          soundPathDialog("export");
        }
        const char *formats[] = {"WAV", "OGG", "MP3", "FLAC"};
        const char *extensions[] = {".wav", ".ogg", ".mp3", ".flac"};
        auto ext = entityLower(
            std::filesystem::u8path(s.fileName).extension().u8string());
        int format = 0;
        for (int i = 0; i < 4; i++)
          if (ext == extensions[i])
            format = i;
        if (ImGui::Combo("Format", &format, "WAV\0OGG\0MP3\0FLAC\0")) {
          auto name = std::filesystem::u8path(s.fileName).stem().u8string() +
                      extensions[format];
          std::snprintf(s.fileName, sizeof s.fileName, "%s", name.c_str());
        }
        int rate = s.exportRate == 48000 ? 1 : s.exportRate == 22050 ? 2 : 0;
        if (ImGui::Combo("Sample rate", &rate,
                         "44,100 Hz\0"
                         "48,000 Hz\0"
                         "22,050 Hz\0"))
          s.exportRate = rate == 1 ? 48000 : rate == 2 ? 22050 : 44100;
        int channels = s.exportChannels - 1;
        if (ImGui::Combo("Channels", &channels, "Mono\0Stereo\0"))
          s.exportChannels = channels + 1;
        int bits = s.exportBits == 24 ? 1 : 0;
        if (ImGui::Combo("Bit depth", &bits,
                         "16-bit\0"
                         "24-bit\0"))
          s.exportBits = bits ? 24 : 16;
        ImGui::SliderInt("Lossy bitrate (kb/s)", &s.exportBitrate, 64, 320);
        ImGui::Checkbox("Use In / Out range", &s.exportRange);
        ImGui::Text(
            "%.3f - %.3f s",
            double(s.exportRange && p.range ? std::min(p.rangeIn, p.rangeOut)
                                            : 0) /
                p.rate,
            double(s.exportRange && p.range ? std::max(p.rangeIn, p.rangeOut)
                                            : p.duration()) /
                p.rate);
        ImGui::TextWrapped(
            "All audible tracks are mixed. Muted tracks are excluded. Existing "
            "files require overwrite confirmation.");
        {
          SoundDisabled disabled(soundBusy());
          if (ImGui::Button("Export audio")) {
            auto target = std::filesystem::u8path(s.path) /
                          std::filesystem::u8path(s.fileName);
            if (std::filesystem::u8path(s.fileName).filename() !=
                std::filesystem::u8path(s.fileName))
              throw std::runtime_error(
                  "Enter a file name without directory components");
            if (std::filesystem::exists(target)) {
              ImGui::CloseCurrentPopup();
              soundDialog("Overwrite audio export");
            } else {
              soundExport(target);
              s.dialog.clear();
              ImGui::CloseCurrentPopup();
            }
          }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
          s.dialog.clear();
          ImGui::CloseCurrentPopup();
        }
      } else if (s.dialog == "Overwrite audio export") {
        ImGui::TextWrapped("Replace %s? Audio sources in the project are not "
                           "implicitly updated.",
                           s.fileName);
        if (ImGui::Button("Replace")) {
          auto target = std::filesystem::u8path(s.path) /
                        std::filesystem::u8path(s.fileName);
          for (auto &source : p.sources)
            if (std::filesystem::weakly_canonical(std::filesystem::u8path(
                    source.uri)) == std::filesystem::weakly_canonical(target))
              throw std::runtime_error(
                  "Export would replace a project source. Use Update source "
                  "with backup instead.");
          soundExport(target);
          s.dialog.clear();
          ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
          s.dialog.clear();
          ImGui::CloseCurrentPopup();
        }
      } else if (s.dialog == "Update source with backup") {
        auto c = soundSelected();
        auto source = c ? p.source(c->source) : nullptr;
        if (source)
          ImGui::TextWrapped(
              "Replace %s with the selected clip's rendered audio? A backup is "
              "retained. This does not export the entire mix.",
              source->uri.c_str());
        else
          ImGui::TextWrapped("Select exactly one clip first.");
        ImGui::TextWrapped("Compressed formats are reencoded. Loop/cue "
                           "metadata is not preserved.");
        {
          SoundDisabled disabled(!source || s.selected.size() != 1 ||
                                 soundBusy());
          if (ImGui::Button("Update source with backup")) {
            soundExport({}, true);
            s.dialog.clear();
            ImGui::CloseCurrentPopup();
          }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
          s.dialog.clear();
          ImGui::CloseCurrentPopup();
        }
      } else if (s.dialog == "Unsaved audio project") {
        ImGui::TextWrapped(
            "Save this audio project before closing or opening another one? "
            "Source audio files are unchanged by saving a project.");
        auto purpose = s.pickerPurpose;
        if (ImGui::Button("Save...")) {
          s.afterSave = purpose;
          s.closing = false;
          s.dialog.clear();
          ImGui::CloseCurrentPopup();
          soundPathDialog("save");
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard")) {
          s.dirty = false;
          ImGui::CloseCurrentPopup();
          s.dialog.clear();
          soundRequest(purpose);
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
          s.dialog.clear();
          s.closing = false;
          ImGui::CloseCurrentPopup();
        }
      } else if (s.dialog == "Delete audio track") {
        auto t = p.track(s.track);
        ImGui::TextWrapped(
            "Delete this track and all its clips? This can be undone.");
        {
          SoundDisabled disabled(!t || t->locked);
          if (ImGui::Button("Delete track")) {
            soundApply("Delete track", [&] {
              auto id = s.track;
              p.clips.erase(
                  std::remove_if(p.clips.begin(), p.clips.end(),
                                 [&](auto c) { return c.track == id; }),
                  p.clips.end());
              p.tracks.erase(std::remove_if(p.tracks.begin(), p.tracks.end(),
                                            [&](auto t) { return t.id == id; }),
                             p.tracks.end());
              s.track = p.tracks.empty() ? 0 : p.tracks.front().id;
              s.selected.clear();
            });
            s.dialog.clear();
            ImGui::CloseCurrentPopup();
          }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
          s.dialog.clear();
          ImGui::CloseCurrentPopup();
        }
      } else if (s.dialog == "Rename audio track") {
        ImGui::InputText("Name", s.rename, sizeof s.rename);
        if (ImGui::Button("Rename")) {
          soundApply("Rename track", [&] {
            if (auto t = p.track(s.track))
              t->name = s.rename;
          });
          s.dialog.clear();
          ImGui::CloseCurrentPopup();
        }
      } else if (s.dialog == "Rename audio project") {
        ImGui::InputText("Name", s.rename, sizeof s.rename);
        if (ImGui::Button("Rename")) {
          soundApply("Rename project", [&] { p.name = s.rename; });
          s.dialog.clear();
          ImGui::CloseCurrentPopup();
        }
      } else if (s.dialog == "Record audio") {
        if (s.devices.empty())
          s.devices = s.capture.devices();
        {
          SoundDisabled disabled(s.capture.active() || s.recordAt >= 0);
          if (ImGui::BeginCombo("Input device",
                                s.devices[std::min(size_t(s.recordDevice),
                                                   s.devices.size() - 1)]
                                    .c_str())) {
            for (size_t i = 0; i < s.devices.size(); i++)
              if (ImGui::Selectable(s.devices[i].c_str(),
                                    s.recordDevice == int(i)))
                s.recordDevice = int(i);
            ImGui::EndCombo();
          }
          ImGui::SliderFloat("Input volume", &s.recordGain, 0, 2);
          ImGui::Checkbox("Mute project while recording", &s.recordMute);
          ImGui::SliderInt("Countdown seconds", &s.countdown, 0, 5);
        }
        ImGui::ProgressBar(s.capture.peak, {-1, 14 * scale},
                           "Microphone input");
        if (s.recordAt >= 0 && !s.capture.active()) {
          double remaining = s.recordAt - ImGui::GetTime();
          if (remaining <= 0) {
            s.capture.begin(s.recordDevice);
            s.recordStarted = ImGui::GetTime();
            if (!s.recordMute)
              soundSeek(double(s.recordPosition) / p.rate, true);
          } else
            ImGui::Text("Recording starts in %.0f...", std::ceil(remaining));
        }
        if (s.capture.active()) {
          ImGui::Text("Recording %.1f s",
                      double(s.capture.samples.size() / 2) / 44100);
          if (ImGui::Button("Stop and insert recording")) {
            soundStop();
            soundRecordFinish();
            s.dialog.clear();
            ImGui::CloseCurrentPopup();
          }
        } else if (s.recordAt < 0 && ImGui::Button("Start recording")) {
          if (soundRoots().empty())
            throw std::runtime_error(
                "Configure a writable mod directory before recording");
          s.recordPosition = s.cursor;
          s.recordTrack = s.track;
          s.recordAt = ImGui::GetTime() + s.countdown;
          soundStop();
        }
        if (!s.capture.active() && s.recordAt < 0 &&
            !s.capture.samples.empty()) {
          if (ImGui::Button("Insert captured take")) {
            soundRecordFinish();
            s.dialog.clear();
            ImGui::CloseCurrentPopup();
          }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
          s.capture.end();
          s.capture.samples.clear();
          s.recordAt = -1;
          s.dialog.clear();
          ImGui::CloseCurrentPopup();
        }
      } else if (s.dialog == "Keyboard shortcuts") {
        soundBindings();
        ImGui::TextWrapped("Audio preset approved with mockup v1. Focus: "
                           "dialog > text input > audio > map.");
        ImGui::InputText("Search", s.filter, sizeof s.filter);
        ImGui::BeginChild("Bindings", {0, 350 * scale},
                          ImGuiChildFlags_Borders);
        std::set<std::tuple<int, bool, bool, bool>> chords;
        bool conflict = false;
        for (auto &b : s.bindings) {
          auto parsed = soundChord(b.chord);
          if (!parsed.valid)
            conflict = true;
          if (!b.chord.empty() &&
              !chords
                   .insert({parsed.key, parsed.ctrl, parsed.shift, parsed.alt})
                   .second)
            conflict = true;
          if (*s.filter && entityLower(b.label).find(entityLower(s.filter)) ==
                               std::string::npos)
            continue;
          ImGui::PushID(b.id.c_str());
          char chord[64];
          std::snprintf(chord, sizeof chord, "%s", b.chord.c_str());
          ImGui::TextUnformatted(b.label.c_str());
          ImGui::SameLine(250 * scale);
          ImGui::SetNextItemWidth(180 * scale);
          if (ImGui::InputText("##chord", chord, sizeof chord))
            b.chord = chord;
          if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s / %s", b.id.c_str(), b.context.c_str());
          ImGui::PopID();
        }
        ImGui::EndChild();
        if (conflict)
          ImGui::TextWrapped("Resolve invalid or duplicate chords before "
                             "saving (for example Ctrl+Shift+B).");
        {
          SoundDisabled disabled(conflict);
          if (ImGui::Button("Save preset")) {
            rbAudio::Json j = rbAudio::Json::object();
            for (auto &b : s.bindings)
              j[b.id] = b.chord;
            rbAudio::writeBytes(documentPath("data/audio-shortcuts.json"),
                                j.dump(2));
            s.dialog.clear();
            ImGui::CloseCurrentPopup();
          }
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset preset")) {
          std::error_code ec;
          std::filesystem::remove(documentPath("data/audio-shortcuts.json"),
                                  ec);
          s.shortcutsLoaded = false;
          soundBindings();
        }
        ImGui::SameLine();
        if (ImGui::Button("Close")) {
          s.dialog.clear();
          ImGui::CloseCurrentPopup();
        }
      } else if (s.dialog == "Audio workspace") {
        ImGui::TextWrapped(
            "Desktop layout: Media left, Player center, Basic / Voice changer "
            "/ Speed right, multitrack timeline below. The audio window can be "
            "resized; panels scroll independently.");
        {
          SoundDisabled disabled(!p.sources.empty() || !p.clips.empty());
          int rate = p.rate == 48000 ? 1 : 0;
          if (ImGui::Combo("New project rate", &rate,
                           "44,100 Hz\0"
                           "48,000 Hz\0"))
            soundApply("Project sample rate",
                       [&] { p.rate = rate ? 48000 : 44100; });
        }
        if (ImGui::Button("Reset timeline view")) {
          s.zoom = 1;
          s.viewStart = 0;
        }
        if (ImGui::Button("Close")) {
          s.dialog.clear();
          ImGui::CloseCurrentPopup();
        }
      }
    } catch (const std::exception &e) {
      soundMessage = e.what();
      s.recordAt = -1;
      if (s.capture.active())
        s.capture.end();
    }
    if (!soundMessage.empty())
      ImGui::TextWrapped("%s", soundMessage.c_str());
    ImGui::EndPopup();
  }
  if (!open) {
    s.capture.end();
    s.recordAt = -1;
    s.dialog.clear();
  }
}
static void soundLibrary() {
  auto &s = soundState;
  auto &p = s.project;
  ImGui::BeginChild("Audio media", {0, 0}, ImGuiChildFlags_Borders);
  ImGui::TextColored(ImGui::GetStyle().Colors[ImGuiCol_CheckMark], "Media");
  ImGui::SameLine(80 * uiScale());
  ImGui::TextUnformatted("Audio");
  ImGui::Separator();
  float sidebar = 110 * uiScale();
  ImGui::BeginChild("Media locations", {sidebar, 0});
  if (ImGui::Selectable("Local", true)) {
  }
  ImGui::Indent(10 * uiScale());
  if (ImGui::Selectable("Import"))
    soundExecute("project.import");
  if (ImGui::Selectable("Mod sounds")) {
    soundPathDialog("import");
  }
  ImGui::Unindent(10 * uiScale());
  ImGui::Separator();
  ImGui::TextDisabled("Project");
  if (ImGui::Selectable("All audio", true))
    s.filter[0] = 0;
  ImGui::TextDisabled("%zu sources", p.sources.size());
  ImGui::EndChild();
  ImGui::SameLine();
  ImGui::BeginChild("Media contents");
  if (ImGui::Button("Import..."))
    soundExecute("project.import");
  ImGui::SameLine();
  if (ImGui::Button("Game sounds")) {
    modFilesOpen = true;
    modFilesVisible = true;
    soundMessage = "Browse Mod file manager or drop an audio file from it into "
                   "the timeline.";
  }
  ImGui::InputTextWithHint("##mediafilter", "Search media", s.filter,
                           sizeof s.filter);
  ImGui::BeginChild("Media library scroll");
  float tile = 160 * uiScale(), gap = ImGui::GetStyle().ItemSpacing.x;
  int columns =
      std::max(1, int(ImGui::GetContentRegionAvail().x / (tile + gap)));
  int count = 0;
  for (auto &source : p.sources) {
    if (*s.filter && entityLower(source.name).find(entityLower(s.filter)) ==
                         std::string::npos)
      continue;
    if (count % columns)
      ImGui::SameLine();
    ImGui::PushID(int(source.id));
    ImGui::BeginGroup();
    auto pos = ImGui::GetCursorScreenPos();
    bool chosen = s.source == source.id;
    ImGui::InvisibleButton("Source", {tile, 72 * uiScale()});
    auto draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(
        pos, {pos.x + tile, pos.y + 72 * uiScale()},
        ImGui::GetColorU32(chosen ? ImGuiCol_HeaderActive : ImGuiCol_FrameBg),
        4 * uiScale());
    soundWaveDraw(
        draw,
        {{pos.x + 5, pos.y + 9}, {pos.x + tile - 5, pos.y + 65 * uiScale()}},
        &source);
    if (chosen)
      draw->AddRect(pos, {pos.x + tile, pos.y + 72 * uiScale()},
                    ImGui::GetColorU32(ImGuiCol_CheckMark), 4 * uiScale());
    if (ImGui::IsItemClicked())
      s.source = source.id;
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) &&
        !soundBusy()) {
      soundApply("Insert source clip", [&] {
        auto t = p.track(s.track);
        if (!t || t->locked)
          s.track = p.addTrack("Audio");
        auto id = p.addClip(source.id, s.track, s.cursor);
        s.selected = {id};
      });
    }
    if (ImGui::BeginDragDropSource()) {
      ImGui::SetDragDropPayload("REYOND_AUDIO_SOURCE", &source.id,
                                sizeof source.id);
      ImGui::TextUnformatted(source.name.c_str());
      ImGui::EndDragDropSource();
    }
    if (ImGui::BeginPopupContextItem("Source menu")) {
      s.source = source.id;
      if (ImGui::MenuItem("Relink..."))
        soundExecute("source.relink");
      ImGui::EndPopup();
    }
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + tile);
    ImGui::TextWrapped("%s", source.name.c_str());
    ImGui::PopTextWrapPos();
    ImGui::TextDisabled("%d Hz / %s / %.2f s", source.rate,
                        source.channels == 1 ? "Mono" : "Stereo",
                        double(source.length()) / p.rate);
    if (!source.pcm)
      ImGui::TextColored({1, .5f, .4f, 1}, "Source unavailable");
    ImGui::EndGroup();
    ImGui::PopID();
    count++;
  }
  if (p.sources.empty())
    ImGui::TextWrapped(
        "Import audio from your mod/authoring directory. Double-click a "
        "library source or drag it to the timeline to add clips.");
  if (ImGui::BeginDragDropTarget()) {
    if (auto payload = ImGui::AcceptDragDropPayload("FTEBROOM_MOD_FILE"))
      soundOpen(std::filesystem::u8path((const char *)payload->Data));
    ImGui::EndDragDropTarget();
  }
  ImGui::EndChild();
  ImGui::EndChild();
  ImGui::EndChild();
}
// Overview samples short blocks of the actual master graph, spread across the
// full duration. It is a navigation overview, not a peak/true-peak analysis.
static void soundPlayer() {
  auto &s = soundState;
  auto &p = s.project;
  float scale = uiScale();
  ImGui::BeginChild("Audio player", {0, 0}, ImGuiChildFlags_Borders);
  ImGui::TextUnformatted("Player");
  ImGui::Separator();
  auto available = ImGui::GetContentRegionAvail();
  auto pos = ImGui::GetCursorScreenPos();
  ImVec2 size{available.x, std::max(50.f, available.y - 110 * scale)};
  auto d = ImGui::GetWindowDrawList();
  d->AddRectFilled(pos, {pos.x + size.x, pos.y + size.y},
                   ImGui::GetColorU32(ImGuiCol_WindowBg), 4 * scale);
  if (s.ready && s.overviewRevision != s.revision && !s.dragging) {
    s.overview.assign(256, {});
    std::array<float, 128> block{};
    for (size_t i = 0; i < s.overview.size(); i++) {
      auto at = rbAudio::Frame(double(i) * p.duration() / s.overview.size());
      s.mixer.render(at, 64, block.data());
      for (auto v : block) {
        s.overview[i].lo = std::min(s.overview[i].lo, v);
        s.overview[i].hi = std::max(s.overview[i].hi, v);
      }
    }
    s.overviewRevision = s.revision;
  }
  for (int grid = 1; grid < 4; grid++)
    d->AddLine({pos.x + 10 * scale, pos.y + size.y * grid / 4},
               {pos.x + size.x - 25 * scale, pos.y + size.y * grid / 4},
               ImGui::GetColorU32(ImGuiCol_Border));
  if (s.ready)
    for (size_t i = 0; i < s.overview.size(); i++) {
      float x = pos.x + 12 * scale +
                float(i) / s.overview.size() * (size.x - 40 * scale),
            mid = pos.y + size.y * .5f;
      auto pk = s.overview[i];
      d->AddLine({x, mid - pk.hi * size.y * .43f},
                 {x, mid - pk.lo * size.y * .43f},
                 ImGui::GetColorU32(ImGuiCol_CheckMark));
    }
  else
    d->AddText({pos.x + 15 * scale, pos.y + size.y * .5f},
               ImGui::GetColorU32(ImGuiCol_TextDisabled),
               "Prepare effects to preview master");
  float meter = std::clamp((s.peak > 0 ? 20 * std::log10(s.peak) : -60.f) + 60,
                           0.f, 60.f) /
                60;
  d->AddRectFilled({pos.x + size.x - 15 * scale, pos.y + 10 * scale},
                   {pos.x + size.x - 8 * scale, pos.y + size.y - 10 * scale},
                   ImGui::GetColorU32(ImGuiCol_FrameBg));
  d->AddRectFilled(
      {pos.x + size.x - 15 * scale,
       pos.y + size.y - 10 * scale - meter * (size.y - 20 * scale)},
      {pos.x + size.x - 8 * scale, pos.y + size.y - 10 * scale},
      s.peak > 1 ? IM_COL32(241, 90, 90, 255)
                 : ImGui::GetColorU32(ImGuiCol_CheckMark));
  float x =
      pos.x + 12 * scale +
      float(double(s.cursor) / std::max(rbAudio::Frame(1), p.duration())) *
          (size.x - 40 * scale);
  d->AddLine({x, pos.y + 8 * scale}, {x, pos.y + size.y - 8 * scale},
             ImGui::GetColorU32(ImGuiCol_Text));
  ImGui::InvisibleButton("Master overview", size);
  if (ImGui::IsItemActivated() && p.duration())
    soundSeek(
        std::clamp(double(ImGui::GetIO().MousePos.x - pos.x - 12 * scale) /
                       (size.x - 40 * scale),
                   0., 1.) *
            p.duration() / p.rate,
        false);
  ImGui::TextDisabled("MASTER OUTPUT");
  ImGui::SameLine();
  ImGui::Text("%.1f dBFS%s", s.peak > 0 ? 20 * std::log10(s.peak) : -96,
              s.peak > 1 ? "  OVER" : "");
  auto before = p;
  ImGui::SetNextItemWidth(-1);
  soundFocusField("master.gain");
  bool changed = ImGui::SliderFloat("##mastergain", &p.masterDb, -96, 12,
                                    "Master %.1f dB");
  soundDirtyField(before, changed, "Master gain");
  ImGui::Text("%02d:%06.3f / %02d:%06.3f", int(s.cursor / p.rate) / 60,
              double(s.cursor) / p.rate - int(s.cursor / p.rate) / 60 * 60,
              int(p.duration() / p.rate) / 60,
              double(p.duration()) / p.rate -
                  int(p.duration() / p.rate) / 60 * 60);
  ImGui::SameLine(std::max(ImGui::GetCursorPosX(), size.x * .60f));
  if (soundIcon("transport.play", soundPlaying ? "Pause" : "Play", soundPlaying,
                24))
    soundExecute("transport.play");
  ImGui::SameLine();
  if (soundIcon("transport.stop", "Stop", false, 24))
    soundExecute("transport.stop");
  ImGui::EndChild();
}
static void soundPanel() {
  auto &s = soundState;
  try {
    soundFinish();
    if (!soundEditorOpen) {
      if (soundPlaying)
        soundStop();
      s.focused = false;
      return;
    }
    if (!soundAPI)
      soundAPI = (plugaudiofuncs_t *)core->GetEngineInterface(
          plugaudiofuncs_name, sizeof(plugaudiofuncs_t));
    soundBindings();
    SoundStyle audioStyle;
    auto display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos({0, 0}, ImGuiCond_Always);
    ImGui::SetNextWindowSize(display, ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(
        {std::min(display.x, 900.f * uiScale()),
         std::min(display.y, 650.f * uiScale())},
        display);
    bool open = true;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        {8 * uiScale(), 6 * uiScale()});
    if (ImGui::Begin("Sound editor", &open,
                     ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse |
                         ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoResize)) {
      s.focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
      ImGui::TextUnformatted("ReyondBroom");
      ImGui::SameLine();
      if (ImGui::Button("Menu"))
        ImGui::OpenPopup("Audio menu");
      if (ImGui::BeginPopup("Audio menu")) {
        for (auto item : {std::make_pair("New audio project", "project.new"),
                          {"Open project...", "project.open"},
                          {"Save project", "project.save"},
                          {"Save project as...", "project.saveAs"},
                          {"Import audio...", "project.import"},
                          {"Export mix / range...", "export.open"},
                          {"Update source with backup...", "resource.update"}})
          if (ImGui::MenuItem(item.first))
            soundExecute(item.second);
        if (ImGui::MenuItem("Recover autosave...")) {
          ImGui::CloseCurrentPopup();
          if (s.dirty) {
            soundMessage = "Save/discard the current project before recovery.";
          } else
            soundProjectLoad(documentPath("data/audio-recovery.rbaudio.json"));
        }
        if (ImGui::MenuItem("Close Sound Editor"))
          soundRequest("close");
        ImGui::EndPopup();
      }
      ImGui::SameLine();
      auto title = s.project.name + (s.dirty ? " *" : "");
      float titleWidth = ImGui::CalcTextSize(title.c_str()).x;
      ImGui::SetCursorPosX(
          std::max(ImGui::GetCursorPosX(),
                   (ImGui::GetWindowWidth() - titleWidth) * .5f));
      ImGui::TextDisabled("%s", title.c_str());
      if (ImGui::BeginPopupContextItem("Audio project name")) {
        if (ImGui::MenuItem("Rename project")) {
          std::snprintf(s.rename, sizeof s.rename, "%s",
                        s.project.name.c_str());
          soundDialog("Rename audio project");
        }
        ImGui::EndPopup();
      }
      ImGui::SameLine();
      float right = 330 * uiScale();
      if (ImGui::GetContentRegionAvail().x > right)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                             ImGui::GetContentRegionAvail().x - right);
      if (ImGui::Button("Layout"))
        soundExecute("workspace.layout");
      ImGui::SameLine();
      if (ImGui::Button("Shortcut"))
        soundExecute("shortcuts.open");
      ImGui::SameLine();
      if (ImGui::Button("Export"))
        soundExecute("export.open");
      ImGui::SameLine();
      if (ImGui::Button("Close"))
        soundRequest("close");
      ImGui::Separator();
      if (soundBusy()) {
        ImGui::ProgressBar(
            s.control ? s.control->progress.load() : 0,
            {ImGui::GetContentRegionAvail().x - 90 * uiScale(), 0},
            soundMessage.c_str());
        ImGui::SameLine();
        if (ImGui::Button("Cancel job") && s.control)
          s.control->cancel = true;
      }
      auto area = ImGui::GetContentRegionAvail();
      float status = ImGui::GetTextLineHeightWithSpacing() * 2,
            upper = std::max(250.f * uiScale(), (area.y - status) * .51f);
      upper = std::min(upper, std::max(100.f, area.y - 200 * uiScale()));
      if (ImGui::BeginTable("Audio upper panels", 3,
                            ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Media", ImGuiTableColumnFlags_WidthStretch,
                                44.5f);
        ImGui::TableSetupColumn("Player", ImGuiTableColumnFlags_WidthStretch,
                                29.5f);
        ImGui::TableSetupColumn("Inspector", ImGuiTableColumnFlags_WidthStretch,
                                26.f);
        ImGui::TableNextRow(0, upper);
        ImGui::TableNextColumn();
        ImGui::BeginChild("Media zone", {0, upper});
        soundLibrary();
        ImGui::EndChild();
        ImGui::TableNextColumn();
        ImGui::BeginChild("Player zone", {0, upper});
        soundPlayer();
        ImGui::EndChild();
        ImGui::TableNextColumn();
        ImGui::BeginChild("Inspector zone", {0, upper});
        soundInspector();
        ImGui::EndChild();
        ImGui::EndTable();
      }
      ImGui::BeginChild("Timeline zone", {0, -status});
      soundTimeline();
      ImGui::EndChild();
      if (!soundMessage.empty())
        ImGui::TextWrapped("%s", soundMessage.c_str());
      if (!s.ready && ImGui::Button("Prepare audio effects"))
        soundPrepare(false);
      soundDialogs();
    }
    ImGui::End();
    ImGui::PopStyleVar();
    if (!open)
      soundRequest("close");
    if (s.capture.active())
      s.capture.poll(s.recordGain);
    if (!s.ready && !soundBusy() && !s.dragging && !ImGui::IsAnyItemActive() &&
        s.prepareAttemptRevision != s.revision) {
      bool available = !s.project.clips.empty();
      for (auto &source : s.project.sources)
        available &= bool(source.pcm);
      if (available)
        soundPrepare(s.autoplay);
    }
    soundPlayback();
    double now = ImGui::GetTime();
    if (s.dirty && !s.dragging && now - s.lastAutosave > 30 && !soundBusy()) {
      s.lastAutosave = now;
      try {
        soundProjectSave(documentPath("data/audio-recovery.rbaudio.json"),
                         true);
        s.autosaveWarning = false;
      } catch (const std::exception &e) {
        if (!s.autosaveWarning) {
          soundMessage = std::string("Autosave failed: ") + e.what();
          s.autosaveWarning = true;
        }
      }
    }
  } catch (const std::exception &e) {
    soundMessage = e.what();
    soundStop();
    s.capture.end();
    s.recordAt = -1;
  }
}
