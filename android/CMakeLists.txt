cmake_minimum_required(VERSION 3.22)
project(fmusic_core)

set(CMAKE_CXX_STANDARD 17)

# Core logic C++ sources
set(CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../core")

# Support prebuilt FFmpeg under app/libs/${ANDROID_ABI}
set(FFMPEG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/app/libs")
set(FFMPEG_LIBS "")

if(EXISTS "${FFMPEG_DIR}/${ANDROID_ABI}")
    include_directories(${FFMPEG_DIR}/include)
    foreach(comp avcodec avformat avutil swresample swscale)
        if(EXISTS "${FFMPEG_DIR}/${ANDROID_ABI}/lib${comp}.so")
            add_library(${comp} SHARED IMPORTED)
            set_target_properties(${comp} PROPERTIES IMPORTED_LOCATION "${FFMPEG_DIR}/${ANDROID_ABI}/lib${comp}.so")
            list(APPEND FFMPEG_LIBS ${comp})
        endif()
    endforeach()
endif()

# Check system / pkg-config if prebuilts not found
if(NOT FFMPEG_LIBS)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(FFMPEG libavcodec libavformat libavutil libswresample libswscale)
        if(FFMPEG_FOUND)
            set(FFMPEG_LIBS ${FFMPEG_LIBRARIES})
            include_directories(${FFMPEG_INCLUDE_DIRS})
        endif()
    endif()
endif()

# Include directories for FFmpeg headers
find_path(AVCODEC_INC libavcodec/avcodec.h
    HINTS ${FFMPEG_DIR}/include /usr/include /usr/include/x86_64-linux-gnu /usr/local/include
)
if(AVCODEC_INC)
    include_directories(${AVCODEC_INC})
endif()

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

target_link_libraries(fmusic_core
    ${FFMPEG_LIBS}
    android
    log
)
