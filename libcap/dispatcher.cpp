#include "libcap/dispatcher.h"

#include "libcap/clock.h"
#include "libcap/devices.h"
#include "logging.h"

#include <fmt/chrono.h>
#include <fmt/format.h>
#include <probe/defer.h>

extern "C" {
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
}

int Dispatcher::create_video_sink(AVFilterContext **ctx, const av::vformat_t& args) const
{
    if (avfilter_graph_create_filter(ctx, avfilter_get_by_name("buffersink"), "video-sink", nullptr,
                                     nullptr, vctx_.graph) < 0) {
        LOG(ERROR) << "[DISPATCHER] [V] failed to create 'buffersink'.";
        return -1;
    }

    const AVPixelFormat pix_fmts[] = { args.pix_fmt, AV_PIX_FMT_NONE };
    if (av_opt_set_int_list(*ctx, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN) < 0) {
        LOG(ERROR) << "[DISPATCHER] [V] av_opt_set_int_list";
        return -1;
    }

    LOG(INFO) << fmt::format("[DISPATCHER] [V] buffersink: '{}'", av::to_string(args.pix_fmt));

    return 0;
}

int Dispatcher::create_audio_sink(AVFilterContext **ctx, const av::aformat_t& args) const
{
    if (avfilter_graph_create_filter(ctx, avfilter_get_by_name("abuffersink"), "audio-sink", nullptr,
                                     nullptr, actx_.graph) < 0) {
        LOG(ERROR) << "[DISPATCHER] [A] failed to create 'buffersink'.";
        return -1;
    }

    // sample_fmts (int list)
    if (const AVSampleFormat sink_fmts[] = { args.sample_fmt, AV_SAMPLE_FMT_NONE };
        av_opt_set_int_list(*ctx, "sample_fmts", sink_fmts, AV_SAMPLE_FMT_NONE, AV_OPT_SEARCH_CHILDREN) <
        0) {
        LOG(ERROR) << "[DISPATCHER] [A] faile to set 'sample_fmts' option.";
        return -1;
    }

    // channel_counts(int list)
    // all_channel_counts(bool)
    if (av_opt_set_int(*ctx, "all_channel_counts", 0, AV_OPT_SEARCH_CHILDREN) < 0) {
        LOG(ERROR) << "[DISPATCHER] [A] failed to set 'all_channel_counts' option.";
        return -1;
    }

    // channel_layouts(int64_t) / >= 5.1  ch_layouts(string)
#if LIBAVFILTER_VERSION_INT >= AV_VERSION_INT(8, 44, 100)
    const auto layout = av::channel_layout_name(args.channels, args.channel_layout);
    if (av_opt_set(*ctx, "ch_layouts", layout.c_str(), AV_OPT_SEARCH_CHILDREN) < 0) {
#else
    if (const int64_t channel_counts[] = { args.channel_layout ? -1 : args.channels, -1 };
        av_opt_set_int_list(*ctx, "channel_counts", channel_counts, -1, AV_OPT_SEARCH_CHILDREN) < 0) {
        LOG(ERROR) << "[DISPATCHER] [A] failed to set 'channel_counts' option.";
        return -1;
    }

    if (const int64_t channel_layouts[] = { static_cast<int64_t>(args.channel_layout), -1 };
        av_opt_set_int_list(*ctx, "channel_layouts", channel_layouts, -1, AV_OPT_SEARCH_CHILDREN) < 0) {
#endif
        LOG(ERROR) << "[DISPATCHER] [A] failed to set 'channel_layouts' option.";
        return -1;
    }

    // sample_rates(int list)
    if (const int64_t sample_rates[] = { args.sample_rate, -1 };
        av_opt_set_int_list(*ctx, "sample_rates", sample_rates, -1, AV_OPT_SEARCH_CHILDREN) < 0) {
        LOG(ERROR) << "[DISPATCHER] [A] failed to set 'sample_rates' option.";
        return -1;
    }

    LOG(INFO) << fmt::format("[DISPATCHER] [A] buffersink: '{}'", av::to_string(args));

    return 0;
}

int Dispatcher::add_input(Producer<av::frame> *producer)
{
    if (!producer) return -1;

    producers_.insert(producer);

    if (producer->has(AVMEDIA_TYPE_AUDIO)) actx_.enabled = true;
    if (producer->has(AVMEDIA_TYPE_VIDEO)) vctx_.enabled = true;

    producer->onarrived = [=, this](const av::frame& frame, auto type) {
        switch (type) {
        case AVMEDIA_TYPE_AUDIO: actx_.queue.wait_and_push({ frame, producer }); break;
        case AVMEDIA_TYPE_VIDEO: vctx_.queue.wait_and_push({ frame, producer }); break;
        default:                 break;
        }
    };

    return 0;
}

void Dispatcher::set_output(Consumer<av::frame> *encoder) { consumer_ = encoder; }

void Dispatcher::set_hwaccel(AVHWDeviceType hwaccel) { vctx_.hwaccel = hwaccel; }

int Dispatcher::initialize(const std::string_view& video_graph_desc,
                           const std::string_view& audio_graph_desc)
{
    if (producers_.empty() || !consumer_) {
        LOG(INFO) << "[DISPATCHER] no input or output.";
        return -1;
    }

    vctx_.graph_desc = video_graph_desc;
    actx_.graph_desc = audio_graph_desc;

    if (actx_.enabled && create_filter_graph(AVMEDIA_TYPE_AUDIO) < 0) return -1;
    if (vctx_.enabled && create_filter_graph(AVMEDIA_TYPE_VIDEO) < 0) return -1;

    consumer_->enable(AVMEDIA_TYPE_AUDIO, actx_.enabled);
    consumer_->enable(AVMEDIA_TYPE_VIDEO, vctx_.enabled);

    set_encoder_format_by_sinks();

    ready_ = true;
    return 0;
}

void Dispatcher::set_video_filters(std::string_view vf)
{
    vctx_.graph_desc = vf;
    vctx_.dirty      = true;
}

void Dispatcher::set_audio_filters(std::string_view af)
{
    actx_.graph_desc = af;
    actx_.dirty      = true;
}

int Dispatcher::create_filter_graph(AVMediaType type)
{
    auto& ctx = (type == AVMEDIA_TYPE_AUDIO) ? actx_ : vctx_;

    // 1. alloc filter graph
    if (ctx.graph) avfilter_graph_free(&ctx.graph);
    if (ctx.graph = avfilter_graph_alloc(); !ctx.graph) {
        loge("[DISPATCHER] [{}] failed to create filter graph.", av::to_char(type));
        return -1;
    }

    // 2. create buffersrc
    std::vector<AVFilterContext *> src_ctxs{};
    for (auto& producer : producers_) {
        if (!producer->has(type)) continue;

        AVFilterContext *src_ctx = nullptr;
        const auto       args =
            (type == AVMEDIA_TYPE_AUDIO) ? to_string(producer->afmt) : to_string(producer->vfmt);

        if (type == AVMEDIA_TYPE_AUDIO) {
            if (avfilter_graph_create_filter(&src_ctx, avfilter_get_by_name("abuffer"), "audio-source",
                                             args.c_str(), nullptr, ctx.graph) < 0) {
                loge("[DISPATCHER] [A] failed to create 'abuffer'.");
                return -1;
            }
            ctx.srcs[producer] = src_ctx;
            src_ctxs.push_back(src_ctx);
        }

        if (type == AVMEDIA_TYPE_VIDEO) {
            if (avfilter_graph_create_filter(&src_ctx, avfilter_get_by_name("buffer"), "video-source",
                                             args.c_str(), nullptr, ctx.graph) < 0) {
                loge("[DISPATCHER] [V] failed to create 'buffer'.");
                return -1;
            }
            ctx.srcs[producer] = src_ctx;
            src_ctxs.push_back(src_ctx);
        }

        LOG(INFO) << fmt::format("[DISPATCHER] [{}] buffersrc : '{}'", av::to_char(type), args);
    }

    // 3. create buffersink
    if (type == AVMEDIA_TYPE_AUDIO && create_audio_sink(&actx_.sink, consumer_->afmt) < 0) return -1;
    if (type == AVMEDIA_TYPE_VIDEO && create_video_sink(&vctx_.sink, consumer_->vfmt) < 0) return -1;

    // 4.
    LOG(INFO) << fmt::format("[DISPATCHER] [{}] creating filter graph: '{}'", av::to_char(type),
                             ctx.graph_desc);

    if (ctx.graph_desc.empty()) {
        // 1 input & 1 output
        if (avfilter_link(src_ctxs[0], 0, ctx.sink, 0) < 0) {
            loge("[DISPATCHER] [{}] failed to link filter graph", av::to_char(type));
            return -1;
        }
    }
    else {
        AVFilterInOut *inputs = nullptr, *outputs = nullptr;
        defer(avfilter_inout_free(&inputs); avfilter_inout_free(&outputs));

        if (avfilter_graph_parse2(ctx.graph, ctx.graph_desc.c_str(), &inputs, &outputs) < 0) {
            LOG(ERROR) << "[DISPATCHER] failed to parse : '" << ctx.graph_desc << "'";
            return -1;
        }

        // link I/O filters
        int i = 0;
        for (auto ptr = inputs; ptr; ptr = ptr->next, i++) {
            if (avfilter_link(src_ctxs[i], 0, ptr->filter_ctx, ptr->pad_idx) < 0) {
                LOG(ERROR) << "[DISPATCHER] failed to link input filters";
                return -1;
            }
        }

        for (auto ptr = outputs; ptr; ptr = ptr->next) {
            if (avfilter_link(ptr->filter_ctx, ptr->pad_idx, ctx.sink, 0) < 0) {
                LOG(ERROR) << "[DISPATCHER] failed to link output filters";
                return -1;
            }
        }
    }

    if (type == AVMEDIA_TYPE_VIDEO && ctx.hwaccel != AV_HWDEVICE_TYPE_NONE) {
        if (av::hwaccel::setup_for_filter_graph(ctx.graph, ctx.hwaccel) != 0) {
            LOG(ERROR) << "[DISPATCHER]  can not set hareware device up for filter graph.";
            return -1;
        }
    }

    // 5. configure
    if (avfilter_graph_config(ctx.graph, nullptr) < 0) {
        LOG(ERROR) << "[DISPATCHER] failed to configure the filter graph";
        return -1;
    }

    LOG(INFO) << "[DISPATCHER] filter graph @{\n" << avfilter_graph_dump(ctx.graph, nullptr);
    LOG(INFO) << "[DISPATCHER] @}";

    return 0;
}

int Dispatcher::set_encoder_format_by_sinks()
{
    if (consumer_ && consumer_->accepts(AVMEDIA_TYPE_VIDEO) && vctx_.sink) {
        consumer_->vfmt.pix_fmt = static_cast<AVPixelFormat>(av_buffersink_get_format(vctx_.sink));
        consumer_->vfmt.width   = av_buffersink_get_w(vctx_.sink);
        consumer_->vfmt.height  = av_buffersink_get_h(vctx_.sink);
        consumer_->vfmt.sample_aspect_ratio = av_buffersink_get_sample_aspect_ratio(vctx_.sink);
        consumer_->vfmt.time_base           = av_buffersink_get_time_base(vctx_.sink);
        consumer_->input_framerate          = av_buffersink_get_frame_rate(vctx_.sink);
    }

    if (consumer_ && consumer_->accepts(AVMEDIA_TYPE_AUDIO) && actx_.sink) {
        consumer_->afmt.sample_fmt     = static_cast<AVSampleFormat>(av_buffersink_get_format(actx_.sink));
        consumer_->afmt.channels       = av_buffersink_get_channels(actx_.sink);
        consumer_->afmt.channel_layout = av_buffersink_get_channel_layout(actx_.sink);
        consumer_->afmt.sample_rate    = av_buffersink_get_sample_rate(actx_.sink);
        consumer_->afmt.time_base      = av_buffersink_get_time_base(actx_.sink);
    }
    return 0;
}

int Dispatcher::start()
{
    for (auto& producer : producers_) {
        if (producer->start() < 0) {
            return -1;
        }
    }

    if (consumer_->start() < 0) return -1;

    //
    start_time_ = resumed_pts_ = av::clock::ns();

    if (vctx_.enabled) {
        vctx_.running = true;
        vctx_.thread  = std::jthread([this] { dispatch_fn(AVMEDIA_TYPE_VIDEO); });
    }

    if (actx_.enabled) {
        actx_.running = true;
        actx_.thread  = std::jthread([this] { dispatch_fn(AVMEDIA_TYPE_AUDIO); });
    }

    return 0;
}

bool Dispatcher::is_valid_pts(const std::chrono::nanoseconds& pts)
{
    std::lock_guard lock(pause_mtx_);

    // [-, max(start_time_, resumed_pts_))
    if (pts < resumed_pts_) {
        return false;
    }

    // pausing:
    //     [paused_pts_, +)
    if (paused_pts_ != av::clock::nopts) {
        return pts <= paused_pts_;
    }

    return true;
}

int Dispatcher::dispatch_fn(const AVMediaType mt)
{
    probe::thread::set_name(fmt::format("DISPATCH-{}", av::to_char(mt)));

    logi("[{}] STARTED", av::to_char(mt));
    defer(logi("[{}] EXITED", av::to_char(mt)));

    auto& ctx = (mt == AVMEDIA_TYPE_AUDIO) ? actx_ : vctx_;

    av::frame frame{};
    while (ctx.running) {
        if (ctx.dirty) {
            if (create_filter_graph(mt) < 0) {
                ctx.running = false;
                return -1;
            }

            ctx.dirty = false;
        }

        auto has_next = ctx.queue.wait_and_pop();
        if (!has_next) continue;

        frame         = has_next.value().first;
        auto producer = has_next.value().second;
        auto src      = ctx.srcs[producer];
        auto tb       = (mt == AVMEDIA_TYPE_AUDIO) ? producer->afmt.time_base : producer->vfmt.time_base;

        // pts
        if (frame) {
            if (producer->is_realtime()) {
                if (!is_valid_pts(av::clock::ns(frame->pts, tb))) {
                    logw("[{}] drop {:.6%T} !in [{:.6%T}, {:.6%T})", av::to_char(mt),
                         av::clock::ns(frame->pts, tb), resumed_pts_, paused_pts_);
                    continue;
                }
                frame->pts -= av_rescale_q(start_time_.count(), OS_TIME_BASE_Q, tb);
            }

            frame->pts -= av_rescale_q(paused_time().count(), OS_TIME_BASE_Q, tb);
        }

        // send the frame to graph
        if (av_buffersrc_add_frame_flags(src, frame.get(), AV_BUFFERSRC_FLAG_PUSH) < 0) {
            LOG(ERROR) << fmt::format("[{}] failed to send the frame to filter graph.", av::to_char(mt));
            ctx.running = false;
            ctx.queue.stop();
            break;
        }

        // output streams
        while (ctx.running) {
            const int ret =
                av_buffersink_get_frame_flags(ctx.sink, frame.put(), AV_BUFFERSINK_FLAG_NO_REQUEST);
            if (ret == AVERROR(EAGAIN)) {
                break;
            }
            else if (ret == AVERROR_EOF) {
                LOG(INFO) << fmt::format("[{}] DISPATCH EOF", av::to_char(mt));

                consumer_->consume(nullptr, mt);
                break;
            }
            else if (ret < 0) {
                LOG(ERROR) << fmt::format("[{}] failed to get frame.", av::to_char(mt));
                ctx.running = false;
                ctx.queue.stop();
                break;
            }

            consumer_->consume(frame, mt);
        }
    }

    consumer_->consume(nullptr, mt);

    return 0;
}

void Dispatcher::pause()
{
    std::lock_guard lock(pause_mtx_);

    if (paused_pts_ == av::clock::nopts) {
        paused_pts_ = av::clock::ns();
    }
}

void Dispatcher::resume()
{
    std::lock_guard lock(pause_mtx_);

    if (paused_pts_ != av::clock::nopts) {
        const auto now  = av::clock::ns();
        paused_time_   += (now - paused_pts_);
        paused_pts_     = av::clock::nopts;
        resumed_pts_    = now;
    }
}

std::chrono::nanoseconds Dispatcher::paused_time()
{
    std::lock_guard lock(pause_mtx_);

    if (paused_pts_ != av::clock::nopts) {
        return paused_time_ + (av::clock::ns() - paused_pts_);
    }
    return paused_time_;
}

void Dispatcher::stop()
{
    ready_ = false;

    // must be called before calling producer->stop()
    actx_.queue.stop();
    vctx_.queue.stop();

    // producers
    for (auto& producer : producers_) {
        producer->stop();
    }

    // consumer
    if (consumer_) consumer_->stop();

    // dispatcher
    vctx_.running = false;
    actx_.running = false;

    if (vctx_.thread.joinable()) vctx_.thread.join();
    if (actx_.thread.joinable()) actx_.thread.join();

    LOG(INFO) << fmt::format("[DISPATCHER] STOPPED");
}

Dispatcher::~Dispatcher()
{
    stop();

    // release fitergraphs
    avfilter_graph_free(&vctx_.graph);
    avfilter_graph_free(&actx_.graph);

    LOG(INFO) << fmt::format("[DISPATCHER] ~");
}

std::chrono::nanoseconds Dispatcher::escaped()
{
    if (!running()) {
        return 0ns;
    }
    return std::max(0ns, av::clock::ns() - start_time_ - paused_time());
}
