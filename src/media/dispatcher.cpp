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
}
#include "clock.h"

int Dispatcher::create_filter_for_input(const Producer<AVFrame>* decoder, AVFilterContext** ctx, enum AVMediaType type)
{
    switch (type) {
    case AVMEDIA_TYPE_VIDEO: return create_filter_for_video_input(decoder, ctx);
    case AVMEDIA_TYPE_AUDIO: return create_filter_for_audio_input(decoder, ctx);
    default: return -1;
    }
}

int Dispatcher::create_filter_for_output(const Consumer<AVFrame>* encoder, AVFilterContext** ctx, enum AVMediaType type)
{
    switch (type) {
    case AVMEDIA_TYPE_VIDEO: return create_filter_for_video_output(encoder, ctx);
    case AVMEDIA_TYPE_AUDIO: return create_filter_for_audio_output(encoder, ctx);
    default: return -1;
    }
}

int Dispatcher::create_filter_for_video_input(const Producer<AVFrame>* decoder, AVFilterContext** ctx)
{
    if (!decoder->ready()) {
        LOG(ERROR) << "[DISPATCHER] decoder is not ready";
        return -1;
    }
    LOG(INFO) << "[DISPATCHER] [V] create src: " << decoder->format_str(AVMEDIA_TYPE_VIDEO);

    const AVFilter* buffersrc = avfilter_get_by_name("buffer");
    if (!buffersrc) {
        LOG(ERROR) << "avfilter_get_by_name";
        return -1;
    }

    if (avfilter_graph_create_filter(ctx, buffersrc, "v_src", decoder->format_str(AVMEDIA_TYPE_VIDEO).c_str(), nullptr, filter_graph_) < 0) {
        LOG(ERROR) << "avfilter_graph_create_filter";
        return -1;
    }

    return 0;
}

int Dispatcher::create_filter_for_video_output(const Consumer<AVFrame>* encoder, AVFilterContext** ctx)
{
    const AVFilter* buffersink = avfilter_get_by_name("buffersink");
    if (!buffersink) {
        LOG(ERROR) << "avfilter_get_by_name";
        return -1;
    }

    if (avfilter_graph_create_filter(ctx, buffersink, "v_sink", nullptr, nullptr, filter_graph_) < 0) {
        LOG(ERROR) << "avfilter_graph_create_filter";
        return -1;
    }

    enum AVPixelFormat pix_fmt = ((AVPixelFormat)encoder->format(AVMEDIA_TYPE_VIDEO) == AV_PIX_FMT_NONE) ? AV_PIX_FMT_YUV420P : (AVPixelFormat)encoder->format(AVMEDIA_TYPE_VIDEO);
    enum AVPixelFormat pix_fmts[] = { pix_fmt, AV_PIX_FMT_NONE };
    if (av_opt_set_int_list(*ctx, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN) < 0) {
        LOG(ERROR) << "av_opt_set_int_list";
        return -1;
    }

    LOG(INFO) << "[DISPATCHER] [V] create sink";

    return 0;
}

int Dispatcher::create_filter_for_audio_input(const Producer<AVFrame>* decoder, AVFilterContext** ctx)
{
    if (!decoder->ready()) {
        LOG(ERROR) << "[DISPATCHER] decoder is not ready";
        return -1;
    }
    LOG(INFO) << "[DISPATCHER] [A] create src: " << decoder->format_str(AVMEDIA_TYPE_AUDIO);

    const AVFilter* buffersrc = avfilter_get_by_name("abuffer");
    if (!buffersrc) {
        LOG(ERROR) << "avfilter_get_by_name";
        return -1;
    }

    if (avfilter_graph_create_filter(ctx, buffersrc, "a_src", decoder->format_str(AVMEDIA_TYPE_AUDIO).c_str(), nullptr, filter_graph_) < 0) {
        LOG(ERROR) << "avfilter_graph_create_filter";
        return -1;
    }

    return 0;
}

int Dispatcher::create_filter_for_audio_output(const Consumer<AVFrame>* encoder, AVFilterContext** ctx)
{
    const AVFilter* abuffersink = avfilter_get_by_name("abuffersink");
    if (!abuffersink) {
        LOG(ERROR) << "avfilter_get_by_name";
        return -1;
    }

    if (avfilter_graph_create_filter(ctx, abuffersink, "a_sink", nullptr, nullptr, filter_graph_) < 0) {
        LOG(ERROR) << "avfilter_graph_create_filter";
        return -1;
    }

    enum AVSampleFormat encoder_fmt = (AVSampleFormat)encoder->format(AVMEDIA_TYPE_AUDIO);
    enum AVSampleFormat sink_fmt = (encoder_fmt == AV_SAMPLE_FMT_NONE) ? AV_SAMPLE_FMT_FLTP : encoder_fmt;
    enum AVSampleFormat sink_fmts[] = { sink_fmt, AV_SAMPLE_FMT_NONE };
    if (av_opt_set_int_list(*ctx, "sample_fmts", sink_fmts, AV_SAMPLE_FMT_NONE, AV_OPT_SEARCH_CHILDREN) < 0) {
        LOG(ERROR) << "av_opt_set_int_list";
        return -1;
    }

    LOG(INFO) << "[DISPATCHER] [A] create sink";

    return 0;
}


int Dispatcher::create_filter_graph(const std::string_view& video_graph_desc, const std::string_view& audio_graph_desc)
{
    video_graph_desc_ = video_graph_desc;
    audio_graph_desc_ = audio_graph_desc;

    if (video_producers_.empty() && audio_producers_.empty()) {
        LOG(INFO) << "[DISPATCHER] At least one input";
        return -1;
    }

    if (consumer_ == nullptr) {
        LOG(INFO) << "[DISPATCHER] At least one output";
        return -1;
    }

    // filter graph
    if (filter_graph_) avfilter_graph_free(&filter_graph_);
    if (!(filter_graph_ = avfilter_graph_alloc())) {
        LOG(ERROR) << "avfilter_graph_alloc";
        return -1;
    }

    if (!video_producers_.empty()) {
        if (video_graph_desc_.empty() && video_producers_.size() > 1) {
            LOG(ERROR) << "no more than 1 video stream for simple filter graph";
            return -1;
        }

        if (create_filter_graph_for(video_producers_, video_graph_desc_, AVMEDIA_TYPE_VIDEO) < 0) {
            LOG(ERROR) << "failed to create graph for processing video";
            return -1;
        }
    }

    if (!audio_producers_.empty()) {
        if (audio_graph_desc_.empty() && audio_producers_.size() > 1) {
            // TODO: the amix may not be closed with duration=longest
            audio_graph_desc_ = fmt::format("amix=inputs={}:duration=first", audio_producers_.size());
        }

        if (create_filter_graph_for(audio_producers_, audio_graph_desc_, AVMEDIA_TYPE_AUDIO) < 0) {
            LOG(ERROR) << "failed to create graph for processing audio";
            return -1;
        }
    }
 
    if (int er = 0; er = avfilter_graph_config(filter_graph_, nullptr) < 0) {
        LOG(ERROR) << "failed to configure the filter graph";
        return -1;
    }

    set_encoder_format_by_sinks();
    
    ready_ = true;
    LOG(INFO) << "[DISPATCHER] " << "filter graph @{\n" << avfilter_graph_dump(filter_graph_, nullptr);
    LOG(INFO) << "[DISPATCHER] " << "@}";
    return 0;
}


int Dispatcher::create_filter_graph_for(const std::vector<Producer<AVFrame>*>& producers, const std::string& desc, enum AVMediaType type)
{
    LOG(INFO) << "[DISPATCHER] create filter graph : " << desc;

    // buffersrc
    std::vector<AVFilterContext*> src_ctxs;
    for (const auto& producer : producers) {
        AVFilterContext* src_ctx = nullptr;

        if (create_filter_for_input(producer, &src_ctx, type) < 0) {
            return -1;
        }

        producer->enable(type);
        src_ctxs.emplace_back(src_ctx);
        i_streams_.emplace_back(src_ctx, producer, false);
    }

    // buffersink
    AVFilterContext* sink_ctx = nullptr;
    if (create_filter_for_output(consumer_, &sink_ctx, type) < 0) {
        return -1;
    }
    o_streams_.emplace_back(sink_ctx, consumer_, false);

    if (desc.empty()) {
        // 1 input & 1 output
        if (avfilter_link(src_ctxs[0], 0, sink_ctx, 0) < 0) {
            return -1;
        }
    }
    else {
        AVFilterInOut* inputs = nullptr, * outputs = nullptr;
        defer(avfilter_inout_free(&inputs); avfilter_inout_free(&outputs));

        if (avfilter_graph_parse2(filter_graph_, desc.c_str(), &inputs, &outputs) < 0) {
            LOG(ERROR) << "avfilter_graph_parse2";
            return -1;
        }

        int i = 0;
        for (auto ptr = inputs; ptr; ptr = ptr->next, i++) {
            if (avfilter_link(src_ctxs[i], 0, ptr->filter_ctx, ptr->pad_idx) < 0) {
                LOG(ERROR) << "avfilter_link";
                return -1;
            }
        }

        for (auto ptr = outputs; ptr; ptr = ptr->next) {
            if(avfilter_link(ptr->filter_ctx, ptr->pad_idx, sink_ctx, 0) < 0) {
                LOG(ERROR) << "avfilter_link";
                return -1;
            }
        }
    }

    return 0;
}

int Dispatcher::set_encoder_format_by_sinks()
{
    if (!consumer_) {
        LOG(ERROR) << "no encoder was found";
        return -1;
    }

    for (const auto& [sink_ctx, coder, _] : o_streams_) {
        switch (av_buffersink_get_type(sink_ctx))
        {
        case AVMEDIA_TYPE_VIDEO:
            consumer_->vfmt_.width = av_buffersink_get_w(sink_ctx);
            consumer_->vfmt_.height = av_buffersink_get_h(sink_ctx);
            consumer_->vfmt_.framerate = av_buffersink_get_frame_rate(sink_ctx);
            consumer_->vfmt_.sample_aspect_ratio = av_buffersink_get_sample_aspect_ratio(sink_ctx);
            consumer_->vfmt_.time_base = av_buffersink_get_time_base(sink_ctx);
            break;

        case AVMEDIA_TYPE_AUDIO:
            consumer_->enable(AVMEDIA_TYPE_AUDIO);

            consumer_->afmt_.channels = av_buffersink_get_channels(sink_ctx);
            consumer_->afmt_.channel_layout = av_buffersink_get_channel_layout(sink_ctx);
            consumer_->afmt_.sample_rate = av_buffersink_get_sample_rate(sink_ctx);
            consumer_->afmt_.time_base = av_buffersink_get_time_base(sink_ctx);
            break;

        default:
            LOG(ERROR) << "unsupport media type";
            return -1;
        }
    }
    return 0;
}

int Dispatcher::start()
{
    if (!ready_ || running_) {
        LOG(INFO) << "[DISPATCHER] already running or not ready!";
        return -1;
    }

    for (auto& coder : video_producers_) {
        if (!coder->ready()) {
            LOG(ERROR) << "[DISPATCHER] [V] decoder not ready!";
            return -1;
        }
    }

    for (auto& coder : audio_producers_) {
        if (!coder->ready()) {
            LOG(ERROR) << "[DISPATCHER] [A] decoder not ready!";
            return -1;
        }
    }

    if (!consumer_->ready()) {
        LOG(ERROR) << "[DISPATCHER] encoder not ready!";
        return -1;
    }

    //
    start_time_ = os_gettime_ns();
    resumed_pts_ = start_time_;

    for (auto& coder : video_producers_) {
        coder->run();
    }

    for (auto& coder : audio_producers_) {
        coder->run();
    }

    consumer_->run();
    //

    running_ = true;
    dispatch_thread_ = std::thread([this]() { dispatch_thread_f(); });
    receive_thread_ = std::thread([this]() { receive_thread_f(); });

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

int Dispatcher::receive_thread_f()
{
    AVFrame* frame = av_frame_alloc();
    defer(av_frame_free(&frame));

    LOG(INFO) << "[DISPATCHER] RECEIVE THREAD STARTED";
    defer(LOG(INFO) << "[DISPATCHER] RECEIVE THREAD EXITED");

    while (running_) {
        bool sleepy = true;

        // input streams
        for (auto& [src_ctx, producer, _] : i_streams_) {
            if (!producer->eof() && running_) {
                auto frame_mt = avfilter_pad_get_type(src_ctx->filter->outputs, 0);
                auto frame_tb = producer->time_base(frame_mt);

                // receive a frame
                if (producer->produce(frame, frame_mt) != 0) {
                    continue;
                }

                if (!is_valid_pts(av_rescale_q(frame->pts, frame_tb, OS_TIME_BASE_Q))) {
                    LOG(INFO) << fmt::format("[DISPATCHER] drop the {} frame {} ({} - {})",
                        av_get_media_type_string(frame_mt), av_rescale_q(frame->pts, frame_tb, OS_TIME_BASE_Q), resumed_pts_, paused_pts_);
                    continue;
                }

                frame->pts -= av_rescale_q(start_time_ + paused_time(), OS_TIME_BASE_Q, frame_tb);

                // send the frame to graph
                if (av_buffersrc_add_frame_flags(src_ctx, frame, AV_BUFFERSRC_FLAG_PUSH) < 0) {
                    LOG(ERROR) << "av_buffersrc_add_frame_flags";
                    running_ = false;
                    break;
                }

                sleepy = false;
            }
        }

        if (sleepy && running_) {
            os_sleep(25ms);
        }
    }

    return 0;
}

int Dispatcher::dispatch_thread_f()
{
    AVFrame* frame = av_frame_alloc();
    defer(av_frame_unref(frame); av_frame_free(&frame));

    LOG(INFO) << "[DISPATCHER] DISPATCH THREAD STARTED";
    defer(LOG(INFO) << "[DISPATCHER] DISPATCH THREAD EXITED");

    while (running_) {
        bool sleepy = true;

        // output streams
        for (auto& [sink_ctx, consumer, sink_eof] : o_streams_) {
            auto media_type = av_buffersink_get_type(sink_ctx);

            if (sink_eof || consumer->full(media_type)) {
                continue;
            }

            av_frame_unref(frame);
            int ret = av_buffersink_get_frame_flags(sink_ctx, frame, AV_BUFFERSINK_FLAG_NO_REQUEST);
            if (ret == AVERROR(EAGAIN)) {
                continue;
            }
            else if (ret == AVERROR_EOF) {
                LOG(INFO) << "[DISPATCHER] " << av_get_media_type_string(media_type) << " EOF";
                sink_eof = true;
                // through and send null packet
                av_frame_unref(frame);
            }
            else if (ret < 0) {
                LOG(ERROR) << "av_buffersink_get_frame_flags()";
                running_ = false;
                break;
            }
            
            if (consumer->consume(frame, media_type) < 0) {
                LOG(ERROR) << "failed to consume a frame.";
            }

            sleepy = false;
        }

        if (sleepy && running_) {
            os_sleep(25ms);
        }
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
    LOG(INFO) << fmt::format("[DISPATCHER] reseted at {}", escaped_us());

    pause();
    ready_ = false;

    // producers
    for (const auto& [ctx, coder, _] : i_streams_) {
        coder->reset();

        // push nullptr frame to close the buffersrc
        if (running_ && av_buffersrc_add_frame_flags(ctx, nullptr, AV_BUFFERSRC_FLAG_PUSH) < 0) {
            LOG(ERROR) << "failed to close buffersrc";
        }
    }

    // consumer
    if (consumer_) {
        // wait 0~3s
        for (int i = 0; i < 100; i++) {
            if (!consumer_->ready() || consumer_->eof()) break;

            os_sleep(30ms);
        }
        consumer_->reset();
    }

    // dispatcher
    running_ = false;

    if (receive_thread_.joinable()) {
        receive_thread_.join();
    }

    if (dispatch_thread_.joinable()) {
        dispatch_thread_.join();
    }

    // clear
    video_producers_.clear();
    audio_producers_.clear();
    consumer_ = nullptr;
    i_streams_.clear();
    o_streams_.clear();

    video_graph_desc_ = {};
    audio_graph_desc_ = {};

    paused_pts_ = AV_NOPTS_VALUE;
    paused_time_ = 0;
    start_time_ = 0;
    resumed_pts_ = 0;

    avfilter_graph_free(&filter_graph_);

    LOG(INFO) << "[DISPATCHER] RESETED";
    return 0;
}

int64_t Dispatcher::escaped_us()
{
    if (!running()) {
        return 0;
    }
    return std::max<int64_t>(0, (os_gettime_ns() - start_time_ - paused_time()) / 1000);
}
