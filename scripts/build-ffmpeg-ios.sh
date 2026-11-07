#!/usr/bin/env bash
# Build FFmpeg (static, tối giản) cho iOS thiết bị + iOS Simulator (arm64) rồi gói thành FFmpeg.xcframework.
# Chạy trên macOS có Xcode:  chmod +x build-ffmpeg-ios.sh && ./build-ffmpeg-ios.sh
set -euo pipefail

FFMPEG_VERSION="${FFMPEG_VERSION:-7.1}"
MIN_IOS="15.0"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="$ROOT/build-ffmpeg"
OUT="$ROOT/ios/Frameworks"
mkdir -p "$WORK" "$OUT"
cd "$WORK"

if [ ! -d "ffmpeg-$FFMPEG_VERSION" ]; then
  curl -L "https://ffmpeg.org/releases/ffmpeg-$FFMPEG_VERSION.tar.xz" | tar xJ
fi

DECODERS="mp3,mp3float,flac,aac,aac_latm,alac,vorbis,opus,wavpack,ape,wmav1,wmav2,pcm_s16le,pcm_s16be,pcm_s24le,pcm_s32le,pcm_f32le,pcm_u8,mjpeg,png,bmp"
DEMUXERS="mp3,flac,mov,ogg,wav,aac,ape,asf,matroska,aiff,wv"
PARSERS="mpegaudio,aac,aac_latm,flac,vorbis,opus,mjpeg,png,bmp"

build() {  # $1=tên  $2=sdk  $3=target triple
  local NAME="$1" SDK="$2" TARGET="$3"
  local SYSROOT PREFIX
  SYSROOT="$(xcrun --sdk "$SDK" --show-sdk-path)"
  PREFIX="$WORK/out/$NAME"
  rm -rf "$PREFIX" "build-$NAME"; mkdir -p "build-$NAME"
  pushd "build-$NAME" >/dev/null
  "../ffmpeg-$FFMPEG_VERSION/configure" \
    --prefix="$PREFIX" \
    --enable-cross-compile --target-os=darwin --arch=arm64 \
    --cc="xcrun -sdk $SDK clang" \
    --extra-cflags="-target $TARGET -isysroot $SYSROOT" \
    --extra-ldflags="-target $TARGET -isysroot $SYSROOT" \
    --enable-static --disable-shared --enable-pic \
    --disable-programs --disable-doc --disable-debug \
    --disable-avdevice --disable-avfilter --disable-network --disable-autodetect \
    --enable-zlib \
    --disable-everything \
    --enable-decoder="$DECODERS" --enable-demuxer="$DEMUXERS" --enable-parser="$PARSERS" \
    --enable-protocol=file
  make -j"$(sysctl -n hw.ncpu)"
  make install
  popd >/dev/null
  libtool -static -o "$PREFIX/libffmpeg.a" "$PREFIX"/lib/libav*.a "$PREFIX"/lib/libsw*.a
}

build ios-device iphoneos        "arm64-apple-ios${MIN_IOS}"
build ios-sim    iphonesimulator "arm64-apple-ios${MIN_IOS}-simulator"

rm -rf "$OUT/FFmpeg.xcframework"
xcodebuild -create-xcframework \
  -library "$WORK/out/ios-device/libffmpeg.a" -headers "$WORK/out/ios-device/include" \
  -library "$WORK/out/ios-sim/libffmpeg.a"    -headers "$WORK/out/ios-sim/include" \
  -output "$OUT/FFmpeg.xcframework"
echo "Xong: $OUT/FFmpeg.xcframework"
