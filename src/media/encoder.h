#ifndef CAPTURER_ENCODER_H
#define CAPTURER_ENCODER_H

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}
#include "consumer.h"
#include "logging.h"
#include "utils.h"
#include "ringvector.h"

class Encoder : public Consumer<AVFrame> {
public:
    enum { V_ENCODING_EOF = 0x01, A_ENCODING_EOF = 0x02, ENCODING_EOF = V_ENCODING_EOF | A_ENCODING_EOF };
public:
    ~Encoder() override { destroy(); }
    void reset() override;

    int open(const std::string& filename, const std::string& codec_name,
             bool is_cfr = false, const std::map<std::string, std::string>& options = {});
    
    int run() override;
    int consume(AVFrame* frame, int type) override;

    bool full(int type) override
    {
        switch (type) {
        case AVMEDIA_TYPE_VIDEO:
            return video_buffer_.full();
        case AVMEDIA_TYPE_AUDIO:
            return video_buffer_.full();
        default: return true;
        }
    }

    int format() const override { return pix_fmt_; }

    bool eof() const override { return eof_ == ENCODING_EOF; }

    void width(int w) { width_ = w; }
    void height(int h) { height_ = h; }
    void format(enum AVPixelFormat pix_fmt) { pix_fmt_ = pix_fmt; }
    void framerate(const AVRational& f) { framerate_ = f; }
    void sample_aspect_ratio(const AVRational& sar) { sample_aspect_ratio_ = sar; }
    void v_stream_tb(const AVRational& tb) { v_stream_time_base_ = tb; }
    void a_stream_tb(const AVRational& tb) { a_stream_time_base_ = tb; }

private:
    int run_f();
    void destroy();

    enum AVPixelFormat pix_fmt_{ AV_PIX_FMT_YUV420P };
    int video_stream_idx_{ -1 };
    int audio_stream_idx_{ -1 };

    int width_{ 0 };
    int height_{ 0 };
    
    bool is_cfr_{ false };
    AVFormatContext* fmt_ctx_{ nullptr };
    AVRational framerate_{ 24, 1 };
    AVRational sample_aspect_ratio_{ 1,1 };
    AVRational v_stream_time_base_{ 1, AV_TIME_BASE };
    AVRational a_stream_time_base_{ 1, AV_TIME_BASE };

    AVCodecContext* video_encoder_ctx_{ nullptr };
    AVCodecContext* audio_encoder_ctx_{ nullptr };
    AVCodec* video_encoder_{ nullptr };
    AVCodec* audio_encoder_{ nullptr };

    int64_t first_pts_{ AV_NOPTS_VALUE };
    int64_t last_dts_{ AV_NOPTS_VALUE };

    AVPacket* packet_{ nullptr };
    AVFrame* filtered_frame_{ nullptr };

    RingVector<AVFrame*, 6> video_buffer_{
        []() { return av_frame_alloc(); },
        [](AVFrame** frame) { av_frame_free(frame); }
    };

    RingVector<AVFrame*, 12> audio_buffer_{
        []() { return av_frame_alloc(); },
        [](AVFrame** frame) { av_frame_free(frame); }
    };
};

#endif // !CAPTURER_ENCODER_H
