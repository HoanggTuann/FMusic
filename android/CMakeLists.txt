cmake_minimum_required(VERSION 3.22)
project(fmusic_core)

set(CMAKE_CXX_STANDARD 17)

# Shared core sources — lives one level above android/ at ../core/
set(CORE_DIR ${CMAKE_SOURCE_DIR}/../core)

add_library(fmusic_core SHARED
    app/src/main/cpp/fmusic_jni.cpp
    ${CORE_DIR}/AudioDecoder.cpp
    ${CORE_DIR}/CoverArt.cpp
    ${CORE_DIR}/Equalizer.cpp
    ${CORE_DIR}/FFT.cpp
    ${CORE_DIR}/Lyrics.cpp
    ${CORE_DIR}/Player.cpp
    ${CORE_DIR}/SpectrumAnalyzer.cpp
    ${CORE_DIR}/EqPreset.cpp
    ${CORE_DIR}/PlaylistFile.cpp
)

target_include_directories(fmusic_core PRIVATE ${CORE_DIR})

# FFmpeg for Android (prebuilt shared libs)
# Recommended: https://github.com/arthenica/ffmpeg-kit or mobile-ffmpeg
# Place prebuilt .so files under android/app/libs/<ABI>/ and headers under android/app/libs/include/
find_package(FFmpeg REQUIRED COMPONENTS avcodec avformat avutil swresample swscale)
target_link_libraries(fmusic_core
    ${FFmpeg_LIBRARIES}
    android
    log
)
