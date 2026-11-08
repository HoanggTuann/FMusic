cmake_minimum_required(VERSION 3.21)
project(AppleStyleMusicPlayer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
if(MSVC)
  add_compile_options(/utf-8 /W3)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGLWidgets Multimedia)

# ---------- FFmpeg ----------
# Cách 1 (khuyến nghị): vcpkg  ->  vcpkg install ffmpeg:x64-windows   rồi dùng -DCMAKE_TOOLCHAIN_FILE=.../vcpkg.cmake
# Cách 2: bản dev "shared" tải về, truyền -DFFMPEG_ROOT=C:/ffmpeg  (có include/ và lib/*.lib)
set(FFMPEG_ROOT "" CACHE PATH "Thư mục gốc FFmpeg (chứa include/ và lib/)")
find_path(FFMPEG_INCLUDE_DIR libavformat/avformat.h HINTS "${FFMPEG_ROOT}/include")
set(FFMPEG_LIBS "")
foreach(comp avformat avcodec avutil swresample swscale)
  find_library(FFMPEG_${comp}_LIB NAMES ${comp} HINTS "${FFMPEG_ROOT}/lib")
  if(NOT FFMPEG_${comp}_LIB)
    message(FATAL_ERROR "Không tìm thấy lib${comp}. Đặt -DFFMPEG_ROOT=... hoặc dùng vcpkg toolchain.")
  endif()
  list(APPEND FFMPEG_LIBS "${FFMPEG_${comp}_LIB}")
endforeach()
if(NOT FFMPEG_INCLUDE_DIR)
  message(FATAL_ERROR "Không tìm thấy libavformat/avformat.h")
endif()

# ---------- Lõi dùng chung (cùng mã nguồn với iOS) ----------
set(CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../core")
file(GLOB CORE_SOURCES CONFIGURE_DEPENDS "${CORE_DIR}/*.cpp")      # tự lấy mọi .cpp trong core/
add_library(mp_core STATIC ${CORE_SOURCES})
target_include_directories(mp_core PUBLIC ${CORE_DIR} ${FFMPEG_INCLUDE_DIR})
target_link_libraries(mp_core PUBLIC ${FFMPEG_LIBS})

# ---------- Ứng dụng Qt + OpenGL ----------
add_executable(AppleStylePlayer WIN32 main.cpp PlayerWidget.cpp PlayerWidget.h)
target_link_libraries(AppleStylePlayer PRIVATE mp_core Qt6::Widgets Qt6::OpenGLWidgets Qt6::Multimedia)
