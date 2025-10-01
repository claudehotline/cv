#pragma once

#include "media/encoder.hpp"

#ifdef USE_FFMPEG
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}
#endif

namespace va::media {

class FfmpegH264Encoder : public IEncoder {
public:
    FfmpegH264Encoder();
    ~FfmpegH264Encoder() override;

    bool open(const Settings& settings) override;
    bool encode(const core::Frame& frame, Packet& out_packet) override;
    void close() override;

private:
    bool opened_ {false};
#ifdef USE_FFMPEG
    AVCodecContext* codec_ctx_ {nullptr};
    AVFrame* frame_ {nullptr};
    AVPacket* packet_ {nullptr};
    SwsContext* sws_ctx_ {nullptr};
    int width_ {0};
    int height_ {0};
    int fps_ {0};
    int64_t pts_ {0};
#endif
};

} // namespace va::media
