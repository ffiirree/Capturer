#ifndef CAPTURER_DECODER_H
#define CAPTURER_DECODER_H

#include "ffmpeg-wrapper.h"
#include "logging.h"
#include "producer.h"
#include "queue.h"

#include <shared_mutex>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

class Decoder final : public Producer<av::frame>
{
    enum : uint8_t
    {
        VDECODING_EOF = 0x01,
        ADECODING_EOF = 0x02,
        SDECODING_EOF = 0x04,
        DECODING_EOF  = VDECODING_EOF | ADECODING_EOF | SDECODING_EOF
    };

public:
    using super = Producer<AVFrame>;

    ~Decoder() override;

    // open input
    int open(const std::string& name, std::map<std::string, std::string> options) override;

    // start thread
    int start() override;

    // reset and stop the thread
    void stop() override;

    void seek(const std::chrono::nanoseconds& ts, const std::chrono::nanoseconds& rel) override;

    bool seeking() const;

    bool has(AVMediaType) const override;

    std::chrono::nanoseconds start_time() const override;

    std::chrono::nanoseconds duration() const override;

    bool eof() override;

    bool is_realtime() const override { return false; }

    std::vector<std::map<std::string, std::string>> properties(AVMediaType) const override;

private:
    int decode_fn();

    std::string name_{ "unknown" };

    std::jthread thread_{};

    AVFormatContext *fmt_ctx_{};
    AVCodecContext  *vcodec_ctx_{};
    AVCodecContext  *acodec_ctx_{};
    AVCodecContext  *scodec_ctx_{};

    int vstream_idx_{ -1 };
    int astream_idx_{ -1 };
    int sstream_idx_{ -1 };

    av::frame frame_{};

    std::atomic<bool> feof_{};

    // seek, AV_TIME_BASE @{
    mutable std::shared_mutex seek_mtx_{};
    int64_t                   seek_min_{ std::numeric_limits<int64_t>::min() };
    int64_t                   seek_pts_{ AV_NOPTS_VALUE };
    int64_t                   seek_max_{ std::numeric_limits<int64_t>::max() };
    // @}

    int64_t    audio_next_pts_{ AV_NOPTS_VALUE }; // samples
    AVRational audio_next_pts_tb_{};

    bool is_realtime_{ false };
};

#endif //! CAPTURER_DECODER_H
