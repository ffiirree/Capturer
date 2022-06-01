#include "encoder.h"
#include "logging.h"
#include "fmt/format.h"
#include "fmt/ranges.h"
#include <chrono>

using namespace std::chrono_literals;

int Encoder::open(const std::string& filename, 
                   const std::string& codec_name,
                   bool is_cfr, 
                   const std::map<std::string, std::string>& options)
{
    std::lock_guard lock(mtx_);

    is_cfr_ = is_cfr;

    LOG(INFO) << fmt::format("[ENCODER] < \"{}\", OPTIONS = {}, CFR = {}, SIZE = {}x{}, FORMAT = {}, FRAMERATE = {}/{}, TIME_BASE = {}/{}",
        codec_name, options, is_cfr, width_, height_, pix_fmt_, framerate_.num, framerate_.den, v_stream_time_base_.num, v_stream_time_base_.den);

    // format context
    if (avformat_alloc_output_context2(&fmt_ctx_, nullptr, nullptr, filename.c_str()) < 0) {
        LOG(ERROR) << "avformat_alloc_output_context2";
        return -1;
    }

    // new stream
    AVStream* stream = avformat_new_stream(fmt_ctx_, nullptr);
    if (!stream) {
        LOG(ERROR) << "avformat_new_stream";
        return -1;
    }
    video_stream_idx_ = stream->index;

    video_encoder_ = avcodec_find_encoder_by_name(codec_name.c_str());
    if (!video_encoder_) {
        LOG(ERROR) << "avcodec_find_encoder_by_name";
        return -1;
    }

    video_encoder_ctx_ = avcodec_alloc_context3(video_encoder_);
    if (!video_encoder_ctx_) {
        LOG(ERROR) << "avcodec_alloc_context3";
        return -1;
    }

    AVDictionary* encoder_options = nullptr;
    defer(av_dict_free(&encoder_options));
    for (const auto& [key, value] : options) {
        av_dict_set(&encoder_options, key.c_str(), value.c_str(), 0);

    }
    if (codec_name == "libx264" || codec_name == "libx265") {
        av_dict_set(&encoder_options, "preset", "ultrafast", AV_DICT_DONT_OVERWRITE);
        av_dict_set(&encoder_options, "tune", "zerolatency", AV_DICT_DONT_OVERWRITE);
        av_dict_set(&encoder_options, "crf", "23", AV_DICT_DONT_OVERWRITE);
        av_dict_set(&encoder_options, "threads", "auto", AV_DICT_DONT_OVERWRITE);
    }

    video_encoder_ctx_->height = height_;
    video_encoder_ctx_->width = width_;
    video_encoder_ctx_->pix_fmt = pix_fmt_;
    video_encoder_ctx_->sample_aspect_ratio = sample_aspect_ratio_;

    video_encoder_ctx_->time_base = is_cfr ? av_inv_q(framerate_) : v_stream_time_base_;
    fmt_ctx_->streams[video_stream_idx_]->time_base = is_cfr ? av_inv_q(framerate_) : v_stream_time_base_;
    video_encoder_ctx_->framerate = framerate_;

    if (fmt_ctx_->oformat->flags & AVFMT_GLOBALHEADER) {
        video_encoder_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    if (avcodec_open2(video_encoder_ctx_, video_encoder_, &encoder_options) < 0) {
        LOG(ERROR) << "avcodec_open2";
        return -1;
    }

    if (avcodec_parameters_from_context(fmt_ctx_->streams[video_stream_idx_]->codecpar, video_encoder_ctx_) < 0) {
        LOG(ERROR) << "avcodec_parameters_from_context";
        return -1;
    }

    if (!(fmt_ctx_->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&fmt_ctx_->pb, filename.c_str(), AVIO_FLAG_WRITE) < 0) {
            LOG(ERROR) << "avio_open";
            return -1;
        }
    }

    if (avformat_write_header(fmt_ctx_, nullptr) < 0) {
        LOG(ERROR) << "avformat_write_header";
        return -1;
    }

    // prepare 
    packet_ = av_packet_alloc();
    filtered_frame_ = av_frame_alloc();
    if (!packet_ || !filtered_frame_) {
        LOG(ERROR) << "av_packet_alloc";
        return -1;
    }

    ready_ = true;

    av_dump_format(fmt_ctx_, 0, filename.c_str(), 1);

    LOG(INFO) << fmt::format("[ENCODER] > \"{}\", OPTIONS = {}, CFR = {}, SIZE = {}x{}, FORMAT = {}, FRAMERATE = {}/{}, TIME_BASE = {}/{}",
        codec_name, options, is_cfr, width_, height_, pix_fmt_, framerate_.num, framerate_.den, 
        fmt_ctx_->streams[video_stream_idx_]->time_base.num, fmt_ctx_->streams[video_stream_idx_]->time_base.den);

    LOG(INFO) << "[ENCODER] " << filename << " is opened";
    return 0;
}

int Encoder::run_f()
{
    LOG(INFO) << "[ENCODER@" << std::this_thread::get_id() << "] STARTED";

    int ret = 0;
    while (running_) {
        if (video_buffer_.empty() && audio_buffer_.empty()) {
            std::this_thread::sleep_for(50ms);
            continue;
        }

        first_pts_ = (first_pts_ == AV_NOPTS_VALUE) ? av_gettime_relative() : first_pts_;

        if (!video_buffer_.empty()) {

            video_buffer_.pop(
                [=](AVFrame* popped) {
                    av_frame_unref(filtered_frame_);
                    av_frame_move_ref(filtered_frame_, popped);
                }
            );


            ret = avcodec_send_frame(video_encoder_ctx_, filtered_frame_);
            while (ret >= 0) {
                av_packet_unref(packet_);
                ret = avcodec_receive_packet(video_encoder_ctx_, packet_);
                if (ret == AVERROR(EAGAIN)) {
                    break;
                }
                else if (ret == AVERROR_EOF) {
                    LOG(INFO) << "[ENCODER@" << std::this_thread::get_id() << "] VIDEO EOF";
                    eof_ |= V_ENCODING_EOF;
                    break;
                }
                else if (ret < 0) {
                    LOG(ERROR) << "[ENCODER@" << std::this_thread::get_id() << "] encode failed";
                    return ret;
                }

                packet_->stream_index = video_stream_idx_;

                av_packet_rescale_ts(packet_, v_stream_time_base_, fmt_ctx_->streams[video_stream_idx_]->time_base);

                LOG(INFO) << "[ENCODER@" << std::this_thread::get_id() << "] "
                    << fmt::format("pts = {:>6.2f}s, frame = {:>5d}, fps = {:>6.2f}", 
                        packet_->pts * av_q2d(fmt_ctx_->streams[video_stream_idx_]->time_base), video_encoder_ctx_->frame_number,
                        (video_encoder_ctx_->frame_number * 1000000.0) / (av_gettime_relative() - first_pts_));

                if (av_interleaved_write_frame(fmt_ctx_, packet_) != 0) {
                    LOG(ERROR) << "[ENCODER@" << std::this_thread::get_id() << "] av_interleaved_write_frame";
                    return -1;
                }
            }
        }

        if (!audio_buffer_.empty()) {

            video_buffer_.pop(
                [=](AVFrame* popped) {
                    av_frame_unref(filtered_frame_);
                    av_frame_move_ref(filtered_frame_, popped);
                }
            );

            ret = avcodec_send_frame(audio_encoder_ctx_, filtered_frame_);
            while (ret >= 0) {
                av_packet_unref(packet_);
                ret = avcodec_receive_packet(audio_encoder_ctx_, packet_);
                if (ret == AVERROR(EAGAIN)) {
                    break;
                }
                else if (ret == AVERROR_EOF) {
                    LOG(INFO) << "[ENCODER@" << std::this_thread::get_id() << "] AUDIO EOF";
                    eof_ |= A_ENCODING_EOF;
                    break;
                }
                else if (ret < 0) {
                    LOG(ERROR) << "[ENCODER@" << std::this_thread::get_id() << "] encode failed";
                    return ret;
                }

                packet_->stream_index = audio_stream_idx_;

                //av_packet_rescale_ts(packet_, audio_encoder_ctx_->time_base, fmt_ctx_->streams[audio_stream_idx_]->time_base);

                if (av_interleaved_write_frame(fmt_ctx_, packet_) != 0) {
                    LOG(ERROR) << "[ENCODER@" << std::this_thread::get_id() << "] av_interleaved_write_frame";
                    return -1;
                }
            }
        }
    }
    LOG(INFO) << "[ENCODER@" << std::this_thread::get_id() << "] "
        << fmt::format("frame = {:>5d}, fps = {:>6.2f}", 
            video_encoder_ctx_->frame_number,
            (video_encoder_ctx_->frame_number * 1000000.0) / (av_gettime_relative() - first_pts_));
    LOG(INFO) << "[ENCODER@" << std::this_thread::get_id() << "] EXITED";

    return ret;
}