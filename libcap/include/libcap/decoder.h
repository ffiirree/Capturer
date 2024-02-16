#ifndef CAPTURER_DECODER_H
#define CAPTURER_DECODER_H

#include "ffmpeg-wrapper.h"
#include "logging.h"
#include "producer.h"
#include "queue.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

class Decoder : public Producer<av::frame>
{
    enum
    {
        DEMUXING_EOF  = 0x10,
        VDECODING_EOF = 0x01,
        ADECODING_EOF = 0x02,
        SDECODING_EOF = 0x02,
        DECODING_EOF  = DEMUXING_EOF | VDECODING_EOF | ADECODING_EOF | SDECODING_EOF
    };

public:
    using super = Producer<AVFrame>;

    ~Decoder() override { destroy(); }

    // reset and stop the thread
    void reset() override;

    // open input
    int open(const std::string& name, std::map<std::string, std::string> options = {}) override;

    // start thread
    int run() override;

    int produce(av::frame& frame, AVMediaType type) override;

    bool empty(AVMediaType type) override
    {
        switch (type) {
        case AVMEDIA_TYPE_VIDEO: return vbuffer_.empty();
        case AVMEDIA_TYPE_AUDIO: return abuffer_.empty();
        default:                 return true;
        }
    }

    // AV_TIME_BASE
    void seek(const std::chrono::nanoseconds& ts, std::chrono::nanoseconds lts,
              std::chrono::nanoseconds rts) override;

    bool has(AVMediaType) const override;

    std::string format_str(AVMediaType) const override;

    AVRational time_base(AVMediaType) const override;

    bool eof() override;

    std::vector<av::vformat_t> vformats() const override { return { vfmt }; }

    std::vector<av::aformat_t> aformats() const override { return { afmt }; }

    int64_t duration() const override { return fmt_ctx_ ? fmt_ctx_->duration : AV_NOPTS_VALUE; }

    bool has_enough(AVMediaType type) const;

    std::vector<std::map<std::string, std::string>> properties(AVMediaType) const override;

private:
    int  decode_fn();
    void destroy();

    std::string name_{ "unknown" };

    std::mutex   mtx_{};
    std::jthread thread_{};

    AVFormatContext *fmt_ctx_{};
    AVCodecContext  *vcodec_ctx_{};
    AVCodecContext  *acodec_ctx_{};
    AVCodecContext  *scodec_ctx_{};

    int vstream_idx_{ -1 };
    int astream_idx_{ -1 };
    int sstream_idx_{ -1 };

    int64_t SYNC_PTS{ 0 };

    av::packet packet_{};
    av::frame  frame_{};

    int64_t    audio_next_pts_{ AV_NOPTS_VALUE }; // samples
    AVRational audio_next_pts_tb_{};

    safe_queue<av::frame>  vbuffer_{};
    safe_queue<av::frame>  abuffer_{};
    safe_queue<AVSubtitle> sbuffer_{};
};

#endif //! CAPTURER_DECODER_H
