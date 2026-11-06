#!/usr/bin/env bash
# Build the isolated audio helper with MSYS2 MinGW64. No network protocols are enabled.
set -eu
export PATH=/mingw64/bin:/usr/bin:$PATH
destination=$(cd "$(dirname "$0")/.." && pwd)/tools/audio
workspace=${1:?Pass a dedicated build directory}
mkdir -p "$workspace" "$destination"
cd "$workspace"
if [ ! -f ffmpeg-8.0.1.tar.xz ]; then curl -4 --retry 5 --retry-all-errors --retry-delay 2 --connect-timeout 20 --max-time 180 -fL https://ffmpeg.org/releases/ffmpeg-8.0.1.tar.xz -o ffmpeg-8.0.1.tar.xz; fi
echo '05ee0b03119b45c0bdb4df654b96802e909e0a752f72e4fe3794f487229e5a41  ffmpeg-8.0.1.tar.xz' | sha256sum -c -
if [ ! -d ffmpeg-8.0.1 ]; then tar -xf ffmpeg-8.0.1.tar.xz; fi
if [ ! -d lame-source ]; then git clone https://github.com/rbrito/lame.git lame-source; fi
git -C lame-source checkout --detach af984672a95bb0eedc9f3193604e269c97cca162
cd lame-source
./configure --host=x86_64-w64-mingw32 --prefix="$PWD/../prefix" --disable-shared --enable-static --disable-frontend --disable-asm
mingw32-make -j8 MAKE=mingw32-make SHELL=C:/msys64/usr/bin/sh.exe
mingw32-make install MAKE=mingw32-make SHELL=C:/msys64/usr/bin/sh.exe
git archive --format=zip --output="$destination/lame-source.zip" HEAD
cp COPYING "$destination/LAME-LGPL.txt"
cd ../ffmpeg-8.0.1
PKG_CONFIG_PATH="$PWD/../prefix/lib/pkgconfig" ./configure \
 --target-os=mingw32 --arch=x86_64 --cc=gcc --cxx=g++ \
 --disable-everything --disable-autodetect --disable-network --disable-doc \
 --disable-x86asm --disable-debug --enable-small --disable-shared --enable-static \
 --enable-ffmpeg --enable-protocol=file \
 --enable-demuxer=wav,ogg,mp3,flac,pcm_s16le,pcm_f32le --enable-muxer=wav,ogg,mp3,flac,pcm_s16le,pcm_f32le,null \
 --enable-decoder=pcm_s16le,pcm_u8,pcm_s24le,pcm_s32le,pcm_f32le,vorbis,mp3,mp3float,flac \
 --enable-encoder=pcm_s16le,pcm_s24le,pcm_f32le,vorbis,flac,libmp3lame --enable-libmp3lame \
 --enable-filter=aresample,volume,atrim,asetpts,asetrate,aformat,anull,atempo,loudnorm,afftdn,highpass,lowpass,acompressor,aecho,tremolo \
 --enable-parser=mpegaudio,flac,vorbis \
 --extra-cflags="-I$PWD/../prefix/include" --extra-ldflags="-static -L$PWD/../prefix/lib" || { tail -n 100 ffbuild/config.log; exit 1; }
mingw32-make -j8 MAKE=mingw32-make SHELL=C:/msys64/usr/bin/sh.exe
cp ffmpeg.exe "$destination/ffmpeg.exe"
cp COPYING.LGPLv2.1 "$destination/FFmpeg-LGPL-2.1.txt"
cp ../ffmpeg-8.0.1.tar.xz "$destination/ffmpeg-8.0.1-source.tar.xz"
