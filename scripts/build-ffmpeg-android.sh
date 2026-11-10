#!/usr/bin/env bash
# Build FFmpeg (tối giản, tối ưu dung lượng) cho Android arm64-v8a với NDK
# Hỗ trợ đầy đủ audio decoders (FLAC, MP3, AAC, ALAC, Vorbis, Opus, WAV...) và HTTP/HTTPS streaming
set -euo pipefail

FFMPEG_VERSION="${FFMPEG_VERSION:-7.1}"
API=26
ARCH="aarch64"
ABI="arm64-v8a"
TARGET="aarch64-linux-android"

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="$ROOT/build-ffmpeg-android"
OUT="$ROOT/android/app/libs"
if [ ! -d "$ROOT/android" ] && [ -d "$ROOT/andr" ]; then
  OUT="$ROOT/andr/app/libs"
fi

mkdir -p "$WORK" "$OUT/$ABI" "$OUT/include"
cd "$WORK"

if [ ! -d "ffmpeg-$FFMPEG_VERSION" ]; then
  echo ">>> Tải FFmpeg $FFMPEG_VERSION..."
  if curl -fsSL --retry 3 --retry-delay 2 "https://codeload.github.com/FFmpeg/FFmpeg/tar.gz/refs/tags/n${FFMPEG_VERSION}" -o ffmpeg.tar.gz; then
    tar -xzf ffmpeg.tar.gz
    mv "FFmpeg-n${FFMPEG_VERSION}" "ffmpeg-${FFMPEG_VERSION}" 2>/dev/null || true
    rm -f ffmpeg.tar.gz
  else
    curl -fsSL --retry 3 --retry-delay 2 "https://ffmpeg.org/releases/ffmpeg-$FFMPEG_VERSION.tar.xz" | tar xJ
  fi
fi

# Tìm Android NDK
NDK=""
if [ -n "${ANDROID_NDK_ROOT:-}" ] && [ -d "${ANDROID_NDK_ROOT:-}" ]; then
  NDK="$ANDROID_NDK_ROOT"
elif [ -n "${ANDROID_NDK_HOME:-}" ] && [ -d "${ANDROID_NDK_HOME:-}" ]; then
  NDK="$ANDROID_NDK_HOME"
elif [ -d "/usr/local/lib/android/sdk/ndk" ]; then
  NDK="$(ls -d /usr/local/lib/android/sdk/ndk/* 2>/dev/null | sort -V | tail -n 1)"
elif [ -d "${ANDROID_SDK_ROOT:-}/ndk" ]; then
  NDK="$(ls -d ${ANDROID_SDK_ROOT}/ndk/* 2>/dev/null | sort -V | tail -n 1)"
fi

if [ -z "$NDK" ] || [ ! -d "$NDK" ]; then
  echo "Lỗi: Không tìm thấy Android NDK!"
  exit 1
fi
echo ">>> Sử dụng Android NDK: $NDK"

TOOLCHAIN="$NDK/toolchains/llvm/prebuilt/linux-x86_64"
CC="$TOOLCHAIN/bin/${TARGET}${API}-clang"
CXX="$TOOLCHAIN/bin/${TARGET}${API}-clang++"
AR="$TOOLCHAIN/bin/llvm-ar"
RANLIB="$TOOLCHAIN/bin/llvm-ranlib"

PREFIX="$WORK/out"
rm -rf "$PREFIX" "build-$ABI"
mkdir -p "build-$ABI"
pushd "build-$ABI" >/dev/null

DECODERS="mp3,mp3float,flac,aac,aac_latm,alac,vorbis,opus,wavpack,ape,wmav1,wmav2,pcm_s16le,pcm_s16be,pcm_s24le,pcm_s32le,pcm_f32le,pcm_u8,mjpeg,png,bmp"
DEMUXERS="mp3,flac,mov,ogg,wav,aac,ape,asf,matroska,aiff,wv"
PARSERS="mpegaudio,aac,aac_latm,flac,vorbis,opus,mjpeg,png,bmp"
PROTOCOLS="file,http,https,tcp,tls"

"../ffmpeg-$FFMPEG_VERSION/configure" \
  --prefix="$PREFIX" \
  --enable-cross-compile \
  --target-os=android \
  --arch=$ARCH \
  --cpu=armv8-a \
  --cc="$CC" \
  --cxx="$CXX" \
  --ar="$AR" \
  --ranlib="$RANLIB" \
  --strip="$TOOLCHAIN/bin/llvm-strip" \
  --nm="$TOOLCHAIN/bin/llvm-nm" \
  --enable-shared \
  --disable-static \
  --disable-stripping \
  --enable-pic \
  --disable-programs \
  --disable-doc \
  --disable-debug \
  --disable-avdevice \
  --disable-avfilter \
  --disable-everything \
  --enable-decoder="$DECODERS" \
  --enable-demuxer="$DEMUXERS" \
  --enable-parser="$PARSERS" \
  --enable-protocol="$PROTOCOLS" \
  --enable-network \
  --disable-autodetect

make -j"$(nproc)"
make install-libs install-headers
popd >/dev/null

cp "$PREFIX"/lib/lib*.so "$OUT/$ABI/"
cp -R "$PREFIX"/include/* "$OUT/include/"
echo ">>> Hoàn tất build FFmpeg cho Android: $OUT/$ABI"
