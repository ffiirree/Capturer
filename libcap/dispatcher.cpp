#include "libcap/dispatcher.h"

#include "libcap/clock.h"
#include "libcap/devices.h"
#include "libcap/filter.h"
#include "logging.h"

#include <fmt/chrono.h>
#include <probe/defer.h>

extern "C" {
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/time.h>
}

int Dispatcher::add_input(Producer<av::frame> *producer)
{
    if (!producer) return av::NULLPTR;
    if (!producer->is_realtime()) return av::INVALID;

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

void Dispatcher::set_hwaccel(const AVHWDeviceType hwaccel) { vctx_.hwaccel = hwaccel; }

int Dispatcher::initialize(const std::string_view& video_filters, const std::string_view& audio_filters)
{
    if (producers_.empty() || !consumer_) return av::INVALID;

    vctx_.graph_desc = video_filters;
    actx_.graph_desc = audio_filters;

    if (actx_.enabled && create_filter_graph(AVMEDIA_TYPE_AUDIO) < 0) return -1;
    if (vctx_.enabled && create_filter_graph(AVMEDIA_TYPE_VIDEO) < 0) return -1;

    consumer_->enable(AVMEDIA_TYPE_AUDIO, actx_.enabled);
    consumer_->enable(AVMEDIA_TYPE_VIDEO, vctx_.enabled);

    update_encoder_format_by_sinks();

    ready_ = true;
    return 0;
}

int Dispatcher::create_filter_graph(const AVMediaType type)
{
    auto& ctx = (type == AVMEDIA_TYPE_AUDIO) ? actx_ : vctx_;

    // 1. alloc filter graph
    if (ctx.graph) avfilter_graph_free(&ctx.graph);
    if (ctx.graph = avfilter_graph_alloc(); !ctx.graph) return av::NOMEM;

    // 2. create buffersrc
    std::vector<AVFilterContext *> src_ctxs{};
    for (auto& producer : producers_) {
        if (!producer->has(type)) continue;

        AVFilterContext *src_ctx = nullptr;
        if (type == AVMEDIA_TYPE_AUDIO) {
            if (av::graph::create_audio_src(actx_.graph, &src_ctx, producer->afmt) < 0) return -1;

            ctx.srcs[producer] = src_ctx;
            src_ctxs.push_back(src_ctx);
        }

        if (type == AVMEDIA_TYPE_VIDEO) {
            if (av::graph::create_video_src(vctx_.graph, &src_ctx, producer->vfmt) < 0) return -1;

            ctx.srcs[producer] = src_ctx;
            src_ctxs.push_back(src_ctx);
        }
    }

    // 3. create buffersink
    if (type == AVMEDIA_TYPE_AUDIO &&
        av::graph::create_audio_sink(actx_.graph, &actx_.sink, consumer_->afmt) < 0)
        return -1;
    if (type == AVMEDIA_TYPE_VIDEO &&
        av::graph::create_video_sink(vctx_.graph, &vctx_.sink, consumer_->vfmt) < 0)
        return -1;

    // 4.
    logi("[DISPATCHER] [{}] creating filter graph: '{}'", av::to_char(type), ctx.graph_desc);

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

        if (avfilter_graph_parse2(ctx.graph, ctx.graph_desc.c_str(), &inputs, &outputs) < 0)
            return av::INVALID;

        // link I/O filters
        int i = 0;
        for (auto ptr = inputs; ptr; ptr = ptr->next, i++) {
            if (avfilter_link(src_ctxs[i], 0, ptr->filter_ctx, ptr->pad_idx) < 0) {
                loge("[DISPATCHER] failed to link input filters");
                return -1;
            }
        }

        for (auto ptr = outputs; ptr; ptr = ptr->next) {
            if (avfilter_link(ptr->filter_ctx, ptr->pad_idx, ctx.sink, 0) < 0) {
                loge("[DISPATCHER] failed to link output filters");
                return -1;
            }
        }
    }

    if (type == AVMEDIA_TYPE_VIDEO && ctx.hwaccel != AV_HWDEVICE_TYPE_NONE) {
        if (av::hwaccel::setup_for_filter_graph(ctx.graph, ctx.hwaccel) != 0) {
            loge("[DISPATCHER] can not set hardware device up for filter graph.");
            return -1;
        }
    }

    // 5. configure
    if (avfilter_graph_config(ctx.graph, nullptr) < 0) {
        loge("[DISPATCHER] failed to configure the filter graph");
        return -1;
    }

    logi("[DISPATCHER] filter graph \n{}\n", avfilter_graph_dump(ctx.graph, nullptr));
    return 0;
}

int Dispatcher::update_encoder_format_by_sinks()
{
    if (consumer_ && consumer_->accepts(AVMEDIA_TYPE_VIDEO) && vctx_.sink) {
        consumer_->vfmt.pix_fmt     = static_cast<AVPixelFormat>(av_buffersink_get_format(vctx_.sink));
        consumer_->vfmt.color.space = av_buffersink_get_colorspace(vctx_.sink);
        consumer_->vfmt.color.range = av_buffersink_get_color_range(vctx_.sink);
        consumer_->vfmt.width       = av_buffersink_get_w(vctx_.sink);
        consumer_->vfmt.height      = av_buffersink_get_h(vctx_.sink);
        consumer_->vfmt.sample_aspect_ratio = av_buffersink_get_sample_aspect_ratio(vctx_.sink);
        consumer_->vfmt.time_base           = av_buffersink_get_time_base(vctx_.sink);
        consumer_->input_framerate          = av_buffersink_get_frame_rate(vctx_.sink);
    }

    if (consumer_ && consumer_->accepts(AVMEDIA_TYPE_AUDIO) && actx_.sink) {
        consumer_->afmt.sample_fmt = static_cast<AVSampleFormat>(av_buffersink_get_format(actx_.sink));
        AVChannelLayout ch_layout{};
        av_buffersink_get_ch_layout(actx_.sink, &ch_layout);
        consumer_->afmt.channels       = ch_layout.nb_channels;
        consumer_->afmt.channel_layout = ch_layout.u.mask;
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
    start_time_ = av::clock::ns();
    timeline_.set(0ns, start_time_);

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
                continue;
            }

            ctx.dirty = false;
        }

        auto has_next = ctx.queue.wait_and_pop();
        if (!has_next || timeline_.paused()) continue;

        frame         = has_next.value().first;
        auto producer = has_next.value().second;
        auto src      = ctx.srcs[producer];
        auto timebase = (mt == AVMEDIA_TYPE_AUDIO) ? producer->afmt.time_base : producer->vfmt.time_base;

        // pts
        if (frame && frame->pts != AV_NOPTS_VALUE)
            frame->pts -= av::clock::to(av::clock::us() - timeline_.time(), timebase);

        // send the frame to graph
        if (av_buffersrc_add_frame_flags(src, frame.get(), AV_BUFFERSRC_FLAG_PUSH) < 0) {
            loge("[{}] failed to send the frame to filter graph.", av::to_char(mt));
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
                logi("[{}] DISPATCH EOF", av::to_char(mt));

                consumer_->consume(nullptr, mt);
                break;
            }
            else if (ret < 0) {
                loge("[{}] failed to get frame: {}", av::to_char(mt), av::ff_errstr(ret));
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

void Dispatcher::pause() { timeline_.pause(); }

void Dispatcher::resume() { timeline_.resume(); }

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

    logi("[DISPATCHER] STOPPED");
}

Dispatcher::~Dispatcher()
{
    stop();

    avfilter_graph_free(&vctx_.graph);
    avfilter_graph_free(&actx_.graph);

    logi("[DISPATCHER] ~");
}

std::chrono::nanoseconds Dispatcher::escaped() const
{
    if (!running()) return 0ns;

    return timeline_.time();
}
