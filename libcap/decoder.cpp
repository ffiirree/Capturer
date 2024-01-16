#include "libcap/decoder.h"

#include "libcap/clock.h"
#include "libcap/hwaccel.h"

#include <cassert>
#include <filesystem>
#include <fmt/chrono.h>
#include <fmt/ranges.h>
#include <probe/defer.h>
#include <probe/thread.h>
#include <probe/util.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libavutil/time.h>
}

#define MIN_FRAMES 8

#if LIBAVFORMAT_VERSION_MAJOR >= 59
#define FFMPEG_59_CONST const
#else
#define FFMPEG_59_CONST
#endif

AVPixelFormat get_hw_format(AVCodecContext *ctx, const AVPixelFormat *pix_fmts)
{
    auto pix_fmt = pix_fmts;
    for (; *pix_fmt != AV_PIX_FMT_NONE; pix_fmt++) {
        const auto desc = av_pix_fmt_desc_get(*pix_fmt);

        if (!(desc->flags & AV_PIX_FMT_FLAG_HWACCEL)) break;

        const AVCodecHWConfig *config = nullptr;
        for (auto i = 0;; ++i) {
            config = avcodec_get_hw_config(ctx->codec, i);
            if (!config) break;

            if (!(config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)) continue;

            if (config->pix_fmt == *pix_fmt) break;
        }
        break;
    }

    LOG(INFO) << "pixel format : " << av_get_pix_fmt_name(*pix_fmt);
    return *pix_fmt;
}

static const AVCodec *choose_decoder(AVStream *stream, AVHWDeviceType hwaccel)
{
    if (hwaccel != AV_HWDEVICE_TYPE_NONE) {
        void *idx = nullptr;
        for (auto codec = av_codec_iterate(&idx); codec; codec = av_codec_iterate(&idx)) {
            if (codec->id != stream->codecpar->codec_id || !av_codec_is_decoder(codec)) continue;

            for (int j = 0;; j++) {
                auto config = avcodec_get_hw_config(codec, j);
                if (!config) break;

                if (config->device_type == hwaccel) {
                    LOG(INFO) << fmt::format(
                        "[   DECODER] [{}] select decoder '{}' because of requested hwaccel method '{}'",
                        av::to_char(stream->codecpar->codec_type), codec->name,
                        av_hwdevice_get_type_name(hwaccel));
                    return codec;
                }
            }
        }
    }

    return avcodec_find_decoder(stream->codecpar->codec_id);
}

int Decoder::open(const std::string& name, std::map<std::string, std::string> options)
{
    LOG(INFO) << fmt::format("[   DECODER] [{:>10}] options = {}", name, options);

    name_ = name;

    // format context
    if (fmt_ctx_) destroy();

    avdevice_register_all();

    // input format
    FFMPEG_59_CONST AVInputFormat *input_fmt = nullptr;
    if (options.contains("format")) {
        if (input_fmt = av_find_input_format(options.at("format").c_str()); !input_fmt) {
            LOG(ERROR) << "[   DECODER] av_find_input_format";
            return -1;
        }
        options.erase("format");
    }

    // hwaccel
    if (options.contains("hwaccel") && options.contains("hwaccel_pix_fmt")) {
        vfmt.hwaccel = av_hwdevice_find_type_by_name(options.at("hwaccel").c_str());
        vfmt.pix_fmt = av_get_pix_fmt(options.at("hwaccel_pix_fmt").c_str());

        LOG(INFO) << fmt::format("[   DECODER] hwaccel = {}, pix_fmt = {}", av::to_string(vfmt.hwaccel),
                                 av::to_string(vfmt.pix_fmt));
        options.erase("hwaccel");
        options.erase("hwaccel_pix_fmt");
    }

    // options
    AVDictionary *input_options = nullptr;
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

    // clock
    switch (clock_) {
    case av::clock_t::none: SYNC_PTS = -fmt_ctx_->start_time; break;
    case av::clock_t::system: SYNC_PTS = av::clock::us().count() - fmt_ctx_->start_time; break;
    case av::clock_t::device:
    default: SYNC_PTS = 0; break;
    }

    // av_dump_format(fmt_ctx_, 0, name.c_str(), 0);

    // find video & audio streams
    video_stream_idx_ = av_find_best_stream(fmt_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    audio_stream_idx_ = av_find_best_stream(fmt_ctx_, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    // subtitle_idx_     = av_find_best_stream(fmt_ctx_, AVMEDIA_TYPE_SUBTITLE, -1, -1, nullptr, 0);
    if (video_stream_idx_ < 0 && audio_stream_idx_ < 0) {
        LOG(ERROR) << "[   DECODER] not found any stream";
        return -1;
    }

    // video stream
    if (video_stream_idx_ >= 0) {
        auto video_decoder = choose_decoder(fmt_ctx_->streams[video_stream_idx_], vfmt.hwaccel);

        // decoder context
        if (video_decoder_ctx_ = avcodec_alloc_context3(video_decoder); !video_decoder_ctx_) {
            LOG(ERROR) << "[   DECODER] [V] failed to alloc decoder context";
            return -1;
        }

        if (avcodec_parameters_to_context(video_decoder_ctx_,
                                          fmt_ctx_->streams[video_stream_idx_]->codecpar) < 0) {
            LOG(ERROR) << "[   DECODER] [V] avcodec_parameters_to_context";
            return -1;
        }

        if (vfmt.hwaccel != AV_HWDEVICE_TYPE_NONE) {
            const auto hwctx                  = av::hwaccel::get_context(vfmt.hwaccel);
            video_decoder_ctx_->opaque        = &vfmt;
            video_decoder_ctx_->hw_device_ctx = hwctx->device_ctx_ref();
            video_decoder_ctx_->get_format    = get_hw_format;
            if (!video_decoder_ctx_->hw_device_ctx) {
                LOG(ERROR) << "[   DECODER] [V] can not create hardware device";
            }
        }

        // open codec
        AVDictionary *decoder_options = nullptr;
        defer(av_dict_free(&decoder_options));
        av_dict_set(&decoder_options, "threads", "auto", 0);
        if (avcodec_open2(video_decoder_ctx_, video_decoder, &decoder_options) < 0) {
            LOG(ERROR) << "[DECODER] [V] can not open the video decoder";
            return -1;
        }

        vfmt = av::vformat_t{
            .width     = video_decoder_ctx_->width,
            .height    = video_decoder_ctx_->height,
            .pix_fmt   = (vfmt.pix_fmt == AV_PIX_FMT_NONE) ? video_decoder_ctx_->pix_fmt : vfmt.pix_fmt,
            .framerate = av_guess_frame_rate(fmt_ctx_, fmt_ctx_->streams[video_stream_idx_], nullptr),
            .sample_aspect_ratio = video_decoder_ctx_->sample_aspect_ratio,
            .time_base           = fmt_ctx_->streams[video_stream_idx_]->time_base,
            .color               = {
                .space      = video_decoder_ctx_->colorspace,
                .range      = video_decoder_ctx_->color_range,
                .primaries  = video_decoder_ctx_->color_primaries,
                .transfer   = video_decoder_ctx_->color_trc,
            },
            .hwaccel         = vfmt.hwaccel,
        };

        LOG(INFO) << fmt::format("[   DECODER] [V] [{:>6}] {}({})", video_decoder->name,
                                 av::to_string(vfmt), av::to_string(vfmt.color));
    }

    // subtitle stream
    if (video_stream_idx_ >= 0 && subtitle_idx_ >= 0) {
        auto subtitle_decoder = choose_decoder(fmt_ctx_->streams[subtitle_idx_], AV_HWDEVICE_TYPE_NONE);

        if (subtitle_decoder_ctx_ = avcodec_alloc_context3(subtitle_decoder); !subtitle_decoder_ctx_) {
            LOG(ERROR) << "[   DECODER] [S] failed to alloc decoder context";
            return -1;
        }

        if (avcodec_parameters_to_context(subtitle_decoder_ctx_,
                                          fmt_ctx_->streams[subtitle_idx_]->codecpar) < 0) {
            LOG(ERROR) << "[   DECODER] [S] avcodec_parameters_to_context";
            return -1;
        }

        if (avcodec_open2(subtitle_decoder_ctx_, subtitle_decoder, nullptr) < 0) {
            LOG(ERROR) << "[   DECODER] [S] can not open the subtitle decoder";
            return -1;
        }

        LOG(INFO) << fmt::format("[   DECODER] [S] [{:>6}] start_time = {}", subtitle_decoder->name,
                                 fmt_ctx_->streams[subtitle_idx_]->start_time);
    }

    // audio stream
    if (audio_stream_idx_ >= 0) {
        const auto audio_stream = fmt_ctx_->streams[audio_stream_idx_];
        auto audio_decoder      = choose_decoder(audio_stream, AV_HWDEVICE_TYPE_NONE);

        if (audio_decoder_ctx_ = avcodec_alloc_context3(audio_decoder); !audio_decoder_ctx_) {
            LOG(ERROR) << "[   DECODER] [A] failed to alloc decoder context";
            return -1;
        }

        if (avcodec_parameters_to_context(audio_decoder_ctx_, audio_stream->codecpar) < 0) {
            LOG(ERROR) << "[   DECODER] [A] avcodec_parameters_to_context";
            return -1;
        }

        if (avcodec_open2(audio_decoder_ctx_, audio_decoder, nullptr) < 0) {
            LOG(ERROR) << "[   DECODER] [A] can not open the audio decoder";
            return -1;
        }

        afmt = av::aformat_t{
            .sample_rate    = audio_decoder_ctx_->sample_rate,
            .sample_fmt     = audio_decoder_ctx_->sample_fmt,
            .channels       = audio_decoder_ctx_->channels,
            .channel_layout = audio_decoder_ctx_->channel_layout,
            .time_base      = { 1, audio_decoder_ctx_->sample_rate },
        };

        audio_next_pts_ = av_rescale_q(audio_stream->start_time, audio_stream->time_base, afmt.time_base);

        LOG(INFO) << fmt::format("[   DECODER] [A] [{:>6}] {}, start_time = {}", audio_decoder->name,
                                 av::to_string(afmt), audio_stream->start_time);
    }

    ready_ = true;

    LOG(INFO) << fmt::format("[   DECODER] [{:>10}] opened", name_);
    return 0;
}

bool Decoder::has(AVMediaType type) const
{
    switch (type) {
    case AVMEDIA_TYPE_VIDEO: return video_stream_idx_ >= 0;
    case AVMEDIA_TYPE_AUDIO: return audio_stream_idx_ >= 0;
    case AVMEDIA_TYPE_SUBTITLE: return subtitle_idx_ >= 0;
    default: return false;
    }
}

std::string Decoder::format_str(AVMediaType type) const
{
    switch (type) {
    case AVMEDIA_TYPE_VIDEO: return (video_stream_idx_ >= 0) ? av::to_string(vfmt) : std::string{};
    case AVMEDIA_TYPE_AUDIO: return (audio_stream_idx_ >= 0) ? av::to_string(afmt) : std::string{};
    default: return {};
    }
}

AVRational Decoder::time_base(AVMediaType type) const
{
    switch (type) {
    case AVMEDIA_TYPE_AUDIO: {
        if (audio_stream_idx_ < 0) {
            LOG(WARNING) << "[   DECODER] [A] no audio stream";
            return OS_TIME_BASE_Q;
        }

        return afmt.time_base;
    }
    case AVMEDIA_TYPE_VIDEO: {
        if (video_stream_idx_ < 0) {
            LOG(WARNING) << "[   DECODER] [V] no video stream";
            return OS_TIME_BASE_Q;
        }

        return vfmt.time_base;
    }

    case AVMEDIA_TYPE_SUBTITLE: {
        if (subtitle_idx_ < 0) {
            LOG(WARNING) << "[   DECODER] [S] no subtitle stream";
            return OS_TIME_BASE_Q;
        }

        return fmt_ctx_->streams[subtitle_idx_]->time_base;
    }

    default: LOG(ERROR) << "[   DECODER] [X] unknown meida type."; return OS_TIME_BASE_Q;
    }
}

bool Decoder::eof() { return eof_ == DECODING_EOF; }

int Decoder::produce(AVFrame *frame, AVMediaType type)
{
    if (seeking()) return AVERROR(EAGAIN);

    switch (type) {
    case AVMEDIA_TYPE_VIDEO:
        if (vbuffer_.empty()) return ((eof_ & VDECODING_EOF) || !running()) ? AVERROR_EOF : AVERROR(EAGAIN);

        av_frame_unref(frame);
        av_frame_move_ref(frame, vbuffer_.pop().get());
        return 0;

    case AVMEDIA_TYPE_AUDIO:
        if (abuffer_.empty()) return ((eof_ & ADECODING_EOF) || !running()) ? AVERROR_EOF : AVERROR(EAGAIN);

        av_frame_unref(frame);
        av_frame_move_ref(frame, abuffer_.pop().get());
        return 0;

    default: return -1;
    }
}

bool Decoder::has_enough(AVMediaType type) const
{
    switch (type) {
    case AVMEDIA_TYPE_VIDEO: return video_stream_idx_ < 0 || vbuffer_.size() > MIN_FRAMES;
    case AVMEDIA_TYPE_AUDIO: return audio_stream_idx_ < 0 || abuffer_.size() > MIN_FRAMES;
    case AVMEDIA_TYPE_SUBTITLE: return audio_stream_idx_ < 0 || sbuffer_.size() > 2;
    default: return true;
    }
}

int Decoder::run()
{
    std::lock_guard lock(mtx_);

    if (!ready_ || running_) {
        LOG(ERROR) << fmt::format("[   DECODER] [{:>10}] already running or not ready", name_);
        return -1;
    }

    eof_     = 0x00;
    running_ = true;
    thread_  = std::thread([this]() {
        probe::thread::set_name("DEC-" + name_);
        decode_fn();
    });

    return 0;
}

int Decoder::decode_fn()
{
    LOG(INFO) << "STARTED";

    // reset the buffer
    vbuffer_.clear();
    abuffer_.clear();
    sbuffer_.clear();

    eof_ |= audio_stream_idx_ < 0 ? ADECODING_EOF : 0x00;
    eof_ |= video_stream_idx_ < 0 ? VDECODING_EOF : 0x00;
    eof_ |= subtitle_idx_ < 0 ? SDECODING_EOF : 0x00;

    while (running_) {
        if (seeking()) {
            LOG(INFO) << fmt::format("seek: {:%T}({:.3%S}, {:.3%S})", seek_.ts.load(), seek_.min.load(),
                                     seek_.max.load());

            if (avformat_seek_file(fmt_ctx_, -1, seek_.min.load().count() / 1'000,
                                   seek_.ts.load().count() / 1'000, seek_.max.load().count() / 1'000,
                                   0) < 0) {
                LOG(ERROR) << fmt::format("failed to seek, VIDEO: [{:%T}, {:%T}]",
                                          std::chrono::microseconds{ fmt_ctx_->start_time },
                                          std::chrono::microseconds{ fmt_ctx_->duration });
            }
            else {
                if (audio_stream_idx_ >= 0) avcodec_flush_buffers(audio_decoder_ctx_);
                if (video_stream_idx_ >= 0) avcodec_flush_buffers(video_decoder_ctx_);
                if (subtitle_idx_ >= 0) avcodec_flush_buffers(subtitle_decoder_ctx_);

                vbuffer_.clear();
                abuffer_.clear();
                sbuffer_.clear();

                audio_next_pts_ = AV_NOPTS_VALUE;
            }

            seek_.ts = av::clock::nopts;
        }

        // read
        int ret = av_read_frame(fmt_ctx_, packet_.put());
        if ((ret == AVERROR_EOF || avio_feof(fmt_ctx_->pb)) && !(eof_ & DEMUXING_EOF)) {
            // [draining] 1. Instead of valid input, send NULL to the avcodec_send_packet() (decoding) or
            //               avcodec_send_frame() (encoding) functions.
            //               This will enter draining mode.
            // [draining] 2. Call avcodec_receive_frame() (decoding) or avcodec_receive_packet() (encoding)
            //               in a loop until AVERROR_EOF is returned.
            //               The functions will not return AVERROR(EAGAIN), unless you forgot to enter
            //               draining mode.
            LOG(INFO) << "EOF => PUT NULL PACKET TO ENTER DRAINING MODE";
            eof_ |= DEMUXING_EOF;
        }
        else if (ret < 0) {
            LOG(ERROR) << "failed to read frame.";
            running_ = false;
            break;
        }

        // video decoding
        if (packet_->stream_index == video_stream_idx_ ||
            ((eof_ & DEMUXING_EOF) && !(eof_ & VDECODING_EOF))) {
            ret = avcodec_send_packet(video_decoder_ctx_, packet_.get());
            while (ret >= 0 && !seeking()) {
                ret = avcodec_receive_frame(video_decoder_ctx_, frame_.put());
                if (ret == AVERROR(EAGAIN)) {
                    break;
                }
                else if (ret == AVERROR_EOF) {
                    avcodec_flush_buffers(video_decoder_ctx_);

                    LOG(INFO) << "[V] EOF";
                    eof_ |= VDECODING_EOF;
                    break;
                }
                else if (ret < 0) {
                    running_ = false;
                    LOG(ERROR) << "[V] DECODING ERROR, ABORTING";
                    return ret;
                }

                frame_->pts = frame_->best_effort_timestamp;
                frame_->pts += av_rescale_q(SYNC_PTS, { 1, AV_TIME_BASE }, vfmt.time_base);
                if (frame_->pkt_dts != AV_NOPTS_VALUE)
                    frame_->pkt_dts += av_rescale_q(SYNC_PTS, { 1, AV_TIME_BASE }, vfmt.time_base);

                // waiting
                while ((has_enough(AVMEDIA_TYPE_VIDEO) && has_enough(AVMEDIA_TYPE_AUDIO)) && running_ &&
                       !seeking()) {
                    std::this_thread::sleep_for(10ms);
                }

                DLOG(INFO) << fmt::format("[V]  frame = {:>5d}, pts = {:>14d}, ts = {:%T}",
                                          video_decoder_ctx_->frame_number, frame_->pts,
                                          av::clock::ns(frame_->pts, vfmt.time_base));

                vbuffer_.push(frame_);
            }
        }

        // subtitle decoding
        if (packet_->stream_index == subtitle_idx_ || ((eof_ & DEMUXING_EOF) && !(eof_ & SDECODING_EOF))) {
            AVSubtitle subtitle;
            int got_frame = 0;
            if (avcodec_decode_subtitle2(subtitle_decoder_ctx_, &subtitle, &got_frame, packet_.get()) >=
                0) {
                if (got_frame) {
                    while ((has_enough(AVMEDIA_TYPE_VIDEO) && has_enough(AVMEDIA_TYPE_AUDIO) &&
                            has_enough(AVMEDIA_TYPE_SUBTITLE)) &&
                           running_ && !seeking()) {
                        std::this_thread::sleep_for(10ms);
                    }

                    DLOG(INFO) << fmt::format("[S] start = {:%T}, end = {:%T}",
                                              std::chrono::milliseconds{ subtitle.start_display_time },
                                              std::chrono::milliseconds{ subtitle.end_display_time });
                    avsubtitle_free(&subtitle);
                }
                else if (!got_frame && !packet_->data) { // AVERROR_EOF
                    avcodec_flush_buffers(subtitle_decoder_ctx_);

                    LOG(INFO) << "[S] EOF";
                    eof_ |= SDECODING_EOF;
                }
            }
        }

        // audio decoding
        if (packet_->stream_index == audio_stream_idx_ ||
            ((eof_ & DEMUXING_EOF) && !(eof_ & ADECODING_EOF))) {
            ret = avcodec_send_packet(audio_decoder_ctx_, packet_.get());
            while (ret >= 0 && !seeking()) {
                ret = avcodec_receive_frame(audio_decoder_ctx_, frame_.put());
                if (ret == AVERROR(EAGAIN)) {
                    break;
                }
                else if (ret == AVERROR_EOF) {
                    avcodec_flush_buffers(audio_decoder_ctx_);

                    LOG(INFO) << "[A] EOF";
                    eof_ |= ADECODING_EOF;
                    break;
                }
                else if (ret < 0) {
                    running_ = false;
                    LOG(ERROR) << "[A] DECODING ERROR, ABORTING";
                    return ret;
                }

                if (frame_->pts != AV_NOPTS_VALUE) {
                    frame_->pts = av_rescale_q(frame_->pts, fmt_ctx_->streams[audio_stream_idx_]->time_base,
                                               afmt.time_base);
                }
                else if (audio_next_pts_ != AV_NOPTS_VALUE) {
                    frame_->pts = audio_next_pts_;
                }

                if (frame_->pts != AV_NOPTS_VALUE) {
                    audio_next_pts_ = frame_->pts + frame_->nb_samples;
                    frame_->pts += av_rescale_q(SYNC_PTS, { 1, AV_TIME_BASE }, afmt.time_base);
                }

                if (frame_->pkt_dts != AV_NOPTS_VALUE)
                    frame_->pkt_dts += av_rescale_q(SYNC_PTS, { 1, AV_TIME_BASE }, afmt.time_base);

                // waiting
                while ((has_enough(AVMEDIA_TYPE_VIDEO) && has_enough(AVMEDIA_TYPE_AUDIO)) && running_ &&
                       !seeking()) {
                    std::this_thread::sleep_for(10ms);
                }

                DLOG(INFO) << fmt::format(
                    "[A]  frame = {:>5d}, pts = {:>14d}, samples = {:>5d}, muted = {}, ts = {:%T}",
                    audio_decoder_ctx_->frame_number, frame_->pts, frame_->nb_samples, muted_.load(),
                    av::clock::ns(frame_->pts, afmt.time_base));

                if (muted_)
                    av_samples_set_silence(frame_->data, 0, frame_->nb_samples, frame_->channels,
                                           static_cast<AVSampleFormat>(frame_->format));

                abuffer_.push(frame_);
            }
        } // decoding
    }

    if (video_stream_idx_ >= 0) {
        LOG(INFO) << fmt::format("[V] decoded frames = {:>5d}", video_decoder_ctx_->frame_number);
    }
    LOG(INFO) << "EXITED";

    running_ = false;

    return 0;
}

void Decoder::seek(const std::chrono::nanoseconds& ts, std::chrono::nanoseconds lts,
                   std::chrono::nanoseconds rts)
{
    assert(fmt_ctx_);

    seek_.ts  = std::clamp<std::chrono::nanoseconds>(ts, std::chrono::microseconds{ fmt_ctx_->start_time },
                                                    std::chrono::microseconds{ fmt_ctx_->duration });
    seek_.min = std::min<std::chrono::nanoseconds>(lts, std::chrono::microseconds{ duration() } - 5s);
    seek_.max =
        std::max<std::chrono::nanoseconds>(rts, std::chrono::microseconds{ fmt_ctx_->start_time } + 5s);

    if (!running_) {
        eof_ = 0x00;
        if (thread_.joinable()) thread_.join();
        run();
    }
}

void Decoder::reset()
{
    LOG(INFO) << fmt::format("[   DECODER] [{:>10}] RESET", name_);

    destroy();
}

void Decoder::destroy()
{
    std::lock_guard lock(mtx_);

    eof_ = DECODING_EOF;

    running_ = false;
    ready_   = false;

    if (thread_.joinable()) thread_.join();

    vbuffer_.clear();
    abuffer_.clear();

    name_  = {};
    muted_ = false;

    video_stream_idx_ = -1;
    audio_stream_idx_ = -1;
    subtitle_idx_     = -1;

    seek_.ts = av::clock::nopts;

    enabled_.clear();

    avcodec_free_context(&video_decoder_ctx_);
    avcodec_free_context(&audio_decoder_ctx_);
    avformat_close_input(&fmt_ctx_);
}

std::vector<std::map<std::string, std::string>> Decoder::properties(AVMediaType mt) const
{
    if (mt == AVMEDIA_TYPE_UNKNOWN) {
        auto map = av::to_map(fmt_ctx_->metadata);

        map["Format"]   = fmt_ctx_->iformat->long_name;
        map["Bitrate"]  = fmt_ctx_->bit_rate ? fmt::format("{} kb/s", fmt_ctx_->bit_rate / 1'024) : "N/A";
        map["Duration"] = fmt::format("{:%T}", std::chrono::microseconds{ fmt_ctx_->duration });

        return { map };
    }

    // video | audio | subtitle
    std::vector<std::map<std::string, std::string>> properties{};

    for (unsigned int i = 0; i < fmt_ctx_->nb_streams; ++i) {
        const auto stream = fmt_ctx_->streams[i];
        const auto params = stream->codecpar;

        if (params->codec_type != mt) continue;

        auto map     = av::to_map(stream->metadata);
        map["Index"] = std::to_string(stream->index);
        map["Duration"] =
            (stream->duration == AV_NOPTS_VALUE)
                ? "N/A"
                : fmt::format("{:%T}", std::chrono::microseconds{ av_rescale_q(
                                           stream->duration, stream->time_base, { 1, AV_TIME_BASE }) });
        map["Codec"] = probe::util::toupper(avcodec_get_name(params->codec_id));

        switch (mt) {
        case AVMEDIA_TYPE_VIDEO: {
            map["Profile"]   = avcodec_profile_name(params->codec_id, params->profile);
            map["Width"]     = std::to_string(params->width);
            map["Height"]    = std::to_string(params->height);
            auto framerate   = av_guess_frame_rate(fmt_ctx_, stream, nullptr);
            map["Framerate"] = fmt::format("{:.3f} ({})", av_q2d(framerate), framerate);
            map["Bitrate"]   = params->bit_rate ? fmt::format("{} kb/s", params->bit_rate / 1'024) : "N/A";
            map["SAR"] =
                params->sample_aspect_ratio.num ? av::to_string(params->sample_aspect_ratio) : "N/A";
            map["Pixel Format"] =
                probe::util::toupper(av::to_string(static_cast<AVPixelFormat>(params->format)));
            map["Color Space"]     = probe::util::toupper(av::to_string(params->color_space));
            map["Color Range"]     = probe::util::toupper(av::to_string(params->color_range));
            map["Color Primaries"] = probe::util::toupper(av::to_string(params->color_primaries));
            map["Color TRC"]       = probe::util::toupper(av::to_string(params->color_trc));
            break;
        }

        case AVMEDIA_TYPE_AUDIO:
            map["Sample Format"] =
                probe::util::toupper(av::to_string(static_cast<AVSampleFormat>(params->format)));
            map["Bitrate"]  = params->bit_rate ? fmt::format("{} kb/s", params->bit_rate / 1'024) : "N/A";
            map["Channels"] = std::to_string(params->channels);
            map["Channel Layout"] = av::channel_layout_name(params->channels, params->channel_layout);
            map["Sample Rate"]    = fmt::format("{} Hz", params->sample_rate);
            break;
        case AVMEDIA_TYPE_SUBTITLE:
        default: break;
        }

        properties.push_back(map);
    }

    return properties;
}