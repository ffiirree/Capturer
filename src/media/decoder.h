#ifndef CAPTURER_DECODER_H
#define CAPTURER_DECODER_H

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}
#include "utils.h"
#include "logging.h"
#include "producer.h"
#include "ringvector.h"

class Decoder : public Producer<AVFrame> {
    enum {
        DEMUXING_EOF = 0x10,
        VDECODING_EOF = 0x01,
        ADECODING_EOF = 0x02,
        DECODING_EOF = DEMUXING_EOF | VDECODING_EOF | ADECODING_EOF
    };
public:
    ~Decoder() override { destroy(); }

    // reset and stop the thread
    void reset() override;
    
    // open input
    int open(const std::string& name, const std::string& format = "", const std::map<std::string, std::string>& options = {});

    // start thread
    int run() override;

    int produce(AVFrame* frame, int type) override;

    bool empty(int type) override
    {
        switch (type)
        {
        case AVMEDIA_TYPE_VIDEO: return video_buffer_.empty();
        case AVMEDIA_TYPE_AUDIO: return audio_buffer_.empty();
        default: return true;
        }
    }

    bool has(int) const override;
    std::string format_str(int) const override;
    AVRational time_base(int) const override;
    bool eof() override;

    void mute(bool v) { muted_ = v; }

private:
    int run_f();
    void destroy();

    std::atomic<bool> muted_{ false };

    std::string name_{"unknown"};

    AVFormatContext* fmt_ctx_{ nullptr };
    AVCodecContext* video_decoder_ctx_{ nullptr };
    AVCodecContext* audio_decoder_ctx_{ nullptr };

    int video_stream_idx_{ -1 };
    int audio_stream_idx_{ -1 };

    AVPacket* packet_{ nullptr };
    AVFrame* decoded_frame_{ nullptr };

    RingVector<AVFrame*, 4> video_buffer_{
        []() { return av_frame_alloc(); },
        [](AVFrame** frame) { av_frame_free(frame); }
    };

    RingVector<AVFrame*, 4> audio_buffer_{
        []() { return av_frame_alloc(); },
        [](AVFrame** frame) { av_frame_free(frame); }
    };
};

#endif // !CAPTURER_DECODER_H
