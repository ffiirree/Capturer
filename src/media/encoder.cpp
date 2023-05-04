#include "encoder.h"

#include "fmt/format.h"
#include "fmt/ranges.h"
#include "logging.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libavutil/time.h>
}
#include "clock.h"
#include "defer.h"
#include "hwaccel.h"

int Encoder::open(const std::string& filename, const std::string& video_codec_name,
                  const std::string& audio_codec_name,
                  const std::unordered_map<std::string, std::string>& video_options,
                  const std::unordered_map<std::string, std::string>& audio_options)
{
    std::lock_guard lock(mtx_);

    if (!audio_enabled_ && !video_enabled_) {
        LOG(WARNING) << "both audio and video encoding are disabled.";
        return -1;
    }

    video_codec_name_ = video_codec_name;
    audio_codec_name_ = audio_codec_name;
    video_options_    = video_options;
    audio_options_    = audio_options;

    if (video_options.contains("vsync")) {
        vsync_ = av::to_vsync(video_options.at("vsync"));
    }

    if (video_enabled_) {
        LOG(INFO) << fmt::format(
            "[   ENCODER] [V] <<< [{}], options = {}, size = {}x{}, format = {}, fps = {}, tbn = {}",
            video_codec_name, video_options, vfmt.width, vfmt.height, av::to_string(vfmt.pix_fmt),
            vfmt.framerate, vfmt.time_base);
    }

    if (audio_enabled_) {
        LOG(INFO) << fmt::format(
            "[   ENCODER] [A] <<< [{}], options = {}, sample_rate = {}Hz, channels = {}, sample_fmt = {}, tbn = {}",
            audio_codec_name, audio_options, afmt.sample_rate, afmt.channels,
            av::to_string(afmt.sample_fmt), afmt.time_base);
    }

    // format context
    if (fmt_ctx_) destroy();

    if (avformat_alloc_output_context2(&fmt_ctx_, nullptr, nullptr, filename.c_str()) < 0) {
        LOG(ERROR) << "[   ENCODER] filed to alloc the output format context.";
        return -1;
    }

    // streams
    if (video_enabled_ && new_video_stream() < 0) return -1;
    if (audio_enabled_ && new_auido_stream() < 0) return -1;

    // output file
    if (!(fmt_ctx_->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&fmt_ctx_->pb, filename.c_str(), AVIO_FLAG_WRITE) < 0) {
            LOG(ERROR) << "[   ENCODER] can not open the output file : " << filename;
            return -1;
        }
    }

    if (avformat_write_header(fmt_ctx_, nullptr) < 0) {
        LOG(ERROR) << "[   ENCODER] can not write the header to the output file.";
        return -1;
    }

    if (video_stream_idx_ >= 0) {
        LOG(INFO) << fmt::format(
            "[   ENCODER] [V] >>> [{}], options = {}, size = {}x{}, format = {}, fps = {}, tbc = {}, tbn = {}, hwaccel = {}",
            video_codec_name_, video_options_, vfmt.width, vfmt.height, av::to_string(vfmt.pix_fmt),
            vfmt.framerate, video_encoder_ctx_->time_base, fmt_ctx_->streams[video_stream_idx_]->time_base,
            av::to_string(vfmt.hwaccel));
    }

    if (audio_stream_idx_ >= 0) {
        LOG(INFO) << fmt::format(
            "[   ENCODER] [A] >>> [{}], options = {}, sample_rate = {}Hz, channels = {}, sample_fmt = {}, frame_size = {}, tbc = {}, tbn = {}",
            audio_codec_name_, audio_options_, audio_encoder_ctx_->sample_rate,
            audio_encoder_ctx_->channels, av::to_string(audio_encoder_ctx_->sample_fmt),
            fmt_ctx_->streams[audio_stream_idx_]->codecpar->frame_size, audio_encoder_ctx_->time_base,
            fmt_ctx_->streams[audio_stream_idx_]->time_base);
    }

    // prepare
    packet_         = av_packet_alloc();
    filtered_frame_ = av_frame_alloc();
    last_frame_     = av_frame_alloc();
    if (!packet_ || !filtered_frame_ || !last_frame_) {
        LOG(ERROR) << "[   ENCODER] failed to alloc memory for frames.";
        return -1;
    }

    ready_ = true;

    av_dump_format(fmt_ctx_, 0, filename.c_str(), 1);

    LOG(INFO) << "[   ENCODER] [" << filename << "] is opened";
    return 0;
}

int Encoder::new_video_stream()
{
    auto v_stream = avformat_new_stream(fmt_ctx_, nullptr);
    if (!v_stream) {
        LOG(ERROR) << "[   ENCODER] avformat_new_stream";
        return -1;
    }
    video_stream_idx_ = v_stream->index;

    auto video_encoder = avcodec_find_encoder_by_name(video_codec_name_.c_str());
    if (!video_encoder) {
        LOG(ERROR) << "[   ENCODER] can not find the video encoder : " << video_codec_name_;
        return -1;
    }

    video_encoder_ctx_ = avcodec_alloc_context3(video_encoder);
    if (!video_encoder_ctx_) {
        LOG(ERROR) << "[   ENCODER] failed to alloc the video encoder context.";
        return -1;
    }

    AVDictionary *encoder_options = nullptr;
    defer(av_dict_free(&encoder_options));
    for (const auto& [key, value] : video_options_) {
        av_dict_set(&encoder_options, key.c_str(), value.c_str(), 0);
    }
    av_dict_set(&encoder_options, "threads", "auto", 0);

    video_encoder_ctx_->height              = vfmt.height;
    video_encoder_ctx_->width               = vfmt.width;
    video_encoder_ctx_->pix_fmt             = vfmt.pix_fmt;
    video_encoder_ctx_->sample_aspect_ratio = vfmt.sample_aspect_ratio;
    video_encoder_ctx_->framerate           = vfmt.framerate;
    video_encoder_ctx_->time_base =
        (vsync_ == av::vsync_t::cfr) ? av_inv_q(vfmt.framerate) : vfmt.time_base;
    fmt_ctx_->streams[video_stream_idx_]->time_base = vfmt.time_base;

    if (fmt_ctx_->oformat->flags & AVFMT_GLOBALHEADER) {
        video_encoder_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    if (vfmt.hwaccel != AV_HWDEVICE_TYPE_NONE) {
        if (hwaccel::setup_for_encoding(video_encoder_ctx_, vfmt.hwaccel) != 0) {
            LOG(ERROR) << "[   ENCODER] failed to set hardware device for encoding.";
            return -1;
        }
    }

    if (avcodec_open2(video_encoder_ctx_, video_encoder, &encoder_options) < 0) {
        LOG(ERROR) << "[   ENCODER] filed to open the video encoder : " << video_codec_name_;
        return -1;
    }

    if (avcodec_parameters_from_context(fmt_ctx_->streams[video_stream_idx_]->codecpar,
                                        video_encoder_ctx_) < 0) {
        LOG(ERROR) << "[   ENCODER] avcodec_parameters_from_context";
        return -1;
    }

    return 0;
}

int Encoder::new_auido_stream()
{
    AVStream *a_stream = avformat_new_stream(fmt_ctx_, nullptr);
    if (!a_stream) {
        LOG(ERROR) << "[   ENCODER] filed to create audio streams.";
        return -1;
    }
    audio_stream_idx_ = a_stream->index;

    auto audio_encoder = avcodec_find_encoder_by_name(audio_codec_name_.c_str());
    if (!audio_encoder) {
        LOG(ERROR) << "[   ENCODER] can not find the audio encoder : " << audio_codec_name_;
        return -1;
    }

    audio_encoder_ctx_ = avcodec_alloc_context3(audio_encoder);
    if (!audio_encoder_ctx_) {
        LOG(ERROR) << "[   ENCODER] failed to alloc the audio encoder context.";
        return -1;
    }

    audio_encoder_ctx_->sample_rate                 = afmt.sample_rate;
    audio_encoder_ctx_->channels                    = afmt.channels;
    audio_encoder_ctx_->channel_layout              = afmt.channel_layout;
    audio_encoder_ctx_->sample_fmt                  = afmt.sample_fmt;
    audio_encoder_ctx_->time_base                   = { 1, afmt.sample_rate };
    fmt_ctx_->streams[audio_stream_idx_]->time_base = afmt.time_base;

    if (fmt_ctx_->oformat->flags & AVFMT_GLOBALHEADER) {
        audio_encoder_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    AVDictionary *encoder_options = nullptr;
    defer(av_dict_free(&encoder_options));
    for (const auto& [key, value] : audio_options_) {
        av_dict_set(&encoder_options, key.c_str(), value.c_str(), 0);
    }
    av_dict_set(&encoder_options, "threads", "auto", 0);

    if (avcodec_open2(audio_encoder_ctx_, audio_encoder, &encoder_options) < 0) {
        LOG(ERROR) << "[   ENCODER] failed to open audio encoder";
        return -1;
    }

    if (avcodec_parameters_from_context(fmt_ctx_->streams[audio_stream_idx_]->codecpar,
                                        audio_encoder_ctx_) < 0) {
        LOG(ERROR) << "[   ENCODER] avcodec_parameters_from_context";
        return -1;
    }

    audio_fifo_buffer_ =
        av_audio_fifo_alloc(audio_encoder_ctx_->sample_fmt, audio_encoder_ctx_->channels, 1);
    if (!audio_fifo_buffer_) {
        LOG(ERROR) << "[   ENCODER] av_audio_fifo_alloc";
        return -1;
    }

    return 0;
}

int Encoder::consume(AVFrame *frame, int type)
{
    switch (type) {
    case AVMEDIA_TYPE_VIDEO:
        if (video_buffer_.full()) return -1;

        video_buffer_.push([frame](AVFrame *p_frame) {
            av_frame_unref(p_frame);
            av_frame_move_ref(p_frame, frame);
        });
        return 0;

    case AVMEDIA_TYPE_AUDIO:
        if (audio_buffer_.full()) return -1;

        audio_buffer_.push([frame](AVFrame *p_frame) {
            av_frame_unref(p_frame);
            av_frame_move_ref(p_frame, frame);
        });
        return 0;

    default: return -1;
    }
}

int Encoder::run()
{
    std::lock_guard lock(mtx_);

    if (!ready_ || running_) {
        LOG(INFO) << "[   ENCODER] already running or not ready";
        return -1;
    }

    running_ = true;
    thread_  = std::thread([this]() {
        probe::util::thread_set_name("encoder");
        run_f();
    });

    return 0;
}

int Encoder::run_f()
{
    LOG(INFO) << "STARTED";

    if (video_stream_idx_ < 0) eof_ |= V_ENCODING_EOF;
    if (audio_stream_idx_ < 0) eof_ |= (A_ENCODING_EOF | A_ENCODING_FIFO_EOF);

    int ret = 0;
    while (running_ && !eof()) {
        if (video_buffer_.empty() && audio_buffer_.empty()) {
            os_sleep(20ms);
            continue;
        }

        ret = process_video_frames();
        ret = process_audio_frames();
    } // running

    LOG(INFO) << "[V] encoded frame = " << video_encoder_ctx_->frame_number;
    LOG(INFO) << "EXITED";

    return ret;
}

std::pair<int, int> Encoder::video_sync_process()
{
    // duration
    double duration = std::min(1 / (av_q2d(vfmt.framerate) * av_q2d(video_encoder_ctx_->time_base)),
                               1 / (av_q2d(sink_framerate) * av_q2d(video_encoder_ctx_->time_base)));

    double floating_pts =
        filtered_frame_->pts * av_q2d(vfmt.time_base) / av_q2d(video_encoder_ctx_->time_base);

    if (expected_pts_ == AV_NOPTS_VALUE) expected_pts_ = std::lround(floating_pts);

    // floating_pts should in (last_pts, next_pts)
    double delta_l = floating_pts - expected_pts_; // expected range : (-duration, duration)
    double delta_r = delta_l + duration;

    int num_frames     = 1;
    int num_pre_frames = 0;

    // 1. pass
    if (delta_l < 0 && delta_r > 0) {
        floating_pts = expected_pts_;
        duration += delta_l;
        delta_l = 0;
    }

    switch (vsync_) {
    case av::vsync_t::cfr:
        // 2. drop
        if (delta_r < -1.1) {
            num_frames = 0;
        }

        // 3. duplicate
        else if (delta_r > 1.1) {
            num_frames = std::lround(delta_r);

            if (delta_l > 1.1) {
                num_pre_frames = std::lround(delta_l - 0.6);
            }
        }
#if LIBAVUTIL_VERSION_MAJOR >= 58
        filtered_frame_->duration = 1;
#endif
        break;

    case av::vsync_t::vfr:
        if (delta_r <= -0.6) {
            num_frames = 0;
        }
        else if (delta_r > 0.6) {
            expected_pts_ = std::lround(floating_pts);
        }
#if LIBAVUTIL_VERSION_MAJOR >= 58
        filtered_frame_->duration = duration;
#endif
        break;
    default: break;
    }

    if (num_frames == 0) {
        LOG(WARNING) << "[V] drop the frame.";
    }
    else if (num_frames > 1) {
        LOG(WARNING) << fmt::format("[V] duplicated {} frames and {} previous frames.", num_frames - 1,
                                    num_pre_frames);
    }

    return { num_frames, num_pre_frames };
}

int Encoder::process_video_frames()
{
    if (video_buffer_.empty()) return AVERROR(EAGAIN);

    video_buffer_.pop([this](AVFrame *popped) {
        av_frame_unref(filtered_frame_);
        av_frame_move_ref(filtered_frame_, popped);
    });

    auto [num_frames, num_pre_frames] = video_sync_process();
    if (!filtered_frame_->buf[0]) {
        num_frames     = 1;
        num_pre_frames = 0;
    }

    AVFrame *encoding_frame_{};
    for (auto i = 0; i < num_frames; ++i) {
        if (i < num_pre_frames && last_frame_->buf[0]) {
            encoding_frame_ = last_frame_;
        }
        else {
            encoding_frame_ = filtered_frame_;
        }

        //
        encoding_frame_->quality   = video_encoder_ctx_->global_quality;
        encoding_frame_->pict_type = AV_PICTURE_TYPE_NONE;
        encoding_frame_->pts       = expected_pts_;

        int ret = avcodec_send_frame(
            video_encoder_ctx_,
            (!encoding_frame_->width || !encoding_frame_->height) ? nullptr : encoding_frame_);
        while (ret >= 0) {
            av_packet_unref(packet_);
            ret = avcodec_receive_packet(video_encoder_ctx_, packet_);
            if (ret == AVERROR(EAGAIN)) {
                break;
            }
            else if (ret == AVERROR_EOF) {
                LOG(INFO) << "[V] EOF";
                eof_ |= V_ENCODING_EOF;
                break;
            }
            else if (ret < 0) {
                LOG(ERROR) << "[V] encode failed";
                return ret;
            }

            av_packet_rescale_ts(packet_, video_encoder_ctx_->time_base,
                                 fmt_ctx_->streams[video_stream_idx_]->time_base);

            if (v_last_dts_ != AV_NOPTS_VALUE && v_last_dts_ >= packet_->dts) {
                LOG(WARNING) << fmt::format("[V] drop the packet with dts {} <= {}", packet_->dts,
                                            v_last_dts_);
                continue;
            }
            v_last_dts_ = packet_->dts;

            DLOG(INFO) << fmt::format(
                "[V] packet = {:>5d}, pts = {:>14d}, dts = {:>14d}, size = {:>6d}, ts = {:>10.6f}",
                video_encoder_ctx_->frame_number, packet_->pts, packet_->dts, packet_->size,
                av_rescale_q(packet_->pts, fmt_ctx_->streams[video_stream_idx_]->time_base,
                             av_get_time_base_q()) /
                    (double)AV_TIME_BASE);

            packet_->stream_index = video_stream_idx_;
            if (av_interleaved_write_frame(fmt_ctx_, packet_) != 0) {
                LOG(ERROR) << "[V] failed to write the the packet to file.";
                return -1;
            }
        }

        expected_pts_++;
    }

    av_frame_unref(last_frame_);
    av_frame_ref(last_frame_, filtered_frame_);

    return 0;
}

int Encoder::process_audio_frames()
{
    if (audio_buffer_.empty()) return AVERROR(EAGAIN);

    int ret = 0;
    // write to fifo buffer
    while (!(eof_ & A_ENCODING_FIFO_EOF) &&
           av_audio_fifo_size(audio_fifo_buffer_) < audio_encoder_ctx_->frame_size) {
        if (audio_buffer_.empty()) {
            break;
        }

        audio_buffer_.pop([this](AVFrame *popped) {
            av_frame_unref(filtered_frame_);
            av_frame_move_ref(filtered_frame_, popped);
        });

        if (filtered_frame_->nb_samples == 0) {
            LOG(INFO) << "[A] DRAINING";
            eof_ |= A_ENCODING_FIFO_EOF;
            break;
        }

        CHECK(av_audio_fifo_realloc(audio_fifo_buffer_, av_audio_fifo_size(audio_fifo_buffer_) +
                                                            filtered_frame_->nb_samples) >= 0);
        CHECK(av_audio_fifo_write(audio_fifo_buffer_, (void **)filtered_frame_->data,
                                  filtered_frame_->nb_samples) >= filtered_frame_->nb_samples);

        audio_pts_ = filtered_frame_->pts + av_audio_fifo_size(audio_fifo_buffer_);
    }

    // encode and write to the output
    while (!(eof_ & A_ENCODING_EOF) &&
           (av_audio_fifo_size(audio_fifo_buffer_) >= audio_encoder_ctx_->frame_size ||
            (eof_ & A_ENCODING_FIFO_EOF))) {

        if (av_audio_fifo_size(audio_fifo_buffer_) >= audio_encoder_ctx_->frame_size) {
            av_frame_unref(filtered_frame_);

            filtered_frame_->nb_samples = audio_encoder_ctx_->frame_size;
            filtered_frame_->channels   = fmt_ctx_->streams[audio_stream_idx_]->codecpar->channels;
            filtered_frame_->channel_layout =
                fmt_ctx_->streams[audio_stream_idx_]->codecpar->channel_layout;
            filtered_frame_->format      = fmt_ctx_->streams[audio_stream_idx_]->codecpar->format;
            filtered_frame_->sample_rate = fmt_ctx_->streams[audio_stream_idx_]->codecpar->sample_rate;

            av_frame_get_buffer(filtered_frame_, 0);

            CHECK(av_audio_fifo_read(audio_fifo_buffer_, (void **)filtered_frame_->data,
                                     audio_encoder_ctx_->frame_size) >= audio_encoder_ctx_->frame_size);

            filtered_frame_->pts = audio_pts_ - av_audio_fifo_size(audio_fifo_buffer_);
            ret                  = avcodec_send_frame(audio_encoder_ctx_, filtered_frame_);
        }
        else if (eof_ & A_ENCODING_FIFO_EOF) {
            ret = avcodec_send_frame(audio_encoder_ctx_, nullptr);
        }
        else {
            LOG(ERROR) << "[A] unknown error";
            break;
        }

        while (ret >= 0) {
            av_packet_unref(packet_);
            ret = avcodec_receive_packet(audio_encoder_ctx_, packet_);
            if (ret == AVERROR(EAGAIN)) {
                break;
            }
            else if (ret == AVERROR_EOF) {
                LOG(INFO) << "[A] EOF";
                eof_ |= A_ENCODING_EOF;
                break;
            }
            else if (ret < 0) {
                LOG(ERROR) << "[A] encode failed";
                return ret;
            }
            av_packet_rescale_ts(packet_, afmt.time_base, fmt_ctx_->streams[audio_stream_idx_]->time_base);

            if (a_last_dts_ != AV_NOPTS_VALUE && a_last_dts_ >= packet_->dts) {
                LOG(WARNING) << fmt::format("[A] drop the frame with dts {} <= {}", packet_->dts,
                                            a_last_dts_);
                continue;
            }
            a_last_dts_ = packet_->dts;

            DLOG(INFO) << fmt::format(
                "[A] packet = {:>5d}, pts = {:>14d}, dts = {:>14d}, size = {:>6d}, ts = {:>10.6f}",
                audio_encoder_ctx_->frame_number, packet_->pts, packet_->dts, packet_->size,
                av_rescale_q(packet_->pts, fmt_ctx_->streams[audio_stream_idx_]->time_base,
                             av_get_time_base_q()) /
                    (double)AV_TIME_BASE);

            packet_->stream_index = audio_stream_idx_;

            if (av_interleaved_write_frame(fmt_ctx_, packet_) != 0) {
                LOG(ERROR) << "[A] failed to write the packet to the file.";
                return -1;
            }
        }
    } // audio output

    return ret;
}

void Encoder::reset()
{
    destroy();
    LOG(INFO) << "[   ENCODER] RESET";
}

void Encoder::destroy()
{
    std::lock_guard lock(mtx_);

    running_ = false;

    wait();

    if (fmt_ctx_ && ready_ && av_write_trailer(fmt_ctx_) != 0) {
        LOG(ERROR) << "[   ENCODER] failed to write trailer";
    }

    if (fmt_ctx_ && !(fmt_ctx_->oformat->flags & AVFMT_NOFILE)) {
        if (avio_close(fmt_ctx_->pb) < 0) {
            LOG(ERROR) << "[   ENCODER] failed to close the file.";
        }
    }

    eof_   = 0x00;
    ready_ = false; // after av_write_trailer()

    v_last_dts_       = AV_NOPTS_VALUE;
    a_last_dts_       = AV_NOPTS_VALUE;
    video_stream_idx_ = -1;
    audio_stream_idx_ = -1;

    av_packet_free(&packet_);
    av_frame_free(&filtered_frame_);
    av_frame_free(&last_frame_);
    av_audio_fifo_free(audio_fifo_buffer_);
    audio_fifo_buffer_ = nullptr;
    expected_pts_      = AV_NOPTS_VALUE;

    avcodec_free_context(&video_encoder_ctx_);
    avcodec_free_context(&audio_encoder_ctx_);
    avformat_free_context(fmt_ctx_);
    fmt_ctx_ = nullptr;

    vfmt = {};
    afmt = {};

    video_buffer_.clear();
    audio_buffer_.clear();
}