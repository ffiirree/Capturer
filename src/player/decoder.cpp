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
#include <libavutil/imgutils.h>
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

    logf_if(level == 0, "[  LIBASS-L{}] {}", level, buffer);
    loge_if(level == 1, "[  LIBASS-L{}] {}", level, buffer);
    logw_if(level == 2 || level == 3, "[  LIBASS-L{}] {}", level, buffer);
    logd_if(level > 3 && level < 7, "[  LIBASS-L{}] {}", level, buffer);
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

    if (open_video_stream(-1) < 0) return -1;
    if (open_audio_stream(-1) < 0) return -1;
    if (open_subtitle_stream(-1) < 0 || ass_init() < 0) return -1;
    if (vctx_.index < 0 && actx_.index < 0) {
        loge("[    DECODER] not found any stream");
        return -1;
    }

    if (sctx_.index >= 0) {
        switch (sctx_.codec->codec_id) {
        case AV_CODEC_ID_DVD_SUBTITLE:
        case AV_CODEC_ID_DVB_SUBTITLE:
        case AV_CODEC_ID_TEXT:
        case AV_CODEC_ID_XSUB:
        case AV_CODEC_ID_SSA:
        case AV_CODEC_ID_MOV_TEXT:
        case AV_CODEC_ID_HDMV_PGS_SUBTITLE:
        case AV_CODEC_ID_DVB_TELETEXT:
        case AV_CODEC_ID_SRT:
        case AV_CODEC_ID_MICRODVD:
        case AV_CODEC_ID_EIA_608:
        case AV_CODEC_ID_JACOSUB:
        case AV_CODEC_ID_SAMI:
        case AV_CODEC_ID_REALTEXT:
        case AV_CODEC_ID_STL:
        case AV_CODEC_ID_SUBVIEWER1:
        case AV_CODEC_ID_SUBVIEWER:
        case AV_CODEC_ID_SUBRIP:
        case AV_CODEC_ID_WEBVTT:
        case AV_CODEC_ID_MPL2:
        case AV_CODEC_ID_VPLAYER:
        case AV_CODEC_ID_PJS:
        case AV_CODEC_ID_ASS:
        case AV_CODEC_ID_HDMV_TEXT_SUBTITLE:
        case AV_CODEC_ID_TTML:
        case AV_CODEC_ID_ARIB_CAPTION:
        default:                             break;
        }
    }

    ready_ = true;

    logi("[    DECODER] [{}] is opened", name);
    return 0;
}

int Decoder::open_video_stream(int index)
{
    vctx_.index = av_find_best_stream(fmt_ctx_, AVMEDIA_TYPE_VIDEO, index, -1, nullptr, 0);

    if (vctx_.index >= 0) {
        vctx_.stream = fmt_ctx_->streams[vctx_.index];
        auto decoder = choose_decoder(vctx_.stream, vfi.hwaccel);
        if (vctx_.codec = avcodec_alloc_context3(decoder); !vctx_.codec) return -1;
        if (avcodec_parameters_to_context(vctx_.codec, vctx_.stream->codecpar) < 0) return -1;

        if (vfi.hwaccel != AV_HWDEVICE_TYPE_NONE) {
            const auto hwctx           = av::hwaccel::get_context(vfi.hwaccel);
            vctx_.codec->opaque        = &vfi;
            vctx_.codec->hw_device_ctx = av_buffer_ref(hwctx->device_ctx.get());
            vctx_.codec->get_format    = get_hw_format;
            if (!vctx_.codec->hw_device_ctx) {
                loge("[    DECODER] [V] can not create hardware device");
            }
        }

        // open codec
        AVDictionary *decoder_options = nullptr;
        defer(av_dict_free(&decoder_options));
        av_dict_set(&decoder_options, "threads", "auto", 0);
        if (avcodec_open2(vctx_.codec, decoder, &decoder_options) < 0) {
            loge("[    DECODER] [V] can not open the video decoder");
            return -1;
        }

        vfi = {
            .width     = vctx_.codec->width,
            .height    = vctx_.codec->height,
            .pix_fmt   = vfi.hwaccel ? vfi.pix_fmt : vctx_.codec->pix_fmt,
            .framerate = av_guess_frame_rate(fmt_ctx_, vctx_.stream, nullptr),
            .sample_aspect_ratio = vctx_.codec->sample_aspect_ratio,
            .time_base           = vctx_.stream->time_base,
            .color               = {
                              .space      = vctx_.codec->colorspace,
                              .range      = vctx_.codec->color_range,
                              .primaries  = vctx_.codec->color_primaries,
                              .transfer   = vctx_.codec->color_trc,
            },
            .hwaccel         = vfi.hwaccel,
            .sw_pix_fmt      = vctx_.codec->pix_fmt,
        };

        logi("[    DECODER] [V] [{:>6}] {}({})", decoder->name, av::to_string(vfi),
             av::to_string(vfi.color));
    }

    return 0;
}

int Decoder::open_audio_stream(int index)
{
    actx_.index = av_find_best_stream(fmt_ctx_, AVMEDIA_TYPE_AUDIO, index, -1, nullptr, 0);

    if (actx_.index >= 0) {
        actx_.stream = fmt_ctx_->streams[actx_.index];
        auto decoder = avcodec_find_decoder(actx_.stream->codecpar->codec_id);
        if (actx_.codec = avcodec_alloc_context3(decoder); !actx_.codec) return -1;
        if (avcodec_parameters_to_context(actx_.codec, actx_.stream->codecpar) < 0) return -1;
        if (avcodec_open2(actx_.codec, decoder, nullptr) < 0) {
            loge("[    DECODER] [A] can not open the audio decoder");
            return -1;
        }

        afi = {
            .sample_rate    = actx_.codec->sample_rate,
            .sample_fmt     = actx_.codec->sample_fmt,
            .channels       = actx_.codec->ch_layout.nb_channels,
            .channel_layout = actx_.codec->ch_layout.u.mask,
            .time_base      = { 1, actx_.codec->sample_rate },
        };

        actx_.next_pts = av_rescale_q(actx_.stream->start_time, actx_.stream->time_base, afi.time_base);

        logi("[    DECODER] [A] [{:>6}] {}", decoder->name, av::to_string(afi));
    }

    return 0;
}

int Decoder::open_subtitle_stream(int index)
{
    sctx_.index = av_find_best_stream(fmt_ctx_, AVMEDIA_TYPE_SUBTITLE, index, -1, nullptr, 0);

    if (sctx_.index >= 0) {
        sctx_.stream = fmt_ctx_->streams[sctx_.index];
        auto decoder = avcodec_find_decoder(sctx_.stream->codecpar->codec_id);
        if (sctx_.codec = avcodec_alloc_context3(decoder); !sctx_.codec) return -1;
        if (avcodec_parameters_to_context(sctx_.codec, sctx_.stream->codecpar) < 0) return -1;

        sctx_.codec->pkt_timebase = sctx_.stream->time_base;

        if (avcodec_open2(sctx_.codec, decoder, nullptr) < 0) {
            loge("[    DECODER] [A] can not open the subtitle decoder");
            return -1;
        }

        logi("[    DECODER] [S] [{:>6}] start_time={}", decoder->name, sctx_.stream->start_time);
    }

    return 0;
}

int Decoder::ass_init()
{
    ass_library_ = ass_library_init();
    if (!ass_library_) {
        loge("[    DECODER] [S] failed to init libass");
        return -1;
    }
    ass_set_message_cb(ass_library_, ass_log, nullptr);

    ass_set_fonts_dir(ass_library_, nullptr);
    ass_set_extract_fonts(ass_library_, 1);

    ass_renderer_ = ass_renderer_init(ass_library_);
    if (!ass_renderer_) {
        loge("[    DECODER] [S] could not create libass renderer");
        return AVERROR(EINVAL);
    }

    ass_set_frame_size(ass_renderer_, vfi.width, vfi.height);
    ass_set_storage_size(ass_renderer_, vfi.width, vfi.height);

    // load attached fonts
    for (size_t i = 0; i < fmt_ctx_->nb_streams; ++i) {
        const auto stream = fmt_ctx_->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_ATTACHMENT && attachment_is_font(stream)) {
            if (const auto tag = av_dict_get(stream->metadata, "filename", nullptr, AV_DICT_MATCH_CASE);
                tag) {
                logd("[    DECODER] [S] loading attached font: {}", tag->value);
                ass_add_font(ass_library_, tag->value,
                             reinterpret_cast<const char *>(stream->codecpar->extradata),
                             stream->codecpar->extradata_size);
            }
            else {
                logw("[    DECODER] [S] font attachment has no filename, ignored");
            }
        }
    }

    ass_set_fonts(ass_renderer_, nullptr, nullptr, ASS_FONTPROVIDER_AUTODETECT, nullptr, 1);

    return 0;
}

int Decoder::ass_open_internal()
{
    if (sctx_.index < 0) return 0;

    if (ass_track_) ass_free_track(ass_track_);

    ass_track_ = ass_new_track(ass_library_);
    if (!ass_track_) {
        loge("[    DECODER] [S] could not create libass track");
        return AVERROR(EINVAL);
    }
    if (sctx_.codec->subtitle_header) {
        ass_process_codec_private(ass_track_, reinterpret_cast<char *>(sctx_.codec->subtitle_header),
                                  sctx_.codec->subtitle_header_size);
    }

    return 0;
}

int Decoder::create_video_graph()
{
    if (vctx_.graph) avfilter_graph_free(&vctx_.graph);
    if (vctx_.graph = avfilter_graph_alloc(); !vctx_.graph) return av::NOMEM;

    if (av::graph::create_video_src(vctx_.graph, &vctx_.src, vfi) < 0) return -1;
    if (av::graph::create_video_sink(vctx_.graph, &vctx_.sink, vfo) < 0) return -1;

    if (avfilter_link(vctx_.src, 0, vctx_.sink, 0) < 0) {
        loge("[    DECODER] [V] failed to link filter graph");
        return -1;
    }

    if (avfilter_graph_config(vctx_.graph, nullptr) < 0) {
        loge("[    DECODER] failed to configure the filter graph");
        return -1;
    }

    logi("[    DECODER] filter graph \n{}\n", avfilter_graph_dump(vctx_.graph, nullptr));

    return 0;
}

int Decoder::create_audio_graph()
{
    if (actx_.graph) avfilter_graph_free(&actx_.graph);
    if (actx_.graph = avfilter_graph_alloc(); !actx_.graph) return av::NOMEM;

    if (av::graph::create_audio_src(actx_.graph, &actx_.src, afi) < 0) return -1;
    if (av::graph::create_audio_sink(actx_.graph, &actx_.sink, afo) < 0) return -1;

    if (avfilter_link(actx_.src, 0, actx_.sink, 0) < 0) {
        loge("[    DECODER] [A] failed to link filter graph");
        return -1;
    }

    if (avfilter_graph_config(actx_.graph, nullptr) < 0) {
        loge("[    DECODER] [A] failed to configure the filter graph");
        return -1;
    }

    return 0;
}

bool Decoder::has(const AVMediaType type) const
{
    switch (type) {
    case AVMEDIA_TYPE_VIDEO:    return vctx_.index >= 0;
    case AVMEDIA_TYPE_AUDIO:    return actx_.index >= 0;
    case AVMEDIA_TYPE_SUBTITLE: return sctx_.index >= 0;
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
    case AVMEDIA_TYPE_VIDEO:    return vctx_.index < 0 || vctx_.done;
    case AVMEDIA_TYPE_AUDIO:    return actx_.index < 0 || actx_.done;
    case AVMEDIA_TYPE_SUBTITLE: return sctx_.index < 0 || sctx_.done;
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

    eof_       = false;
    vctx_.done = false;
    actx_.done = false;
    sctx_.done = false;
    running_   = true;

    rthread_ = std::jthread([this] { readpkt_thread_fn(); });

    if (vctx_.index >= 0) vctx_.thread = std::jthread([this] { vdecode_thread_fn(); });
    if (actx_.index >= 0) actx_.thread = std::jthread([this] { adecode_thread_fn(); });
    if (sctx_.index >= 0) sctx_.thread = std::jthread([this] { sdecode_thread_fn(); });

    return 0;
}

int Decoder::select(AVMediaType type, int index)
{
    std::scoped_lock lock(selected_mtx_);

    if (selected_type_ != AVMEDIA_TYPE_UNKNOWN) return 0;

    selected_type_  = type;
    selected_index_ = index;

    switch (type) {
    case AVMEDIA_TYPE_VIDEO:
        vctx_.queue.stop();
        vctx_.dirty = true;
        return 0;

    case AVMEDIA_TYPE_AUDIO:
        actx_.queue.stop();
        actx_.dirty = true;
        return 0;

    case AVMEDIA_TYPE_SUBTITLE:
        sctx_.queue.stop();
        sctx_.dirty = true;
        return 0;

    default:
        selected_type_  = AVMEDIA_TYPE_UNKNOWN;
        selected_index_ = -1;
        return -1;
    }
}

int Decoder::ass_open_external(const std::string& filename)
{
    std::scoped_lock lock(ass_mtx_);

    if (sctx_.index >= 0) {
        sctx_.queue.stop();
        sctx_.done = true;
        if (sctx_.thread.joinable()) sctx_.thread.join();
        sctx_.index = -1;
        avcodec_free_context(&sctx_.codec);
    }

    if (ass_track_) ass_free_track(ass_track_);

    ass_track_ = ass_read_file(ass_library_, const_cast<char *>(filename.c_str()), nullptr);
    if (!ass_track_) {
        loge("[     LIBASS] failed to open file: {}", filename);
        return -1;
    }

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

    vctx_.queue.stop();
    actx_.queue.stop();
    sctx_.queue.stop();

    {
        std::scoped_lock sublock(sub_mtx_);
        subtitles_changed_ = true;
        subtitles_.clear();
    }

    notenough_.notify_all();

    vctx_.dirty = true;
    actx_.dirty = true;
    sctx_.dirty = true;

    vctx_.synced = actx_.synced = false;

    // restart if needed
    if (!running_ || eof_ || vctx_.done || actx_.done) {
        running_ = false;

        if (rthread_.joinable()) rthread_.join();
        if (vctx_.thread.joinable()) vctx_.thread.join();
        if (actx_.thread.joinable()) actx_.thread.join();
        if (sctx_.thread.joinable()) sctx_.thread.join();

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
                actx_.next_pts = AV_NOPTS_VALUE;
                eof_           = false;
                vctx_.done     = false;
                actx_.done     = false;
                sctx_.done     = false;
            }

            vctx_.queue.start();
            actx_.queue.start();
            sctx_.queue.start();

            seek_pts_ = AV_NOPTS_VALUE;
        }

        if (selected_type_ != AVMEDIA_TYPE_UNKNOWN) {

            switch (selected_type_) {
            case AVMEDIA_TYPE_SUBTITLE: {
                std::scoped_lock lock(selected_mtx_, ass_mtx_);

                avcodec_free_context(&sctx_.codec);

                if (open_subtitle_stream(selected_index_) < 0 || ass_open_internal() < 0) {
                    loge("failed to switch subtitle stream to {}", selected_index_);
                }

                break;
            }
            default: break;
            }

            sctx_.queue.start();

            selected_type_  = AVMEDIA_TYPE_UNKNOWN;
            selected_index_ = -1;
        }

        // read
        const int ret = av_read_frame(fmt_ctx_, packet.put());
        if (ret < 0) {
            if (ret == AVERROR_EOF || avio_feof(fmt_ctx_->pb)) {
                eof_ = true;

                vctx_.queue.wait_and_push(nullptr);
                actx_.queue.wait_and_push(nullptr);

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
            return (vctx_.index >= 0 && (vctx_.queue.stopped() || vctx_.queue.size() < MIN_FRAMES)) ||
                   (actx_.index >= 0 && (actx_.queue.stopped() || actx_.queue.size() < MIN_FRAMES));
        });
        lock.unlock();

        if (packet->stream_index == vctx_.index) vctx_.queue.wait_and_push(packet);
        if (packet->stream_index == actx_.index) actx_.queue.wait_and_push(packet);
        if (packet->stream_index == sctx_.index) sctx_.queue.wait_and_push(packet);
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
    while (running_ && !vctx_.done) {
        const auto& pkt = vctx_.queue.wait_and_pop();
        notenough_.notify_all();
        if (!pkt.has_value()) continue;

        if (vctx_.dirty) {
            avcodec_flush_buffers(vctx_.codec);
            create_video_graph();
            vctx_.dirty = false;
        }

        // video decoding
        auto ret = avcodec_send_packet(vctx_.codec, pkt.value().get());
        while (ret >= 0) {
            ret = avcodec_receive_frame(vctx_.codec, frame.put());
            if (ret < 0) {
                if (ret == AVERROR(EAGAIN) || seek_pts_ != AV_NOPTS_VALUE) break;

                if (ret == AVERROR_EOF) {
                    filter_frame(vctx_, nullptr, AVMEDIA_TYPE_VIDEO);
                    logi("[V] DECODING EOF, SEND NULL");
                    break;
                }

                running_ = false;
                loge("[V] DECODING ERROR, ABORT");
                break;
            }

            frame->pts = frame->best_effort_timestamp;

            if (frame->pts != AV_NOPTS_VALUE) {
                const auto vpts = av::clock::us(frame->pts, vctx_.stream->time_base).count();
                if (!vctx_.synced && !vctx_.dirty) {
                    vctx_.synced = true;
                    trim_pts_    = std::max<int64_t>(trim_pts_, vpts);
                }

                if (vpts >= trim_pts_) {
                    filter_frame(vctx_, frame, AVMEDIA_TYPE_VIDEO);
                }
            }
        }
    }

    vctx_.queue.wait_and_push(nullptr);
    logi("[V] SEND NULL, EXITED");
}

void Decoder::adecode_thread_fn()
{
    probe::thread::set_name("DEC-AUDIO");
    logd("STARTED");

    av::frame frame{};
    while (running_ && !actx_.done) {
        const auto& pkt = actx_.queue.wait_and_pop();
        notenough_.notify_all();
        if (!pkt.has_value()) continue;

        if (actx_.dirty) {
            avcodec_flush_buffers(actx_.codec);
            create_audio_graph();
            actx_.dirty = false;
        }

        auto ret = avcodec_send_packet(actx_.codec, pkt.value().get());
        while (ret >= 0) {
            ret = avcodec_receive_frame(actx_.codec, frame.put());
            if (ret < 0) {
                if (ret == AVERROR(EAGAIN) || seek_pts_ != AV_NOPTS_VALUE) break;

                if (ret == AVERROR_EOF) {
                    filter_frame(actx_, nullptr, AVMEDIA_TYPE_AUDIO);
                    logi("[A] DECODING EOF, SEND NULL");
                    break;
                }

                running_ = false;
                loge("[A] DECODING ERROR, ABORT");
                break;
            }

            if (frame->pts != AV_NOPTS_VALUE) {
                frame->pts = av_rescale_q(frame->pts, actx_.stream->time_base, afi.time_base);
            }
            else if (actx_.next_pts != AV_NOPTS_VALUE) {
                frame->pts = actx_.next_pts;
            }

            if (frame->pts != AV_NOPTS_VALUE) {
                actx_.next_pts = frame->pts + frame->nb_samples;

                const auto apts = av::clock::us(frame->pts, afi.time_base).count();
                if (!actx_.synced && !actx_.dirty) {
                    actx_.synced = true;
                    trim_pts_    = std::max<int64_t>(trim_pts_, apts);
                }

                if (apts >= trim_pts_) {
                    filter_frame(actx_, frame, AVMEDIA_TYPE_AUDIO);
                }
            }
        }
    }

    actx_.queue.wait_and_push(nullptr);
    logi("[A] SEND NULL, EXITED");
}

void Decoder::sdecode_thread_fn()
{
    probe::thread::set_name("DEC-SUBTITLE");

    while (running_ && !sctx_.done) {
        const auto& pkt = sctx_.queue.wait_and_pop();
        if (!pkt.has_value()) continue;

        std::shared_lock lock(ass_mtx_);

        if (sctx_.dirty) {
            avcodec_flush_buffers(sctx_.codec);
            if (ass_track_) ass_flush_events(ass_track_);
            sctx_.dirty = false;
        }

        AVSubtitle subtitle{};
        int        got = 0;
        if (avcodec_decode_subtitle2(sctx_.codec, &subtitle, &got, pkt.value().get()) >= 0) {
            if (got) {
                const auto    pts      = subtitle.pts / 1000;
                const int64_t duration = subtitle.end_display_time;

                if (subtitle.format == 0) {
                    subtitles_.clear();
                    subtitles_changed_ = true;
                }

                for (size_t i = 0; i < subtitle.num_rects; ++i) {
                    const auto rect = subtitle.rects[i];
                    if (rect->type == SUBTITLE_ASS && rect->ass != nullptr && ass_track_) {
                        const auto length = static_cast<int>(strlen(rect->ass));
                        ass_process_chunk(ass_track_, rect->ass, length, pts, duration);
                    }
                    else if (rect->type == SUBTITLE_BITMAP && rect->data[0]) {
                        std::scoped_lock sublock(sub_mtx_);
                        const uint32_t  *colors = reinterpret_cast<uint32_t *>(rect->data[1]);
                        logd("graphics +{} {:.3%T}+{:%S}", subtitle.num_rects,
                             std::chrono::milliseconds{ pts }, std::chrono::milliseconds{ duration });

                        Subtitle sub{
                            .x      = { rect->x, vfi.width },
                            .y      = { rect->y - std::max(0, rect->y + rect->h - vfi.height), vfi.height },
                            .w      = { rect->w, vfi.width },
                            .h      = { rect->h, vfi.height },
                            .stride = rect->w * 4,
                            .pts    = std::chrono::milliseconds{ pts },
                            .duration = std::chrono::milliseconds{ duration },
                            .format   = AV_PIX_FMT_ARGB,
                            .buffer   = std::make_shared<uint8_t[]>(rect->w * rect->h * 4),
                        };

                        const auto dst = reinterpret_cast<uint32_t *>(sub.buffer.get());
                        for (int h = 0; h < rect->h; ++h) {
                            const auto ptr = rect->data[0] + h * rect->linesize[0];
                            for (int c = 0; c < rect->w; ++c) {
                                dst[h * rect->w + c] = colors[ptr[c]];
                            }
                        }
                        subtitles_.push_back(sub);
                    }
                }
            }
        }
        else {
            loge("[S] error");
        }
        avsubtitle_free(&subtitle);
    }
}

#define GET_R(C) static_cast<uint8_t>(((C) >> 24))
#define GET_G(C) static_cast<uint8_t>(((C) >> 16) & 0x00ff)
#define GET_B(C) static_cast<uint8_t>(((C) >> 8) & 0x00ff)
#define GET_A(C) static_cast<uint8_t>((C) & 0x00ff)

std::pair<int, std::list<Subtitle>> Decoder::subtitle(const std::chrono::milliseconds& now)
{
    std::scoped_lock lock(ass_mtx_, sub_mtx_);

    int changed = subtitles_changed_;

    subtitles_.remove_if([&](auto& sub) {
        const auto ex = sub.pts + sub.duration < now;
        if (ex) changed = 2;
        return ex;
    });

    subtitles_changed_ = 0;

    if (!ass_renderer_ || !ass_track_) return { changed, subtitles_ };

    ASS_Image *image = ass_render_frame(ass_renderer_, ass_track_, now.count(), &changed);

    if (changed) {
        subtitles_.clear();

        for (; image; image = image->next) {

            const size_t bytes = image->stride * (image->h - 1) + image->w;

            Subtitle subtitle{
                    .x      = {image->dst_x, vfi.width},
                    .y      = {image->dst_y, vfi.height},
                    .w      = {image->w, vfi.width},
                    .h      = {image->h, vfi.height},
                    .stride = image->stride,
                    .pts    = now,
                    .color  = {
                        GET_R(image->color),
                        GET_G(image->color),
                        GET_B(image->color),
                        GET_A(image->color),
                    },
                    .buffer  = std::make_shared<uint8_t[]>(bytes),
                };

            std::memcpy(subtitle.buffer.get(), image->bitmap, bytes);

            subtitles_.push_back(subtitle);
        }
    }

    logd_if(changed || image, "changed {}, @{}", changed, (void *)image);

    return { changed, subtitles_ };
}

void Decoder::set_ass_render_size(int w, int h)
{
    std::shared_lock lock(ass_mtx_);

    if (!ass_renderer_ || !w || !h) return;

    ass_set_frame_size(ass_renderer_, w, h);
    ass_set_storage_size(ass_renderer_, w, h);
}

bool Decoder::seeking(const AVMediaType type) const
{
    return (seek_pts_ != AV_NOPTS_VALUE) || ((type == AVMEDIA_TYPE_VIDEO) ? vctx_.dirty : actx_.dirty);
}

void Decoder::stop()
{
    running_ = false;
    ready_   = false;

    vctx_.queue.stop();
    actx_.queue.stop();
    sctx_.queue.stop();

    notenough_.notify_all();

    if (rthread_.joinable()) rthread_.join();
    if (vctx_.thread.joinable()) vctx_.thread.join();
    if (actx_.thread.joinable()) actx_.thread.join();
    if (sctx_.thread.joinable()) sctx_.thread.join();

    logi("[    DECODER] STOPPED");
}

Decoder::~Decoder()
{
    stop();

    avcodec_free_context(&vctx_.codec);
    avcodec_free_context(&actx_.codec);
    avcodec_free_context(&sctx_.codec);
    avformat_close_input(&fmt_ctx_);

    avfilter_graph_free(&vctx_.graph);
    avfilter_graph_free(&actx_.graph);

    if (ass_track_) ass_free_track(ass_track_);
    if (ass_renderer_) ass_renderer_done(ass_renderer_);
    if (ass_library_) ass_library_done(ass_library_);

    logi("[    DECODER] ~");
}

std::vector<AVStream *> Decoder::streams(AVMediaType type)
{
    std::vector<AVStream *> list{};
    for (size_t i = 0; i < fmt_ctx_->nb_streams; ++i) {
        if (fmt_ctx_->streams[i]->codecpar->codec_type == type) {
            list.push_back(fmt_ctx_->streams[i]);
        }
    }
    return list;
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
            map["Channels"] = std::to_string(params->ch_layout.nb_channels);
            map["Channel Layout"] = av::to_string(params->ch_layout);
            map["Sample Rate"]    = fmt::format("{} Hz", params->sample_rate);
            break;
        case AVMEDIA_TYPE_SUBTITLE:
        default:                    break;
        }

        properties.push_back(map);
    }

    return properties;
}