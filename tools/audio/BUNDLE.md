# Audio conversion helper

FTEBroom invokes a separate minimal FFmpeg 8.0.1 executable for WAV, Ogg Vorbis,
MP3 and FLAC decoding/encoding. Network protocols are disabled. FFmpeg is
LGPL-2.1-or-later; the included LAME encoder is LGPL-2.0-or-later.

Build with `scripts/build-audio.sh` from MSYS2 MinGW64 (GCC, binutils, pkgconf,
curl, git, diffutils and tar). The script pins both source revisions and ships
the complete corresponding source archives and license notices beside the
executable. Release packaging moves the source archives into the separate Windows Sources
asset; licenses stay beside the executable. Sources are not checked into
the editor Git history. You can rebuild or replace this independent helper.

FFmpeg source: https://ffmpeg.org/releases/ffmpeg-8.0.1.tar.xz

SHA256: `05ee0b03119b45c0bdb4df654b96802e909e0a752f72e4fe3794f487229e5a41`

LAME source: https://github.com/rbrito/lame/tree/af984672a95bb0eedc9f3193604e269c97cca162

The multitrack editor helper also builds static Ogg 1.3.5 and Vorbis 1.3.7
(BSD licenses). Their exact source revisions, source archives and notices are
included by `build-audio.sh`; CMake is required. New float PCM codecs/muxers and
the approved local DSP filters are enabled. Rebuild the helper with the native
module: older helper builds do not provide the new filter graph.

Ogg: https://github.com/xiph/ogg/tree/e1774cd77f471443541596e09078e78fdc342e4f

Vorbis: https://github.com/xiph/vorbis/tree/0657aee69dec8508a0011f47f3b69d7538e9d262
