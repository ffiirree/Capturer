#include "libcap/decoder.h"

#include "libcap/hwaccel.h"

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
    avdevice_register_all();

    // input format
    FFMPEG_59_CONST AVInputFormat *input_fmt = nullptr;
    if (options.contains("format")) {
        const auto format = options.at("format");

        if (format == "x11grab" || format == "v4l2" || format == "dshow") is_realtime_ = true;

        if (input_fmt = av_find_input_format(format.c_str()); !input_fmt) {
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

    // av_dump_format(fmt_ctx_, 0, name.c_str(), 0);

    // find video & audio streams
    vstream_idx_ = av_find_best_stream(fmt_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    astream_idx_ = av_find_best_stream(fmt_ctx_, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    // subtitle_idx_     = av_find_best_stream(fmt_ctx_, AVMEDIA_TYPE_SUBTITLE, -1, -1, nullptr, 0);
    if (vstream_idx_ < 0 && astream_idx_ < 0) {
        LOG(ERROR) << "[   DECODER] not found any stream";
        return -1;
    }

    // video stream
    if (vstream_idx_ >= 0) {
        auto video_decoder = choose_decoder(fmt_ctx_->streams[vstream_idx_], vfmt.hwaccel);

        // decoder context
        if (vcodec_ctx_ = avcodec_alloc_context3(video_decoder); !vcodec_ctx_) {
            LOG(ERROR) << "[   DECODER] [V] failed to alloc decoder context";
            return -1;
        }

        if (avcodec_parameters_to_context(vcodec_ctx_, fmt_ctx_->streams[vstream_idx_]->codecpar) < 0) {
            LOG(ERROR) << "[   DECODER] [V] avcodec_parameters_to_context";
            return -1;
        }

        if (vfmt.hwaccel != AV_HWDEVICE_TYPE_NONE) {
            const auto hwctx           = av::hwaccel::get_context(vfmt.hwaccel);
            vcodec_ctx_->opaque        = &vfmt;
            vcodec_ctx_->hw_device_ctx = hwctx->device_ctx_ref();
            vcodec_ctx_->get_format    = get_hw_format;
            if (!vcodec_ctx_->hw_device_ctx) {
                LOG(ERROR) << "[   DECODER] [V] can not create hardware device";
            }
        }

        // open codec
        AVDictionary *decoder_options = nullptr;
        defer(av_dict_free(&decoder_options));
        av_dict_set(&decoder_options, "threads", "auto", 0);
        if (avcodec_open2(vcodec_ctx_, video_decoder, &decoder_options) < 0) {
            LOG(ERROR) << "[DECODER] [V] can not open the video decoder";
            return -1;
        }

        vfmt = av::vformat_t{
            .width     = vcodec_ctx_->width,
            .height    = vcodec_ctx_->height,
            .pix_fmt   = (vfmt.pix_fmt == AV_PIX_FMT_NONE) ? vcodec_ctx_->pix_fmt : vfmt.pix_fmt,
            .framerate = av_guess_frame_rate(fmt_ctx_, fmt_ctx_->streams[vstream_idx_], nullptr),
            .sample_aspect_ratio = vcodec_ctx_->sample_aspect_ratio,
            .time_base           = fmt_ctx_->streams[vstream_idx_]->time_base,
            .color               = {
                .space      = vcodec_ctx_->colorspace,
                .range      = vcodec_ctx_->color_range,
                .primaries  = vcodec_ctx_->color_primaries,
                .transfer   = vcodec_ctx_->color_trc,
            },
            .hwaccel         = vfmt.hwaccel,
        };

        LOG(INFO) << fmt::format("[   DECODER] [V] [{:>6}] {}({})", video_decoder->name,
                                 av::to_string(vfmt), av::to_string(vfmt.color));
    }

    // subtitle stream
    if (vstream_idx_ >= 0 && sstream_idx_ >= 0) {
        auto subtitle_decoder = avcodec_find_decoder(fmt_ctx_->streams[sstream_idx_]->codecpar->codec_id);

        if (scodec_ctx_ = avcodec_alloc_context3(subtitle_decoder); !scodec_ctx_) {
            LOG(ERROR) << "[   DECODER] [S] failed to alloc decoder context";
            return -1;
        }

        if (avcodec_parameters_to_context(scodec_ctx_, fmt_ctx_->streams[sstream_idx_]->codecpar) < 0) {
            LOG(ERROR) << "[   DECODER] [S] avcodec_parameters_to_context";
            return -1;
        }

        if (avcodec_open2(scodec_ctx_, subtitle_decoder, nullptr) < 0) {
            LOG(ERROR) << "[   DECODER] [S] can not open the subtitle decoder";
            return -1;
        }

        LOG(INFO) << fmt::format("[   DECODER] [S] [{:>6}] start_time = {}", subtitle_decoder->name,
                                 fmt_ctx_->streams[sstream_idx_]->start_time);
    }

    // audio stream
    if (astream_idx_ >= 0) {
        auto audio_decoder = avcodec_find_decoder(fmt_ctx_->streams[astream_idx_]->codecpar->codec_id);

        if (acodec_ctx_ = avcodec_alloc_context3(audio_decoder); !acodec_ctx_) {
            LOG(ERROR) << "[   DECODER] [A] failed to alloc decoder context";
            return -1;
        }

        if (avcodec_parameters_to_context(acodec_ctx_, fmt_ctx_->streams[astream_idx_]->codecpar) < 0) {
            LOG(ERROR) << "[   DECODER] [A] avcodec_parameters_to_context";
            return -1;
        }

        if (avcodec_open2(acodec_ctx_, audio_decoder, nullptr) < 0) {
            LOG(ERROR) << "[   DECODER] [A] can not open the audio decoder";
            return -1;
        }

        afmt = av::aformat_t{
            .sample_rate    = acodec_ctx_->sample_rate,
            .sample_fmt     = acodec_ctx_->sample_fmt,
            .channels       = acodec_ctx_->channels,
            .channel_layout = acodec_ctx_->channel_layout,
            .time_base      = { 1, acodec_ctx_->sample_rate },
        };

        audio_next_pts_ = av_rescale_q(fmt_ctx_->streams[astream_idx_]->start_time,
                                       fmt_ctx_->streams[astream_idx_]->time_base, afmt.time_base);

        LOG(INFO) << fmt::format("[   DECODER] [A] [{:>6}] {}", audio_decoder->name, av::to_string(afmt));
    }

    ready_ = true;

    LOG(INFO) << fmt::format("[   DECODER] [{:>10}] opened", name_);
    return 0;
}

bool Decoder::has(const AVMediaType type) const
{
    switch (type) {
    case AVMEDIA_TYPE_VIDEO:    return vstream_idx_ >= 0;
    case AVMEDIA_TYPE_AUDIO:    return astream_idx_ >= 0;
    case AVMEDIA_TYPE_SUBTITLE: return sstream_idx_ >= 0;
    default:                    return false;
    }
}

std::chrono::nanoseconds Decoder::start_time() const
{
    return fmt_ctx_ ? av::clock::ns(fmt_ctx_->start_time, AV_TIME_BASE_Q) : av::clock::nopts;
}

std::chrono::nanoseconds Decoder::duration() const
{
    return fmt_ctx_ ? av::clock::ns(fmt_ctx_->duration, AV_TIME_BASE_Q) : av::clock::nopts;
}

bool Decoder::eof()
{
    return (vstream_idx_ < 0 || (eof_ & VDECODING_EOF)) && (astream_idx_ < 0 || (eof_ & ADECODING_EOF)) &&
           (sstream_idx_ < 0 || (eof_ & SDECODING_EOF));
}

int Decoder::start()
{
    if (!ready_ || running_) {
        LOG(ERROR) << fmt::format("[   DECODER] [{:>10}] already running or not ready", name_);
        return -1;
    }

    eof_     = 0x00;
    running_ = true;
    thread_  = std::jthread([this]() {
        probe::thread::set_name("DEC-" + name_);
        decode_fn();
    });

    return 0;
}

int Decoder::decode_fn()
{
    LOG(INFO) << "STARTED";

    av::packet packet{};
    while (running_) {
        if (seeking()) {
            std::scoped_lock lock(seek_mtx_);

            if (avformat_seek_file(fmt_ctx_, -1, seek_min_, seek_pts_, seek_max_, 0) < 0) {
                LOG(ERROR) << fmt::format("failed to seek");
            }
            else {
                if (astream_idx_ >= 0) avcodec_flush_buffers(acodec_ctx_);
                if (vstream_idx_ >= 0) avcodec_flush_buffers(vcodec_ctx_);
                if (sstream_idx_ >= 0) avcodec_flush_buffers(scodec_ctx_);

                audio_next_pts_ = AV_NOPTS_VALUE;
                eof_            = 0x00;
                feof_           = false;
            }

            seek_pts_ = AV_NOPTS_VALUE;
            seek_min_ = std::numeric_limits<int64_t>::min();
            seek_max_ = std::numeric_limits<int64_t>::max();
        }

        // EOF
        if (eof()) {
            std::this_thread::sleep_for(30ms);
            continue;
        }

        // read
        int ret = av_read_frame(fmt_ctx_, packet.put());
        if ((ret == AVERROR_EOF || avio_feof(fmt_ctx_->pb)) && !feof_) {
            feof_ = true;
            LOG(INFO) << "ENTER DRAINING MODE";
        }
        else if (ret < 0) {
            LOG(ERROR) << "failed to read frame.";
            running_ = false;
            continue;
        }

        // video decoding
        if ((feof_ || packet->stream_index == vstream_idx_) && vstream_idx_ >= 0 &&
            !(eof_ & VDECODING_EOF)) {
            ret = avcodec_send_packet(vcodec_ctx_, packet.get());
            while (ret >= 0 && !seeking()) {
                ret = avcodec_receive_frame(vcodec_ctx_, frame_.put());
                if (ret == AVERROR(EAGAIN)) {
                    break;
                }
                else if (ret == AVERROR_EOF) {
                    onarrived(nullptr, AVMEDIA_TYPE_VIDEO);

                    LOG(INFO) << "[V] EOF, SEND NULL";
                    eof_ |= VDECODING_EOF;
                    break;
                }
                else if (ret < 0) {
                    running_ = false;
                    LOG(ERROR) << "[V] DECODING ERROR, ABORTING";
                    return ret;
                }

                frame_->pts = frame_->best_effort_timestamp;

                if (is_realtime_)
                    frame_->pts -= av_rescale_q(fmt_ctx_->start_time, AV_TIME_BASE_Q, vfmt.time_base);

                onarrived(frame_, AVMEDIA_TYPE_VIDEO);

                DLOG(INFO) << fmt::format("[V] # {:>5d}, pts = {:>14d}, ts = {:%T}",
                                          vcodec_ctx_->frame_number, frame_->pts,
                                          av::clock::ns(frame_->pts, vfmt.time_base));
            }
        } // video decoding

        // audio decoding
        if ((feof_ || packet->stream_index == astream_idx_) && astream_idx_ >= 0 &&
            !(eof_ & ADECODING_EOF)) {
            ret = avcodec_send_packet(acodec_ctx_, packet.get());
            while (ret >= 0 && !seeking()) {
                ret = avcodec_receive_frame(acodec_ctx_, frame_.put());
                if (ret == AVERROR(EAGAIN)) {
                    break;
                }
                else if (ret == AVERROR_EOF) {
                    onarrived(nullptr, AVMEDIA_TYPE_AUDIO);
                    eof_ |= ADECODING_EOF;

                    LOG(INFO) << "[A] EOF, SEND NULL";
                    break;
                }
                else if (ret < 0) {
                    running_ = false;
                    LOG(ERROR) << "[A] DECODING ERROR, ABORTING";
                    return ret;
                }

                if (frame_->pts != AV_NOPTS_VALUE) {
                    frame_->pts = av_rescale_q(frame_->pts, fmt_ctx_->streams[astream_idx_]->time_base,
                                               afmt.time_base);
                }
                else if (audio_next_pts_ != AV_NOPTS_VALUE) {
                    frame_->pts = audio_next_pts_;
                }

                if (frame_->pts != AV_NOPTS_VALUE) {
                    if (is_realtime_)
                        frame_->pts -= av_rescale_q(fmt_ctx_->start_time, AV_TIME_BASE_Q, afmt.time_base);

                    audio_next_pts_ = frame_->pts + frame_->nb_samples;
                }

                if (muted_)
                    av_samples_set_silence(frame_->data, 0, frame_->nb_samples, frame_->channels,
                                           static_cast<AVSampleFormat>(frame_->format));

                onarrived(frame_, AVMEDIA_TYPE_AUDIO);

                DLOG(INFO) << fmt::format("[A] # {:>5d}, pts = {:>14d}, ts = {:%T}",
                                          acodec_ctx_->frame_number, frame_->pts,
                                          av::clock::ns(frame_->pts, afmt.time_base));
            }
        } // auido decoding

        // subtitle decoding
        if ((feof_ || packet->stream_index == sstream_idx_) && sstream_idx_ >= 0 &&
            !(eof_ & SDECODING_EOF)) {
            AVSubtitle subtitle;
            int        got_frame = 0;
            if (avcodec_decode_subtitle2(scodec_ctx_, &subtitle, &got_frame, packet.get()) >= 0) {
                if (got_frame) {
                    DLOG(INFO) << fmt::format("[S] start = {:%T}, end = {:%T}",
                                              std::chrono::milliseconds{ subtitle.start_display_time },
                                              std::chrono::milliseconds{ subtitle.end_display_time });
                    avsubtitle_free(&subtitle);
                }
                else if (!got_frame && !packet->data) { // AVERROR_EOF
                    avcodec_flush_buffers(scodec_ctx_);

                    LOG(INFO) << "[S] EOF";
                    eof_ |= SDECODING_EOF;
                }
            }
        } // subtitle decoding
    }

    LOG(INFO) << "DECODING THREAD EXITED";

    return 0;
}

void Decoder::seek(const std::chrono::nanoseconds& ts, const std::chrono::nanoseconds& rel)
{
    std::scoped_lock lock(seek_mtx_);

    LOG(INFO) << fmt::format("seek: {:.3%T} {:+}s", ts, rel.count() / 1000000000);

    seek_pts_ = std::clamp<int64_t>(ts.count() / 1000, fmt_ctx_->start_time, fmt_ctx_->duration);
    seek_min_ = rel > 0s ? (ts - rel).count() / 1000 + 2 : std::numeric_limits<int64_t>::min();
    seek_max_ = rel < 0s ? (ts - rel).count() / 1000 - 2 : std::numeric_limits<int64_t>::max();

    eof_ = 0x00;

    if (!running_) {
        if (thread_.joinable()) thread_.join();
        start();
    }
}

bool Decoder::seeking() const
{
    std::shared_lock lock(seek_mtx_);
    return seek_pts_ != AV_NOPTS_VALUE;
}

void Decoder::stop()
{
    running_ = false;
    ready_   = false;

    if (thread_.joinable()) thread_.join();

    LOG(INFO) << fmt::format("[   DECODER] [{:>10}] STOPPED", name_);
}

Decoder::~Decoder()
{
    stop();

    avcodec_free_context(&vcodec_ctx_);
    avcodec_free_context(&acodec_ctx_);
    avcodec_free_context(&scodec_ctx_);
    avformat_close_input(&fmt_ctx_);

    LOG(INFO) << fmt::format("[   DECODER] [{:>10}] ~", name_);
}

std::vector<std::map<std::string, std::string>> Decoder::properties(const AVMediaType mt) const
{
    if (mt == AVMEDIA_TYPE_UNKNOWN) {
        auto map = av::to_map(fmt_ctx_->metadata);

        map["Format"]   = fmt_ctx_->iformat->long_name;
        map["Bitrate"]  = fmt_ctx_->bit_rate ? fmt::format("{} kb/s", fmt_ctx_->bit_rate / 1024) : "N/A";
        map["Duration"] = fmt_ctx_->duration < 0
                              ? "N/A"
                              : fmt::format("{:%T}", std::chrono::microseconds{ fmt_ctx_->duration });

        return { map };
    }

    // video | audio | subtitle
    std::vector<std::map<std::string, std::string>> properties{};

    for (unsigned int i = 0; i < fmt_ctx_->nb_streams; ++i) {
        const auto stream = fmt_ctx_->streams[i];
        const auto params = stream->codecpar;

        if (params->codec_type != mt) continue;

        auto map        = av::to_map(stream->metadata);
        map["Index"]    = std::to_string(stream->index);
        map["Duration"] = (stream->duration < 0)
                              ? "N/A"
                              : fmt::format("{:%T}", av::clock::ms(stream->duration, stream->time_base));
        map["Codec"]    = probe::util::toupper(avcodec_get_name(params->codec_id));

        switch (mt) {
        case AVMEDIA_TYPE_VIDEO: {
            const auto profile = avcodec_profile_name(params->codec_id, params->profile);
            map["Profile"]     = profile ? profile : "";
            map["Width"]       = std::to_string(params->width);
            map["Height"]      = std::to_string(params->height);
            auto framerate     = av_guess_frame_rate(fmt_ctx_, stream, nullptr);
            map["Framerate"]   = fmt::format("{:.3f} ({})", av_q2d(framerate), framerate);
            map["Bitrate"]     = params->bit_rate ? fmt::format("{} kb/s", params->bit_rate / 1024) : "N/A";
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
            map["Bitrate"]  = params->bit_rate ? fmt::format("{} kb/s", params->bit_rate / 1024) : "N/A";
            map["Channels"] = std::to_string(params->channels);
            map["Channel Layout"] = av::channel_layout_name(params->channels, params->channel_layout);
            map["Sample Rate"]    = fmt::format("{} Hz", params->sample_rate);
            break;
        case AVMEDIA_TYPE_SUBTITLE:
        default:                    break;
        }

        properties.push_back(map);
    }

    return properties;
}