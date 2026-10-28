#!/usr/bin/env bash
# Costruisce il pacchetto video: shim dylib SELF-CONTAINED. ffmpeg (libavformat/libavcodec/
# libswscale/libavutil) e' COMPILATO DAI SORGENTI (build minimale) e linkato staticamente
# -> nessuna lib di sistema a runtime. Il package manager esegue questo build all'install.
# Prerequisiti di build: git, make, compilatore. (Su Apple Silicon non serve nasm.)
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
BUILD="$HERE/.build"; FF="$BUILD/ffmpeg"
OUT="$HERE/lib/osx-arm64"
mkdir -p "$BUILD" "$OUT" "$HERE/include"
J="$(sysctl -n hw.ncpu 2>/dev/null || echo 8)"

[ -d "$FF" ] || git clone --depth 1 https://github.com/FFmpeg/FFmpeg.git "$FF"

cd "$FF"
if [ ! -f config.h ]; then
  ./configure --disable-shared --enable-static --disable-programs --disable-doc --disable-network \
    --disable-everything \
    --enable-demuxer=mov,matroska,avi,mpegts,flv,asf,ogg,wav \
    --enable-decoder=h264,hevc,mpeg4,mpeg2video,vp8,vp9,av1,rawvideo,mjpeg \
    --enable-parser=h264,hevc,mpeg4video,vp8,vp9,av1,mjpeg \
    --enable-protocol=file \
    --enable-swscale --enable-swresample --enable-avformat --enable-avcodec --enable-avutil \
    --disable-x86asm >/dev/null
fi
make -j"$J" >/dev/null
cd "$HERE"

AVF="$FF/libavformat/libavformat.a"; AVC="$FF/libavcodec/libavcodec.a"
SWS="$FF/libswscale/libswscale.a"; SWR="$FF/libswresample/libswresample.a"; AVU="$FF/libavutil/libavutil.a"
[ -f "$AVF" ] && [ -f "$AVC" ] && [ -f "$AVU" ] || { echo "statiche ffmpeg non prodotte"; exit 1; }

rm -f "$OUT/libgmbvideo.dylib"
cc -dynamiclib -O2 -w -I"$FF" -I"$HERE/include" "$HERE/src/gmbvideo.c" \
   -o "$OUT/libgmbvideo.dylib" \
   "$AVF" "$AVC" "$SWS" "$SWR" "$AVU" \
   -framework CoreFoundation -framework CoreVideo -framework CoreMedia -framework VideoToolbox \
   -lz -lbz2 -liconv -lm -lpthread
echo "ok: $OUT/libgmbvideo.dylib (self-contained)"
