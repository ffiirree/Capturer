#include "dispatcher.h"
#include "logging.h"
#include "fmt/format.h"
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/time.h>
#include <libavutil/opt.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/pixdesc.h>
}
#include "clock.h"
#include "defer.h"

int Dispatcher::create_src_filter(const Producer<AVFrame>* decoder, AVFilterContext** ctx, enum AVMediaType type)
{
    switch (type) {
    case AVMEDIA_TYPE_VIDEO: return create_video_src(decoder, ctx);
    case AVMEDIA_TYPE_AUDIO: return create_audio_src(decoder, ctx);
    default: return -1;
    }
}

int Dispatcher::create_sink_filter(const Consumer<AVFrame>* encoder, AVFilterContext** ctx, enum AVMediaType type)
{
    switch (type) {
    case AVMEDIA_TYPE_VIDEO: return create_video_sink(encoder, ctx);
    case AVMEDIA_TYPE_AUDIO: return create_audio_sink(encoder, ctx);
    default: return -1;
    }
}

int Dispatcher::create_video_src(const Producer<AVFrame>* decoder, AVFilterContext** ctx)
{
    if (!decoder->ready()) {
        LOG(ERROR) << "[DISPATCHER] [V] decoder is not ready";
        return -1;
    }

    auto buffer_args = decoder->format_str(AVMEDIA_TYPE_VIDEO);
    if (avfilter_graph_create_filter(ctx, avfilter_get_by_name("buffer"), "v_src", buffer_args.c_str(), nullptr, video_graph_) < 0) {
        LOG(ERROR) << "[DISPATCHER] [V] failed to create 'buffer'.";
        return -1;
    }

    LOG(INFO) << "[DISPATCHER] [V] create vbuffer, args '" << buffer_args << "'";

    return 0;
}

int Dispatcher::create_video_sink(const Consumer<AVFrame>* encoder, AVFilterContext** ctx)
{
    if (avfilter_graph_create_filter(ctx, avfilter_get_by_name("buffersink"), "v_sink", nullptr, nullptr, video_graph_) < 0) {
        LOG(ERROR) << "[DISPATCHER] [V] failed to create 'buffersink'.";
        return -1;
    }

    vfmt.pix_fmt = (vfmt.pix_fmt == AV_PIX_FMT_NONE) ? encoder->vfmt.pix_fmt : vfmt.pix_fmt;
    if (vfmt.pix_fmt != AV_PIX_FMT_NONE) {
        enum AVPixelFormat pix_fmts[] = { vfmt.pix_fmt, AV_PIX_FMT_NONE };
        if (av_opt_set_int_list(*ctx, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN) < 0) {
            LOG(ERROR) << "[DISPATCHER] [V] av_opt_set_int_list";
            return -1;
        }
    }
    LOG(INFO) << "[DISPATCHER] [V] created vbuffersink, pix_fmt = " << pix_fmt_name(vfmt.pix_fmt);

    return 0;
}

int Dispatcher::create_audio_src(const Producer<AVFrame>* decoder, AVFilterContext** ctx)
{
    if (!decoder->ready()) {
        LOG(ERROR) << "[DISPATCHER] [A] decoder is not ready.";
        return -1;
    }

    auto buffer_args = decoder->format_str(AVMEDIA_TYPE_AUDIO);
    if (avfilter_graph_create_filter(ctx, avfilter_get_by_name("abuffer"), "a_src", buffer_args.c_str(), nullptr, audio_graph_) < 0) {
        LOG(ERROR) << "[DISPATCHER] [A] filter to create 'abuffer'.";
        return -1;
    }

    LOG(INFO) << "[DISPATCHER] [A] create abuffer, args '" << buffer_args << "'";

    return 0;
}

int Dispatcher::create_audio_sink(const Consumer<AVFrame>* encoder, AVFilterContext** ctx)
{
    if (avfilter_graph_create_filter(ctx, avfilter_get_by_name("abuffersink"), "a_sink", nullptr, nullptr, audio_graph_) < 0) {
        LOG(ERROR) << "[DISPATCHER] [A] failed to create 'abuffersink'.";
        return -1;
    }

    afmt.sample_fmt = (afmt.sample_fmt == AV_SAMPLE_FMT_NONE) ? encoder->afmt.sample_fmt : afmt.sample_fmt;
    // TODO: force
    afmt.channels = 2;
    afmt.channel_layout = AV_CH_LAYOUT_STEREO;
    if (afmt.sample_fmt != AV_SAMPLE_FMT_NONE) {
        // sample_fmts (int list)
        // sample_rates(int list)
        // ch_layouts(string)
        // channel_counts(int list)
        // TODO: other options
        enum AVSampleFormat sink_fmts[] = { afmt.sample_fmt, AV_SAMPLE_FMT_NONE };
        if (av_opt_set_int_list(*ctx, "sample_fmts", sink_fmts, AV_SAMPLE_FMT_NONE, AV_OPT_SEARCH_CHILDREN) < 0) {
            LOG(ERROR) << "[DISPATCHER] [A] faile to set 'sample_fmts' option.";
            return -1;
        }
        int sink_channels[] = { 2, 0 };
        if (av_opt_set(*ctx, "ch_layouts", channel_layout_name(afmt.channels, afmt.channel_layout).c_str(), AV_OPT_SEARCH_CHILDREN) < 0) {
            LOG(ERROR) << "[DISPATCHER] [A] failed to set 'channel_counts' option.";
            return -1;
        }
    }

    LOG(INFO) << fmt::format("[DISPATCHER] [A] created abuffersink, sample_fmt = {}, channels = {}, ch_layout = {}", sample_fmt_name(afmt.sample_fmt), afmt.channels, "stereo");

    return 0;
}

int Dispatcher::create_filter_graph(const std::string_view& video_graph_desc, const std::string_view& audio_graph_desc)
{
    if (v_producer_ctxs_.empty() && a_producer_ctxs_.empty()) {
        LOG(INFO) << "[DISPATCHER] no input.";
        return -1;
    }

    if (consumer_ctx_.consumer == nullptr) {
        LOG(INFO) << "[DISPATCHER] no output.";
        return -1;
    }

    video_graph_desc_ = video_graph_desc;
    audio_graph_desc_ = audio_graph_desc;

    if (audio_enabled_) {
        if (audio_graph_desc_.empty() && a_producer_ctxs_.size() > 1) {
            // TODO: the amix may not be closed with duration=longest
            audio_graph_desc_ = fmt::format("amix=inputs={}:duration=first", a_producer_ctxs_.size());
        }

        consumer_ctx_.consumer->enable(AVMEDIA_TYPE_AUDIO);

        if (create_filter_graph_for(a_producer_ctxs_, audio_graph_desc_, AVMEDIA_TYPE_AUDIO) < 0) {
            LOG(ERROR) << "[DISPATCHER] failed to create graph for processing audio";
            return -1;
        }
    }

    if (video_enabled_) {
        if (video_graph_desc_.empty() && v_producer_ctxs_.size() > 1) {
            LOG(ERROR) << "[DISPATCHER] no more than 1 video stream for simple filter graph";
            return -1;
        }

        consumer_ctx_.consumer->enable(AVMEDIA_TYPE_VIDEO);

        if (video_graph_desc_.empty() && vfmt.hwaccel != AV_HWDEVICE_TYPE_NONE) {
            video_graph_desc_ = "format=nv12,hwupload"; // TODO: assume the decoding do not use hwaccel
        }

        if (create_filter_graph_for(v_producer_ctxs_, video_graph_desc_, AVMEDIA_TYPE_VIDEO) < 0) {
            LOG(ERROR) << "[DISPATCHER] failed to create graph for processing video";
            return -1;
        }
    }

    set_encoder_format_by_sinks();
    
    ready_ = true;
    return 0;
}

int Dispatcher::create_filter_graph_for(std::vector<ProducerContext>& producer_ctxs, const std::string& desc, enum AVMediaType type)
{
    AVFilterGraph* graph = nullptr;
    switch (type)
    {
    case AVMEDIA_TYPE_VIDEO:
        if (video_graph_) avfilter_graph_free(&video_graph_);
        if (!(video_graph_ = avfilter_graph_alloc())) {
            LOG(ERROR) << "[DISPATCHER] failed to create video filter graph.";
            return -1;
        }
        graph = video_graph_; 
        break;
    case AVMEDIA_TYPE_AUDIO:
        if (audio_graph_) avfilter_graph_free(&audio_graph_);
        if (!(audio_graph_ = avfilter_graph_alloc())) {
            LOG(ERROR) << "[DISPATCHER] failed to create auido filter graph.";
            return -1;
        }
        graph = audio_graph_;
        break;
    default: break;
    }

    // buffersrc
    for (auto& [ctx, producer, _] : producer_ctxs) {
        if (create_src_filter(producer, &ctx, type) < 0) {
            return -1;
        }

        producer->enable(type);
    }

    auto& sink = (type == AVMEDIA_TYPE_VIDEO) ? consumer_ctx_.vctx : consumer_ctx_.actx;

    // buffersink
    if (create_sink_filter(consumer_ctx_.consumer, &sink, type) < 0) {
        return -1;
    }

    if (desc.empty()) {
        // 1 input & 1 output
        if (avfilter_link(producer_ctxs[0].ctx, 0, sink, 0) < 0) {
            return -1;
        }
    }
    else {
        LOG(INFO) << fmt::format("[DISPATCHER] [{}] creating filter graph, args '{}'", av_get_media_type_string(type)[0], desc);

        AVFilterInOut* inputs = nullptr, * outputs = nullptr;
        defer(avfilter_inout_free(&inputs); avfilter_inout_free(&outputs));

        if (avfilter_graph_parse2(graph, desc.c_str(), &inputs, &outputs) < 0) {
            LOG(ERROR) << "[DISPATCHER] failed to parse filter graph desc : '" << desc << "'";
            return -1;
        }

        //
        int i = 0;
        for (auto ptr = inputs; ptr; ptr = ptr->next, i++) {
            if (avfilter_link(producer_ctxs[i].ctx, 0, ptr->filter_ctx, ptr->pad_idx) < 0) {
                LOG(ERROR) << "[DISPATCHER] failed to link input filters";
                return -1;
            }
        }

        for (auto ptr = outputs; ptr; ptr = ptr->next) {
            if(avfilter_link(ptr->filter_ctx, ptr->pad_idx, sink, 0) < 0) {
                LOG(ERROR) << "[DISPATCHER] failed to link output filters";
                return -1;
            }
        }
    }

    if (type == AVMEDIA_TYPE_VIDEO && vfmt.hwaccel != AV_HWDEVICE_TYPE_NONE) {
        if (hwaccel::setup_for_filter_graph(graph, vfmt.hwaccel) != 0) {
            LOG(ERROR) << "[DISPATCHER]  can not set hareware device up for filter graph.";
            return -1;
        }
    }

    if (avfilter_graph_config(graph, nullptr) < 0) {
        LOG(ERROR) << "[DISPATCHER] failed to configure the filter graph";
        return -1;
    }

    if (type == AVMEDIA_TYPE_VIDEO && vfmt.hwaccel != AV_HWDEVICE_TYPE_NONE) {
        if (hwaccel::set_sink_frame_ctx_for_encoding(sink, vfmt.hwaccel) < 0) {
            LOG(ERROR) << "[DISPATCHER] can not set sink frame context for encoder.";
            return -1;
        }
    }

    LOG(INFO) << "[DISPATCHER] filter graph @{\n" << avfilter_graph_dump(graph, nullptr);
    LOG(INFO) << "[DISPATCHER] @}";

    return 0;
}

int Dispatcher::set_encoder_format_by_sinks()
{
    if (!consumer_ctx_.consumer && !consumer_ctx_.vctx && !consumer_ctx_.actx) {
        LOG(ERROR) << "no encoder was found";
        return -1;
    }

    if (consumer_ctx_.consumer->accepts(AVMEDIA_TYPE_VIDEO) && (consumer_ctx_.vctx != nullptr)) {
        consumer_ctx_.consumer->vfmt.pix_fmt = static_cast<AVPixelFormat>(av_buffersink_get_format(consumer_ctx_.vctx));
        consumer_ctx_.consumer->vfmt.width = av_buffersink_get_w(consumer_ctx_.vctx);
        consumer_ctx_.consumer->vfmt.height = av_buffersink_get_h(consumer_ctx_.vctx);
        consumer_ctx_.consumer->vfmt.sample_aspect_ratio = av_buffersink_get_sample_aspect_ratio(consumer_ctx_.vctx);
        consumer_ctx_.consumer->vfmt.time_base = av_buffersink_get_time_base(consumer_ctx_.vctx);

        // min
        auto sink_fr = av_buffersink_get_frame_rate(consumer_ctx_.vctx);
        consumer_ctx_.consumer->vfmt.framerate = (av_q2d(vfmt.framerate) > av_q2d(sink_fr)) ? sink_fr : vfmt.framerate;
    }

    if (consumer_ctx_.consumer->accepts(AVMEDIA_TYPE_AUDIO) && (consumer_ctx_.actx != nullptr)) {
        consumer_ctx_.consumer->afmt.sample_fmt = static_cast<AVSampleFormat>(av_buffersink_get_format(consumer_ctx_.actx));
        consumer_ctx_.consumer->afmt.channels = av_buffersink_get_channels(consumer_ctx_.actx);
        consumer_ctx_.consumer->afmt.channel_layout = av_buffersink_get_channel_layout(consumer_ctx_.actx);
        consumer_ctx_.consumer->afmt.sample_rate = av_buffersink_get_sample_rate(consumer_ctx_.actx);
        consumer_ctx_.consumer->afmt.time_base = av_buffersink_get_time_base(consumer_ctx_.actx);
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

    for (auto& coder : v_producer_ctxs_) {
        if (!coder.producer->ready()) {
            LOG(ERROR) << "[DISPATCHER] [V] decoder not ready!";
            return -1;
        }
    }

    for (auto& coder : a_producer_ctxs_) {
        if (!coder.producer->ready()) {
            LOG(ERROR) << "[DISPATCHER] [A] decoder not ready!";
            return -1;
        }
    }

    if (!consumer_ctx_.consumer->ready()) {
        LOG(ERROR) << "[DISPATCHER] encoder not ready!";
        return -1;
    }

    //
    start_time_ = os_gettime_ns();
    resumed_pts_ = start_time_;

    for (auto& coder : v_producer_ctxs_) {
        coder.producer->run();
    }

    for (auto& coder : a_producer_ctxs_) {
        coder.producer->run();
    }

    consumer_ctx_.consumer->run();
    //

    running_ = true;
    if (consumer_ctx_.consumer->accepts(AVMEDIA_TYPE_VIDEO)) {
        video_thread_ = std::thread([this]() {
            platform::util::thread_set_name("dispatch-video");
            dispatch_fn(AVMEDIA_TYPE_VIDEO);
        });
    }
    
    if (consumer_ctx_.consumer->accepts(AVMEDIA_TYPE_AUDIO)) {
        auido_thread_ = std::thread([this]() {
            platform::util::thread_set_name("dispatch-audio");
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
    LOG(INFO) << fmt::format("[{}] STARTED", av_get_media_type_string(mt)[0]);
    defer(LOG(INFO) << fmt::format("[{}] EXITED", av_get_media_type_string(mt)[0]));

    AVFrame* frame = av_frame_alloc();
    defer(av_frame_free(&frame));

    auto& ctxs = (mt == AVMEDIA_TYPE_VIDEO) ? v_producer_ctxs_ : a_producer_ctxs_;
    auto& sink = (mt == AVMEDIA_TYPE_VIDEO) ? consumer_ctx_.vctx : consumer_ctx_.actx;
    auto& consumer_eof = (mt == AVMEDIA_TYPE_VIDEO) ? consumer_ctx_.v_eof : consumer_ctx_.a_eof;

    bool sleepy = false;
    while (running_ && !consumer_eof) {

        if (sleepy) {
            os_sleep(10ms);
            sleepy = true;
        }

        // input streams
        for (auto& [src_ctx, producer, _] : ctxs) {
            if (!producer->eof() && running_) {
                auto frame_tb = producer->time_base(mt);

                // receive a frame
                if (producer->produce(frame, mt) != 0) {
                    continue;
                }

                sleepy = false;

                if (!is_valid_pts(av_rescale_q(frame->pts, frame_tb, OS_TIME_BASE_Q))) {
                    LOG(INFO) << fmt::format("[{}] drop frame {} ({} - {})",
                        av_get_media_type_string(mt)[0], av_rescale_q(frame->pts, frame_tb, OS_TIME_BASE_Q), resumed_pts_, paused_pts_);
                    continue;
                }

                frame->pts -= av_rescale_q(start_time_ + paused_time(), OS_TIME_BASE_Q, frame_tb);
                if (frame->pkt_dts != AV_NOPTS_VALUE)
                    frame->pkt_dts -= av_rescale_q(start_time_ + paused_time(), OS_TIME_BASE_Q, frame_tb);

                // send the frame to graph
                if (av_buffersrc_add_frame_flags(src_ctx, frame, AV_BUFFERSRC_FLAG_PUSH) < 0) {
                    LOG(ERROR) << fmt::format("[{}] failed to send the frame to filter graph.", av_get_media_type_string(mt)[0]);
                    running_ = false;
                    break;
                }
            }
        }

        // output streams
        if (consumer_ctx_.consumer->full(mt)) {
            sleepy = true;
            continue;
        }

        av_frame_unref(frame);
        int ret = av_buffersink_get_frame_flags(sink, frame, AV_BUFFERSINK_FLAG_NO_REQUEST);
        if (ret == AVERROR(EAGAIN)) {
            continue;
        }
        else if (ret == AVERROR_EOF) {
            LOG(INFO) << fmt::format("[{}] EOF",av_get_media_type_string(mt)[0]);
            consumer_eof = true;
            // through and send null packet
            av_frame_unref(frame);
        }
        else if (ret < 0) {
            LOG(ERROR) << fmt::format("[{}] failed to get frame.", av_get_media_type_string(mt)[0]);
            running_ = false;
            break;
        }

        sleepy = false;
        consumer_ctx_.consumer->consume(frame, mt);
    }

    return 0;
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
        paused_pts_ = AV_NOPTS_VALUE;
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
    for (auto& [ctx, coder, _] : v_producer_ctxs_) {
        coder->reset();

        // push nullptr frame to close the video buffersrc
        if (running_ && av_buffersrc_add_frame_flags(ctx, nullptr, AV_BUFFERSRC_FLAG_PUSH) < 0) {
            LOG(ERROR) << "[DISPATCHER] failed to close video buffersrc";
        }
    }

    for (auto& [ctx, coder, _] : a_producer_ctxs_) {
        coder->reset();

        // push nullptr frame to close the auido buffersrc
        if (running_ && av_buffersrc_add_frame_flags(ctx, nullptr, AV_BUFFERSRC_FLAG_PUSH) < 0) {
            LOG(ERROR) << "[DISPATCHER] failed to close audio buffersrc";
        }
    }

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
    running_ = false;

    if (video_thread_.joinable()) {
        video_thread_.join();
    }

    if (auido_thread_.joinable()) {
        auido_thread_.join();
    }

    // clear
    a_producer_ctxs_.clear();
    v_producer_ctxs_.clear();
    consumer_ctx_ = {};

    video_graph_desc_ = {};
    audio_graph_desc_ = {};

    audio_enabled_ = false;
    video_enabled_ = false;

    vfmt = {};
    afmt = {};

    avfilter_graph_free(&video_graph_);
    avfilter_graph_free(&audio_graph_);

    paused_pts_ = AV_NOPTS_VALUE;
    paused_time_ = 0;
    start_time_ = 0;
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
    return std::max<int64_t>(0, (os_gettime_ns() - start_time_ - paused_time()) / 1000);
}
