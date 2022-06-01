#ifndef CAPTURER_DECODER_H
#define CAPTURER_DECODER_H

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
#include <libavutil/time.h>
#include <libavutil/imgutils.h>
}
#include "utils.h"
#include "logging.h"
#include "producer.h"
#include "ringvector.h"

const int FRAME_BUFFER_SIZE = 20;

class Decoder : public Producer<AVFrame> {
    enum {
        DEMUXING_EOF = 0x10,
        VDECODING_EOF = 0x01,
        ADECODING_EOF = 0x02,
        DECODING_EOF = DEMUXING_EOF | VDECODING_EOF | ADECODING_EOF
    };
public:
    void reset() override 
    {
        std::lock_guard lock(mtx_);

        running_ = false;
        ready_ = false;
        eof_ = false;

        wait();

        av_packet_free(&packet_);
        av_frame_free(&decoded_frame_);

        avcodec_free_context(&video_decoder_ctx_);
        avcodec_free_context(&audio_decoder_ctx_);
        avformat_close_input(&fmt_ctx_);

        video_buffer_.clear();
        audio_buffer_.clear();

        LOG(INFO) << "[DECODER] RESETED";
    }

    int open(const std::string& name, const std::string& format = "", const std::map<std::string, std::string>& options = {});

    int run() override
    {
        std::lock_guard lock(mtx_);

        if (!ready_ || running_) {
            LOG(ERROR) << "[DECODER] already running or not ready";
            return -1;
        }

        running_ = true;
        thread_ = std::thread([this]() { run_f(); });
        
        return 0;
    }

    int produce(AVFrame* frame, int type) override
    {
        switch (type)
        {
        case AVMEDIA_TYPE_VIDEO:
            if (video_buffer_.empty()) return -1;

            video_buffer_.pop(
                [frame](AVFrame* popped) {
                    av_frame_unref(frame);
                    av_frame_move_ref(frame, popped);
                }
            );
            return 0;

        case AVMEDIA_TYPE_AUDIO:
            if (audio_buffer_.empty()) return -1;

            audio_buffer_.pop(
                [frame](AVFrame* popped) {
                    av_frame_unref(frame);
                    av_frame_move_ref(frame, popped);
                }
            );
            return 0;

        default: return -1;
        }
    }

    bool empty(int type) override
    {
        switch (type)
        {
        case AVMEDIA_TYPE_VIDEO: return video_buffer_.empty();
        case AVMEDIA_TYPE_AUDIO: return audio_buffer_.empty();
        default: return true;
        }
    }

    std::string format_str() const;
    bool eof() override { return eof_ == DECODING_EOF; }

private:
    int run_f();

    AVFormatContext* fmt_ctx_{ nullptr };
    AVCodecContext* video_decoder_ctx_{ nullptr };
    AVCodecContext* audio_decoder_ctx_{ nullptr };
    AVCodec* video_decoder_{ nullptr };
    AVCodec* audio_decoder_{ nullptr };

    int video_stream_idx_{ -1 };
    int audio_stream_idx_{ -1 };

    int64_t first_pts_{ AV_NOPTS_VALUE };
    int64_t last_pts_{ AV_NOPTS_VALUE };

    AVPacket* packet_{ nullptr };
    AVFrame* decoded_frame_{ nullptr };

    RingVector<AVFrame*, FRAME_BUFFER_SIZE> video_buffer_{
        []() { return av_frame_alloc(); },
        [](AVFrame** frame) { av_frame_free(frame); }
    };

    RingVector<AVFrame*, FRAME_BUFFER_SIZE> audio_buffer_{
        []() { return av_frame_alloc(); },
        [](AVFrame** frame) { av_frame_free(frame); }
    };
};

#endif // !CAPTURER_DECODER_H
