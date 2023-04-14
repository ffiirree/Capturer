#include "decoder.h"
#include "fmt/format.h"
#include "fmt/ranges.h"
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavutil/time.h>
#include <libavutil/pixdesc.h>
}
#include "clock.h"
#include "defer.h"

int Decoder::open(const std::string& name, const std::string& format, const std::map<std::string, std::string>& options)
{
    LOG(INFO) << fmt::format("[   DECODER] [{:>10}] format = {}, options = {}", name, format, options);

    name_ = name;

    // format context
    if (fmt_ctx_) destroy();

    avdevice_register_all();

    // input format
#if LIBAVFORMAT_VERSION_MAJOR >= 59
    const AVInputFormat* input_fmt = nullptr;
#else
    AVInputFormat* input_fmt = nullptr;
#endif
    if (!format.empty()) {
        input_fmt = av_find_input_format(format.c_str());
        if (!input_fmt) {
            LOG(ERROR) << "[   DECODER] av_find_input_format";
            return -1;
        }
    }

    // options
    AVDictionary* input_options = nullptr;
    defer(av_dict_free(&input_options));
    for (const auto& [key, value] : options) {
        av_dict_set(&input_options, key.c_str(), value.c_str(), 0);
    }

    // open input
    if (avformat_open_input(&fmt_ctx_, name.c_str(), input_fmt, &input_options) < 0) {
        LOG(ERROR) << "[   DECODER] failed to open " << name_;
        return -1;
    }

    if (avformat_find_stream_info(fmt_ctx_, nullptr) < 0) {
        LOG(ERROR) << "[   DECODER] avformat_find_stream_info";
        return -1;
    }

    // av_dump_format(fmt_ctx_, 0, name.c_str(), 0);

    // find video & audio streams
#if LIBAVCODEC_VERSION_MAJOR >= 59
    const AVCodec* video_decoder = nullptr;
    const AVCodec* audio_decoder = nullptr;
#else
    AVCodec* video_decoder = nullptr;
    AVCodec* audio_decoder = nullptr;
#endif
    video_stream_idx_ = av_find_best_stream(fmt_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, &video_decoder, 0);
    audio_stream_idx_ = av_find_best_stream(fmt_ctx_, AVMEDIA_TYPE_AUDIO, -1, -1, &audio_decoder, 0);
    if (video_stream_idx_ < 0 && audio_stream_idx_ < 0) {
        LOG(ERROR) << "[   DECODER] not found any stream";
        return -1;
    }

    VIDEO_OFFSET_TIME = video_stream_idx_ < 0 ? 0 : av_rescale_q(os_gettime_ns(), OS_TIME_BASE_Q, fmt_ctx_->streams[video_stream_idx_]->time_base) - fmt_ctx_->streams[video_stream_idx_]->start_time;
    AUDIO_OFFSET_TIME = audio_stream_idx_ < 0 ? 0 : av_rescale_q(os_gettime_ns(), OS_TIME_BASE_Q, fmt_ctx_->streams[audio_stream_idx_]->time_base) - fmt_ctx_->streams[audio_stream_idx_]->start_time;

    if (video_stream_idx_ >= 0) {
        // decoder context
        video_decoder_ctx_ = avcodec_alloc_context3(video_decoder);
        if (!video_decoder_ctx_) {
            LOG(ERROR) << "[   DECODER] avcodec_alloc_context3";
            return -1;
        }

        if (avcodec_parameters_to_context(video_decoder_ctx_, fmt_ctx_->streams[video_stream_idx_]->codecpar) < 0) {
            LOG(ERROR) << "[   DECODER] avcodec_parameters_to_context";
            return -1;
        }

        // open codec
        AVDictionary* decoder_options = nullptr;
        defer(av_dict_free(&decoder_options));
        av_dict_set(&decoder_options, "threads", "auto", 0);
        if (avcodec_open2(video_decoder_ctx_, video_decoder, &decoder_options) < 0) {
            LOG(ERROR) << "[DECODER] avcodec_open2";
            return -1;
        }

        auto fr = av_guess_frame_rate(fmt_ctx_, fmt_ctx_->streams[video_stream_idx_], nullptr);
        LOG(INFO) << fmt::format("[   DECODER] [{:>10}] {}x{}, format = {}, fps = {}/{}, tbn = {}/{}",
            name_,
            video_decoder_ctx_->width, video_decoder_ctx_->height, av_get_pix_fmt_name(video_decoder_ctx_->pix_fmt),
            fr.num, fr.den,
            fmt_ctx_->streams[video_stream_idx_]->time_base.num, fmt_ctx_->streams[video_stream_idx_]->time_base.den
        );
    }

    if (audio_stream_idx_ >= 0) {
        audio_decoder_ctx_ = avcodec_alloc_context3(audio_decoder);
        if (!audio_decoder_ctx_) {
            LOG(ERROR) << "[   DECODER] avcodec_alloc_context3 failed for audio";
            return false;
        }

        if (avcodec_parameters_to_context(audio_decoder_ctx_, fmt_ctx_->streams[audio_stream_idx_]->codecpar) < 0) {
            LOG(ERROR) << "[   DECODER] avcodec_parameters_to_context";
            return false;
        }

        if (avcodec_open2(audio_decoder_ctx_, audio_decoder, nullptr) < 0) {
            LOG(ERROR) << "[   DECODER] avcodec_open2 failed for audio";
            return false;
        }

        LOG(INFO) << fmt::format("[    DECODER] [{:>10}] sample_rate = {}, channels = {}, channel_layout = {}, format = {}, frame_size = {}, start_time = {}, tbn = {}/{}",
            name_,
            audio_decoder_ctx_->sample_rate, audio_decoder_ctx_->channels,
            av_get_default_channel_layout(audio_decoder_ctx_->channels),
            av_get_sample_fmt_name(audio_decoder_ctx_->sample_fmt),
            audio_decoder_ctx_->frame_size,
            fmt_ctx_->streams[audio_stream_idx_]->start_time,
            fmt_ctx_->streams[audio_stream_idx_]->time_base.num, fmt_ctx_->streams[audio_stream_idx_]->time_base.den
        );
    }

    // prepare 
    packet_ = av_packet_alloc();
    decoded_frame_ = av_frame_alloc();
    if (!packet_ || !decoded_frame_) {
        LOG(ERROR) << "[   DECODER] av_frame_alloc";
        return -1;
    }

    ready_ = true;

    LOG(INFO) << fmt::format("[   DECODER] [{:>10}] is opened", name_);
    return 0;
}

bool Decoder::has(int type) const
{
    switch (type)
    {
    case AVMEDIA_TYPE_VIDEO: return video_stream_idx_ >= 0;
    case AVMEDIA_TYPE_AUDIO: return audio_stream_idx_ >= 0;
    default: return false;
    }
}

std::string Decoder::format_str(int type) const
{
    switch (type)
    {
    case AVMEDIA_TYPE_VIDEO:
    {
        if (video_stream_idx_ < 0) return {};

        auto video_stream = fmt_ctx_->streams[video_stream_idx_];
        auto fr = av_guess_frame_rate(fmt_ctx_, video_stream, nullptr);
        return fmt::format(
            "video_size={}x{}:pix_fmt={}:time_base={}/{}:pixel_aspect={}/{}:frame_rate={}/{}",
            video_decoder_ctx_->width, video_decoder_ctx_->height, video_decoder_ctx_->pix_fmt,
            video_stream->time_base.num, video_stream->time_base.den,
            video_stream->codecpar->sample_aspect_ratio.num, std::max<int>(1, video_stream->codecpar->sample_aspect_ratio.den),
            fr.num, fr.den
        );
    }
    case AVMEDIA_TYPE_AUDIO:
    {
        if (audio_stream_idx_ < 0) return {};

        auto audio_stream = fmt_ctx_->streams[audio_stream_idx_];
        return fmt::format(
            "sample_rate={}:sample_fmt={}:channels={}:channel_layout={}:time_base={}/{}",
            audio_decoder_ctx_->sample_rate, audio_decoder_ctx_->sample_fmt, 
            audio_decoder_ctx_->channels,
            av_get_default_channel_layout(audio_decoder_ctx_->channels),
            audio_stream->time_base.num, audio_stream->time_base.den
        );
    }
    default: return {};
    }
}


AVRational Decoder::time_base(int type) const
{
    switch (type)
    {
    case AVMEDIA_TYPE_AUDIO:
    {
        if (audio_stream_idx_ < 0) {
            LOG(WARNING) << "no audio stream";
            return OS_TIME_BASE_Q;
        }

        return fmt_ctx_->streams[audio_stream_idx_]->time_base;
    }
    case AVMEDIA_TYPE_VIDEO:
    {
        if (video_stream_idx_ < 0) {
            LOG(WARNING) << "[   DECODER] [V] no video stream";
            return OS_TIME_BASE_Q;
        }

        return fmt_ctx_->streams[video_stream_idx_]->time_base;
    }
    default: 
        LOG(ERROR) << "[   DECODER] [X] unknown meida type.";
        return OS_TIME_BASE_Q;
    }
}

bool Decoder::eof()
{ 
    return (eof_ == DECODING_EOF) && video_buffer_.empty() && audio_buffer_.empty(); 
}

int Decoder::produce(AVFrame* frame, int type)
{
    switch (type)
    {
    case AVMEDIA_TYPE_VIDEO:
        if (video_buffer_.empty()) return (eof_ & VDECODING_EOF) ? AVERROR_EOF : AVERROR(EAGAIN);

        video_buffer_.pop(
            [frame](AVFrame* popped) {
                av_frame_unref(frame);
                av_frame_move_ref(frame, popped);
            }
        );
        return 0;

    case AVMEDIA_TYPE_AUDIO:
        if (audio_buffer_.empty()) return (eof_ & ADECODING_EOF) ? AVERROR_EOF : AVERROR(EAGAIN);

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

int Decoder::run()
{
    std::lock_guard lock(mtx_);

    if (!ready_ || running_) {
        LOG(ERROR) << fmt::format("[   DECODER] [{:>10}] already running or not ready", name_);
        return -1;
    }

    eof_ = 0x00;
    running_ = true;
    thread_ = std::thread([this]() {
        platform::util::thread_set_name("dec-" + name_);
        run_f();
    });

    return 0;
}

int Decoder::run_f()
{
    LOG(INFO) << "STARTED";

    // reset the buffer
    video_buffer_.clear();
    audio_buffer_.clear();

    eof_ |= audio_stream_idx_ < 0 ? ADECODING_EOF : 0x00;
    eof_ |= video_stream_idx_ < 0 ? VDECODING_EOF : 0x00;

    while (running_) {
        if (video_buffer_.full()) {
            LOG(WARNING) <<"[V] buffer is full, drop the packet";
        }

        if (audio_buffer_.full()) {
            LOG(WARNING) << "[A] buffer is full, drop the packet";
        }

        // read
        av_packet_unref(packet_);
        int ret = av_read_frame(fmt_ctx_, packet_);
        if ((ret == AVERROR_EOF || avio_feof(fmt_ctx_->pb)) && !(eof_ & DEMUXING_EOF)) {
            // [draining] 1. Instead of valid input, send NULL to the avcodec_send_packet() (decoding) or avcodec_send_frame() (encoding) functions. 
            //               This will enter draining mode.
            // [draining] 2. Call avcodec_receive_frame() (decoding) or avcodec_receive_packet() (encoding) in a loop until AVERROR_EOF is returned.
            //               The functions will not return AVERROR(EAGAIN), unless you forgot to enter draining mode.
            LOG(INFO) << "EOF => PUT NULL PACKET TO ENTER DRAINING MODE";
            eof_ |= DEMUXING_EOF;
        }
        else if (ret < 0) {
            LOG(ERROR) << "failed to read frame.";
            running_ = false;
            break;
        }

        // video decoding
        if (packet_->stream_index == video_stream_idx_ || ((eof_ & DEMUXING_EOF) && !(eof_ & VDECODING_EOF))) {
            ret = avcodec_send_packet(video_decoder_ctx_, packet_);
            while (ret >= 0) {
                av_frame_unref(decoded_frame_);
                ret = avcodec_receive_frame(video_decoder_ctx_, decoded_frame_);
                if (ret == AVERROR(EAGAIN)) {
                    break;
                }
                else if (ret == AVERROR_EOF) {
                    LOG(INFO) << "[V] EOF";
                    eof_ |= VDECODING_EOF;
                    break;
                }
                else if (ret < 0) {
                    running_ = false;
                    LOG(INFO) << "[V] DECODING ERROR";
                    return ret;
                }

                decoded_frame_->pts += VIDEO_OFFSET_TIME;

                DLOG(INFO) << fmt::format("[V] frame = {:>5d}, pts = {:>14d}",
                    video_decoder_ctx_->frame_number, decoded_frame_->pts);

                video_buffer_.push(
                    [=](AVFrame* frame) {
                        av_frame_unref(frame);
                        av_frame_move_ref(frame, decoded_frame_);
                    }
                );
            }
        }
        
        // audio decoding
        if (packet_->stream_index == audio_stream_idx_ || ((eof_ & DEMUXING_EOF) && !(eof_ & ADECODING_EOF))) {
            ret = avcodec_send_packet(audio_decoder_ctx_, packet_);
            while (ret >= 0) {
                av_frame_unref(decoded_frame_);
                ret = avcodec_receive_frame(audio_decoder_ctx_, decoded_frame_);
                if (ret == AVERROR(EAGAIN)) {
                    break;
                }
                else if (ret == AVERROR_EOF) {
                    LOG(INFO) <<"[A] EOF";
                    eof_ |= ADECODING_EOF;
                    break;
                }
                else if (ret < 0) {
                    running_ = false;
                    LOG(INFO) << "[A] DECODING ERROR";
                    return ret;
                }

                decoded_frame_->pts += AUDIO_OFFSET_TIME;

                DLOG(INFO) << fmt::format("[A] frame = {:>5d}, pts = {:>14d}, samples = {:>5d}, muted = {}",
                    audio_decoder_ctx_->frame_number, decoded_frame_->pts, decoded_frame_->nb_samples, muted_);

                if (muted_) {
                    av_samples_set_silence(
                        decoded_frame_->data,
                        0,
                        decoded_frame_->nb_samples,
                        decoded_frame_->channels,
                        static_cast<AVSampleFormat>(decoded_frame_->format)
                    );
                }

                audio_buffer_.push(
                    [=](AVFrame* frame) {
                        av_frame_unref(frame);
                        av_frame_move_ref(frame, decoded_frame_);
                    }
                );
            }
        } // decoding
    } // while(running_)

    if (video_stream_idx_ >= 0) {
        LOG(INFO) << fmt::format("[V] decoded frames = {:>5d}",
            video_decoder_ctx_->frame_number);
    }
    LOG(INFO) << "EXITED";

    return 0;
}

void Decoder::reset()
{
    LOG(INFO) << fmt::format("[   DECODER] [{:>10}] RESET", name_);

    destroy();
}

void Decoder::destroy()
{
    std::lock_guard lock(mtx_);

    running_ = false;
    ready_ = false;

    eof_ = DECODING_EOF;

    wait();

    name_ = {};
    muted_ = false;

    video_stream_idx_ = -1;
    audio_stream_idx_ = -1;

    enabled_.clear();

    av_packet_free(&packet_);
    av_frame_free(&decoded_frame_);

    avcodec_free_context(&video_decoder_ctx_);
    avcodec_free_context(&audio_decoder_ctx_);
    avformat_close_input(&fmt_ctx_);
}