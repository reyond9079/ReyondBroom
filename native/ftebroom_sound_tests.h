// SPDX-License-Identifier: GPL-2.0-or-later
// Included inside testDesktop(): opt-in native integration checks, never run in
// normal editor sessions. Fixtures live in ftebroom_test_import_root.
if (std::string_view(mode).substr(0, 6) == "audio4") {
  auto &s = soundState;
  auto &p = s.project;
  auto folder = std::filesystem::u8path(root);
  static rbAudio::Json saved;
  static std::string original;
  static rbAudio::Id voice = 0;
  auto finish = [&] {
    soundFinish();
    expect(!soundBusy(), "audio4_worker_completed");
  };
  std::string test = mode;
  if (test == "audio4begin") {
    gameProfile = 0;
    std::snprintf(baseq3, sizeof baseq3, "%s", root);
    uiTheme = 0;
    uiScaleChoice = 0;
    uiStyle();
    modelViewerOpen = false;
    modFilesVisible = false;
    modFilesOpen = false;
    soundStop();
    p = {};
    s.history.clear();
    s.selected.clear();
    s.track = s.source = 0;
    s.cursor = 0;
    soundOpen(folder / "voice.wav");
    expect(soundBusy(), "audio4_async_import");
  } else if (test == "audio4music") {
    finish();
    expect(p.sources.size() == 1 && p.sources[0].channels == 1,
           "audio4_mono_import_metadata");
    s.importAddTrack = true;
    soundOpen(folder / "music.ogg");
  } else if (test == "audio4impact") {
    finish();
    s.importAddTrack = true;
    soundOpen(folder / "impact.flac");
  } else if (test == "audio4edit") {
    finish();
    expect(p.sources.size() == 3 && p.tracks.size() == 3 && p.clips.size() == 3,
           "audio4_three_real_sources_tracks");
    p.name = "Citadel - sound design";
    const char *names[] = {"Voice", "Music", "Sound effects"};
    for (size_t i = 0; i < p.tracks.size(); i++)
      p.tracks[i].name = names[i];
    auto &c = p.clips[0];
    voice = c.id;
    c.at = 2 * p.rate;
    c.db = -3;
    c.fadeIn = p.rate / 4;
    c.fadeOut = p.rate / 2;
    c.keys = {{0, -6}, {p.rate, 0}, {4 * p.rate, -2}, {6 * p.rate, -8}};
    p.clips[1].db = -18;
    p.clips[1].fadeIn = p.rate;
    p.clips[1].fadeOut = 2 * p.rate;
    p.clips[2].at = rbAudio::frames(5.5, p.rate);
    auto copy = p.clips[2];
    copy.id = p.next++;
    copy.at = rbAudio::frames(10.5, p.rate);
    p.clips.push_back(copy);
    p.range = true;
    p.rangeIn = 2 * p.rate;
    p.rangeOut = 12 * p.rate;
    p.markers = {{rbAudio::frames(5.5, p.rate), "Door"},
                 {rbAudio::frames(10.5, p.rate), "Impact"}};
    s.selected = {voice};
    s.track = p.clips[0].track;
    s.source = p.clips[0].source;
    s.cursor = rbAudio::frames(3.6, p.rate);
    s.dirty = true;
    s.revision++;
    soundRefreshMixer();
    s.history.clear();
    soundMessage.clear();
    frame();
    ImGui::SetWindowFocus("Sound editor");
    frame();
    expect(s.ready, "audio4_dry_mix_ready");
  } else if (test == "audio4keys") {
    ImGui::SetWindowFocus("Sound editor");
    frame();
    s.focused = s.timelineFocused = true;
    auto before = rbAudio::serialize(p);
    auto previous = s.cursor;
    s.cursor = 4 * p.rate;
    auto count = p.clips.size();
    ctrlHeld = true;
    expect(soundKeyboardEvent(input->GetKeyCode("b", nullptr), true),
           "audio4_ctrl_b_consumed");
    ctrlHeld = false;
    expect(p.clips.size() == count + 1, "audio4_ctrl_b_audio_split");
    soundExecute("edit.undo");
    s.cursor = previous;
    expect(rbAudio::serialize(p) == before, "audio4_independent_undo");
    s.selected = {voice};
    bool prior = ImGui::GetIO().WantTextInput;
    ImGui::GetIO().WantTextInput = true;
    ctrlHeld = true;
    soundKeyboardEvent(input->GetKeyCode("b", nullptr), true);
    ctrlHeld = false;
    ImGui::GetIO().WantTextInput = prior;
    expect(rbAudio::serialize(p) == before,
           "audio4_text_focus_blocks_shortcut");
    s.focused = false;
    expect(!soundKeyboardEvent(input->GetKeyCode("b", nullptr), true),
           "audio4_outside_focus_passes_to_map");
    s.focused = true;
    frame();
    expect(uiBlocksScene(900, 700), "audio4_timeline_mouse_blocks_map");
    s.selected = {voice};
    s.track = p.clip(voice)->track;
    s.cursor = 4 * p.rate;
    s.focused = s.timelineFocused = true;
    auto ctrl = input->GetKeyCode("ctrl", nullptr),
         b = input->GetKeyCode("b", nullptr);
    count = p.clips.size();
    event(1, ctrl, 0, 600, 800, testW, testH);
    event(1, b, 0, 600, 800, testW, testH);
    event(2, b, 0, 600, 800, testW, testH);
    event(2, ctrl, 0, 600, 800, testW, testH);
    expect(p.clips.size() == count + 1, "audio4_engine_event_routes_to_audio");
    soundExecute("edit.undo");
    s.cursor = previous;
    s.selected = {voice};
  } else if (test == "audio4save") {
    saved = rbAudio::serialize(p);
    original = rbAudio::readBytes(folder / "voice.wav");
    soundProjectSave(folder / "session.rbaudio.json");
    soundProjectSave(folder / "recovery.rbaudio.json", true);
    expect(rbAudio::readBytes(folder / "voice.wav") == original,
           "audio4_save_project_preserves_source");
    soundProjectLoad(folder / "session.rbaudio.json");
  } else if (test == "audio4load") {
    finish();
    expect(rbAudio::serialize(p) == saved && s.ready,
           "audio4_project_roundtrip");
    s.selected = {voice};
    s.source = p.clip(voice)->source;
    s.track = p.clip(voice)->track;
    s.exportRange = true;
    s.exportRate = 48000;
    s.exportChannels = 1;
    soundExport(folder / "range.wav");
  } else if (test == "audio4export") {
    finish();
    auto mix =
        rbAudio::decode(soundFFmpeg(), "range.wav",
                        rbAudio::readBytes(folder / "range.wav"), p.rate);
    expect(mix.channels == 1 && mix.rate == 48000 &&
               std::abs(double(mix.length()) / p.rate - 10) < .002,
           "audio4_range_export_48k_mono");
    s.exportRange = false;
    soundExport({}, true);
  } else if (test == "audio4update") {
    finish();
    auto source = p.source(p.clip(voice)->source);
    expect(rbAudio::readBytes(folder / "voice.wav") != original,
           "audio4_source_update_real_audio");
    expect(source && source->original && *source->original == original &&
               rbAudio::readBytes(std::filesystem::u8path(source->uri)) ==
                   original,
           "audio4_immutable_source_snapshot");
    expect(p.clip(voice)->db == -3 && p.clip(voice)->at == 2 * p.rate,
           "audio4_source_update_retains_edit_decisions");
    bool backup = false;
    for (auto &e : std::filesystem::directory_iterator(folder))
      if (e.path().extension() == ".bak")
        backup = true;
    expect(backup, "audio4_source_backup");
    soundProjectSave(folder / "updated.rbaudio.json");
    soundProjectLoad(folder / "updated.rbaudio.json");
  } else if (test == "audio4updatedload") {
    finish();
    expect(s.ready && p.sources[0].pcm && *p.sources[0].original == original,
           "audio4_updated_project_reopens_original");
    saved = rbAudio::serialize(p);
    rbAudio::writeBytes(folder / "bad.rbaudio.json", "{broken");
    soundProjectLoad(folder / "bad.rbaudio.json");
  } else if (test == "audio4badload") {
    finish();
    expect(rbAudio::serialize(p) == saved,
           "audio4_failed_load_retains_project");
    auto roots = soundRoots();
    auto target = folder / "conflict.wav";
    rbAudio::writeBytes(target, "external");
    bool rejected = false;
    try {
      soundReplaceResource(target, "expected", "new", roots);
    } catch (...) {
      rejected = true;
    }
    expect(rejected && rbAudio::readBytes(target) == "external",
           "audio4_external_change_guard");
    rejected = false;
    try {
      soundValidate(folder / "../outside.wav", roots);
    } catch (...) {
      rejected = true;
    }
    expect(rejected, "audio4_roots_guard");
    auto archive = folder / "sounds.pak";
    {
      std::ofstream out(archive, std::ios::binary);
      out.write("PACK", 4);
      ftebroom::pak::put(out, 12);
      ftebroom::pak::put(out, 0);
    }
    ftebroom::pak::replace(archive / "sound/voice.wav", original);
    ftebroom::pak::replace(archive / "keep.txt", "untouched");
    soundReplaceResource(archive / "sound/voice.wav", original, "replacement",
                         roots);
    expect(ftebroom::pak::read(archive / "keep.txt") == "untouched" &&
               ftebroom::pak::read(archive / "sound/voice.wav") ==
                   "replacement",
           "audio4_pak_update_preserves_neighbors");
    s.selected = {voice};
    s.tab = 0;
    soundMessage.clear();
  } else if (test == "audio4voice") {
    s.tab = 1;
  } else if (test == "audio4speed") {
    s.tab = 2;
  } else if (test == "audio4exportdialog") {
    soundExecute("export.open");
  } else if (test == "audio4shortcuts") {
    soundExecute("shortcuts.open");
  } else if (test == "audio4record") {
    soundExecute("record.open");
  } else if (test == "audio4recordstart") {
    s.recordPosition = s.cursor;
    s.recordTrack = s.track;
    s.countdown = 1;
    s.recordAt = ImGui::GetTime() + 1;
  } else if (test == "audio4recordfinish") {
    expect(s.capture.active(), "audio4_capture_started_after_countdown");
    soundRecordFinish();
    ImGui::ClosePopupToLevel(0, true);
    s.dialog.clear();
  } else if (test == "audio4recordcheck") {
    finish();
    expect(p.sources.size() == 4 && p.sources.back().pcm &&
               p.sources.back().length() > p.rate,
           "audio4_recording_creates_clip_source");
    expect(
        std::filesystem::exists(std::filesystem::u8path(p.sources.back().uri)),
        "audio4_recording_file_persisted");
  } else if (test == "audio4light") {
    uiTheme = 3;
    uiStyle();
  } else if (test == "audio4retro") {
    uiTheme = 2;
    uiStyle();
  } else if (test == "audio4scale150") {
    uiTheme = 0;
    uiScaleChoice = 2;
    uiStyle();
  } else if (test == "audio4scale200") {
    uiTheme = 0;
    uiScaleChoice = 4;
    uiStyle();
  } else if (test == "audio4dialogclose") {
    ImGui::ClosePopupToLevel(0, true);
    s.dialog.clear();
    s.pendingDialog.clear();
  }
  return;
}
