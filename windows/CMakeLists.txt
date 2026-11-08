cmake_minimum_required(VERSION 3.21)
project(FMusic LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
if(MSVC)
  add_compile_options(/utf-8 /W3)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia)

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

# ---------- Media Windows (SMTC): tự thử biên dịch, không được thì dùng bản dự phòng ----------
set(MEDIA_SRC MediaSessionStub.cpp)
set(MEDIA_LIBS "")
if(WIN32)
  find_library(RUNTIMEOBJECT_LIB NAMES runtimeobject)
  set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
  try_compile(FMUSIC_SMTC_OK "${CMAKE_BINARY_DIR}/smtc_check"
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/MediaSessionSmtc.cpp"
    CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${CMAKE_CURRENT_SOURCE_DIR}"
    CXX_STANDARD 17
    OUTPUT_VARIABLE SMTC_LOG)
  if(FMUSIC_SMTC_OK AND (MSVC OR RUNTIMEOBJECT_LIB))
    set(MEDIA_SRC MediaSessionSmtc.cpp)
    set(MEDIA_LIBS runtimeobject ole32)
    message(STATUS "FMusic: SMTC (khai báo media với Windows) = BẬT")
  else()
    message(WARNING "FMusic: không biên dịch được SMTC với toolchain này -> dùng phím media toàn cục (RegisterHotKey)")
  endif()
endif()

# ---------- Ứng dụng FMusic ----------
set(APP_SOURCES main.cpp PlayerWidget.cpp PlayerWidget.h ${MEDIA_SRC} resources.qrc)
if(WIN32)
  enable_language(RC)
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/FMusic.rc.in" "${CMAKE_BINARY_DIR}/FMusic.rc" @ONLY)
  list(APPEND APP_SOURCES "${CMAKE_BINARY_DIR}/FMusic.rc")
endif()
add_executable(FMusic WIN32 ${APP_SOURCES})
target_include_directories(FMusic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(FMusic PRIVATE mp_core Qt6::Widgets Qt6::Multimedia ${MEDIA_LIBS})
