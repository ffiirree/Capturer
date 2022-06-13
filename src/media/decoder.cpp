#include <chrono>
#include "decoder.h"
#include "fmt/format.h"
#include "fmt/ranges.h"
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavutil/time.h>
}

using namespace std::chrono_literals;

int Decoder::open(const std::string& name, const std::string& format, const std::map<std::string, std::string>& options)
{
    LOG(INFO) << fmt::format("[DECODER] \"{}\", format = {}, options = {}", name, format, options);

    // format context
    if (fmt_ctx_) destroy();
    fmt_ctx_ = avformat_alloc_context();
    if (!fmt_ctx_) {
        LOG(INFO) << "avformat_alloc_context";
        return -1;
    }

    avdevice_register_all();

    // input format
    AVInputFormat* input_fmt = nullptr;
    if (!format.empty()) {
        input_fmt = av_find_input_format(format.c_str());
        if (!input_fmt) {
            LOG(ERROR) << "av_find_input_format";
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
        LOG(ERROR) << "avformat_open_input";
        return -1;
    }

    if (avformat_find_stream_info(fmt_ctx_, nullptr) < 0) {
        LOG(ERROR) << "avformat_find_stream_info";
        return -1;
    }

    av_dump_format(fmt_ctx_, 0, name.c_str(), 0);

    // find video & audio streams
    video_stream_idx_ = av_find_best_stream(fmt_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    audio_stream_idx_ = av_find_best_stream(fmt_ctx_, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (video_stream_idx_ < 0 && audio_stream_idx_ < 0) {
        LOG(ERROR) << "not found any stream";
        return -1;
    }

    if (video_stream_idx_ >= 0) {
        // decoder
        video_decoder_ = avcodec_find_decoder(fmt_ctx_->streams[video_stream_idx_]->codecpar->codec_id);
        if (!video_decoder_) {
            LOG(ERROR) << "avcodec_find_decoder";
            return -1;
        }

        // decoder context
        video_decoder_ctx_ = avcodec_alloc_context3(video_decoder_);
        if (!video_decoder_ctx_) {
            LOG(ERROR) << "avcodec_alloc_context3";
            return -1;
        }

        if (avcodec_parameters_to_context(video_decoder_ctx_, fmt_ctx_->streams[video_stream_idx_]->codecpar) < 0) {
            LOG(ERROR) << "avcodec_parameters_to_context";
            return -1;
        }

        // open codec
        AVDictionary* decoder_options = nullptr;
        defer(av_dict_free(&decoder_options));
        av_dict_set(&decoder_options, "threads", "auto", 0);
        if (avcodec_open2(video_decoder_ctx_, video_decoder_, &decoder_options) < 0) {
            LOG(ERROR) << "avcodec_open2";
            return -1;
        }

        auto fr = av_guess_frame_rate(fmt_ctx_, fmt_ctx_->streams[video_stream_idx_], nullptr);
        LOG(INFO) << fmt::format("[DECODER] {}x{}, format = {}, fps = {}/{}, tbn = {}/{}",
            video_decoder_ctx_->width, video_decoder_ctx_->height, video_decoder_ctx_->pix_fmt,
            fr.num, fr.den,
            fmt_ctx_->streams[video_stream_idx_]->time_base.num, fmt_ctx_->streams[video_stream_idx_]->time_base.den
        );
    }

    if (audio_stream_idx_ >= 0) {
        audio_decoder_ = avcodec_find_decoder(fmt_ctx_->streams[audio_stream_idx_]->codecpar->codec_id);
        if (!audio_decoder_) {
            LOG(ERROR) << "avcodec_find_decoder() for audio";
            return false;
        }

        audio_decoder_ctx_ = avcodec_alloc_context3(audio_decoder_);
        if (!audio_decoder_ctx_) {
            LOG(ERROR) << "avcodec_alloc_context3 failed for audio";
            return false;
        }

        if (avcodec_parameters_to_context(audio_decoder_ctx_, fmt_ctx_->streams[audio_stream_idx_]->codecpar) < 0) {
            LOG(ERROR) << "avcodec_parameters_to_context";
            return false;
        }

        if (avcodec_open2(audio_decoder_ctx_, audio_decoder_, nullptr) < 0) {
            LOG(ERROR) << "avcodec_open2 failed for audio";
            return false;
        }

        LOG(INFO) << fmt::format("[DECODER] sample_rate = {}, channels = {}, channel_layout = {}, format = {}, frame_size = {}, start_time = {}",
            audio_decoder_ctx_->sample_rate, audio_decoder_ctx_->channels,
            av_get_default_channel_layout(audio_decoder_ctx_->channels),
            av_get_sample_fmt_name(audio_decoder_ctx_->sample_fmt),
            fmt_ctx_->streams[audio_stream_idx_]->nb_frames,
            fmt_ctx_->streams[audio_stream_idx_]->start_time
        );
    }

    // prepare 
    packet_ = av_packet_alloc();
    decoded_frame_ = av_frame_alloc();
    if (!packet_ || !decoded_frame_) {
        LOG(ERROR) << "av_frame_alloc";
        return -1;
    }

    ready_ = true;

    LOG(INFO) << "[DECODER]: \"" << name << "\" is opened";
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

int Decoder::produce(AVFrame* frame, int type)
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

int Decoder::run()
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

int Decoder::run_f()
{
    LOG(INFO) << "[DECODER@" << std::this_thread::get_id() << "] STARTED";

    // reset the buffer
    video_buffer_.clear();
    audio_buffer_.clear();

    while (running_) {
        if (paused() || video_buffer_.full() || audio_buffer_.full()) {
            std::this_thread::sleep_for(20ms);
            continue;
        }

        // read
        av_packet_unref(packet_);
        int ret = av_read_frame(fmt_ctx_, packet_);
        if ((ret == AVERROR_EOF || avio_feof(fmt_ctx_->pb)) && !(eof_ & DEMUXING_EOF)) {
            // [draining] 1. Instead of valid input, send NULL to the avcodec_send_packet() (decoding) or avcodec_send_frame() (encoding) functions. 
            //               This will enter draining mode.
            // [draining] 2. Call avcodec_receive_frame() (decoding) or avcodec_receive_packet() (encoding) in a loop until AVERROR_EOF is returned.
            //               The functions will not return AVERROR(EAGAIN), unless you forgot to enter draining mode.
            LOG(INFO) << "[DECODER@" << std::this_thread::get_id() << "] " << "EOF => PUT NULL PACKET TO ENTER DRAINING MODE";
            eof_ |= DEMUXING_EOF;
        }
        else if (ret < 0) {
            LOG(ERROR) << "[DECODER@" << std::this_thread::get_id() << "] " << "READ FRAME FAILED";
            running_ = false;
            break;
        }

        first_pts_ = (first_pts_ == AV_NOPTS_VALUE) ? av_gettime_relative() : first_pts_;

        // resume @{
        if (packet_->pts - time_offset_ <= last_pts_) {
            // drop the packet
            LOG(INFO) << "[DECODER@" << std::this_thread::get_id() << "] DROP A PACKET";
            continue;
        }
        packet_->pts -= time_offset_;
        last_pts_ = packet_->pts;
        // @}

        // video decoing 
        if (packet_->stream_index == video_stream_idx_ || ((eof_ & DEMUXING_EOF) && !(eof_ & VDECODING_EOF))) {
            ret = avcodec_send_packet(video_decoder_ctx_, packet_);
            while (ret >= 0) {
                av_frame_unref(decoded_frame_);
                ret = avcodec_receive_frame(video_decoder_ctx_, decoded_frame_);
                if (ret == AVERROR(EAGAIN)) {
                    break;
                }
                else if (ret == AVERROR_EOF) {
                    LOG(INFO) << "[DECODER@" << std::this_thread::get_id() << "] " << "VIDEO EOF";
                    eof_ |= VDECODING_EOF;
                    break;
                }
                else if (ret < 0) {
                    running_ = false;
                    LOG(INFO) << "[DECODER@" << std::this_thread::get_id() << "] " << "VIDEO DECODING ERROR";
                    return ret;
                }

                decoded_frame_->pts -= fmt_ctx_->streams[video_stream_idx_]->start_time;
                LOG(INFO) << "[DECODER@" << std::this_thread::get_id() << "] "
                    << fmt::format("video frame = {:>5d}, pts = {:>9d}", video_decoder_ctx_->frame_number, decoded_frame_->pts);

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
                    LOG(INFO) << "[DECODER@" << std::this_thread::get_id() << "] " << "AUDIO EOF";
                    eof_ |= ADECODING_EOF;
                    break;
                }
                else if (ret < 0) {
                    running_ = false;
                    LOG(INFO) << "[DECODER@" << std::this_thread::get_id() << "] " << "AUDIO DECODING ERROR";
                    return ret;
                }

                decoded_frame_->pts -= fmt_ctx_->streams[audio_stream_idx_]->start_time;
                LOG(INFO)
                    << "[DECODER@" << std::this_thread::get_id() << "] "
                    << fmt::format("audio frame = {:>5d}, pts = {:>9d}, samples = {:>5d}, muted = {}",
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

    // EOF
    if (video_stream_idx_ >= 0) video_buffer_.push([](AVFrame* nil) { av_frame_unref(nil); });
    if (audio_stream_idx_ >= 0) audio_buffer_.push([](AVFrame* nil) { av_frame_unref(nil); });
    eof_ = DECODING_EOF;

    if (video_stream_idx_ >= 0) {
        LOG(INFO) << fmt::format("[DECODER] frames = {:>5d}, fps = {:>6.2f}",
            video_decoder_ctx_->frame_number,
            (video_decoder_ctx_->frame_number * 1000000.0) / (av_gettime_relative() - first_pts_ - time_offset_));
    }
    LOG(INFO) << "[DECODER] EXITED";

    return 0;
}

void Decoder::reset()
{
    destroy();
    LOG(INFO) << "[DECODER] RESET";
}

void Decoder::destroy()
{
    std::lock_guard lock(mtx_);

    running_ = false;
    paused_ = false;
    eof_ = 0x00;
    ready_ = false;

    wait();

    first_pts_ = AV_NOPTS_VALUE;
    last_pts_ = AV_NOPTS_VALUE;
    time_offset_ = 0;
    video_stream_idx_ = -1;
    audio_stream_idx_ = -1;

    av_packet_free(&packet_);
    av_frame_free(&decoded_frame_);

    avcodec_free_context(&video_decoder_ctx_);
    avcodec_free_context(&audio_decoder_ctx_);
    avformat_close_input(&fmt_ctx_);

    // clear before starting the decoding stread since the buffers may be not empty here
    //LOG(INFO) << "[DECODER] VIDEO BUFFER = " << video_buffer_.size();
    //video_buffer_.clear();
    //audio_buffer_.clear();
}