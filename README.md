# Apple Music-Style Audio Player (C++ / FFmpeg / Qt / SwiftUI)

A high-performance, cross-platform audio player leveraging a shared C++ core for decoding and real-time DSP, coupled with native GUI frontends for Windows and iOS.

---

## 🏗 System Architecture & Directory Structure

```
MusicPlayer/
├── core/                       # Platform-Agnostic Core Logic
│   ├── AudioDecoder.h/.cpp     # Demuxing, decoding (FFmpeg), metadata & cover extraction
│   ├── CoverArt.h/.cpp         # Cover art decoding & scaling (libswscale)
│   ├── FFT.h/.cpp              # Radix-2 Fast Fourier Transform
│   ├── SpectrumAnalyzer.h/.cpp # Hann windowing -> 4096-pt FFT -> 64 log bands (40 Hz–16 kHz)
│   ├── RingBuffer.h            # Lock-free PCM FIFO buffer between decode thread & audio callback
│   └── Player.h/.cpp           # Playback engine, thread orchestration, DSP integration
├── windows/                    # Qt6 Frontend (OpenGL / QAudioSink / QPainter)
├── ios/                        # iOS Frontend (SwiftUI / Metal / Core Image / AVAudioEngine)
├── scripts/build-ffmpeg-ios.sh # FFmpeg iOS cross-compilation & XCFramework packager
└── tests/test_dsp.cpp          # Standalone DSP test suite
```

### Audio & Visual Data Pipeline
`Audio Source File` ➔ `AudioDecoder (Background Thread)` ➔ `RingBuffer` ➔ `Audio Callback (Player::pull)` ➔ `Hardware Output`
* **Real-time Visualization:** `pull()` streams PCM data into `SpectrumAnalyzer`. The UI queries `Player::spectrum()` frame-by-frame (~60 FPS) to render reactive audio visuals.

---

## 🛠 Platform Setup & Build Instructions

### 1. Windows Deployment (Visual Studio 2022 + CMake)
* **Prerequisites:** Visual Studio 2022 (C++ Desktop Workload), CMake ≥ 3.21, Qt 6.5+ (MSVC 64-bit + Qt Multimedia), vcpkg.

```cmd
:: Install dependencies via vcpkg
git clone https://github.com/microsoft/vcpkg C:\vcpkg && C:\vcpkg\bootstrap-vcpkg.bat
C:\vcpkg\vcpkg install ffmpeg:x64-windows

:: Build project
cd MusicPlayer\windows
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
  -DCMAKE_PREFIX_PATH=C:/Qt/6.7.2/msvc2019_64
cmake --build build --config Release

:: Deploy Qt runtime dependencies
C:\Qt\6.7.2\msvc2019_64\bin\windeployqt.exe build\Release\AppleStylePlayer.exe
```
* **Hotkeys:** `Space` (Play/Pause) | `←/→` (Seek ±5s) | `N/P` (Next/Prev) | `O` (Open) | `↑/↓` (Volume) | Drag & Drop support.

### 2. iOS Deployment (Xcode 15+ / iOS 15+)
1. **Compile Static FFmpeg Framework:** Run `./scripts/build-ffmpeg-ios.sh` on macOS to generate `ios/Frameworks/FFmpeg.xcframework`.
2. **Setup Xcode Workspace:**
   * Create a standard SwiftUI App target.
   * Add `ios/` wrapper files, `MusicPlayer-Bridging-Header.h`, and link the `core/` folder.
   * Embed `FFmpeg.xcframework` (**Do Not Embed** — Static Library) and link `libz.tbd`.
3. **Configure Build Settings:**
   * `Objective-C Bridging Header` ➔ `MusicPlayer-Bridging-Header.h`
   * `Header Search Paths` ➔ `$(SRCROOT)/core`
   * `C++ Language Dialect` ➔ GNU++17
   * `Capabilities` ➔ Enable **Background Modes (Audio, AirPlay, and PiP)**.

---

## ⚡ Technical Highlights & DSP Pipeline

* **Robust Resource Management:** Strict lifecycle control over FFmpeg contexts (`avformat`, `avcodec`, `swr`) with explicit frame/packet unreferencing to eliminate memory leaks.
* **Cover Art Extraction:** Automatic parsing of embedded artwork across container formats (`AV_DISPOSITION_ATTACHED_PIC` for MP3, FLAC, M4A).
* **Audio Visualizer & DSP Engine:**
  * 4096-point FFT at 44.1 kHz sampling (~10.8 Hz bin resolution).
  * 64 logarithmic frequency bands mapped from -80 dB to -10 dB normalized ranges.
  * Attack/release smoothing with +3 dB/octave tilt compensation.
* **Ambient UI Rendering:** Dynamic background generation using heavily blurred and desaturated album artwork (Multi-pass Box Blur on Windows / GPU-accelerated `CIGaussianBlur` on iOS) featuring dual-layer counter-rotation and seamless cross-fading.
* **Dynamic Spectrum Visualizer:** 96 radial visualization bars positioned along the rounded album cover border, symmetrically mapped from bass (top center) to treble (bottom center).