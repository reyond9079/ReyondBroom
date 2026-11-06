# Sound Editor

Open mono/stereo WAV, OGG, MP3 or FLAC from the Mod file manager, including PAK/PK3 members.
The editor opens a full-window audio workspace with Media on the left, the master
monitor in the middle, the clip inspector on the right and tracks below.
The layout and audio shortcut preset follow the approved REYON-4 mockup v1.
This is an audio-only adaptation of the public CapCut Desktop references recorded
in the mockup; it does not depend on CapCut or claim an exact unknown CapCut build.

## Editing

Import adds a source and a clip at the playhead. Double-click a library source or
drag it onto a track to add another clip. Dropping below the tracks creates a
track. A locked destination refuses editing. Right-click track names to rename,
reorder or delete tracks. Lock / Visibility / Mute / Solo appear in that order;
Visibility hides the waveform, while Mute and Solo affect both preview and export.

Click clips to select; Ctrl-click selects several. Drag bodies to move, edges to
trim, the two upper fade handles to change fades, and volume points to edit their
time and gain. Right-click a volume point to delete it. The inspector also edits
all points numerically; interpolation is linear in dB. Fades multiply when they
overlap. Trim retains the decoded source, so dragging the edge back restores the
source audio. History retains up to 100 edits, subject to a shared-buffer limit.

A selects, B chooses the split tool, Ctrl+B splits selected clips at the playhead,
Ctrl+Shift+B splits all unlocked clips. Q/W trim to the playhead. Standard
copy/cut/paste/duplicate/group and undo/redo are available in context menus and
the Shortcut panel. The Select icon's context menu switches tools. The full
71-action preset is visible and can be remapped; invalid/duplicate chords cannot
be saved. Project and audio input focus prevent audio keys from editing the map.
Text fields and dialogs take priority over audio shortcuts.

Space toggles playback/pause; Stop returns to the start of the active range.
Click the ruler or master overview to seek. I/O set the export range; drag its
handles on the ruler. Left/Right step 10 ms, Shift steps 100 ms. The bottom
scrollbar and zoom/fit controls navigate the timeline. Source boundaries and the
playhead attract clip moves when Snapping is enabled. Magnet closes gaps on the
first track, respecting linked groups and locks. Linkage moves/copies/deletes
whole groups; ripple delete closes the union of deleted intervals on affected
tracks. Preview axis displays a hover line without moving the playhead.

## Processing and recording

Basic edits gain, fades, balance, normalization and noise/voice processing.
Speed is 0.25–4×; Voice tone change links pitch to speed, while turning it off
uses time stretching to preserve pitch. Effects prepare on a worker and reuse
immutable buffers. Stale or missing results are never silently played as dry audio.

Normalization uses FFmpeg's two-pass `loudnorm` profile −16 LUFS / −1 dBTP and
shows the input measurements. Silence stays silent. Reduce noise is FFT spectral
reduction (`afftdn`, noise floor −35 dB, reduction 5–30 dB). Enhance voice applies
80 Hz high-pass, 12 kHz low-pass and 3:1 compression. Local voice filters are
Original, Low pitch, High pitch, Radio, Echo and Tremolo. Intensity controls noise
reduction, echo decay and tremolo depth; fixed pitch/radio presets use fixed
parameters. Bypass disables effects while retaining speed and edit parameters.
Processing is per clip: splitting a processed clip reanalyses its segments;
filters can have new boundary transients. Echo tails are bounded by clip duration.

The microphone button opens device selection, input gain, countdown and project
mute. Stop inserts a new WAV source/clip at the original playhead. Windows capture
uses the system WaveIn API; Linux uses the system SDL2 capture queue. Both request
44.1 kHz stereo PCM. Devices unable to provide/convert that format report an error.
Recordings are kept in `.reyond-recordings` under the configured mod root.

## Project, export and resource update

These are separate actions:

- **Save Project** writes versioned `.rbaudio.json` edit decisions, source URIs and
  fingerprints. It does not render or overwrite source audio. The Menu opens,
  creates, renames and recovers projects. Autosave runs every 30 seconds while
  idle; Menu → Recover autosave loads `data/audio-recovery.rbaudio.json` after the
  current project is saved/discarded. Missing/changed sources stay offline and can
  be relinked. Failed project loads leave the current project intact.
- **Export** mixes all audible tracks or In/Out to WAV (16/24 bit), OGG, MP3 or
  FLAC, with output rate/channels. Conversion is cancellable; the target is only
  replaced after successful encoding and destination staging. Existing files
  require confirmation. The master overview samples the real output graph for
  navigation; it is not a full-resolution peak analysis.
- **Update source with backup** renders exactly one selected clip in its original
  rate/channel configuration. It checks the live resource bytes, retains a file
  or archive backup and replaces the file/member. Other PAK/PK3 entries remain
  intact. The project's original source is retained separately in
  `.reyond-audio-sources`, so the operation does not bake every clip in the
  project or make later edits depend on an overwritten source. Save the project
  after updating; keep its source snapshot directory with the mod. Compressed
  formats reencode; loop/cue/custom metadata is not preserved.

Project rate defaults to 44.1 kHz; Layout allows 48 kHz for an empty project.
Preview/export use the same float32 stereo graph with dB automation, gain,
balance, fades and mute/solo. The final graph clamps sample peaks to 0 dBFS and
displays overload before clamping. Clip normalization's −1 dBTP target is not a
guarantee about the true peak of a later sum, resampling or lossy encoding.

## Runtime requirements and limits

Linux needs the system FFmpeg (invoked through the bundled wrapper); recording
also needs `libSDL2-2.0.so.0`. Windows must rebuild the audio helper using the
updated `scripts/build-audio.sh`: the old helper lacks float PCM and the new DSP
filters. The isolated helper enables only file protocol, four audio formats and
the required filters. OGG uses libvorbis; MP3 uses LAME. The Windows build includes pinned static
Ogg/Vorbis sources and license notices. Worker failures include converter diagnostics.

Limits: 64 tracks, 256 sources, 4096 clips, 10,000 keys per clip/markers, 24-hour
time coordinates, 512 MiB encoded input, less than 256 MiB decoded/processed per
clip, 512 MiB project source buffers and 512 MiB prepared buffers. History shares
immutable PCM and has a separate 512 MiB budget. These are short game-audio
projects, not an unbounded streaming workstation. Converter jobs time out after
120 seconds and can be cancelled. Read/decode/DSP/export run outside the UI thread.

Preview keeps roughly 60–85 ms in FTE's RawAudio queue. Its API exposes no consumed
sample counter: the visible playhead estimates consumption from the monotonic
clock and queue origin. Device/driver latency is additional and is not measured.
A UI stall over 250 ms stops playback with an underrun message. Physical output
and microphone quality/latency still need device-level verification on each OS.

Validation records and native screenshots: [REYON-4 QA](qa/REYON-4/README.md).
