# Apple-Music-style Music Player (FFmpeg) — Windows + iOS

Một lõi C++ dùng chung (FFmpeg + FFT) và hai giao diện riêng:

```
MusicPlayer/
├─ core/                       <-- DÙNG CHUNG cho cả 2 nền tảng
│  ├─ AudioDecoder.h/.cpp      libavformat/libavcodec/libswresample: mở, demux, decode -> PCM float32 stereo 44.1k, seek, metadata, trích cover (ATTACHED_PIC)
│  ├─ CoverArt.h/.cpp          libavcodec + libswscale: giải mã ảnh bìa -> RGBA (thu nhỏ)
│  ├─ FFT.h/.cpp               FFT radix-2
│  ├─ SpectrumAnalyzer.h/.cpp  cửa sổ Hann -> FFT 4096 -> 64 dải log 40 Hz–16 kHz -> dB -> 0..1, làm mượt attack/release
│  ├─ RingBuffer.h             FIFO PCM giữa decode thread và audio callback
│  └─ Player.h/.cpp            decode thread + ring buffer + analyzer + play/pause/seek
├─ windows/                    Qt6 + QOpenGLWidget (QPainter trên OpenGL) + QAudioSink
├─ ios/                        SwiftUI + Core Graphics Canvas + Core Image (Metal) + AVAudioEngine, cầu nối Obj-C++
├─ scripts/build-ffmpeg-ios.sh build FFmpeg tĩnh -> FFmpeg.xcframework
└─ tests/test_dsp.cpp          kiểm tra FFT/analyzer (không cần FFmpeg)
```

Luồng dữ liệu: `file -> AudioDecoder (decode thread) -> RingBuffer -> audio callback (Player::pull) -> loa`,
đồng thời `pull()` đẩy PCM vào `SpectrumAnalyzer`; UI gọi `Player::spectrum()` mỗi frame (~60 Hz) để vẽ sóng nhạc.

---------------------------------------------------------------------------

## 1) Windows (Visual Studio 2022 + CMake)

Yêu cầu: Visual Studio 2022 (workload *Desktop development with C++*), CMake ≥ 3.21,
Qt 6.5+ (MSVC 2022 64-bit, có module *Qt Multimedia*), vcpkg.

```bat
:: 1. FFmpeg qua vcpkg (bản shared, có DLL)
git clone https://github.com/microsoft/vcpkg C:\vcpkg && C:\vcpkg\bootstrap-vcpkg.bat
C:\vcpkg\vcpkg install ffmpeg:x64-windows

:: 2. Cấu hình + build (trong thư mục windows/)
cd MusicPlayer\windows
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
  -DCMAKE_PREFIX_PATH=C:/Qt/6.7.2/msvc2019_64
cmake --build build --config Release

:: 3. Gom DLL Qt để chạy độc lập (DLL FFmpeg được vcpkg tự copy cạnh .exe)
C:\Qt\6.7.2\msvc2019_64\bin\windeployqt.exe build\Release\AppleStylePlayer.exe
build\Release\AppleStylePlayer.exe "D:\Music\song.flac"
```

Dùng Visual Studio trực tiếp: *File > Open > CMake...* chọn `windows/CMakeLists.txt`, thêm vào *CMakeSettings/Presets* hai biến
`CMAKE_TOOLCHAIN_FILE` và `CMAKE_PREFIX_PATH` như trên. Không dùng vcpkg: tải bản FFmpeg *shared dev* rồi truyền `-DFFMPEG_ROOT=C:/ffmpeg`
(phải có `include/` và `lib/*.lib`), nhớ copy các DLL `av*.dll`, `sw*.dll` cạnh file .exe.

Phím tắt: `Space` phát/dừng · `←/→` ±5 s · `N/P` bài kế/trước · `O` mở tệp · `↑/↓` âm lượng · kéo thả tệp vào cửa sổ.

---------------------------------------------------------------------------

## 2) iOS (Xcode 15+, iOS 15+)

**Bước 1 – build FFmpeg** (trên macOS): `./scripts/build-ffmpeg-ios.sh` -> tạo `ios/Frameworks/FFmpeg.xcframework`
(decoder: MP3, FLAC, AAC/M4A, ALAC, Vorbis, Opus, WavPack, APE, WMA, PCM; ảnh bìa: MJPEG/PNG/BMP). Cần thêm codec khác thì sửa biến `DECODERS/DEMUXERS`.

**Bước 2 – tạo project**
1. Xcode > *File > New > Project > iOS App* (Interface: SwiftUI, Language: Swift). Xoá `ContentView.swift`/`App.swift` mặc định.
2. Kéo vào project: toàn bộ file trong `ios/` (`*.swift`, `MusicPlayerCore.h/.mm`, `MusicPlayer-Bridging-Header.h`) và **thư mục `core/`** (chọn *Create folder references*, target membership = app).
3. Kéo `ios/Frameworks/FFmpeg.xcframework` vào *Frameworks, Libraries, and Embedded Content* (chọn **Do Not Embed** — thư viện tĩnh).
4. Thêm thư viện hệ thống: *Link Binary With Libraries* -> `libz.tbd`.
5. *Build Settings*:
   - `Objective-C Bridging Header` = `MusicPlayer-Bridging-Header.h` (đường dẫn tương đối tới file)
   - `Header Search Paths` += `$(SRCROOT)/core` (và nếu Xcode không tự thấy header FFmpeg: `$(SRCROOT)/Frameworks/FFmpeg.xcframework/ios-arm64/Headers`)
   - `C++ Language Dialect` = GNU++17
6. *Signing & Capabilities* > **+ Capability > Background Modes > Audio, AirPlay, and Picture in Picture** (phát nền / màn hình khoá).
7. Chọn thiết bị hoặc Simulator (arm64) và Run. Nhấn dấu **+** để chọn tệp nhạc từ app Files.

> Lưu ý giấy phép: FFmpeg build ở đây là **LGPL** (không bật `--enable-gpl`). Liên kết tĩnh vào app đóng nguồn cần
> tuân thủ nghĩa vụ LGPL (cho phép người dùng thay thư viện / công bố bản FFmpeg đã dùng). Hãy xem lại điều khoản trước khi phát hành.

---------------------------------------------------------------------------

## 3) Giải thích các kỹ thuật chính

* **Chống rò rỉ bộ nhớ FFmpeg**: `AudioDecoder::close()` luôn gọi `swr_free`, `avcodec_free_context`, `avformat_close_input`;
  destructor gọi thêm `av_packet_free`, `av_frame_free`; mỗi `AVPacket` đọc xong đều `av_packet_unref`, mỗi `AVFrame` đều `av_frame_unref`;
  `CoverArt.cpp` giải phóng `AVCodecContext/AVPacket/AVFrame/SwsContext` trên mọi đường thoát (do-while + dọn cuối).
* **Cover art**: luồng có `AV_DISPOSITION_ATTACHED_PIC` -> `attached_pic.data/size` (MP3 APIC, FLAC PICTURE, M4A `covr`).
* **FFT**: Hann window, FFT 4096 điểm @44.1 kHz (≈10.8 Hz/bin), gom 64 dải log, bù tilt +3 dB/octave, ánh xạ −80…−10 dB -> 0…1.
* **Nền ambient**: ảnh bìa -> thu nhỏ -> blur nặng -> tăng bão hoà + tối (Windows: box blur 3 lần ≈ Gaussian; iOS: `CIGaussianBlur` trên GPU),
  phóng to phủ toàn màn hình, 2 lớp xoay/zoom chậm ngược chiều, cross-fade khi đổi bài.
* **Ảnh bìa**: hình vuông, bo góc 4.5%, đổ bóng nhiều lớp (Windows: shadow blur cache; iOS: `.shadow`), thu nhỏ còn 86% khi tạm dừng và "thở" theo bass.
* **Spectrum**: 96 thanh đặt dọc đường viền bo góc của ảnh bìa (tham số hoá theo độ dài cung, hướng theo pháp tuyến ngoài),
  bass ở giữa cạnh trên, treble ở giữa cạnh dưới, đối xứng trái/phải.

## 4) Giới hạn đã biết

* Khối DSP (`FFT`, `SpectrumAnalyzer`) đã được kiểm thử bằng `tests/test_dsp.cpp`
  (`g++ -std=c++17 tests/test_dsp.cpp core/FFT.cpp core/SpectrumAnalyzer.cpp`). Phần FFmpeg, Qt và SwiftUI **chưa được biên dịch/chạy thử** trong môi trường tạo mã này — hãy build thử và báo lỗi (nếu có) để chỉnh.
* Mã dùng API FFmpeg ≥ 5.1 (`AVChannelLayout`, `swr_alloc_set_opts2`); khuyến nghị FFmpeg 6.x/7.x.
* Spectrum được tính ở thời điểm audio callback lấy dữ liệu, sớm hơn tiếng thật khoảng độ trễ buffer của thiết bị (~100 ms); cần đồng bộ tuyệt đối thì dịch phổ theo độ trễ output.
* Metal: iOS dùng Core Image/SwiftUI (đều chạy trên Metal) thay vì shader `MTKView` tự viết; có thể thay `SpectrumRing` bằng `MTKView` nếu cần.
