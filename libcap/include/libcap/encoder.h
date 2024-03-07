#ifndef CAPTURER_ENCODER_H
#define CAPTURER_ENCODER_H

#include "audio-fifo.h"
#include "consumer.h"
#include "ffmpeg-wrapper.h"
#include "logging.h"
#include "queue.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

class Encoder final : public Consumer<av::frame>
{
public:
    enum
    {
        V_ENCODING_EOF = 0x01,
        A_ENCODING_EOF = 0x02,
        ENCODING_EOF   = V_ENCODING_EOF | A_ENCODING_EOF,
    };

public:
    ~Encoder() override;

    int open(const std::string&, std::map<std::string, std::string>) override;

    int start() override;

    void stop() override;

    int consume(const av::frame& frame, AVMediaType type) override;

    bool accepts(AVMediaType type) const override;

    void enable(AVMediaType type, bool v) override;

    bool eof() const override
    {
        return (vstream_idx_ < 0 || eof_ & V_ENCODING_EOF) && (astream_idx_ < 0 || eof_ & A_ENCODING_EOF);
    }

private:
    int new_video_stream(const std::string& codec_name);
    int new_auido_stream(const std::string& codec_name);

    std::pair<int, int> video_sync_process(av::frame& frame);
    int                 process_video_frames();
    int                 process_audio_frames();
    void                close_output_file();

    int               vstream_idx_{ -1 };
    int               astream_idx_{ -1 };
    std::atomic<bool> video_enabled_{ false };
    std::atomic<bool> audio_enabled_{ false };

    int crf_{ -1 };

    // ffmpeg encoders @ {
    AVFormatContext *fmt_ctx_{};
    AVCodecContext  *vcodec_ctx_{};
    AVCodecContext  *acodec_ctx_{};
    // @}

    std::jthread thread_{};

    int64_t v_last_dts_{ AV_NOPTS_VALUE };
    int64_t a_last_dts_{ AV_NOPTS_VALUE };
    int64_t audio_pts_{ 0 };

    av::packet packet_{};
    av::frame  last_frame_{};

    // the expected pts of next video frame computed by last pts and duration
    int64_t expected_pts_{ AV_NOPTS_VALUE };

    std::atomic<bool>                asrc_eof_{};
    std::unique_ptr<safe_audio_fifo> abuffer_{};
    safe_queue<av::frame>            vbuffer_{ 8 };

    av::vsync_t vsync_{ av::vsync_t::cfr };
};

#endif //! CAPTURER_ENCODER_H
