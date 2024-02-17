#include "libcap/dispatcher.h"

#include "libcap/clock.h"
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

int Dispatcher::create_video_sink(const Consumer<av::frame> *encoder, AVFilterContext **ctx)
{
    if (avfilter_graph_create_filter(ctx, avfilter_get_by_name("buffersink"), "video-sink", nullptr,
                                     nullptr, vgraph_) < 0) {
        LOG(ERROR) << "[DISPATCHER] [V] failed to create 'buffersink'.";
        return -1;
    }

    vfmt.pix_fmt = (vfmt.pix_fmt == AV_PIX_FMT_NONE) ? encoder->vfmt.pix_fmt : vfmt.pix_fmt;
    if (vfmt.pix_fmt != AV_PIX_FMT_NONE) {
        const AVPixelFormat pix_fmts[] = { vfmt.pix_fmt, AV_PIX_FMT_NONE };

        if (av_opt_set_int_list(*ctx, "pix_fmts", pix_fmts, static_cast<uint64_t>(AV_PIX_FMT_NONE),
                                AV_OPT_SEARCH_CHILDREN) < 0) {
            LOG(ERROR) << "[DISPATCHER] [V] av_opt_set_int_list";
            return -1;
        }
    }
    LOG(INFO) << fmt::format("[DISPATCHER] [V] created vbuffersink: 'pix_fmt={}'",
                             av::to_string(vfmt.pix_fmt));

    return 0;
}

int Dispatcher::create_audio_sink(const Consumer<av::frame> *encoder, AVFilterContext **ctx)
{
    if (avfilter_graph_create_filter(ctx, avfilter_get_by_name("abuffersink"), "audio-sink", nullptr,
                                     nullptr, agraph_) < 0) {
        LOG(ERROR) << "[DISPATCHER] [A] failed to create 'abuffersink'.";
        return -1;
    }

    afmt.sample_fmt = (afmt.sample_fmt == AV_SAMPLE_FMT_NONE) ? encoder->afmt.sample_fmt : afmt.sample_fmt;
    afmt.channels   = (afmt.channels == 0) ? encoder->afmt.channels : afmt.channels;
    afmt.channel_layout = (afmt.channel_layout == 0) ? encoder->afmt.channel_layout : afmt.channel_layout;
    afmt.sample_rate    = (afmt.sample_rate == 0) ? encoder->afmt.sample_rate : afmt.sample_rate;

    if (afmt.sample_fmt != AV_SAMPLE_FMT_NONE) {
        // sample_fmts (int list)
        // sample_rates(int list)
        // channel_layouts(int64_t) / >= 5.1  ch_layouts(string)
        // channel_counts(int list)
        // all_channel_counts(bool)
        if (const AVSampleFormat sink_fmts[] = { afmt.sample_fmt, AV_SAMPLE_FMT_NONE };
            av_opt_set_int_list(*ctx, "sample_fmts", sink_fmts, static_cast<uint64_t>(AV_SAMPLE_FMT_NONE),
                                AV_OPT_SEARCH_CHILDREN) < 0) {
            LOG(ERROR) << "[DISPATCHER] [A] faile to set 'sample_fmts' option.";
            return -1;
        }

        if (av_opt_set_int(*ctx, "all_channel_counts", 0, AV_OPT_SEARCH_CHILDREN) < 0) {
            LOG(ERROR) << "[DISPATCHER] [A] failed to set 'all_channel_counts' option.";
            return -1;
        }

#if LIBAVFILTER_VERSION_INT >= AV_VERSION_INT(8, 44, 100)
        if (av_opt_set(*ctx, "ch_layouts",
                       av::channel_layout_name(afmt.channels, afmt.channel_layout).c_str(),
                       AV_OPT_SEARCH_CHILDREN) < 0) {
#else
        if (const int64_t channel_counts[] = { afmt.channel_layout ? -1 : afmt.channels, -1 };
            av_opt_set_int_list(*ctx, "channel_counts", channel_counts, -1, AV_OPT_SEARCH_CHILDREN) < 0) {
            LOG(ERROR) << "[DISPATCHER] [A] failed to set 'channel_counts' option.";
            return -1;
        }

        if (const int64_t channel_layouts[] = { static_cast<int64_t>(afmt.channel_layout), -1 };
            av_opt_set_int_list(*ctx, "channel_layouts", channel_layouts, -1, AV_OPT_SEARCH_CHILDREN) < 0) {
#endif
            LOG(ERROR) << "[DISPATCHER] [A] failed to set 'channel_layouts' option.";
            return -1;
        }

        if (const int64_t sample_rates[] = { afmt.sample_rate, -1 };
            av_opt_set_int_list(*ctx, "sample_rates", sample_rates, -1, AV_OPT_SEARCH_CHILDREN) < 0) {
            LOG(ERROR) << "[DISPATCHER] [A] failed to set 'sample_rates' option.";
            return -1;
        }
    }

    LOG(INFO) << fmt::format("[DISPATCHER] [A] created abuffersink: '{}'", av::to_string(afmt));

    return 0;
}

void Dispatcher::append(Producer<av::frame> *decoder)
{
    if (decoder && decoder->has(AVMEDIA_TYPE_AUDIO)) {
        producers_.insert(decoder);

        aproducer_ctxs_.push_back({ nullptr, decoder });
        audio_enabled_ = true;
    }

    if (decoder && decoder->has(AVMEDIA_TYPE_VIDEO)) {
        producers_.insert(decoder);

        vproducer_ctxs_.push_back({ nullptr, decoder });
        video_enabled_ = true;
    }
}

void Dispatcher::set_encoder(Consumer<av::frame> *encoder) { consumer_ctx_.consumer = encoder; }

int Dispatcher::initialize(const std::string_view& video_graph_desc,
                           const std::string_view& audio_graph_desc)
{
    if (producers_.empty()) {
        LOG(INFO) << "[DISPATCHER] no input.";
        return -1;
    }

    if (consumer_ctx_.consumer == nullptr) {
        LOG(INFO) << "[DISPATCHER] no output.";
        return -1;
    }

    vfilters_ = video_graph_desc;
    afilters_ = audio_graph_desc;

    // audio filter graph
    if (audio_enabled_) {
        if (afilters_.empty() && aproducer_ctxs_.size() > 1) {
            // TODO: the amix may not be closed with duration=longest
            afilters_ = fmt::format("amix=inputs={}:duration=first", aproducer_ctxs_.size());
        }

        consumer_ctx_.consumer->enable(AVMEDIA_TYPE_AUDIO);

        if (create_filter_graph(AVMEDIA_TYPE_AUDIO) < 0) {
            LOG(ERROR) << "[DISPATCHER] failed to create graph for processing audio";
            return -1;
        }
    }

    // video filter graph
    if (video_enabled_) {
        if (vfilters_.empty() && vproducer_ctxs_.size() > 1) {
            LOG(ERROR) << "[DISPATCHER] no more than 1 video stream for simple filter graph";
            return -1;
        }

        consumer_ctx_.consumer->enable(AVMEDIA_TYPE_VIDEO);

        if (create_filter_graph(AVMEDIA_TYPE_VIDEO) < 0) {
            LOG(ERROR) << "[DISPATCHER] failed to create graph for processing video";
            return -1;
        }
    }

    set_encoder_format_by_sinks();

    ready_ = true;
    return 0;
}

void Dispatcher::set_video_filters(std::string_view vf)
{
    vfilters_ = vf;
    vf_dirty_ = true;
}

void Dispatcher::set_audio_filters(std::string_view af)
{
    afilters_ = af;
    af_dirty_ = true;
}

int Dispatcher::send_command(AVMediaType mt, const std::string& target, const std::string& cmd,
                             const std::string& arg)
{
    LOG(INFO) << fmt::format("[DISPATCHER] [{}] {}: {}={}", av::to_char(mt), target, cmd, arg);

    // clang-format off
    switch (mt) {
    case AVMEDIA_TYPE_AUDIO: return avfilter_graph_send_command(agraph_, target.c_str(), cmd.c_str(), arg.c_str(), nullptr, 0, 0);
    case AVMEDIA_TYPE_VIDEO: return avfilter_graph_send_command(vgraph_, target.c_str(), cmd.c_str(), arg.c_str(), nullptr, 0, 0);
    default: return -1;
    }
    // clang-format on
}

int Dispatcher::create_filter_graph(AVMediaType type)
{
    if (type != AVMEDIA_TYPE_AUDIO && type != AVMEDIA_TYPE_VIDEO) return -1;

    auto& graph         = (type == AVMEDIA_TYPE_AUDIO) ? agraph_ : vgraph_;
    auto& filters       = (type == AVMEDIA_TYPE_AUDIO) ? afilters_ : vfilters_;
    auto& producer_ctxs = (type == AVMEDIA_TYPE_AUDIO) ? aproducer_ctxs_ : vproducer_ctxs_;
    auto& sink          = (type == AVMEDIA_TYPE_AUDIO) ? consumer_ctx_.asink_ctx : consumer_ctx_.vsink_ctx;

    // 1. alloc filter graph
    if (graph) avfilter_graph_free(&graph);
    if (graph = avfilter_graph_alloc(); !graph) {
        LOG(ERROR) << fmt::format("[DISPATCHER] [{}] failed to create filter graph.", av::to_char(type));
        return -1;
    }

    // 2. create buffersrc
    for (auto& [ctx, producer] : producer_ctxs) {
        const auto args = producer->format_str(type);
        switch (type) {
        case AVMEDIA_TYPE_AUDIO:
            if (avfilter_graph_create_filter(&ctx, avfilter_get_by_name("abuffer"), "audio-source",
                                             args.c_str(), nullptr, agraph_) < 0) {
                LOG(ERROR) << "[DISPATCHER] [A] failed to create 'abuffer'.";
                return -1;
            }
            break;

        default:
            if (avfilter_graph_create_filter(&ctx, avfilter_get_by_name("buffer"), "video-source",
                                             args.c_str(), nullptr, vgraph_) < 0) {
                LOG(ERROR) << "[DISPATCHER] [V] failed to create 'buffer'.";
                return -1;
            }
            break;
        }

        LOG(INFO) << fmt::format("[DISPATCHER] [{}] created buffer: {}", av::to_char(type), args);

        producer->enable(type);
    }

    // 3. create buffersink
    switch (type) {
    case AVMEDIA_TYPE_AUDIO:
        if (create_audio_sink(consumer_ctx_.consumer, &sink) < 0) return -1;
        break;
    default:
        if (create_video_sink(consumer_ctx_.consumer, &sink) < 0) return -1;
        break;
    }

    // 4.
    LOG(INFO) << fmt::format("[DISPATCHER] [{}] creating filter graph: '{}'", av::to_char(type), filters);

    if (filters.empty()) {
        // 1 input & 1 output
        if (avfilter_link(producer_ctxs[0].src_ctx, 0, sink, 0) < 0) {
            LOG(ERROR) << fmt::format("[DISPATCHER] [{}] failed to link filter graph", av::to_char(type));
            return -1;
        }
    }
    else {
        AVFilterInOut *inputs = nullptr, *outputs = nullptr;
        defer(avfilter_inout_free(&inputs); avfilter_inout_free(&outputs));

        if (avfilter_graph_parse2(graph, filters.c_str(), &inputs, &outputs) < 0) {
            LOG(ERROR) << "[DISPATCHER] failed to parse : '" << filters << "'";
            return -1;
        }

        // link I/O filters
        int i = 0;
        for (auto ptr = inputs; ptr; ptr = ptr->next, i++) {
            if (avfilter_link(producer_ctxs[i].src_ctx, 0, ptr->filter_ctx, ptr->pad_idx) < 0) {
                LOG(ERROR) << "[DISPATCHER] failed to link input filters";
                return -1;
            }
        }

        for (auto ptr = outputs; ptr; ptr = ptr->next) {
            if (avfilter_link(ptr->filter_ctx, ptr->pad_idx, sink, 0) < 0) {
                LOG(ERROR) << "[DISPATCHER] failed to link output filters";
                return -1;
            }
        }
    }

    if (type == AVMEDIA_TYPE_VIDEO && vfmt.hwaccel != AV_HWDEVICE_TYPE_NONE) {
        if (av::hwaccel::setup_for_filter_graph(graph, vfmt.hwaccel) != 0) {
            LOG(ERROR) << "[DISPATCHER]  can not set hareware device up for filter graph.";
            return -1;
        }
    }

    // 5. configure
    if (avfilter_graph_config(graph, nullptr) < 0) {
        LOG(ERROR) << "[DISPATCHER] failed to configure the filter graph";
        return -1;
    }

    LOG(INFO) << "[DISPATCHER] filter graph @{\n" << avfilter_graph_dump(graph, nullptr);
    LOG(INFO) << "[DISPATCHER] @}";

    return 0;
}

int Dispatcher::set_encoder_format_by_sinks()
{
    if (!consumer_ctx_.consumer && !consumer_ctx_.vsink_ctx && !consumer_ctx_.asink_ctx) {
        LOG(ERROR) << "no encoder was found";
        return -1;
    }

    const auto consumer = consumer_ctx_.consumer;
    if (consumer && consumer->accepts(AVMEDIA_TYPE_VIDEO) && (consumer_ctx_.vsink_ctx != nullptr)) {
        consumer->vfmt.pix_fmt =
            static_cast<AVPixelFormat>(av_buffersink_get_format(consumer_ctx_.vsink_ctx));
        consumer->vfmt.width               = av_buffersink_get_w(consumer_ctx_.vsink_ctx);
        consumer->vfmt.height              = av_buffersink_get_h(consumer_ctx_.vsink_ctx);
        consumer->vfmt.sample_aspect_ratio = av_buffersink_get_sample_aspect_ratio(consumer_ctx_.vsink_ctx);
        consumer->vfmt.time_base           = av_buffersink_get_time_base(consumer_ctx_.vsink_ctx);
        consumer->vfmt.framerate           = av_buffersink_get_frame_rate(consumer_ctx_.vsink_ctx);
        consumer->sink_framerate           = av_buffersink_get_frame_rate(consumer_ctx_.vsink_ctx);
    }

    if (consumer && consumer->accepts(AVMEDIA_TYPE_AUDIO) && (consumer_ctx_.asink_ctx != nullptr)) {
        consumer->afmt.sample_fmt =
            static_cast<AVSampleFormat>(av_buffersink_get_format(consumer_ctx_.asink_ctx));
        consumer->afmt.channels       = av_buffersink_get_channels(consumer_ctx_.asink_ctx);
        consumer->afmt.channel_layout = av_buffersink_get_channel_layout(consumer_ctx_.asink_ctx);
        consumer->afmt.sample_rate    = av_buffersink_get_sample_rate(consumer_ctx_.asink_ctx);
        consumer->afmt.time_base      = av_buffersink_get_time_base(consumer_ctx_.asink_ctx);
    }

    return 0;
}

int Dispatcher::start()
{
    for (auto& producer : producers_) {
        if (!producer->ready()) {
            LOG(ERROR) << "[DISPATCHER] [V] producer is not ready!";
            return -1;
        }
        producer->run();
    }

    if (!consumer_ctx_.consumer->ready()) {
        LOG(ERROR) << "[DISPATCHER] consumer is not ready!";
        return -1;
    }
    consumer_ctx_.consumer->run();

    //
    start_time_ = resumed_pts_ = av::clock::ns();

    vrunning_ = true;
    if (video_enabled_) {
        video_thread_ = std::jthread([this] {
            probe::thread::set_name("DISPATCH-V");
            dispatch_fn(AVMEDIA_TYPE_VIDEO);
        });
    }

    arunning_ = true;
    if (audio_enabled_) {
        audio_thread_ = std::jthread([this] {
            probe::thread::set_name("DISPATCH-A");
            dispatch_fn(AVMEDIA_TYPE_AUDIO);
        });
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

int Dispatcher::dispatch_fn(AVMediaType mt)
{
    LOG(INFO) << fmt::format("[{}] STARTED", av::to_char(mt));
    defer(LOG(INFO) << fmt::format("[{}] EXITED", av::to_char(mt)));

    av::frame frame{};

    auto& ctxs         = (mt == AVMEDIA_TYPE_VIDEO) ? vproducer_ctxs_ : aproducer_ctxs_;
    auto& sink         = (mt == AVMEDIA_TYPE_VIDEO) ? consumer_ctx_.vsink_ctx : consumer_ctx_.asink_ctx;
    auto& seeking      = (mt == AVMEDIA_TYPE_VIDEO) ? vseeking_ : aseeking_;
    auto& running      = (mt == AVMEDIA_TYPE_VIDEO) ? vrunning_ : arunning_;
    auto& dirty        = (mt == AVMEDIA_TYPE_VIDEO) ? vf_dirty_ : af_dirty_;
    auto  consumer_eof = false;

    while (running && !consumer_eof) {
        if (seeking || dirty) {
            if (create_filter_graph(mt) < 0) {
                LOG(ERROR) << "[DISPATCHER] failed to reconfigure graph for " << av::to_string(mt);
                running = false;
                return -1;
            }
            seeking = false;
            dirty   = false;
        }

        bool sleepy  = true;
        bool all_eof = true;
        // input streams
        for (auto& [src_ctx, producer] : ctxs) {
            if ((producer->eof() && producer->empty(mt)) || !running) continue;

            all_eof = false;

            if (producer->eof() && producer->empty(mt)) {
                if (av_buffersrc_add_frame_flags(src_ctx, nullptr, AV_BUFFERSRC_FLAG_PUSH) < 0) {
                    LOG(ERROR) << "failed to send NULL to filter graph";
                }
                LOG(INFO) << fmt::format("[{}] send NULL", av::to_char(mt));
                sleepy = false;
                continue;
            }

            // process next frame
            if (producer->produce(frame, mt) != 0) continue;

            sleepy              = false;
            const auto frame_tb = producer->time_base(mt);

            // pts
            if (clock() == av::clock_t::system) {
                if (frame->data[0] && !is_valid_pts(av::clock::ns(frame->pts, frame_tb))) {
                    LOG(WARNING) << fmt::format("[{}] drop frame {} -- [-, {}) || [{}, +)", av::to_char(mt),
                                                av::clock::ns(frame->pts, frame_tb),
                                                std::max(start_time_, resumed_pts_), paused_pts_);
                    continue;
                }
                frame->pts -= av_rescale_q(start_time_.count(), OS_TIME_BASE_Q, frame_tb);
            }
            frame->pts -= av_rescale_q(paused_time().count(), OS_TIME_BASE_Q, frame_tb);

            // send the frame to graph
            if (av_buffersrc_add_frame_flags(src_ctx, frame.get(), AV_BUFFERSRC_FLAG_PUSH) < 0) {
                LOG(ERROR) << fmt::format("[{}] failed to send the frame to filter graph.",
                                          av::to_char(mt));
                running = false;
                break;
            }
        }

        // no new frames
        if (sleepy && !draining_ && !seeking && !all_eof) {
            std::this_thread::sleep_for(10ms);
            sleepy = false;
            continue;
        }

        // output streams
        while (!consumer_eof && !seeking) {
            const int ret = av_buffersink_get_frame_flags(sink, frame.put(), AV_BUFFERSINK_FLAG_NO_REQUEST);
            if (ret == AVERROR_EOF || (ret == AVERROR(EAGAIN) && all_eof)) {
                LOG(INFO) << fmt::format("[{}] EOF", av::to_char(mt));
                consumer_eof = true;
                // through and send null packet
                frame.unref();
            }
            else if (ret == AVERROR(EAGAIN)) {
                break;
            }
            else if (ret < 0) {
                LOG(ERROR) << fmt::format("[{}] failed to get frame.", av::to_char(mt));
                running = false;
                break;
            }

            while (consumer_ctx_.consumer->full(mt) && running && !seeking) {
                std::this_thread::sleep_for(10ms);
            }

            if (!seeking) consumer_ctx_.consumer->consume(frame, mt);
        }
    }

    running = false;

    return 0;
}

void Dispatcher::seek(const std::chrono::nanoseconds& ts, std::chrono::nanoseconds lts,
                      std::chrono::nanoseconds rts)
{
    for (auto& producer : producers_) {
        producer->seek(ts, lts, rts);
    }

    vseeking_ = true;
    aseeking_ = true;

    if (!vrunning_ || !arunning_) {
        vrunning_ = false;
        arunning_ = false;

        if (video_thread_.joinable()) video_thread_.join();

        if (audio_thread_.joinable()) audio_thread_.join();

        start();
    }
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

int Dispatcher::reset()
{
    ready_ = false;

    // producers
    for (auto& [ctx, coder] : vproducer_ctxs_) {
        coder->reset();

        // push nullptr frame to close the video buffersrc
        if (vrunning_ && av_buffersrc_add_frame_flags(ctx, nullptr, AV_BUFFERSRC_FLAG_PUSH) < 0) {
            LOG(ERROR) << "[DISPATCHER] failed to close video buffersrc";
        }
    }

    for (auto& [ctx, coder] : aproducer_ctxs_) {
        coder->reset();

        // push nullptr frame to close the auido buffersrc
        if (arunning_ && av_buffersrc_add_frame_flags(ctx, nullptr, AV_BUFFERSRC_FLAG_PUSH) < 0) {
            LOG(ERROR) << "[DISPATCHER] failed to close audio buffersrc";
        }
    }

    draining_ = true;

    // consumer
    if (consumer_ctx_.consumer) {
        // wait 0~3s
        for (int i = 0; i < 100; i++) {
            if (!consumer_ctx_.consumer->ready() || consumer_ctx_.consumer->eof()) {
                LOG(INFO) << "[DISPATCHER] encoder exit with EOF";
                break;
            }

            std::this_thread::sleep_for(30ms);
        }
        consumer_ctx_.consumer->reset();
    }

    // dispatcher
    vrunning_ = false;
    arunning_ = false;
    draining_ = false;

    if (video_thread_.joinable()) video_thread_.join();

    if (audio_thread_.joinable()) audio_thread_.join();

    // clear
    aproducer_ctxs_.clear();
    vproducer_ctxs_.clear();
    consumer_ctx_ = {};

    vfilters_ = {};
    afilters_ = {};

    audio_enabled_ = false;
    video_enabled_ = false;

    vfmt = {};
    afmt = {};

    avfilter_graph_free(&vgraph_);
    avfilter_graph_free(&agraph_);

    paused_pts_  = av::clock::nopts;
    paused_time_ = 0ns;
    start_time_  = av::clock::nopts;
    resumed_pts_ = av::clock::nopts;

    LOG(INFO) << "[DISPATCHER] RESET";
    return 0;
}

std::chrono::nanoseconds Dispatcher::escaped()
{
    if (!running()) {
        return 0ns;
    }
    return std::max(0ns, av::clock::ns() - start_time_ - paused_time());
}
