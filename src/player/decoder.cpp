#include "decoder.h"

#include "libcap/filter.h"
#include "libcap/hwaccel.h"
#include "logging.h"

#include <filesystem>
#include <fmt/chrono.h>
#include <probe/defer.h>
#include <probe/thread.h>
#include <probe/util.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libavutil/time.h>
}

#define MIN_FRAMES 8

static AVPixelFormat get_hw_format(AVCodecContext *ctx, const AVPixelFormat *pix_fmts)
{
    const auto vfmt = static_cast<av::vformat_t *>(ctx->opaque);

    auto pix_fmt = pix_fmts;
    for (; *pix_fmt != AV_PIX_FMT_NONE; pix_fmt++) {

        if (const auto desc = av_pix_fmt_desc_get(*pix_fmt); !(desc->flags & AV_PIX_FMT_FLAG_HWACCEL))
            break;

        const AVCodecHWConfig *config;
        for (auto i = 0;; ++i) {
            config = avcodec_get_hw_config(ctx->codec, i);

            if (!config) break;

            if (!(config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)) continue;

            if (config->pix_fmt == *pix_fmt) break;
        }

        if (config && config->device_type == vfmt->hwaccel) break;
    }

    logi("pixel format : {}", av_get_pix_fmt_name(*pix_fmt));
    return *pix_fmt;
}

static const AVCodec *choose_decoder(const AVStream *stream, const AVHWDeviceType hwaccel)
{
    if (hwaccel != AV_HWDEVICE_TYPE_NONE) {
        void *idx = nullptr;
        for (auto codec = av_codec_iterate(&idx); codec; codec = av_codec_iterate(&idx)) {
            if (codec->id != stream->codecpar->codec_id || !av_codec_is_decoder(codec)) continue;

            for (int j = 0;; j++) {
                const auto config = avcodec_get_hw_config(codec, j);
                if (!config) break;

                if (config->device_type == hwaccel) {
                    logi("[    DECODER] [{}] select decoder '{}' because of requested hwaccel method '{}'",
                         av::to_char(stream->codecpar->codec_type), codec->name,
                         av_hwdevice_get_type_name(hwaccel));
                    return codec;
                }
            }
        }
    }

    return avcodec_find_decoder(stream->codecpar->codec_id);
}

static void ass_log(int level, const char *fmt, va_list args, void *)
{
    char buffer[1024]{};
    vsnprintf(buffer, 1024, fmt, args);
    logd("[ASS] -- {} -- {}", level, buffer);
}

static const char *const font_mimetypes[] = {
    "font/ttf",
    "font/otf",
    "font/sfnt",
    "font/woff",
    "font/woff2",
    "application/font-sfnt",
    "application/font-woff",
    "application/x-truetype-font",
    "application/vnd.ms-opentype",
    "application/x-font-ttf",
    nullptr,
};

static bool attachment_is_font(AVStream *st)
{
    if (const auto tag = av_dict_get(st->metadata, "mimetype", nullptr, AV_DICT_MATCH_CASE); tag) {
        for (int n = 0; font_mimetypes[n]; n++) {
            if (std::string{ font_mimetypes[n] } == probe::util::tolower(tag->value) == 0) {
                return true;
            }
        }
    }

    return false;
}

int Decoder::open(const std::string& name)
{
    if (avformat_open_input(&fmt_ctx_, name.c_str(), nullptr, nullptr) < 0) {
        loge("[    DECODER] failed to open file: {}", name);
        return -1;
    }

    if (avformat_find_stream_info(fmt_ctx_, nullptr) < 0) {
        loge("[    DECODER] failed to find the stream information");
        return -1;
    }

    // find video & audio streams
    vctx.index = av_find_best_stream(fmt_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    actx.index = av_find_best_stream(fmt_ctx_, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    sctx.index = av_find_best_stream(fmt_ctx_, AVMEDIA_TYPE_SUBTITLE, -1,
                                     actx.index >= 0 ? actx.index : vctx.index, nullptr, 0);
    if (vctx.index < 0 && actx.index < 0) {
        loge("[    DECODER] not found any stream");
        return -1;
    }

    // video stream
    if (vctx.index >= 0) {
        vctx.stream  = fmt_ctx_->streams[vctx.index];
        auto decoder = choose_decoder(vctx.stream, vfi.hwaccel);
        if (vctx.codec = avcodec_alloc_context3(decoder); !vctx.codec) return -1;
        if (avcodec_parameters_to_context(vctx.codec, vctx.stream->codecpar) < 0) return -1;

        if (vfi.hwaccel != AV_HWDEVICE_TYPE_NONE) {
            const auto hwctx          = av::hwaccel::get_context(vfi.hwaccel);
            vctx.codec->opaque        = &vfi;
            vctx.codec->hw_device_ctx = av_buffer_ref(hwctx->device_ctx.get());
            vctx.codec->get_format    = get_hw_format;
            if (!vctx.codec->hw_device_ctx) {
                loge("[    DECODER] [V] can not create hardware device");
            }
        }

        // open codec
        AVDictionary *decoder_options = nullptr;
        defer(av_dict_free(&decoder_options));
        av_dict_set(&decoder_options, "threads", "auto", 0);
        if (avcodec_open2(vctx.codec, decoder, &decoder_options) < 0) {
            loge("[    DECODER] [V] can not open the video decoder");
            return -1;
        }

        vfi = {
            .width     = vctx.codec->width,
            .height    = vctx.codec->height,
            .pix_fmt   = vfi.hwaccel ? vfi.pix_fmt : vctx.codec->pix_fmt,
            .framerate = av_guess_frame_rate(fmt_ctx_, vctx.stream, nullptr),
            .sample_aspect_ratio = vctx.codec->sample_aspect_ratio,
            .time_base           = vctx.stream->time_base,
            .color               = {
                .space      = vctx.codec->colorspace,
                .range      = vctx.codec->color_range,
                .primaries  = vctx.codec->color_primaries,
                .transfer   = vctx.codec->color_trc,
            },
            .hwaccel         = vfi.hwaccel,
        };

        logi("[    DECODER] [V] [{:>6}] {}({})", decoder->name, av::to_string(vfi),
             av::to_string(vfi.color));
    }

    // audio stream
    if (actx.index >= 0) {
        actx.stream  = fmt_ctx_->streams[actx.index];
        auto decoder = avcodec_find_decoder(actx.stream->codecpar->codec_id);
        if (actx.codec = avcodec_alloc_context3(decoder); !actx.codec) return -1;
        if (avcodec_parameters_to_context(actx.codec, actx.stream->codecpar) < 0) return -1;
        if (avcodec_open2(actx.codec, decoder, nullptr) < 0) {
            loge("[    DECODER] [A] can not open the audio decoder");
            return -1;
        }

        afi = {
            .sample_rate    = actx.codec->sample_rate,
            .sample_fmt     = actx.codec->sample_fmt,
            .channels       = actx.codec->channels,
            .channel_layout = actx.codec->channel_layout,
            .time_base      = { 1, actx.codec->sample_rate },
        };

        actx.next_pts = av_rescale_q(actx.stream->start_time, actx.stream->time_base, afi.time_base);

        logi("[    DECODER] [A] [{:>6}] {}", decoder->name, av::to_string(afi));
    }

    if (sctx.index >= 0) {
        sctx.stream  = fmt_ctx_->streams[sctx.index];
        auto decoder = avcodec_find_decoder(sctx.stream->codecpar->codec_id);
        if (sctx.codec = avcodec_alloc_context3(decoder); !sctx.codec) return -1;
        if (avcodec_parameters_to_context(sctx.codec, sctx.stream->codecpar) < 0) return -1;
        if (avcodec_open2(sctx.codec, decoder, nullptr) < 0) {
            loge("[    DECODER] [A] can not open the subtitle decoder");
            return -1;
        }

        // libass
        ass_.library = ass_library_init();
        if (!ass_.library) {
            loge("[    DECODER] [S] failed to init libass");
            return -1;
        }
        ass_set_message_cb(ass_.library, ass_log, &ass_);

        ass_set_fonts_dir(ass_.library, nullptr);
        ass_set_extract_fonts(ass_.library, 1);

        ass_.renderer = ass_renderer_init(ass_.library);
        if (!ass_.renderer) {
            loge("could not initialize libass renderer");
            return AVERROR(EINVAL);
        }

        ass_.track = ass_new_track(ass_.library);
        if (!ass_.track) {
            loge("could not create a libass track");
            return AVERROR(EINVAL);
        }

        // load attached fonts
        for (int i = 0; i < fmt_ctx_->nb_streams; ++i) {
            const auto stream = fmt_ctx_->streams[i];
            if (stream->codecpar->codec_type == AVMEDIA_TYPE_ATTACHMENT && attachment_is_font(stream)) {
                if (const auto tag = av_dict_get(stream->metadata, "filename", nullptr, AV_DICT_MATCH_CASE);
                    tag) {
                    logd("Loading attached font: {}", tag->value);
                    ass_add_font(ass_.library, tag->value,
                                 reinterpret_cast<const char *>(stream->codecpar->extradata),
                                 stream->codecpar->extradata_size);
                }
                else {
                    logw("font attachment has no filename, ignored");
                }
            }
        }

        ass_set_fonts(ass_.renderer, nullptr, nullptr, 1, nullptr, 1);

        if (sctx.codec->subtitle_header) {
            ass_process_codec_private(ass_.track, reinterpret_cast<char *>(sctx.codec->subtitle_header),
                                      sctx.codec->subtitle_header_size);
        }

        logi("[    DECODER] [S] [{:>6}] {}", decoder->name, sctx.stream->start_time);
    }

    ready_ = true;

    logi("[    DECODER] [{}] is opened", name);
    return 0;
}

int Decoder::create_video_graph()
{
    if (vctx.graph) avfilter_graph_free(&vctx.graph);
    if (vctx.graph = avfilter_graph_alloc(); !vctx.graph) return av::NOMEM;

    if (av::graph::create_video_src(vctx.graph, &vctx.src, vfi) < 0) return -1;
    if (av::graph::create_video_sink(vctx.graph, &vctx.sink, vfo) < 0) return -1;

    if (avfilter_link(vctx.src, 0, vctx.sink, 0) < 0) {
        loge("[    DECODER] [V] failed to link filter graph");
        return -1;
    }

    if (avfilter_graph_config(vctx.graph, nullptr) < 0) {
        loge("[    DECODER] failed to configure the filter graph");
        return -1;
    }

    return 0;
}

int Decoder::create_audio_graph()
{
    if (actx.graph) avfilter_graph_free(&actx.graph);
    if (actx.graph = avfilter_graph_alloc(); !actx.graph) return av::NOMEM;

    if (av::graph::create_audio_src(actx.graph, &actx.src, afi) < 0) return -1;
    if (av::graph::create_audio_sink(actx.graph, &actx.sink, afo) < 0) return -1;

    if (avfilter_link(actx.src, 0, actx.sink, 0) < 0) {
        loge("[    DECODER] [A] failed to link filter graph");
        return -1;
    }

    if (avfilter_graph_config(actx.graph, nullptr) < 0) {
        loge("[    DECODER] [A] failed to configure the filter graph");
        return -1;
    }

    return 0;
}

bool Decoder::has(const AVMediaType type) const
{
    switch (type) {
    case AVMEDIA_TYPE_VIDEO:    return vctx.index >= 0;
    case AVMEDIA_TYPE_AUDIO:    return actx.index >= 0;
    case AVMEDIA_TYPE_SUBTITLE: return sctx.index >= 0;
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

bool Decoder::eof(const AVMediaType type) const
{
    switch (type) {
    case AVMEDIA_TYPE_VIDEO:    return vctx.index < 0 || vctx.done;
    case AVMEDIA_TYPE_AUDIO:    return actx.index < 0 || actx.done;
    case AVMEDIA_TYPE_SUBTITLE: return sctx.index < 0 || sctx.done;
    default:                    return true;
    }
}

bool Decoder::eof() const { return eof(AVMEDIA_TYPE_AUDIO) && eof(AVMEDIA_TYPE_VIDEO); }

int Decoder::start()
{
    if (!ready_ || running_) {
        loge("[   DECODER] already running or not ready");
        return -1;
    }

    eof_      = false;
    vctx.done = false;
    actx.done = false;
    running_  = true;

    rthread_ = std::jthread([this] { readpkt_thread_fn(); });

    if (vctx.index >= 0) vctx.thread = std::jthread([this] { vdecode_thread_fn(); });
    if (actx.index >= 0) actx.thread = std::jthread([this] { adecode_thread_fn(); });
    if (sctx.index >= 0) sctx.thread = std::jthread([this] { sdecode_thread_fn(); });

    return 0;
}

void Decoder::seek(const std::chrono::nanoseconds& ts, const std::chrono::nanoseconds& rel)
{
    std::scoped_lock lock(seek_mtx_);

    if (seek_pts_ != AV_NOPTS_VALUE) return;

    seek_pts_ = std::clamp<int64_t>(ts.count() / 1000, fmt_ctx_->start_time, fmt_ctx_->duration);
    seek_min_ = rel > 0s ? seek_pts_ - rel.count() / 1000 + 2 : std::numeric_limits<int64_t>::min();
    seek_max_ = rel < 0s ? seek_pts_ - rel.count() / 1000 - 2 : std::numeric_limits<int64_t>::max();

    trim_pts_ = (seek_pts_ <= std::max<int64_t>(500, fmt_ctx_->start_time)) ? 0 : seek_pts_.load();
    logi("seek: {:.3%T} {:+}s ({:.3%T}, {:.3%T}), trim: {:.3%T}", std::chrono::microseconds{ seek_pts_ },
         av::clock::s(rel).count(), std::chrono::microseconds{ seek_min_ },
         std::chrono::microseconds{ seek_max_ }, std::chrono::microseconds{ trim_pts_ });

    vctx.queue.stop();
    actx.queue.stop();

    notenough_.notify_all();

    vctx.dirty = actx.dirty = true;
    vctx.synced = actx.synced = false;

    // restart if needed
    if (!running_ || eof_ || vctx.done || actx.done) {
        running_ = false;

        if (rthread_.joinable()) rthread_.join();
        if (vctx.thread.joinable()) vctx.thread.join();
        if (actx.thread.joinable()) actx.thread.join();
        if (sctx.thread.joinable()) sctx.thread.join();

        start();
    }
}

void Decoder::readpkt_thread_fn()
{
    probe::thread::set_name("DEC-READ");

    av::packet packet{};
    while (running_ && !eof_) {
        if (seek_pts_ != AV_NOPTS_VALUE) {
            std::scoped_lock lock(seek_mtx_);

            if (avformat_seek_file(fmt_ctx_, -1, seek_min_, seek_pts_, seek_max_, 0) < 0) {
                loge("failed to seek");
            }
            else {
                actx.next_pts = AV_NOPTS_VALUE;
                eof_          = false;
                vctx.done     = false;
                actx.done     = false;
                sctx.done     = false;
            }

            vctx.queue.start();
            actx.queue.start();
            sctx.queue.start();

            seek_pts_ = AV_NOPTS_VALUE;
        }

        // read
        const int ret = av_read_frame(fmt_ctx_, packet.put());
        if (ret < 0) {
            if (ret == AVERROR_EOF || avio_feof(fmt_ctx_->pb)) {
                eof_ = true;

                vctx.queue.wait_and_push(nullptr);
                actx.queue.wait_and_push(nullptr);
                sctx.queue.wait_and_push(nullptr);

                logi("DRAINING");
            }

            if (fmt_ctx_->pb && fmt_ctx_->pb->error) {
                loge("FAILED TO READ PACKET, ABORT");
                running_ = false;
            }

            continue;
        }

        std::unique_lock lock(notenough_mtx_);
        notenough_.wait(lock, [this] {
            return (vctx.index >= 0 && (vctx.queue.stopped() || vctx.queue.size() < MIN_FRAMES)) ||
                   (actx.index >= 0 && (actx.queue.stopped() || actx.queue.size() < MIN_FRAMES));
        });
        lock.unlock();

        if (packet->stream_index == vctx.index) vctx.queue.wait_and_push(packet);
        if (packet->stream_index == actx.index) actx.queue.wait_and_push(packet);
        if (packet->stream_index == sctx.index) sctx.queue.wait_and_push(packet);
    }

    logi("R-THREAD EXITED");
}

int Decoder::filter_frame(DecodingContext& ctx, const av::frame& frame, const AVMediaType type)
{
    // send the frame to graph
    if (av_buffersrc_add_frame_flags(ctx.src, frame.get(), AV_BUFFERSRC_FLAG_PUSH) < 0) {
        loge("[{}] failed to send the frame to filter graph.", av::to_char(type));
        running_ = false;
        return -1;
    }

    // output streams
    while (running_) {
        const int ret =
            av_buffersink_get_frame_flags(ctx.sink, ctx.frame.put(), AV_BUFFERSINK_FLAG_NO_REQUEST);
        if (ret == AVERROR(EAGAIN) || seek_pts_ != AV_NOPTS_VALUE) {
            return 0;
        }

        if (ret == AVERROR_EOF) {
            logi("[{}] FILTER EOF", av::to_char(type));

            ctx.done = true;
            onarrived(nullptr, type);
            return 0;
        }

        if (ret < 0) {
            logi("[{}] failed to get frame.", av::to_char(type));
            running_ = false;
            return ret;
        }

        onarrived(ctx.frame, type);
    }

    return 0;
}

void Decoder::vdecode_thread_fn()
{
    probe::thread::set_name("DEC-VIDEO");
    logi("STARTED");

    av::frame frame{};
    while (running_ && !vctx.done) {
        const auto& pkt = vctx.queue.wait_and_pop();
        notenough_.notify_all();
        if (!pkt.has_value()) continue;

        if (vctx.dirty) {
            avcodec_flush_buffers(vctx.codec);
            create_video_graph();
            vctx.dirty = false;
        }

        // video decoding
        auto ret = avcodec_send_packet(vctx.codec, pkt.value().get());
        while (ret >= 0) {
            ret = avcodec_receive_frame(vctx.codec, frame.put());
            if (ret < 0) {
                if (ret == AVERROR(EAGAIN) || seek_pts_ != AV_NOPTS_VALUE) break;

                if (ret == AVERROR_EOF) {
                    filter_frame(vctx, nullptr, AVMEDIA_TYPE_VIDEO);
                    logi("[V] DECODING EOF, SEND NULL");
                    break;
                }

                running_ = false;
                loge("[V] DECODING ERROR, ABORT");
                break;
            }

            frame->pts = frame->best_effort_timestamp;

            if (frame->pts != AV_NOPTS_VALUE) {
                const auto vpts = av::clock::us(frame->pts, vctx.stream->time_base).count();
                if (!vctx.synced && !vctx.dirty) {
                    vctx.synced = true;
                    trim_pts_   = std::max<int64_t>(trim_pts_, vpts);
                }

                if (vpts >= trim_pts_) {
                    logd("[V] pts = {:>14d} - {:.3%T}", frame->pts,
                         av::clock::ms(frame->pts, vctx.stream->time_base));
                    filter_frame(vctx, frame, AVMEDIA_TYPE_VIDEO);
                }
            }
        }
    }

    vctx.queue.wait_and_push(nullptr);
    logi("[V] SEND NULL, EXITED");
}

void Decoder::adecode_thread_fn()
{
    probe::thread::set_name("DEC-AUDIO");
    logd("STARTED");

    av::frame frame{};
    while (running_ && !actx.done) {
        const auto& pkt = actx.queue.wait_and_pop();
        notenough_.notify_all();
        if (!pkt.has_value()) continue;

        if (actx.dirty) {
            avcodec_flush_buffers(actx.codec);
            create_audio_graph();
            actx.dirty = false;
        }

        auto ret = avcodec_send_packet(actx.codec, pkt.value().get());
        while (ret >= 0) {
            ret = avcodec_receive_frame(actx.codec, frame.put());
            if (ret < 0) {
                if (ret == AVERROR(EAGAIN) || seek_pts_ != AV_NOPTS_VALUE) break;

                if (ret == AVERROR_EOF) {
                    filter_frame(actx, nullptr, AVMEDIA_TYPE_AUDIO);
                    logi("[A] DECODING EOF, SEND NULL");
                    break;
                }

                running_ = false;
                loge("[A] DECODING ERROR, ABORT");
                break;
            }

            if (frame->pts != AV_NOPTS_VALUE) {
                frame->pts = av_rescale_q(frame->pts, actx.stream->time_base, afi.time_base);
            }
            else if (actx.next_pts != AV_NOPTS_VALUE) {
                frame->pts = actx.next_pts;
            }

            if (frame->pts != AV_NOPTS_VALUE) {
                actx.next_pts = frame->pts + frame->nb_samples;

                const auto apts = av::clock::us(frame->pts, afi.time_base).count();
                if (!actx.synced && !actx.dirty) {
                    actx.synced = true;
                    trim_pts_   = std::max<int64_t>(trim_pts_, apts);
                }

                if (apts >= trim_pts_) {
                    logd("[A] pts = {:>14d} - {:.3%T}", frame->pts,
                         av::clock::ms(frame->pts, afi.time_base));

                    filter_frame(actx, frame, AVMEDIA_TYPE_AUDIO);
                }
            }
        }
    }

    actx.queue.wait_and_push(nullptr);
    logi("[A] SEND NULL, EXITED");
}

void Decoder::sdecode_thread_fn()
{
    probe::thread::set_name("DEC-SUBTITLE");
    logd("STARTED");

    AVSubtitle subtitle;
    while (running_ && !sctx.done) {
        const auto& pkt = sctx.queue.wait_and_pop();
        notenough_.notify_all();
        if (!pkt.has_value()) continue;

        int got = 0;
        if (avcodec_decode_subtitle2(sctx.codec, &subtitle, &got, pkt.value().get()) >= 0) {
            if (got) {
                logd("[S] start = {:%T}, end = {:%T}",
                     std::chrono::milliseconds{ subtitle.start_display_time },
                     std::chrono::milliseconds{ subtitle.end_display_time });

                const int64_t start_time = av_rescale_q(subtitle.pts, AV_TIME_BASE_Q, av_make_q(1, 1000));
                const int64_t duration   = subtitle.end_display_time;

                for (int i = 0; i < subtitle.num_rects; ++i) {
                    const auto line = subtitle.rects[i]->ass;
                    if (!line) break;
                    ass_process_chunk(ass_.track, line, strlen(line), start_time, duration);
                }

                //                ASS_Image *image = ass_render_frame(ass_.renderer, ass_.track,
                //                                                    time_ms, &detect_change);
                avsubtitle_free(&subtitle);
            }
        }
        else {
            loge("[S] error");
        }
    }
}

bool Decoder::seeking(const AVMediaType type) const
{
    return (seek_pts_ != AV_NOPTS_VALUE) || ((type == AVMEDIA_TYPE_VIDEO) ? vctx.dirty : actx.dirty);
}

void Decoder::stop()
{
    running_ = false;
    ready_   = false;

    vctx.queue.stop();
    actx.queue.stop();

    notenough_.notify_all();

    if (rthread_.joinable()) rthread_.join();
    if (vctx.thread.joinable()) vctx.thread.join();
    if (actx.thread.joinable()) actx.thread.join();

    logi("[    DECODER] STOPPED");
}

Decoder::~Decoder()
{
    stop();

    avcodec_free_context(&vctx.codec);
    avcodec_free_context(&actx.codec);
    avcodec_free_context(&sctx.codec);
    avformat_close_input(&fmt_ctx_);

    avfilter_graph_free(&vctx.graph);
    avfilter_graph_free(&actx.graph);

    if (ass_.track) ass_free_track(ass_.track);
    if (ass_.renderer) ass_renderer_done(ass_.renderer);
    if (ass_.library) ass_library_done(ass_.library);

    logi("[    DECODER] ~");
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