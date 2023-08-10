#include "libcap/dispatcher.h"

#include "fmt/format.h"
#include "libcap/clock.h"
#include "logging.h"
#include "probe/defer.h"

extern "C" {
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/time.h>
}

int Dispatcher::create_src_filter(const Producer<AVFrame> *decoder, AVFilterContext **ctx,
                                  enum AVMediaType type)
{
    switch (type) {
    case AVMEDIA_TYPE_VIDEO: return create_video_src(decoder, ctx);
    case AVMEDIA_TYPE_AUDIO: return create_audio_src(decoder, ctx);
    default: return -1;
    }
}

int Dispatcher::create_sink_filter(const Consumer<AVFrame> *encoder, AVFilterContext **ctx,
                                   enum AVMediaType type)
{
    switch (type) {
    case AVMEDIA_TYPE_VIDEO: return create_video_sink(encoder, ctx);
    case AVMEDIA_TYPE_AUDIO: return create_audio_sink(encoder, ctx);
    default: return -1;
    }
}

int Dispatcher::create_video_src(const Producer<AVFrame> *decoder, AVFilterContext **ctx)
{
    if (!decoder->ready()) {
        LOG(ERROR) << "[DISPATCHER] [V] decoder is not ready";
        return -1;
    }

    auto buffer_args = decoder->format_str(AVMEDIA_TYPE_VIDEO);
    if (avfilter_graph_create_filter(ctx, avfilter_get_by_name("buffer"), "video-source",
                                     buffer_args.c_str(), nullptr, vgraph_) < 0) {
        LOG(ERROR) << "[DISPATCHER] [V] failed to create 'buffer'.";
        return -1;
    }

    LOG(INFO) << "[DISPATCHER] [V] created vbuffer: '" << buffer_args << "'";

    return 0;
}

int Dispatcher::create_video_sink(const Consumer<AVFrame> *encoder, AVFilterContext **ctx)
{
    if (avfilter_graph_create_filter(ctx, avfilter_get_by_name("buffersink"), "video-sink", nullptr,
                                     nullptr, vgraph_) < 0) {
        LOG(ERROR) << "[DISPATCHER] [V] failed to create 'buffersink'.";
        return -1;
    }

    vfmt.pix_fmt = (vfmt.pix_fmt == AV_PIX_FMT_NONE) ? encoder->vfmt.pix_fmt : vfmt.pix_fmt;
    if (vfmt.pix_fmt != AV_PIX_FMT_NONE) {
        enum AVPixelFormat pix_fmts[] = { vfmt.pix_fmt, AV_PIX_FMT_NONE };

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

int Dispatcher::create_audio_src(const Producer<AVFrame> *decoder, AVFilterContext **ctx)
{
    if (!decoder->ready()) {
        LOG(ERROR) << "[DISPATCHER] [A] decoder is not ready.";
        return -1;
    }

    auto buffer_args = decoder->format_str(AVMEDIA_TYPE_AUDIO);
    if (avfilter_graph_create_filter(ctx, avfilter_get_by_name("abuffer"), "audio-source",
                                     buffer_args.c_str(), nullptr, agraph_) < 0) {
        LOG(ERROR) << "[DISPATCHER] [A] failed to create 'abuffer'.";
        return -1;
    }

    LOG(INFO) << "[DISPATCHER] [A] created abuffer: '" << buffer_args << "'";

    return 0;
}

int Dispatcher::create_audio_sink(const Consumer<AVFrame> *encoder, AVFilterContext **ctx)
{
    if (avfilter_graph_create_filter(ctx, avfilter_get_by_name("abuffersink"), "audio-sink", nullptr,
                                     nullptr, agraph_) < 0) {
        LOG(ERROR) << "[DISPATCHER] [A] failed to create 'abuffersink'.";
        return -1;
    }

    afmt.sample_fmt = (afmt.sample_fmt == AV_SAMPLE_FMT_NONE) ? encoder->afmt.sample_fmt : afmt.sample_fmt;
    afmt.channels   = (afmt.channels == 0) ? encoder->afmt.channels : afmt.channels;
    afmt.channel_layout = (afmt.channel_layout == 0) ? encoder->afmt.channel_layout : afmt.channel_layout;
    afmt.sample_rate    = (afmt.sample_rate == -1) ? encoder->afmt.sample_rate : afmt.sample_rate;

    if (afmt.sample_fmt != AV_SAMPLE_FMT_NONE) {
        // sample_fmts (int list)
        // sample_rates(int list)
        // channel_layouts(int64_t) / >= 5.1  ch_layouts(string)
        // channel_counts(int list)
        // all_channel_counts(bool)
        enum AVSampleFormat sink_fmts[] = { afmt.sample_fmt, AV_SAMPLE_FMT_NONE };
        if (av_opt_set_int_list(*ctx, "sample_fmts", sink_fmts, static_cast<uint64_t>(AV_SAMPLE_FMT_NONE),
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
        int64_t channel_counts[] = { afmt.channel_layout ? -1 : afmt.channels, -1 };
        if (av_opt_set_int_list(*ctx, "channel_counts", channel_counts, -1, AV_OPT_SEARCH_CHILDREN) < 0) {
            LOG(ERROR) << "[DISPATCHER] [A] failed to set 'channel_counts' option.";
            return -1;
        }

        int64_t channel_layouts[] = { static_cast<int64_t>(afmt.channel_layout), -1 };
        if (av_opt_set_int_list(*ctx, "channel_layouts", channel_layouts, -1, AV_OPT_SEARCH_CHILDREN) < 0) {
#endif
            LOG(ERROR) << "[DISPATCHER] [A] failed to set 'channel_layouts' option.";
            return -1;
        }

        int64_t sample_rates[] = { afmt.sample_rate, -1 };
        if (av_opt_set_int_list(*ctx, "sample_rates", sample_rates, -1, AV_OPT_SEARCH_CHILDREN) < 0) {
            LOG(ERROR) << "[DISPATCHER] [A] failed to set 'sample_rates' option.";
            return -1;
        }
    }

    LOG(INFO) << fmt::format("[DISPATCHER] [A] created abuffersink: '{}'", av::to_string(afmt));

    return 0;
}

int Dispatcher::initialize(const std::string_view& video_graph_desc,
                           const std::string_view& audio_graph_desc)
{
    if (vproducer_ctxs_.empty() && aproducer_ctxs_.empty()) {
        LOG(INFO) << "[DISPATCHER] no input.";
        return -1;
    }

    if (consumer_ctx_.consumer == nullptr) {
        LOG(INFO) << "[DISPATCHER] no output.";
        return -1;
    }

    vfilters = video_graph_desc;
    afilters = audio_graph_desc;

    // audio filter graph
    if (audio_enabled_) {
        if (afilters.empty() && aproducer_ctxs_.size() > 1) {
            // TODO: the amix may not be closed with duration=longest
            afilters = fmt::format("amix=inputs={}:duration=first", aproducer_ctxs_.size());
        }

        consumer_ctx_.consumer->enable(AVMEDIA_TYPE_AUDIO);

        if (create_filter_graph(AVMEDIA_TYPE_AUDIO) < 0) {
            LOG(ERROR) << "[DISPATCHER] failed to create graph for processing audio";
            return -1;
        }
    }

    // video filter graph
    if (video_enabled_) {
        if (vfilters.empty() && vproducer_ctxs_.size() > 1) {
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

int Dispatcher::send_command(AVMediaType mt, const std::string& target, const std::string& cmd,
                             const std::string& arg)
{
    LOG(INFO) << fmt::format("[DISPATCHER] [{}] {}: {}={}", av::to_char(mt), target, cmd, arg);

    switch (mt) {
    case AVMEDIA_TYPE_AUDIO:
        return avfilter_graph_send_command(agraph_, target.c_str(), cmd.c_str(), arg.c_str(), nullptr, 0,
                                           0);
    case AVMEDIA_TYPE_VIDEO:
        return avfilter_graph_send_command(vgraph_, target.c_str(), cmd.c_str(), arg.c_str(), nullptr, 0,
                                           0);
    default: return -1;
    }
}

int Dispatcher::create_filter_graph(enum AVMediaType type)
{
    auto& graph         = (type == AVMEDIA_TYPE_AUDIO) ? agraph_ : vgraph_;
    auto& filters       = (type == AVMEDIA_TYPE_AUDIO) ? afilters : vfilters;
    auto& producer_ctxs = (type == AVMEDIA_TYPE_AUDIO) ? aproducer_ctxs_ : vproducer_ctxs_;
    auto& sink          = (type == AVMEDIA_TYPE_AUDIO) ? consumer_ctx_.asink_ctx : consumer_ctx_.vsink_ctx;

    // 1. alloc filter graph
    if (graph) avfilter_graph_free(&graph);
    if (graph = avfilter_graph_alloc(); !graph) {
        LOG(ERROR) << fmt::format("[DISPATCHER] [{}] failed to create filter graph.", av::to_char(type));
        return -1;
    }

    // 2. create buffersrc
    for (auto& [ctx, producer, _] : producer_ctxs) {
        if (create_src_filter(producer, &ctx, type) < 0) return -1;

        producer->enable(type);
    }

    // 3. create buffersink
    if (create_sink_filter(consumer_ctx_.consumer, &sink, type) < 0) return -1;

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

    auto consumer = consumer_ctx_.consumer;
    if (consumer && consumer->accepts(AVMEDIA_TYPE_VIDEO) && (consumer_ctx_.vsink_ctx != nullptr)) {
        consumer->vfmt.pix_fmt =
            static_cast<AVPixelFormat>(av_buffersink_get_format(consumer_ctx_.vsink_ctx));
        consumer->vfmt.width               = av_buffersink_get_w(consumer_ctx_.vsink_ctx);
        consumer->vfmt.height              = av_buffersink_get_h(consumer_ctx_.vsink_ctx);
        consumer->vfmt.sample_aspect_ratio = av_buffersink_get_sample_aspect_ratio(consumer_ctx_.vsink_ctx);
        consumer->vfmt.time_base           = av_buffersink_get_time_base(consumer_ctx_.vsink_ctx);
        consumer->vfmt.framerate           = vfmt.framerate;
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
    if (locked_) {
        LOG(INFO) << "[DISPATCHER] dispatcher is locked, please reset first!";
        return -1;
    }

    locked_ = true;

    for (auto& coder : vproducer_ctxs_) {
        coder.eof = false;
        if (!coder.producer->ready()) {
            LOG(ERROR) << "[DISPATCHER] [V] producer is not ready!";
            return -1;
        }
    }

    for (auto& coder : aproducer_ctxs_) {
        coder.eof = false;
        if (!coder.producer->ready()) {
            LOG(ERROR) << "[DISPATCHER] [A] producer is not ready!";
            return -1;
        }
    }

    if (!consumer_ctx_.consumer->ready()) {
        LOG(ERROR) << "[DISPATCHER] consumer is not ready!";
        return -1;
    }

    //
    start_time_ = resumed_pts_ = os_gettime_ns();

    for (auto& coder : vproducer_ctxs_) {
        coder.producer->run();
    }

    for (auto& coder : aproducer_ctxs_) {
        coder.producer->run();
    }

    consumer_ctx_.aeof = false;
    consumer_ctx_.veof = false;
    consumer_ctx_.consumer->run();
    //

    vrunning_ = true;
    if (consumer_ctx_.consumer->accepts(AVMEDIA_TYPE_VIDEO)) {
        video_thread_ = std::thread([this]() {
            probe::thread::set_name("dispatch-video");
            dispatch_fn(AVMEDIA_TYPE_VIDEO);
        });
    }

    arunning_ = true;
    if (consumer_ctx_.consumer->accepts(AVMEDIA_TYPE_AUDIO)) {
        audio_thread_ = std::thread([this]() {
            probe::thread::set_name("dispatch-audio");
            dispatch_fn(AVMEDIA_TYPE_AUDIO);
        });
    }

    return 0;
}

bool Dispatcher::is_valid_pts(int64_t pts)
{
    std::lock_guard<std::mutex> lock(pause_mtx_);

    // [-, max(start_time_, resumed_pts_))
    if (pts < resumed_pts_) {
        return false;
    }

    // pausing:
    //     [paused_pts_, +)
    if (paused_pts_ != AV_NOPTS_VALUE) {
        return pts <= paused_pts_;
    }

    return true;
}

int Dispatcher::dispatch_fn(enum AVMediaType mt)
{
    LOG(INFO) << fmt::format("[{}] STARTED", av::to_char(mt));
    defer(LOG(INFO) << fmt::format("[{}] EXITED", av::to_char(mt)));

    av::frame frame{};

    auto& ctxs         = (mt == AVMEDIA_TYPE_VIDEO) ? vproducer_ctxs_ : aproducer_ctxs_;
    auto& sink         = (mt == AVMEDIA_TYPE_VIDEO) ? consumer_ctx_.vsink_ctx : consumer_ctx_.asink_ctx;
    auto& consumer_eof = (mt == AVMEDIA_TYPE_VIDEO) ? consumer_ctx_.veof : consumer_ctx_.aeof;
    auto& seeking      = (mt == AVMEDIA_TYPE_VIDEO) ? vseeking_ : aseeking_;
    auto& running      = (mt == AVMEDIA_TYPE_VIDEO) ? vrunning_ : arunning_;

    while (running && !consumer_eof) {
        if (seeking) {
            if (create_filter_graph(mt) < 0) {
                LOG(ERROR) << "[DISPATCHER] failed to reconfigure graph for " << av::to_string(mt);
                running = false;
                return -1;
            }
            seeking = false;
        }

        bool sleepy  = true;
        bool all_eof = true;
        // input streams
        for (auto& [src_ctx, producer, stream_eof] : ctxs) {
            if (stream_eof || !running) continue;

            all_eof = false;

            if (producer->eof() && producer->empty(mt)) {
                if (av_buffersrc_add_frame_flags(src_ctx, nullptr, AV_BUFFERSRC_FLAG_PUSH) < 0) {
                    LOG(ERROR) << "failed to send NULL to filter graph";
                }
                LOG(INFO) << fmt::format("[{}] send NULL", av::to_char(mt));
                stream_eof = true;
                sleepy     = false;
                continue;
            }

            // process next frame
            if (producer->produce(frame.put(), mt) != 0) continue;

            sleepy        = false;
            auto frame_tb = producer->time_base(mt);

            // pts
            if (timing_ == av::timing_t::system) {
                if (frame->data[0] && !is_valid_pts(av_rescale_q(frame->pts, frame_tb, OS_TIME_BASE_Q))) {
                    LOG(WARNING) << fmt::format("[{}] drop frame {} -- [-, {}) || [{}, +)", av::to_char(mt),
                                                av_rescale_q(frame->pts, frame_tb, OS_TIME_BASE_Q),
                                                std::max(start_time_.load(), resumed_pts_), paused_pts_);
                    continue;
                }
                frame->pts -= av_rescale_q(start_time_, OS_TIME_BASE_Q, frame_tb);
                if (frame->pkt_dts != AV_NOPTS_VALUE)
                    frame->pkt_dts -= av_rescale_q(start_time_, OS_TIME_BASE_Q, frame_tb);
            }
            frame->pts -= av_rescale_q(paused_time(), OS_TIME_BASE_Q, frame_tb);
            if (frame->pkt_dts != AV_NOPTS_VALUE)
                frame->pkt_dts -= av_rescale_q(paused_time(), OS_TIME_BASE_Q, frame_tb);

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
            int ret = av_buffersink_get_frame_flags(sink, frame.put(), AV_BUFFERSINK_FLAG_NO_REQUEST);
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

            if (!seeking) consumer_ctx_.consumer->consume(frame.get(), mt);
        }
    }

    running = false;

    return 0;
}

void Dispatcher::seek(const std::chrono::microseconds& ts, int64_t lts, int64_t rts)
{
    for (auto& [_0, coder, _2] : vproducer_ctxs_) {
        coder->seek(ts, lts, rts);
    }

    vseeking_ = true;
    aseeking_ = true;

    if (!vrunning_ || !arunning_) {
        vrunning_ = false;
        arunning_ = false;

        if (video_thread_.joinable()) video_thread_.join();

        if (audio_thread_.joinable()) audio_thread_.join();

        locked_ = false;
        start();
    }
}

void Dispatcher::pause()
{
    std::lock_guard<std::mutex> lock(pause_mtx_);

    if (paused_pts_ == AV_NOPTS_VALUE) {
        paused_pts_ = os_gettime_ns();
    }
}

void Dispatcher::resume()
{
    std::lock_guard<std::mutex> lock(pause_mtx_);

    if (paused_pts_ != AV_NOPTS_VALUE) {
        auto now = os_gettime_ns();
        paused_time_ += (now - paused_pts_);
        paused_pts_  = AV_NOPTS_VALUE;
        resumed_pts_ = now;
    }
}

int64_t Dispatcher::paused_time()
{
    std::lock_guard<std::mutex> lock(pause_mtx_);

    if (paused_pts_ != AV_NOPTS_VALUE) {
        return paused_time_ + (os_gettime_ns() - paused_pts_);
    }
    return paused_time_;
}

int Dispatcher::reset()
{
    pause();
    ready_ = false;

    // producers
    for (auto& [ctx, coder, _] : vproducer_ctxs_) {
        coder->reset();

        // push nullptr frame to close the video buffersrc
        if (vrunning_ && av_buffersrc_add_frame_flags(ctx, nullptr, AV_BUFFERSRC_FLAG_PUSH) < 0) {
            LOG(ERROR) << "[DISPATCHER] failed to close video buffersrc";
        }
    }

    for (auto& [ctx, coder, _] : aproducer_ctxs_) {
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

            os_sleep(30ms);
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

    vfilters = {};
    afilters = {};

    audio_enabled_ = false;
    video_enabled_ = false;

    vfmt = {};
    afmt = {};

    avfilter_graph_free(&vgraph_);
    avfilter_graph_free(&agraph_);

    paused_pts_  = AV_NOPTS_VALUE;
    paused_time_ = 0;
    start_time_  = 0;
    resumed_pts_ = 0;

    locked_ = false;

    LOG(INFO) << "[DISPATCHER] RESET";
    return 0;
}

int64_t Dispatcher::escaped_us()
{
    if (!running()) {
        return 0;
    }
    return std::max<int64_t>(0, (os_gettime_ns() - start_time_ - paused_time()) / 1'000);
}
