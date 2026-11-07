#include "CoverArt.h"
#include <algorithm>
#include <cstring>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

bool decodeCoverArt(int codecId, const uint8_t* data, size_t size, int maxSide,
                    std::vector<uint8_t>& rgba, int& outW, int& outH) {
    const AVCodec* codec = avcodec_find_decoder(static_cast<AVCodecID>(codecId));
    if (!codec || !data || size == 0) return false;

    AVCodecContext* ctx = avcodec_alloc_context3(codec);
    AVPacket* pkt = av_packet_alloc();
    AVFrame* frm = av_frame_alloc();
    SwsContext* sws = nullptr;
    bool ok = false;

    do {
        if (!ctx || !pkt || !frm) break;
        if (avcodec_open2(ctx, codec, nullptr) < 0) break;
        if (av_new_packet(pkt, int(size)) < 0) break;
        std::memcpy(pkt->data, data, size);
        if (avcodec_send_packet(ctx, pkt) < 0) break;
        avcodec_send_packet(ctx, nullptr);                  // flush để chắc chắn nhận được frame
        if (avcodec_receive_frame(ctx, frm) < 0) break;

        const int w = frm->width, h = frm->height;
        if (w <= 0 || h <= 0) break;
        const double sc = std::min(1.0, double(maxSide) / std::max(w, h));
        outW = std::max(1, int(w * sc));
        outH = std::max(1, int(h * sc));

        sws = sws_getContext(w, h, static_cast<AVPixelFormat>(frm->format),
                             outW, outH, AV_PIX_FMT_RGBA, SWS_BICUBIC, nullptr, nullptr, nullptr);
        if (!sws) break;
        rgba.resize(size_t(outW) * outH * 4);
        uint8_t* dst[4] = { rgba.data(), nullptr, nullptr, nullptr };
        int dstStride[4] = { outW * 4, 0, 0, 0 };
        sws_scale(sws, frm->data, frm->linesize, 0, h, dst, dstStride);
        ok = true;
    } while (false);

    sws_freeContext(sws);
    av_frame_free(&frm);
    av_packet_free(&pkt);
    avcodec_free_context(&ctx);
    return ok;
}
