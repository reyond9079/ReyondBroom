#!/usr/bin/env bash
# Build the isolated audio helper with MSYS2 MinGW64. No network protocols are enabled.
set -eu
export PATH=/mingw64/bin:/usr/bin:$PATH
destination=$(cd "$(dirname "$0")/.." && pwd)/tools/audio
workspace=${1:?Pass a dedicated build directory}
msys_shell=$(cygpath -m /usr/bin/sh.exe)
mkdir -p "$workspace" "$destination"
cd "$workspace"
if [ ! -f ffmpeg-8.0.1.tar.xz ]; then curl -4 --retry 5 --retry-all-errors --retry-delay 2 --connect-timeout 20 --max-time 180 -fL https://ffmpeg.org/releases/ffmpeg-8.0.1.tar.xz -o ffmpeg-8.0.1.tar.xz; fi
echo '05ee0b03119b45c0bdb4df654b96802e909e0a752f72e4fe3794f487229e5a41  ffmpeg-8.0.1.tar.xz' | sha256sum -c -
if [ ! -d ffmpeg-8.0.1 ]; then tar -xf ffmpeg-8.0.1.tar.xz; fi
if [ ! -d lame-source ]; then git clone https://github.com/rbrito/lame.git lame-source; fi
git -C lame-source checkout --detach af984672a95bb0eedc9f3193604e269c97cca162
cd lame-source
./configure --host=x86_64-w64-mingw32 --prefix="$PWD/../prefix" --disable-shared --enable-static --disable-frontend --disable-nasm
mingw32-make -j8 MAKE=mingw32-make SHELL="$msys_shell"
mingw32-make install MAKE=mingw32-make SHELL="$msys_shell"
# Flatten libtool's convenience-library objects explicitly. On MSYS2 the
# libtool nested-archive extraction can omit decoder/SSE objects from the final
# static archive even though both convenience libraries built successfully.
for object in mpglib/*.o libmp3lame/vector/*.o; do
  if [ -f "$object" ]; then ar r ../prefix/lib/libmp3lame.a "$object"; fi
done
ranlib ../prefix/lib/libmp3lame.a
git archive --format=zip --output="$destination/lame-source.zip" HEAD
cp COPYING "$destination/LAME-LGPL.txt"
cd ..
prefix_native=$(cygpath -m "$PWD/prefix")
for library in ogg vorbis; do
  if [ "$library" = ogg ]; then revision=e1774cd77f471443541596e09078e78fdc342e4f; else revision=0657aee69dec8508a0011f47f3b69d7538e9d262; fi
  if [ ! -d "$library-source" ]; then git clone --no-checkout "https://github.com/xiph/$library.git" "$library-source"; fi
  git -C "$library-source" checkout --detach "$revision"
  cmake -S "$library-source" -B "$library-build" -G 'MinGW Makefiles' -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix_native" -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=OFF -DINSTALL_DOCS=OFF -DOGG_ROOT="$prefix_native"
  cmake --build "$library-build" -j4
  cmake --install "$library-build"
  git -C "$library-source" archive --format=zip --output="$destination/$library-source.zip" HEAD
  cp "$library-source/COPYING" "$destination/$library-BSD.txt"
done
cd ffmpeg-8.0.1
PKG_CONFIG_PATH="$PWD/../prefix/lib/pkgconfig" ./configure \
 --target-os=mingw32 --arch=x86_64 --cc=gcc --cxx=g++ \
 --disable-everything --disable-autodetect --disable-network --disable-doc \
 --disable-x86asm --disable-debug --enable-small --disable-shared --enable-static \
 --enable-ffmpeg --enable-protocol=file \
 --enable-demuxer=wav,ogg,mp3,flac,pcm_s16le,pcm_f32le --enable-muxer=wav,ogg,mp3,flac,pcm_s16le,pcm_f32le,null \
 --enable-decoder=pcm_s16le,pcm_u8,pcm_s24le,pcm_s32le,pcm_f32le,vorbis,mp3,mp3float,flac \
 --enable-encoder=pcm_s16le,pcm_s24le,pcm_f32le,libvorbis,flac,libmp3lame --enable-libmp3lame --enable-libvorbis \
 --enable-filter=aresample,volume,atrim,asetpts,asetrate,aformat,anull,atempo,loudnorm,afftdn,highpass,lowpass,acompressor,aecho,tremolo \
 --enable-parser=mpegaudio,flac,vorbis \
 --extra-cflags="-I$prefix_native/include" --extra-ldflags="-static -L$prefix_native/lib" || { tail -n 100 ffbuild/config.log; exit 1; }
mingw32-make -j8 MAKE=mingw32-make SHELL="$msys_shell"
cp ffmpeg.exe "$destination/ffmpeg.exe"
cp COPYING.LGPLv2.1 "$destination/FFmpeg-LGPL-2.1.txt"
cp ../ffmpeg-8.0.1.tar.xz "$destination/ffmpeg-8.0.1-source.tar.xz"
