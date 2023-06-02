#ifndef CAPTURER_ENCODER_H
#define CAPTURER_ENCODER_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
}
#include "consumer.h"
#include "logging.h"
#include "ringvector.h"

class Encoder : public Consumer<AVFrame>
{
public:
    enum
    {
        V_ENCODING_EOF      = 0x01,
        A_ENCODING_EOF      = 0x02,
        A_ENCODING_FIFO_EOF = 0x04,
        ENCODING_EOF        = V_ENCODING_EOF | A_ENCODING_EOF | A_ENCODING_FIFO_EOF
    };

public:
    ~Encoder() override { destroy(); }

    void reset() override;

    int open(const std::string&, std::map<std::string, std::string>) override;

    int run() override;
    int consume(AVFrame *frame, int type) override;

    bool full(int type) const override
    {
        switch (type) {
        case AVMEDIA_TYPE_VIDEO: return video_buffer_.full();
        case AVMEDIA_TYPE_AUDIO: return audio_buffer_.full();
        default: return true;
        }
    }

    bool accepts(int type) const override
    {
        switch (type) {
        case AVMEDIA_TYPE_VIDEO: return video_enabled_;
        case AVMEDIA_TYPE_AUDIO: return audio_enabled_;
        default: return false;
        }
    }

    void enable(int type, bool v = true) override
    {
        switch (type) {
        case AVMEDIA_TYPE_VIDEO: video_enabled_ = v; break;
        case AVMEDIA_TYPE_AUDIO: audio_enabled_ = v; break;
        }
    }

    bool eof() const override { return eof_ == ENCODING_EOF; }

private:
    int new_video_stream();
    int new_auido_stream();

    int run_f();
    std::pair<int, int> video_sync_process();
    int process_video_frames();
    int process_audio_frames();
    void destroy();

    int video_stream_idx_{ -1 };
    int audio_stream_idx_{ -1 };
    std::atomic<bool> video_enabled_{ false };
    std::atomic<bool> audio_enabled_{ false };

    // ffmpeg encoders @ {
    AVFormatContext *fmt_ctx_{ nullptr };
    std::string video_codec_name_{};
    std::string audio_codec_name_{};
    std::map<std::string, std::string> video_options_;
    std::map<std::string, std::string> audio_options_;
    AVCodecContext *video_encoder_ctx_{ nullptr };
    AVCodecContext *audio_encoder_ctx_{ nullptr };
    // @}

    int64_t v_last_dts_{ AV_NOPTS_VALUE };
    int64_t a_last_dts_{ AV_NOPTS_VALUE };
    int64_t audio_pts_{ 0 };

    AVPacket *packet_{ nullptr };
    AVFrame *filtered_frame_{ nullptr };
    AVFrame *last_frame_{ nullptr };
    // the expected pts of next video frame computed by last pts and duration
    int64_t expected_pts_{ AV_NOPTS_VALUE };

    AVAudioFifo *audio_fifo_buffer_{ nullptr };

    RingVector<AVFrame *, 8> video_buffer_{
        []() { return av_frame_alloc(); },
        [](AVFrame **frame) { av_frame_free(frame); },
    };

    RingVector<AVFrame *, 32> audio_buffer_{
        []() { return av_frame_alloc(); },
        [](AVFrame **frame) { av_frame_free(frame); },
    };
};

#endif //! CAPTURER_ENCODER_H
