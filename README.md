# FMusic — Apple Music-Style Audio Player (v1.1)

A high-performance, cross-platform audio player leveraging a shared C++ core for decoding and real-time DSP, coupled with native GUI frontends for **Windows (Qt6)**, **iOS (SwiftUI)**, and **Android (Jetpack Compose + JNI)**.

---

## 🌟 What's New in v1.1

1. **🎚️ Dynamic Sample Rate Switching**:
   - Runtime sample rate switching (44.1 kHz, 48 kHz, 96 kHz) on Windows, iOS, and Android.
   - Core recalculates biquad equalizer coefficients, FFT bin mapping, and reinitializes FFmpeg resamplers (`SwrContext`).
2. **🤖 Android Native Support**:
   - Built with **Kotlin + Jetpack Compose + C++ JNI bridge**.
   - `AudioTrack` float32 streaming pipeline, lock screen `MediaSession`, background playback service.
   - 96-bar radial spectrum visualizer & ambient dynamic blurred background.
3. **💾 Smart Playlist Saving (.fpl)**:
   - Save playlists without duplicating/copying audio files on Windows, Android, and iOS.
   - Plain text `.fpl` format referencing file paths directly; drag & drop support, `Ctrl+S` shortcut.
4. **🌐 Streaming Support**:
   - Stream audio directly via Localhost (`http://localhost:...`) or remote direct URLs (HTTP/HTTPS, Google Drive direct media links).
   - FFmpeg network streaming enabled (`avformat_network_init`).
5. **✨ Silky Smooth 60 FPS Animations**:
   - Overhauled motion smoothing with delta-time lerp for album art scale, volume, and panel sliding.
   - Eliminates stutter and frame drops across UI state transitions.
6. **🧹 Clean Interface**:
   - Removed redundant top "+" button in favor of unified "Thêm" (Add) actions inside the Queue panel.
7. **🔒 Background Playback on iOS**:
   - Continuous audio playback when screen is locked or turned off.
   - Automatic handling of audio interruptions (phone calls, Siri) and route changes (headphone disconnect).
8. **🌊 iOS-Equivalent Ambient Blur on Windows**:
   - Upgraded backdrop blur from 40x40 to **120x120** with deep 3-pass Gaussian approximation.
   - Matched iOS color profile (+50% saturation, -12% brightness) with smooth dual-layer counter-rotating ambient motion.
9. **🔤 Intuitive Navigation Order**:
   - Reordered tabs to standard workflow: `[Đang phát]` ➔ `[Lời]` ➔ `[Danh sách]` ➔ `[EQ]` ➔ `[Cài đặt]`.
10. **🎛️ Equalizer Presets & Export (.feq)**:
    - Save custom 10-band EQ presets and export/import them as human-readable `.FEQ` (JSON) files.

---

## 🏗 System Architecture & Directory Structure

```
FMusic/
├── core/                           # Platform-Agnostic Core Logic (C++17)
│   ├── AudioDecoder.h/.cpp         # Demuxing, decoding (FFmpeg), dynamic sample rate, streaming
│   ├── CoverArt.h/.cpp             # Cover art decoding & scaling (libswscale)
│   ├── Equalizer.h/.cpp            # 10-band biquad peaking filters (RBJ) with dynamic SR
│   ├── EqPreset.h/.cpp             # FEQ format save/load parser
│   ├── FFT.h/.cpp                  # Radix-2 Fast Fourier Transform
│   ├── Lyrics.h/.cpp               # Synced LRC / unsynced lyrics parser
│   ├── Player.h/.cpp               # Playback engine, thread orchestration, DSP integration
│   ├── PlaylistFile.h/.cpp         # .fpl playlist load/save parser
│   ├── RingBuffer.h                # Interleaved PCM FIFO buffer
│   └── SpectrumAnalyzer.h/.cpp     # Hann window -> 4096-pt FFT -> 64 log bands (40 Hz–16 kHz)
├── windows/                        # Qt6 Frontend (QAudioSink / QPainter)
│   ├── CMakeLists.txt
│   ├── main.cpp
│   ├── PlayerWidget.h/.cpp
│   ├── MediaSession.h              # Windows SMTC / Global media keys
│   └── resources.qrc
├── ios/                            # iOS Frontend (SwiftUI / Metal / Core Image / AVAudioEngine)
│   ├── FMusicApp.swift
│   ├── Chrome.swift                # Liquid Glass tab bar, mini-player, settings
│   ├── NowPlayingViews.swift       # AmbientBackground, ArtworkStage, SpectrumRing, ProgressBar
│   ├── PanelViews.swift            # LyricsView, QueueView, EQView (.feq save/export)
│   ├── PlayerViewModel.swift       # AVAudioEngine pull-mode source node & background management
│   ├── MusicPlayerCore.h/.mm       # Obj-C++ bridge to C++ core
│   └── project.yml                 # XcodeGen configuration
├── android/                        # Android Frontend (Jetpack Compose / JNI / Kotlin)
│   ├── CMakeLists.txt
│   ├── app/
│   │   ├── build.gradle.kts
│   │   └── src/main/
│   │       ├── AndroidManifest.xml
│   │       ├── cpp/fmusic_jni.cpp  # JNI bindings to C++ core
│   │       └── java/com/fmusic/
│   │           ├── MainActivity.kt
│   │           ├── MusicPlayerCore.kt
│   │           ├── PlayerViewModel.kt
│   │           └── ui/             # Compose screens (NowPlaying, Lyrics, Queue, EQ, Settings)
├── scripts/build-ffmpeg-ios.sh     # FFmpeg iOS cross-compilation & XCFramework packager
└── tests/                          # Standalone DSP & Core test suite
    ├── test_core.cpp
    └── test_dsp.cpp
```

---

## 🛠 Platform Setup & Build Instructions

### 1. Windows Deployment (Visual Studio 2022 + CMake)
* **Prerequisites:** Visual Studio 2022 (C++ Desktop Workload), CMake ≥ 3.21, Qt 6.5+ (MSVC 64-bit + Qt Multimedia), vcpkg.

```cmd
:: Install dependencies via vcpkg
git clone https://github.com/microsoft/vcpkg C:\vcpkg && C:\vcpkg\bootstrap-vcpkg.bat
C:\vcpkg\vcpkg install ffmpeg:x64-windows

:: Build project
cd FMusic\windows
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
  -DCMAKE_PREFIX_PATH=C:/Qt/6.7.2/msvc2019_64
cmake --build build --config Release

:: Deploy Qt runtime dependencies
C:\Qt\6.7.2\msvc2019_64\bin\windeployqt.exe build\Release\FMusic.exe
```
* **Hotkeys:**
  - `Space`: Play/Pause
  - `←` / `→`: Seek ±5s
  - `N` / `P`: Next / Previous track
  - `O`: Open local files
  - `U` / `Ctrl+U`: Open stream URL (localhost/HTTP)
  - `Ctrl+S`: Save playlist as `.fpl`
  - `L` / `Q` / `E`: Toggle Lyrics / Queue / EQ panel
  - `↑` / `↓`: Volume ±5%
  - Drag & Drop: `.mp3`, `.flac`, `.fpl`, `.feq` files

### 2. iOS Deployment (Xcode 15+ / iOS 16+)
1. **Compile Static FFmpeg Framework:** Run `./scripts/build-ffmpeg-ios.sh` on macOS to generate `ios/Frameworks/FFmpeg.xcframework`.
2. **Generate Xcode Project:** Run `cd ios && xcodegen generate`.
3. Open `FMusic.xcodeproj` in Xcode and run on device or simulator.

### 3. Android Deployment (Android Studio / Gradle)
1. Open the `android/` directory in Android Studio (Giraffe+ / Ladybug+).
2. Ensure Android NDK (r25+) and CMake (3.22+) are installed.
3. Build & Run: `./gradlew assembleDebug` or run directly on device.
