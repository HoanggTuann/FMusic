#include "AudioDecoder.h"
#include <algorithm>
#include <cstring>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/dict.h>
#include <libswresample/swresample.h>
}

static std::string metaGet(const AVDictionary* d, const char* key) {
    const AVDictionaryEntry* e = av_dict_get(d, key, nullptr, 0);   // so khớp không phân biệt hoa/thường
    return e && e->value ? e->value : std::string();
}

static std::string baseName(const std::string& p) {
    size_t s = p.find_last_of("/\\");
    std::string n = (s == std::string::npos) ? p : p.substr(s + 1);
    size_t dot = n.find_last_of('.');
    return dot == std::string::npos || dot == 0 ? n : n.substr(0, dot);
}


// Đọc title/artist/album/lyrics/duration. idx = chỉ số luồng audio (hoặc -1).
static void readMeta(AVFormatContext* fmt, int idx, const std::string& path, TrackInfo& info) {
    AVStream* st = idx >= 0 ? fmt->streams[idx] : nullptr;
    info.title  = metaGet(fmt->metadata, "title");
    info.artist = metaGet(fmt->metadata, "artist");
    info.album  = metaGet(fmt->metadata, "album");
    if (st) {
        if (info.title.empty())  info.title  = metaGet(st->metadata, "title");
        if (info.artist.empty()) info.artist = metaGet(st->metadata, "artist");
    }
    if (info.title.empty())  info.title  = baseName(path);
    if (info.artist.empty()) info.artist = metaGet(fmt->metadata, "album_artist");
    // "lyrics", "lyrics-eng", "LYRICS"...: khớp theo tiền tố
    const AVDictionaryEntry* e = av_dict_get(fmt->metadata, "lyrics", nullptr, AV_DICT_IGNORE_SUFFIX);
    if (!e && st) e = av_dict_get(st->metadata, "lyrics", nullptr, AV_DICT_IGNORE_SUFFIX);
    if (e && e->value) info.lyrics = e->value;
    if (fmt->duration > 0) info.duration = double(fmt->duration) / AV_TIME_BASE;
}

bool AudioDecoder::probe(const std::string& path, TrackInfo& out) {
    AVFormatContext* fmt = nullptr;
    if (avformat_open_input(&fmt, path.c_str(), nullptr, nullptr) < 0) return false;
    bool ok = false;
    if (avformat_find_stream_info(fmt, nullptr) >= 0) {
        const int idx = av_find_best_stream(fmt, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        if (idx >= 0) { out = TrackInfo(); readMeta(fmt, idx, path, out); ok = true; }
    }
    avformat_close_input(&fmt);
    return ok;
}

// Khởi tạo decoder với sample rate đầu ra tuỳ chỉnh (mặc định 44100).
AudioDecoder::AudioDecoder(int outRate)
    : outRate_(outRate > 0 ? outRate : kOutRate) {
    avformat_network_init();
    pkt_   = av_packet_alloc();
    frame_ = av_frame_alloc();
}

AudioDecoder::~AudioDecoder() {
    close();
    av_packet_free(&pkt_);
    av_frame_free(&frame_);
}

void AudioDecoder::close() {
    if (frame_) av_frame_unref(frame_);
    if (pkt_) av_packet_unref(pkt_);
    swr_free(&swr_);                       // an toàn khi nullptr
    avcodec_free_context(&codec_);
    avformat_close_input(&fmt_);
    streamIdx_ = -1;
    flushed_ = false;
    info_ = TrackInfo();
    cover_.clear();
}

bool AudioDecoder::open(const std::string& path, std::string* err) {
    close();
    auto fail = [&](const char* msg, int code = 0) {
        if (err) {
            *err = msg;
            if (code < 0) {
                char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
                av_strerror(code, buf, sizeof buf);
                *err += std::string(": ") + buf;
            }
        }
        close();
        return false;
    };

    if (!pkt_ || !frame_) return fail("Không đủ bộ nhớ");

    int r = avformat_open_input(&fmt_, path.c_str(), nullptr, nullptr);   // fmt_ tự về nullptr nếu lỗi
    if (r < 0) return fail("Không mở được tệp", r);
    if ((r = avformat_find_stream_info(fmt_, nullptr)) < 0) return fail("Không đọc được thông tin luồng", r);

    streamIdx_ = av_find_best_stream(fmt_, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (streamIdx_ < 0) return fail("Không có luồng audio", streamIdx_);
    AVStream* st = fmt_->streams[streamIdx_];

    const AVCodec* dec = avcodec_find_decoder(st->codecpar->codec_id);
    if (!dec) return fail("FFmpeg build này không có decoder cho codec của tệp");
    codec_ = avcodec_alloc_context3(dec);
    if (!codec_) return fail("Không đủ bộ nhớ");
    if ((r = avcodec_parameters_to_context(codec_, st->codecpar)) < 0) return fail("avcodec_parameters_to_context", r);
    codec_->pkt_timebase = st->time_base;
    if ((r = avcodec_open2(codec_, dec, nullptr)) < 0) return fail("avcodec_open2", r);

    if (codec_->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC)
        av_channel_layout_default(&codec_->ch_layout, codec_->ch_layout.nb_channels > 0 ? codec_->ch_layout.nb_channels : 2);

    // Dùng outRate_ thay vì hằng số kOutRate để hỗ trợ sample rate động (v1.1)
    AVChannelLayout outLayout;
    av_channel_layout_default(&outLayout, kOutChannels);
    r = swr_alloc_set_opts2(&swr_, &outLayout, AV_SAMPLE_FMT_FLT, outRate_,
                            &codec_->ch_layout, codec_->sample_fmt, codec_->sample_rate, 0, nullptr);
    av_channel_layout_uninit(&outLayout);
    if (r < 0) return fail("swr_alloc_set_opts2", r);
    if ((r = swr_init(swr_)) < 0) return fail("swr_init", r);

    // ----- metadata -----
    readMeta(fmt_, streamIdx_, path, info_);
    info_.sampleRate = codec_->sample_rate;
    info_.channels = codec_->ch_layout.nb_channels;

    // ----- ảnh bìa: luồng "video" có cờ ATTACHED_PIC (MP3 APIC, FLAC PICTURE, M4A covr...) -----
    for (unsigned i = 0; i < fmt_->nb_streams; ++i) {
        AVStream* s = fmt_->streams[i];
        if ((s->disposition & AV_DISPOSITION_ATTACHED_PIC) && s->attached_pic.size > 0) {
            cover_.assign(s->attached_pic.data, s->attached_pic.data + s->attached_pic.size);
            info_.coverCodecId = int(s->codecpar->codec_id);
            break;
        }
    }
    return true;
}

bool AudioDecoder::convert(const AVFrame* f, std::vector<float>& out) {
    const int maxOut = swr_get_out_samples(swr_, f->nb_samples);
    if (maxOut < 0) return false;
    const size_t old = out.size();
    out.resize(old + size_t(maxOut) * kOutChannels);
    uint8_t* dst[1] = { reinterpret_cast<uint8_t*>(out.data() + old) };
    const int got = swr_convert(swr_, dst, maxOut, (const uint8_t**)f->extended_data, f->nb_samples);
    if (got < 0) { out.resize(old); return false; }
    out.resize(old + size_t(got) * kOutChannels);
    return true;
}

void AudioDecoder::flushResampler(std::vector<float>& out) {
    const int maxOut = swr_get_out_samples(swr_, 0);
    if (maxOut <= 0) return;
    const size_t old = out.size();
    out.resize(old + size_t(maxOut) * kOutChannels);
    uint8_t* dst[1] = { reinterpret_cast<uint8_t*>(out.data() + old) };
    const int got = swr_convert(swr_, dst, maxOut, nullptr, 0);
    out.resize(old + size_t(std::max(got, 0)) * kOutChannels);
}

int AudioDecoder::readFrames(std::vector<float>& out) {
    out.clear();
    if (!fmt_) return -1;
    while (out.empty()) {
        int r = avcodec_receive_frame(codec_, frame_);
        if (r == 0) {
            const bool ok = convert(frame_, out);
            av_frame_unref(frame_);
            if (!ok) return -1;
            continue;
        }
        if (r == AVERROR_EOF) { flushResampler(out); break; }          // decoder đã xả hết
        if (r != AVERROR(EAGAIN)) return -1;
        if (flushed_) break;

        r = av_read_frame(fmt_, pkt_);
        if (r < 0) {                                                    // hết file -> xả decoder
            avcodec_send_packet(codec_, nullptr);
            flushed_ = true;
            continue;
        }
        if (pkt_->stream_index == streamIdx_) avcodec_send_packet(codec_, pkt_);
        av_packet_unref(pkt_);                                          // luôn giải phóng packet
    }
    return int(out.size() / kOutChannels);
}

bool AudioDecoder::seek(double sec) {
    if (!fmt_ || !codec_) return false;
    sec = std::max(0.0, sec);
    if (av_seek_frame(fmt_, -1, int64_t(sec * AV_TIME_BASE), AVSEEK_FLAG_BACKWARD) < 0) return false;
    avcodec_flush_buffers(codec_);
    swr_close(swr_);
    swr_init(swr_);
    av_packet_unref(pkt_);
    av_frame_unref(frame_);
    flushed_ = false;
    return true;
}
