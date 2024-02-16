#ifndef CAPTURER_ENCODER_H
#define CAPTURER_ENCODER_H

#include "audio-fifo.h"
#include "consumer.h"
#include "logging.h"
#include "ringvector.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

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
    int consume(AVFrame *frame, AVMediaType type) override;

    bool full(AVMediaType type) const override
    {
        switch (type) {
        case AVMEDIA_TYPE_VIDEO: return vbuffer_.full();
        case AVMEDIA_TYPE_AUDIO: return abuffer_->size() > 4096;
        default:                 return true;
        }
    }

    bool accepts(AVMediaType type) const override
    {
        switch (type) {
        case AVMEDIA_TYPE_VIDEO: return video_enabled_;
        case AVMEDIA_TYPE_AUDIO: return audio_enabled_;
        default:                 return false;
        }
    }

    void enable(AVMediaType type, bool v = true) override
    {
        switch (type) {
        case AVMEDIA_TYPE_VIDEO: video_enabled_ = v; break;
        case AVMEDIA_TYPE_AUDIO: audio_enabled_ = v; break;
        default:                 break;
        }
    }

    bool eof() const override { return eof_ == ENCODING_EOF; }

private:
    int new_video_stream(const std::string& codec_name);
    int new_auido_stream(const std::string& codec_name);

    std::pair<int, int> video_sync_process();
    int                 process_video_frames();
    int                 process_audio_frames();
    void                destroy();

    int               video_stream_idx_{ -1 };
    int               audio_stream_idx_{ -1 };
    std::atomic<bool> video_enabled_{ false };
    std::atomic<bool> audio_enabled_{ false };

    // ffmpeg encoders @ {
    AVFormatContext *fmt_ctx_{};
    AVCodecContext  *vcodec_ctx_{};
    AVCodecContext  *acodec_ctx_{};
    // @}

    int64_t v_last_dts_{ AV_NOPTS_VALUE };
    int64_t a_last_dts_{ AV_NOPTS_VALUE };
    int64_t audio_pts_{ 0 };

    av::packet packet_{};
    av::frame  filtered_frame_{};
    av::frame  last_frame_{};

    // the expected pts of next video frame computed by last pts and duration
    int64_t expected_pts_{ AV_NOPTS_VALUE };

    std::unique_ptr<safe_audio_fifo> abuffer_{};
    RingVector<AVFrame *, 8>         vbuffer_{
        []() { return av_frame_alloc(); },
        [](AVFrame **frame) {         av_frame_free(frame); },
    };
};

#endif //! CAPTURER_ENCODER_H
