#include "libcap/encoder.h"

#include "libcap/clock.h"
#include "libcap/hwaccel.h"
#include "logging.h"

#include <fmt/chrono.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <probe/defer.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/time.h>
}

int Encoder::open(const std::string& filename, std::map<std::string, std::string> options)
{
    if (!audio_enabled_ && !video_enabled_) {
        LOG(WARNING) << "both audio and video encoding are disabled.";
        return -1;
    }

    LOG(INFO) << fmt::format("[   ENCODER] {}, options = {}", filename, options);

    const auto vcodec_name = options.contains("vcodec") ? options.at("vcodec") : "libx264";
    const auto acodec_name = options.contains("acodec") ? options.at("acodec") : "aac";

    if (options.contains("vsync")) {
        vsync_ = av::to_vsync(options.at("vsync"));
    }

    // format context
    if (avformat_alloc_output_context2(&fmt_ctx_, nullptr, nullptr, filename.c_str()) < 0) {
        LOG(ERROR) << "[   ENCODER] filed to alloc the output format context.";
        return -1;
    }

    // streams
    if (video_enabled_ && new_video_stream(vcodec_name) < 0) return -1;
    if (audio_enabled_ && new_auido_stream(acodec_name) < 0) return -1;

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

    av_dump_format(fmt_ctx_, 0, filename.c_str(), 1);

    LOG(INFO) << "[   ENCODER] [" << filename << "] is opened";

    ready_ = true;

    return 0;
}

int Encoder::new_video_stream(const std::string& codec_name)
{
    LOG(INFO) << fmt::format("[   ENCODER] [V] <<< [{}], {}", codec_name, av::to_string(vfmt));

    const auto stream = avformat_new_stream(fmt_ctx_, nullptr);
    if (!stream) {
        LOG(ERROR) << "[   ENCODER] avformat_new_stream";
        return -1;
    }
    vstream_idx_ = stream->index;

    auto video_encoder = avcodec_find_encoder_by_name(codec_name.c_str());
    if (!video_encoder) {
        LOG(ERROR) << "[   ENCODER] can not find the video encoder : " << codec_name;
        return -1;
    }

    vcodec_ctx_ = avcodec_alloc_context3(video_encoder);
    if (!vcodec_ctx_) {
        LOG(ERROR) << "[   ENCODER] failed to alloc the video encoder context.";
        return -1;
    }

    AVDictionary *options = nullptr;
    defer(av_dict_free(&options));
    av_dict_set(&options, "threads", "auto", 0);

    vcodec_ctx_->height              = vfmt.height;
    vcodec_ctx_->width               = vfmt.width;
    vcodec_ctx_->pix_fmt             = vfmt.pix_fmt;
    vcodec_ctx_->sample_aspect_ratio = vfmt.sample_aspect_ratio;
    vcodec_ctx_->framerate           = vfmt.framerate;
    vcodec_ctx_->time_base = (vsync_ == av::vsync_t::cfr) ? av_inv_q(vfmt.framerate) : vfmt.time_base;
    fmt_ctx_->streams[vstream_idx_]->time_base = vfmt.time_base;

    if (fmt_ctx_->oformat->flags & AVFMT_GLOBALHEADER) {
        vcodec_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    if (vfmt.hwaccel != AV_HWDEVICE_TYPE_NONE) {
        if (av::hwaccel::setup_for_encoding(vcodec_ctx_, vfmt.hwaccel) != 0) {
            LOG(ERROR) << "[   ENCODER] failed to set hardware device for encoding.";
            return -1;
        }
    }

    if (avcodec_open2(vcodec_ctx_, video_encoder, &options) < 0) {
        LOG(ERROR) << "[   ENCODER] filed to open the video encoder : " << codec_name;
        return -1;
    }

    if (avcodec_parameters_from_context(fmt_ctx_->streams[vstream_idx_]->codecpar, vcodec_ctx_) < 0) {
        LOG(ERROR) << "[   ENCODER] avcodec_parameters_from_context";
        return -1;
    }

    if (vstream_idx_ >= 0) {
        LOG(INFO) << fmt::format(
            "[   ENCODER] [V] >>> [{}], video_size = {}x{}, pix_fmt = {}, frame_rate = {}, tbc = {}, tbn = {}, hwaccel = {}",
            codec_name, vfmt.width, vfmt.height, av::to_string(vfmt.pix_fmt), vfmt.framerate,
            vcodec_ctx_->time_base, fmt_ctx_->streams[vstream_idx_]->time_base,
            av::to_string(vfmt.hwaccel));
    }

    return 0;
}

int Encoder::new_auido_stream(const std::string& codec_name)
{
    LOG(INFO) << fmt::format("[   ENCODER] [A] <<< [{}], {}", codec_name, av::to_string(afmt));

    const auto stream = avformat_new_stream(fmt_ctx_, nullptr);
    if (!stream) {
        LOG(ERROR) << "[   ENCODER] filed to create audio streams.";
        return -1;
    }
    astream_idx_ = stream->index;

    auto audio_encoder = avcodec_find_encoder_by_name(codec_name.c_str());
    if (!audio_encoder) {
        LOG(ERROR) << "[   ENCODER] can not find the audio encoder : " << codec_name;
        return -1;
    }

    acodec_ctx_ = avcodec_alloc_context3(audio_encoder);
    if (!acodec_ctx_) {
        LOG(ERROR) << "[   ENCODER] failed to alloc the audio encoder context.";
        return -1;
    }

    acodec_ctx_->sample_rate                   = afmt.sample_rate;
    acodec_ctx_->channels                      = afmt.channels;
    acodec_ctx_->channel_layout                = afmt.channel_layout;
    acodec_ctx_->sample_fmt                    = afmt.sample_fmt;
    acodec_ctx_->time_base                     = { 1, afmt.sample_rate };
    fmt_ctx_->streams[astream_idx_]->time_base = afmt.time_base;

    if (fmt_ctx_->oformat->flags & AVFMT_GLOBALHEADER) {
        acodec_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    AVDictionary *options = nullptr;
    defer(av_dict_free(&options));
    av_dict_set(&options, "threads", "auto", 0);

    if (avcodec_open2(acodec_ctx_, audio_encoder, &options) < 0) {
        LOG(ERROR) << "[   ENCODER] failed to open audio encoder";
        return -1;
    }

    if (avcodec_parameters_from_context(fmt_ctx_->streams[astream_idx_]->codecpar, acodec_ctx_) < 0) {
        LOG(ERROR) << "[   ENCODER] avcodec_parameters_from_context";
        return -1;
    }

    abuffer_ =
        std::make_unique<safe_audio_fifo>(afmt.sample_fmt, afmt.channels, acodec_ctx_->frame_size * 4);

    if (astream_idx_ >= 0) {
        LOG(INFO) << fmt::format(
            "[   ENCODER] [A] >>> [{}], sample_rate = {}, channels = {}, sample_fmt = {}, tbc = {}, tbn = {}",
            codec_name, acodec_ctx_->sample_rate, acodec_ctx_->channels,
            av::to_string(acodec_ctx_->sample_fmt), acodec_ctx_->time_base,
            fmt_ctx_->streams[astream_idx_]->time_base);
    }

    return 0;
}

bool Encoder::accepts(const AVMediaType type) const
{
    switch (type) {
    case AVMEDIA_TYPE_VIDEO: return video_enabled_;
    case AVMEDIA_TYPE_AUDIO: return audio_enabled_;
    default:                 return false;
    }
}

void Encoder::enable(const AVMediaType type, const bool v)
{
    switch (type) {
    case AVMEDIA_TYPE_VIDEO: video_enabled_ = v; break;
    case AVMEDIA_TYPE_AUDIO: audio_enabled_ = v; break;
    default:                 break;
    }
}

int Encoder::consume(const av::frame& frame, const AVMediaType type)
{
    switch (type) {
    case AVMEDIA_TYPE_VIDEO: vbuffer_.wait_and_push(frame); return 0;

    case AVMEDIA_TYPE_AUDIO:
        if (!frame || frame->nb_samples == 0) {
            LOG(INFO) << "[A] INPUT EOF";
            asrc_eof_ = true;
            return 0;
        }

        abuffer_->write(reinterpret_cast<void **>(frame->data), frame->nb_samples);
        audio_pts_ = frame->pts + abuffer_->size();

        return 0;

    default: return -1;
    }
}

int Encoder::start()
{
    if (!ready_ || running_) {
        LOG(WARNING) << "[   ENCODER] already running or not ready";
        return -1;
    }

    running_ = true;
    thread_  = std::jthread([this]() {
        probe::thread::set_name("ENCODER");

        LOG(INFO) << "STARTED";

        while (running_ && !eof()) {
            if (vbuffer_.empty() && (!abuffer_ || abuffer_->empty())) {
                std::this_thread::sleep_for(20ms);
                continue;
            }

            if (vstream_idx_ >= 0) process_video_frames();
            if (astream_idx_ >= 0) process_audio_frames();
        } // running

        LOG(INFO) << "[V] encoded frame = " << vcodec_ctx_->frame_number;
        LOG(INFO) << "EXITED";
    });

    return 0;
}

std::pair<int, int> Encoder::video_sync_process(av::frame& vframe)
{
    if (!vframe || !vframe->data[0]) return { 1, 0 };

    // duration
    double duration = std::min(1 / (av_q2d(vfmt.framerate) * av_q2d(vcodec_ctx_->time_base)),
                               1 / (av_q2d(input_framerate) * av_q2d(vcodec_ctx_->time_base)));

    double floating_pts = vframe->pts * av_q2d(vfmt.time_base) / av_q2d(vcodec_ctx_->time_base);

    if (expected_pts_ == AV_NOPTS_VALUE) expected_pts_ = std::lround(floating_pts);

    // floating_pts should in (last_pts, next_pts)
    double delta_l = floating_pts - expected_pts_; // expected range : (-duration, duration)
    double delta_r = delta_l + duration;

    int num_frames     = 1;
    int num_pre_frames = 0;

    // 1. pass
    if (delta_l < 0 && delta_r > 0) {
        floating_pts  = static_cast<double>(expected_pts_);
        duration     += delta_l;
        delta_l       = 0;
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
        vframe->duration = 1;
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
        vframe->duration = static_cast<int64_t>(duration);
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
    if (vbuffer_.empty()) return AVERROR(EAGAIN);

    auto vframe                       = vbuffer_.pop().value();
    auto [num_frames, num_pre_frames] = video_sync_process(vframe);

    av::frame encoding_frame{};
    for (auto i = 0; i < num_frames; ++i) {
        encoding_frame = (i < num_pre_frames && last_frame_->buf[0]) ? last_frame_ : vframe;

        //
        if (encoding_frame) {
            encoding_frame->quality   = vcodec_ctx_->global_quality;
            encoding_frame->pict_type = AV_PICTURE_TYPE_NONE;
            encoding_frame->pts       = expected_pts_;
        }

        int ret = avcodec_send_frame(vcodec_ctx_, encoding_frame.get());
        while (ret >= 0) {
            ret = avcodec_receive_packet(vcodec_ctx_, packet_.put());
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

            av_packet_rescale_ts(packet_.get(), vcodec_ctx_->time_base,
                                 fmt_ctx_->streams[vstream_idx_]->time_base);

            if (v_last_dts_ != AV_NOPTS_VALUE && v_last_dts_ >= packet_->dts) {
                LOG(WARNING) << fmt::format("[V] drop the packet with dts {} <= {}", packet_->dts,
                                            v_last_dts_);
                continue;
            }
            v_last_dts_ = packet_->dts;

            DLOG(INFO) << fmt::format(
                "[V] # {:>5d}, pts = {:>14d}, dts = {:>14d}, ts = {:.3%T}", vcodec_ctx_->frame_number,
                packet_->pts, packet_->dts,
                av::clock::ns(packet_->pts, fmt_ctx_->streams[vstream_idx_]->time_base));

            packet_->stream_index = vstream_idx_;
            if (av_interleaved_write_frame(fmt_ctx_, packet_.get()) != 0) {
                LOG(ERROR) << "[V] failed to write the the packet to file.";
                return -1;
            }
        }

        expected_pts_++;
    }

    last_frame_ = vframe;

    return 0;
}

int Encoder::process_audio_frames()
{
    if (abuffer_->size() < acodec_ctx_->frame_size && !asrc_eof_) return AVERROR(EAGAIN);

    const auto stream = fmt_ctx_->streams[astream_idx_];
    av::frame  aframe{};

    int ret = 0;
    // encode and write to the output
    while (!(eof_ & A_ENCODING_EOF) && (abuffer_->size() >= acodec_ctx_->frame_size || asrc_eof_)) {

        if ((abuffer_->size() >= acodec_ctx_->frame_size) || (!abuffer_->empty() && asrc_eof_)) {
            aframe.unref();

            aframe->nb_samples     = std::min(acodec_ctx_->frame_size, abuffer_->size());
            aframe->channels       = stream->codecpar->channels;
            aframe->channel_layout = stream->codecpar->channel_layout;
            aframe->format         = stream->codecpar->format;
            aframe->sample_rate    = stream->codecpar->sample_rate;
            aframe->pts            = audio_pts_ - abuffer_->size();

            av_frame_get_buffer(aframe.get(), 0);

            CHECK(abuffer_->read(reinterpret_cast<void **>(aframe->data), aframe->nb_samples) >=
                  aframe->nb_samples);

            ret = avcodec_send_frame(acodec_ctx_, aframe.get());
        }
        else if (asrc_eof_) {
            ret = avcodec_send_frame(acodec_ctx_, nullptr);
        }
        else {
            LOG(ERROR) << "[A] unknown error";
            break;
        }

        while (ret >= 0) {
            ret = avcodec_receive_packet(acodec_ctx_, packet_.put());
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

            av_packet_rescale_ts(packet_.get(), acodec_ctx_->time_base, stream->time_base);

            if (a_last_dts_ != AV_NOPTS_VALUE && a_last_dts_ >= packet_->dts) {
                LOG(WARNING) << fmt::format("[A] drop the frame: dts {} <= {}", packet_->dts, a_last_dts_);
                continue;
            }
            a_last_dts_ = packet_->dts;

            DLOG(INFO) << fmt::format("[A] # {:>5d}, pts = {:>14d}, dts = {:>14d}, ts = {:.3%T}",
                                      acodec_ctx_->frame_number, packet_->pts, packet_->dts,
                                      av::clock::ns(packet_->pts, stream->time_base));

            packet_->stream_index = astream_idx_;

            if (av_interleaved_write_frame(fmt_ctx_, packet_.get()) != 0) {
                LOG(ERROR) << "[A] failed to write the packet to the file.";
                return -1;
            }
        }
    }

    return ret;
}

void Encoder::close_output_file()
{
    if (!fmt_ctx_) return;

    if (av_write_trailer(fmt_ctx_) < 0) {
        LOG(ERROR) << "[   ENCODER] failed to write trailer";
    }

    if (!(fmt_ctx_->oformat->flags & AVFMT_NOFILE) && avio_close(fmt_ctx_->pb) < 0) {
        LOG(ERROR) << "[   ENCODER] failed to close the output file.";
    }

    avcodec_free_context(&vcodec_ctx_);
    avcodec_free_context(&acodec_ctx_);
    avformat_free_context(fmt_ctx_);
    fmt_ctx_ = nullptr;
}

void Encoder::stop()
{
    asrc_eof_ = true;
    vbuffer_.push(nullptr);

    // wait <= 3s for draining
    for (int i = 0; (i < 300) && ready() && !eof(); i++) {
        std::this_thread::sleep_for(10ms);
    }

    if (abuffer_) abuffer_->stop();
    vbuffer_.stop();

    ready_   = false;
    running_ = false;

    if (thread_.joinable()) thread_.join();

    close_output_file();

    LOG(INFO) << "[   ENCODER] STOPPED";
}

Encoder::~Encoder()
{
    vbuffer_.stop();
    vbuffer_.drain();

    if (abuffer_) {
        abuffer_->stop();
        abuffer_->drain();
    }

    asrc_eof_ = true;
    ready_    = false;
    running_  = false;

    if (thread_.joinable()) thread_.join();

    close_output_file();

    LOG(INFO) << "[   ENCODER] ~";
}