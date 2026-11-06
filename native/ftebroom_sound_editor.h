// SPDX-License-Identifier: GPL-2.0-or-later
#include "ftebroom_audio_capture.h"
#include "ftebroom_audio_process.h"
#include <future>
#include <mutex>
namespace rbAudio = ftebroom::audio;
static bool soundFile(const std::filesystem::path &p) {
  auto e = entityLower(p.extension().u8string());
  return e == ".wav" || e == ".ogg" || e == ".mp3" || e == ".flac";
}
static bool soundEditorOpen = false, soundPlaying = false, soundTrim = false,
            soundStreamQueued = false;
static std::filesystem::path soundEditorPath;
static std::string soundMessage;
static float soundVolume = 1, soundSpeed = 1, soundStart = 0, soundEnd = 0;
static double soundPlayCursor = 0, soundQueueClock = 0;
static int soundTool = 0;
static ImVec2 soundVolumeMin, soundVolumeMax;
static plugaudiofuncs_t *soundAPI;
struct SoundPCMView {
  std::shared_ptr<const rbAudio::PCM> data;
  size_t size() const { return data ? data->size() : 0; }
};
static SoundPCMView soundPCM;
static void soundWrite(const std::filesystem::path &p, const std::string &b) {
  rbAudio::writeBytes(p, b);
}
struct SoundBinding {
  std::string id, label, chord, context;
};
struct SoundWorkResult {
  enum Kind {
    Import,
    Prepare,
    Export,
    Update,
    Load,
    Relink,
    Legacy
  } kind = Prepare;
  rbAudio::Source source;
  rbAudio::Project project;
  rbAudio::Mixer mixer;
  uint64_t revision = 0;
  rbAudio::Id sourceId = 0, track = 0;
  rbAudio::Frame at = 0;
  std::string error;
  bool loaded = false;
  std::filesystem::path path;
};
struct SoundState {
  rbAudio::Project project;
  rbAudio::History history;
  rbAudio::Mixer mixer;
  std::set<rbAudio::Id> selected;
  std::vector<rbAudio::Clip> clipboard;
  rbAudio::Id track = 0, source = 0, nextGroup = 1;
  rbAudio::Frame cursor = 0, queuedCursor = 0, viewStart = 0;
  double playStarted = 0;
  rbAudio::Frame playOrigin = 0;
  rbAudio::Id keyClip = 0;
  int keyIndex = -1;
  uint64_t overviewRevision = UINT64_MAX;
  std::vector<rbAudio::Peak> overview;
  uint64_t revision = 0, prepareAttemptRevision = UINT64_MAX;
  bool dirty = false, ready = false, focused = false, timelineFocused = false,
       autoplay = false;
  bool importAddTrack = false, exportRange = false, shortcutsLoaded = false,
       closing = false;
  float zoom = 1, peak = 0;
  int tab = 0, recordDevice = 0;
  float recordGain = 1;
  bool recordMute = true;
  int countdown = 3;
  double recordAt = -1, recordStarted = 0;
  rbAudio::Frame recordPosition = 0;
  rbAudio::Id recordTrack = 0;
  rbAudio::Capture capture;
  std::vector<std::string> devices;
  std::future<SoundWorkResult> job;
  std::shared_ptr<rbAudio::JobControl> control;
  std::filesystem::path projectPath;
  std::string savedProject, dialog, pendingDialog, pickerPurpose;
  char path[4096]{}, fileName[256] = "mix.wav", rename[256]{}, filter[256]{};
  int exportRate = 44100, exportChannels = 2, exportBits = 16,
      exportBitrate = 192;
  std::vector<SoundBinding> bindings;
  std::string bindingError;
  std::filesystem::path browseFolder;
  std::vector<std::filesystem::path> browseEntries;
  std::set<std::filesystem::path> browseDirectories;
  std::string afterSave, focusField;
  double lastAutosave = 0;
  bool autosaveWarning = false;
  rbAudio::Frame dragAt = 0, contextAt = 0;
  rbAudio::Id dragClip = 0;
  int dragMode = 0, rangeDrag = 0;
  bool dragging = false;
  rbAudio::Project beforeDrag;
  ImVec2 timelineMin, timelineMax;
  float pixelsPerSecond = 40;
  ~SoundState() {
    if (control)
      control->cancel = true;
    capture.end();
  }
};
static SoundState soundState;
static bool soundBusy() { return soundState.job.valid(); }
static std::filesystem::path soundFFmpeg() {
  auto p = documentPath("ftebroom.cfg").parent_path().parent_path() /
           "tools/audio/ffmpeg.exe";
#ifndef _WIN32
  p.replace_extension();
#endif
  return p;
}
static std::vector<std::filesystem::path> soundRoots() {
  std::vector<std::filesystem::path> roots;
  for (auto p : modFileRoots())
    if (!p.empty() && std::filesystem::is_directory(p))
      roots.push_back(std::filesystem::weakly_canonical(p));
  return roots;
}
static void soundValidate(const std::filesystem::path &uri,
                          const std::vector<std::filesystem::path> &roots) {
  std::filesystem::path physical, archive;
  std::string member;
  if (ftebroom::pak::split(uri, archive, member)) {
    if (member.empty() || !ftebroom::pak::validName(member))
      throw std::runtime_error("Choose a valid archive audio member");
    physical = archive;
  } else
    physical = uri;
  auto p = std::filesystem::weakly_canonical(physical);
  for (auto root : roots) {
    auto relative = p.lexically_relative(root);
    if (!relative.empty() && !relative.is_absolute() &&
        *relative.begin() != "..")
      return;
  }
  throw std::runtime_error(
      "Audio source must be inside a configured mod/authoring directory");
}
static std::string
soundReadResource(const std::filesystem::path &p,
                  const std::vector<std::filesystem::path> &roots) {
  soundValidate(p, roots);
  if (modFileMember(p))
    return ftebroom::pak::read(p);
  return rbAudio::readBytes(p);
}
static void
soundReplaceResource(const std::filesystem::path &p,
                     const std::string &original, const std::string &bytes,
                     const std::vector<std::filesystem::path> &roots) {
  soundValidate(p, roots);
  if (soundReadResource(p, roots) != original)
    throw std::runtime_error("Source changed outside the editor. Relink/reload "
                             "it or export a copy; original unchanged.");
  if (modFileMember(p)) {
    ftebroom::pak::replace(p, bytes);
    return;
  }
  auto backup = p;
  backup += ".ftebroom-" +
            std::to_string(std::chrono::high_resolution_clock::now()
                               .time_since_epoch()
                               .count()) +
            ".bak";
  auto temp = backup;
  temp += ".tmp";
  try {
    rbAudio::writeBytes(temp, bytes);
    std::filesystem::copy_file(p, backup);
    if (rbAudio::readBytes(p) != original)
      throw std::runtime_error(
          "Source changed while saving; original retained");
#ifdef _WIN32
    if (!ReplaceFileW(p.c_str(), temp.c_str(), nullptr,
                      REPLACEFILE_WRITE_THROUGH, nullptr, nullptr))
      throw std::runtime_error("Cannot replace source; original retained");
#else
    std::filesystem::rename(temp, p);
#endif
  } catch (...) {
    std::error_code ec;
    std::filesystem::remove(temp, ec);
    throw;
  }
}
static void soundStop() {
  if (soundPlaying) {
    auto &s = soundState;
    s.cursor = std::min(
        s.queuedCursor,
        s.playOrigin +
            rbAudio::Frame(std::max(0., ImGui::GetTime() - s.playStarted) *
                           s.project.rate));
  }
  soundPlaying = false;
  soundState.autoplay = false;
  if (soundAPI && soundStreamQueued)
    soundAPI->RawAudio(1836, nullptr, soundState.project.rate, 0, 2, 2, 1);
  soundStreamQueued = false;
}
static double soundDuration() {
  return soundPCM.data
             ? double(soundPCM.data->size() / 2) / soundState.project.rate
             : 0;
}
static double soundRangeStart() { return soundTool == 1 ? soundStart : 0; }
static double soundRangeEnd() {
  return soundTool == 1 ? soundEnd : soundDuration();
}
static rbAudio::Clip *soundSelected() {
  for (auto id : soundState.selected)
    if (auto c = soundState.project.clip(id))
      return c;
  return nullptr;
}
static void soundLegacySource(const rbAudio::Source &s) {
  soundEditorPath = std::filesystem::u8path(s.uri);
  soundPCM.data = s.pcm;
  soundVolume = soundSpeed = 1;
  soundStart = 0;
  soundEnd = float(s.length()) / soundState.project.rate;
  soundTrim = false;
  soundTool = 0;
}
static void soundRefreshMixer() {
  auto &s = soundState;
  s.ready = true;
  auto old = s.mixer;
  s.mixer.project = s.project;
  s.mixer.prepared.clear();
  for (auto &c : s.project.clips) {
    auto source = s.project.source(c.source);
    if (!source || !source->pcm) {
      s.ready = false;
      continue;
    }
    if (!rbAudio::needsProcessing(c)) {
      s.mixer.prepared[c.id] = {c.id, source->pcm, c.in};
      continue;
    }
    auto oldClip = old.project.clip(c.id);
    auto oldSource = oldClip ? old.project.source(oldClip->source) : nullptr;
    auto buffer = old.prepared.find(c.id);
    if (oldClip && oldSource && buffer != old.prepared.end() &&
        rbAudio::processingKey(c, *source, s.project.rate) ==
            rbAudio::processingKey(*oldClip, *oldSource, old.project.rate))
      s.mixer.prepared[c.id] = buffer->second;
    else
      s.ready = false;
  }
}
static void soundChanged(rbAudio::Project before, const std::string &label) {
  auto &s = soundState;
  try {
    s.project.validate();
  } catch (...) {
    s.project = std::move(before);
    throw;
  }
  if (rbAudio::serialize(before) == rbAudio::serialize(s.project))
    return;
  s.history.remember(before, label);
  s.dirty = true;
  s.revision++;
  soundRefreshMixer();
  if (!s.ready && soundPlaying) {
    soundStop();
    s.autoplay = true;
  }
}
static void soundApply(const std::string &label,
                       const std::function<void()> &action) {
  auto before = soundState.project;
  auto selected = soundState.selected;
  auto track = soundState.track;
  auto source = soundState.source;
  try {
    action();
    soundChanged(before, label);
  } catch (const std::exception &e) {
    soundState.project = std::move(before);
    soundState.selected = std::move(selected);
    soundState.track = track;
    soundState.source = source;
    soundMessage = e.what();
  }
}
static void soundHistory(bool redo) {
  auto &s = soundState;
  soundStop();
  if (redo ? s.history.forward(s.project) : s.history.back(s.project)) {
    s.revision++;
    s.dirty = true;
    s.selected.clear();
    soundRefreshMixer();
  }
}
static void soundJobStart(
    SoundWorkResult::Kind kind,
    const std::function<void(SoundWorkResult &,
                             const std::shared_ptr<rbAudio::JobControl> &)>
        &work) {
  if (soundBusy())
    throw std::runtime_error("Wait for or cancel the current audio operation");
  soundState.control = std::make_shared<rbAudio::JobControl>();
  auto control = soundState.control;
  auto revision = soundState.revision;
  soundState.job = std::async(std::launch::async, [=] {
    SoundWorkResult r;
    r.kind = kind;
    r.revision = revision;
    try {
      work(r, control);
    } catch (const std::exception &e) {
      r.error = e.what();
      r.loaded = false;
    }
    return r;
  });
}
static void soundPrepare(bool autoplay) {
  if (soundBusy())
    return;
  soundState.prepareAttemptRevision = soundState.revision;
  auto project = soundState.project;
  auto old = soundState.mixer;
  auto exe = soundFFmpeg();
  soundState.autoplay = autoplay;
  soundMessage = "Preparing audio effects...";
  soundJobStart(SoundWorkResult::Prepare, [=](auto &r, auto control) {
    r.mixer = rbAudio::prepare(exe, project, control, &old);
  });
}
static void soundSeek(double seconds, bool play) {
  soundStop();
  auto &s = soundState;
  s.cursor = std::min(rbAudio::frames(std::max(0., seconds), s.project.rate),
                      s.project.duration());
  soundPlayCursor = double(s.cursor) * 44100 / s.project.rate;
  soundQueueClock = ImGui::GetTime();
  s.queuedCursor = s.cursor;
  s.playOrigin = s.cursor;
  s.playStarted = soundQueueClock;
  if (!play)
    return;
  if (!s.ready) {
    soundPrepare(true);
    return;
  }
  soundPlaying = soundAPI && s.cursor < s.project.duration();
  if (!soundAPI)
    soundMessage =
        "No audio output device/API is available; offline export still works.";
}
static void soundOpen(const std::filesystem::path &path) {
  if (soundBusy()) {
    soundMessage = "Wait for or cancel the current audio operation";
    return;
  }
  try {
    modFileValidate(path);
    if (!soundFile(path))
      throw std::runtime_error("Choose WAV, OGG, MP3 or FLAC");
    soundEditorOpen = true;
    auto roots = soundRoots();
    auto exe = soundFFmpeg();
    int rate = soundState.project.rate;
    auto track = soundState.track;
    auto source = soundState.source;
    auto at = soundState.cursor;
    soundMessage = "Decoding audio...";
    soundJobStart(SoundWorkResult::Import, [=](auto &r, auto control) {
      r.source = rbAudio::decode(exe, path.u8string(),
                                 soundReadResource(path, roots), rate, control);
      r.track = track;
      r.at = at;
    });
  } catch (const std::exception &e) {
    soundMessage = e.what();
  }
}
static void soundProjectSave(const std::filesystem::path &p,
                             bool autosave = false) {
  auto &s = soundState;
  auto text = rbAudio::serialize(s.project).dump(2);
  auto temp = p;
  temp += ".tmp";
  try {
    rbAudio::writeBytes(temp, text);
#ifdef _WIN32
    if (!MoveFileExW(temp.c_str(), p.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
      throw std::runtime_error("Cannot save audio project");
#else
    std::filesystem::rename(temp, p);
#endif
  } catch (...) {
    std::error_code ec;
    std::filesystem::remove(temp, ec);
    throw;
  }
  if (!autosave) {
    s.projectPath = p;
    s.savedProject = text;
    s.dirty = false;
    soundMessage = "Audio project saved. Source audio is unchanged.";
  }
}
static void soundProjectLoad(const std::filesystem::path &p) {
  if (soundBusy()) {
    soundMessage = "Wait for or cancel the current audio operation before "
                   "loading/recovery.";
    return;
  }
  auto roots = soundRoots();
  auto exe = soundFFmpeg();
  soundStop();
  soundMessage = "Loading audio project and sources...";
  soundJobStart(SoundWorkResult::Load, [=](auto &r, auto control) {
    auto text = rbAudio::readBytes(p, 16u * 1024 * 1024);
    r.project = rbAudio::deserialize(rbAudio::Json::parse(text));
    r.loaded = true;
    r.path = p;
    for (size_t i = 0; i < r.project.sources.size(); i++) {
      auto &source = r.project.sources[i];
      try {
        auto decoded = rbAudio::decode(
            exe, source.uri,
            soundReadResource(std::filesystem::u8path(source.uri), roots),
            r.project.rate, control);
        if (source.fingerprint != decoded.fingerprint)
          throw std::runtime_error("Source changed: " + source.name);
        auto id = source.id;
        auto name = source.name;
        auto resource = source.resourceUri;
        auto expected = source.resourceFingerprint;
        source = std::move(decoded);
        source.id = id;
        source.name = name;
        source.resourceUri = resource;
        source.resourceFingerprint = expected;
        if (!resource.empty()) {
          auto target =
              soundReadResource(std::filesystem::u8path(resource), roots);
          if (rbAudio::fingerprint(target) == expected)
            source.targetOriginal =
                std::make_shared<const std::string>(std::move(target));
        }
      } catch (const std::exception &e) {
        rbAudio::cancelled(control);
        r.error += std::string(e.what()) + "\n";
      }
      if (control)
        control->progress =
            float(i + 1) / std::max(size_t(1), r.project.sources.size());
    }
  });
}
static void soundExport(const std::filesystem::path &target,
                        bool update = false, bool legacy = false) {
  auto &s = soundState;
  if (soundBusy())
    throw std::runtime_error("Wait for the current audio operation");
  if (!update) {
    auto destination = std::filesystem::weakly_canonical(target);
    for (const auto &source : s.project.sources)
      for (const auto &uri : {source.uri, source.resourceUri})
        if (!uri.empty() && !modFileMember(std::filesystem::u8path(uri)) &&
            std::filesystem::weakly_canonical(std::filesystem::u8path(uri)) ==
                destination)
          throw std::runtime_error(
              "Export would overwrite a project source. Choose another file or "
              "use Update source with backup.");
  }
  auto project = s.project;
  auto previous = s.mixer;
  auto roots = soundRoots();
  auto exe = soundFFmpeg();
  rbAudio::ExportOptions options;
  options.rate = s.exportRate;
  options.channels = s.exportChannels;
  options.bits = s.exportBits;
  options.bitrate = s.exportBitrate;
  options.begin = s.exportRange && project.range
                      ? std::min(project.rangeIn, project.rangeOut)
                      : 0;
  options.end = s.exportRange && project.range
                    ? std::max(project.rangeIn, project.rangeOut)
                    : project.duration();
  rbAudio::Source original;
  if (update) {
    auto c = soundSelected();
    if (!c || s.selected.size() != 1)
      throw std::runtime_error("Select one clip to update its source resource");
    auto src = project.source(c->source);
    if (!src || !src->original)
      throw std::runtime_error("Relink the source before updating");
    original = *src;
    project.clips = {*c};
    project.clips[0].at = 0;
    for (auto &t : project.tracks) {
      t.muted = false;
      t.solo = false;
      t.db = 0;
    }
    project.masterDb = 0;
    options.begin = 0;
    options.end = project.duration();
    options.rate = original.rate;
    options.channels = original.channels;
  }
  soundMessage =
      update ? "Updating resource with backup..." : "Exporting audio mix...";
  soundJobStart(
      legacy   ? SoundWorkResult::Legacy
      : update ? SoundWorkResult::Update
               : SoundWorkResult::Export,
      [=](auto &r, auto control) {
        auto mixer = rbAudio::prepare(exe, project, control, &previous);
        if (update) {
          rbAudio::Workspace w;
          auto output =
              w.path /
              ("resource" +
               std::filesystem::u8path(original.uri).extension().u8string());
          rbAudio::exportAudio(exe, mixer, output, options, control);
          rbAudio::cancelled(control);
          auto result = rbAudio::readBytes(output);
          auto target = std::filesystem::u8path(original.resourceUri.empty()
                                                    ? original.uri
                                                    : original.resourceUri);
          if (legacy) {
            soundReplaceResource(target, *original.original, result, roots);
            r.source =
                rbAudio::decode(exe, target.u8string(), std::move(result),
                                project.rate, control);
          } else {
            auto expected = original.resourceUri.empty()
                                ? original.original
                                : original.targetOriginal;
            if (!expected)
              throw std::runtime_error("Source resource changed; relink/reload "
                                       "or export a copy before updating");
            std::filesystem::path physical, archive;
            std::string member;
            physical = ftebroom::pak::split(target, archive, member) ? archive
                                                                     : target;
            auto folder = physical.parent_path() / ".reyond-audio-sources";
            std::filesystem::create_directories(folder);
            auto key = original.fingerprint;
            std::replace(key.begin(), key.end(), ':', '-');
            auto backup = folder / (key + target.extension().u8string());
            soundValidate(backup, roots);
            if (std::filesystem::exists(backup)) {
              if (rbAudio::readBytes(backup) != *original.original)
                throw std::runtime_error(
                    "Source snapshot collision; resource unchanged");
            } else
              rbAudio::writeBytes(backup, *original.original);
            rbAudio::cancelled(control);
            soundReplaceResource(target, *expected, result, roots);
            r.source = original;
            r.source.uri = backup.u8string();
            r.source.resourceUri = target.u8string();
            r.source.resourceFingerprint = rbAudio::fingerprint(result);
            r.source.targetOriginal =
                std::make_shared<const std::string>(std::move(result));
          }
          r.sourceId = original.id;
        } else {
          rbAudio::exportAudio(exe, mixer, target, options, control);
          r.path = target;
        }
      });
}
// Compatibility adapter for the previous one-file API and its regression tests.
// The UI's Update source action uses soundExport directly and never flattens a
// project.
static void soundLaunch(bool save) {
  if (!save)
    return;
  if (soundBusy())
    return;
  auto &s = soundState;
  auto source = s.project.source(s.source);
  if (!source)
    return;
  rbAudio::Project p;
  p.rate = s.project.rate;
  auto sid = p.addSource(*source), tid = p.addTrack("Audio"),
       id = p.addClip(sid, tid);
  auto &c = *p.clip(id);
  c.in = soundTrim ? rbAudio::frames(soundStart, p.rate) : 0;
  c.out = soundTrim ? rbAudio::frames(soundEnd, p.rate) : source->length();
  c.speed = soundSpeed;
  c.pitchLinked = true;
  c.db = soundVolume <= 0 ? -96 : 20 * std::log10(soundVolume);
  s.project = p;
  s.selected = {id};
  s.source = sid;
  s.track = tid;
  s.exportRange = false;
  try {
    soundExport({}, true, true);
  } catch (const std::exception &e) {
    soundMessage = e.what();
  }
}
static void soundFinish() {
  auto &s = soundState;
  if (!soundBusy() ||
      s.job.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
    return;
  auto result = s.job.get();
  if ((!result.loaded || result.kind != SoundWorkResult::Load) &&
      !result.error.empty()) {
    soundMessage = result.error;
    s.autoplay = false;
    return;
  }
  if (result.kind == SoundWorkResult::Import) {
    soundApply("Import audio", [&] {
      auto sid = s.project.addSource(std::move(result.source));
      auto tid = result.track;
      auto t = s.project.track(tid);
      if (!t || t->locked || s.importAddTrack)
        tid = s.project.addTrack("Audio " +
                                 std::to_string(s.project.tracks.size() + 1));
      auto id = s.project.addClip(sid, tid, result.at);
      s.project.validate();
      s.track = tid;
      s.source = sid;
      s.selected = {id};
      soundLegacySource(*s.project.source(sid));
      soundMessage = "Audio imported. Sources remain unchanged.";
    });
    s.importAddTrack = false;
  } else if (result.kind == SoundWorkResult::Prepare) {
    if (result.revision == s.revision) {
      s.mixer = std::move(result.mixer);
      s.ready = true;
      soundMessage.clear();
      if (s.autoplay) {
        s.autoplay = false;
        soundQueueClock = ImGui::GetTime();
        s.queuedCursor = s.cursor;
        s.playOrigin = s.cursor;
        s.playStarted = soundQueueClock;
        soundPlaying = soundAPI && s.cursor < s.project.duration();
      }
    }
  } else if (result.kind == SoundWorkResult::Load) {
    s.project = std::move(result.project);
    s.projectPath = result.path;
    s.history.clear();
    s.clipboard.clear();
    s.selected.clear();
    s.cursor = 0;
    s.track = s.project.tracks.empty() ? 0 : s.project.tracks.front().id;
    s.source = s.project.sources.empty() ? 0 : s.project.sources.front().id;
    s.revision++;
    s.savedProject = rbAudio::serialize(s.project).dump(2);
    s.dirty = false;
    soundRefreshMixer();
    soundMessage =
        result.error.empty()
            ? "Project loaded."
            : "Missing or changed sources. Relink before playback/export:\n" +
                  result.error;
  } else if (result.kind == SoundWorkResult::Update ||
             result.kind == SoundWorkResult::Legacy) {
    auto source = s.project.source(result.sourceId);
    if (source) {
      auto before = s.project;
      auto id = source->id;
      *source = std::move(result.source);
      source->id = id;
      if (result.kind == SoundWorkResult::Legacy) {
        for (auto &c : s.project.clips)
          if (c.source == id) {
            c.in = 0;
            c.out = source->length();
            c.speed = 1;
            c.db = 0;
            c.fadeIn = c.fadeOut = 0;
            c.keys.clear();
            c.fx = {};
          }
        soundLegacySource(*source);
      }
      if (result.kind == SoundWorkResult::Legacy)
        soundChanged(before, "Update source");
      else {
        auto current = *source;
        auto rewrite = [&](rbAudio::Project &project) {
          for (auto &v : project.sources)
            if (v.fingerprint == current.fingerprint &&
                (v.uri == before.source(id)->uri ||
                 v.resourceUri == current.resourceUri)) {
              v.uri = current.uri;
              v.resourceUri = current.resourceUri;
              v.resourceFingerprint = current.resourceFingerprint;
              v.targetOriginal = current.targetOriginal;
            }
        };
        for (auto &h : s.history.undo)
          rewrite(h.project);
        for (auto &h : s.history.redo)
          rewrite(h.project);
        s.dirty = true;
        s.revision++;
        soundRefreshMixer();
      }
      modFilesRefresh = true;
      soundMessage = "Resource updated; backup and immutable project source "
                     "retained. Loop/cue metadata is not preserved.";
    }
  } else if (result.kind == SoundWorkResult::Relink) {
    auto source = s.project.source(result.sourceId);
    if (source) {
      auto before = s.project;
      auto id = source->id;
      *source = std::move(result.source);
      source->id = id;
      soundChanged(before, "Relink source");
      soundMessage =
          "Source relinked. Review clip boundaries if its duration changed.";
    }
  } else
    soundMessage = "Exported audio: " + result.path.u8string();
}
static void soundPlayback() {
  auto &s = soundState;
  if (!soundPlaying || !soundAPI || !s.ready)
    return;
  double now = ImGui::GetTime();
  if (now - soundQueueClock > .25) {
    soundStop();
    soundMessage =
        "Audio output underrun: playback stopped. Reduce project load.";
    return;
  }
  auto end = s.project.range ? std::max(s.project.rangeIn, s.project.rangeOut)
                             : s.project.duration();
  std::array<float, 2048> mix{};
  std::array<int16_t, 2048> pcm{};
  int blocks = 0;
  while (soundQueueClock < now + .06 && soundPlaying && blocks++ < 6) {
    auto count =
        size_t(std::max(rbAudio::Frame(0),
                        std::min(rbAudio::Frame(1024), end - s.queuedCursor)));
    if (!count) {
      break;
    }
    s.mixer.render(s.queuedCursor, count, mix.data(), &s.peak);
    for (size_t n = 0; n < count * 2; n++) {
      pcm[n] = int16_t(std::clamp(mix[n], -1.f, 1.f) * 32767);
    }
    soundAPI->RawAudio(1836, pcm.data(), s.project.rate, int(count), 2, 2, 1);
    soundStreamQueued = true;
    s.queuedCursor += count;
    soundQueueClock += double(count) / s.project.rate;
  }
  s.cursor =
      std::min(s.queuedCursor,
               s.playOrigin + rbAudio::Frame(std::max(0., now - s.playStarted) *
                                             s.project.rate));
  soundPlayCursor = double(s.cursor) * 44100 / s.project.rate;
  if (s.cursor >= end) {
    soundPlaying = false;
    s.cursor = end;
  }
}
static void soundExecute(const std::string &id);
static void soundBindings() {
  auto &s = soundState;
  if (s.shortcutsLoaded)
    return;
  s.shortcutsLoaded = true;
  s.bindings = {
#include "ftebroom_audio_shortcuts.inc"
  };
  try {
    auto path = documentPath("data/audio-shortcuts.json");
    if (std::filesystem::exists(path)) {
      auto j = rbAudio::Json::parse(rbAudio::readBytes(path, 128 * 1024));
      for (auto &b : s.bindings)
        if (j.contains(b.id))
          b.chord = j.at(b.id).get<std::string>();
    }
  } catch (const std::exception &e) {
    s.bindingError = e.what();
  }
}
struct SoundChord {
  int key = 0;
  bool ctrl = false, shift = false, alt = false, valid = false;
};
static SoundChord soundChord(std::string text) {
  SoundChord chord;
  if (text.empty()) {
    chord.valid = true;
    return chord;
  }
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char c) { return char(std::tolower(c)); });
  size_t at = 0;
  while (text.find('+', at) != text.npos) {
    auto end = text.find('+', at);
    auto modifier = text.substr(at, end - at);
    if (modifier == "ctrl" && !chord.ctrl)
      chord.ctrl = true;
    else if (modifier == "shift" && !chord.shift)
      chord.shift = true;
    else if (modifier == "alt" && !chord.alt)
      chord.alt = true;
    else
      return chord;
    at = end + 1;
  }
  auto name = text.substr(at);
  if (name == "delete")
    name = "del";
  if (name == "left" || name == "right" || name == "up" || name == "down")
    name += "arrow";
  if (name.empty())
    return chord;
  chord.key = input->GetKeyCode(name.c_str(), nullptr);
  chord.valid = chord.key > 0;
  return chord;
}
static bool soundKeyboardEvent(int key, bool down) {
  auto &s = soundState;
  if (!soundEditorOpen || !s.focused)
    return false;
  for (auto mouse : mouseKeys)
    if (key == mouse)
      return false;
  if (key == wheelUp || key == wheelDown)
    return false;
  if (!down || ImGui::GetIO().WantTextInput)
    return true;
  if (ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId |
                                      ImGuiPopupFlags_AnyPopupLevel))
    return true;
  soundBindings();
  for (const auto &binding : s.bindings) {
    if (binding.chord.empty())
      continue;
    auto chord = soundChord(binding.chord);
    if (!chord.valid || chord.key != key || chord.ctrl != ctrlHeld ||
        chord.shift != shiftHeld || chord.alt != altHeld)
      continue;
    if ((binding.context == "audio-timeline" ||
         binding.context == "track-context-menu") &&
        !s.timelineFocused)
      continue;
    soundExecute(binding.id);
    break;
  }
  return true;
}
#include "ftebroom_sound_ui.h"
