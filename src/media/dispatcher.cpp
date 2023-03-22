#include <chrono>
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
using namespace std::chrono_literals;

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

    has_audio_in_ = true;
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

        create_filter_graph_for(video_producers_, video_graph_desc_, AVMEDIA_TYPE_VIDEO);
    }

    if (!audio_producers_.empty()) {
        if (audio_graph_desc_.empty() && audio_producers_.size() > 1) {
            audio_graph_desc_ = fmt::format("amix=inputs={}:duration=first", audio_producers_.size());
        }

        create_filter_graph_for(audio_producers_, audio_graph_desc_, AVMEDIA_TYPE_AUDIO);
    }
 
    if (int er = 0; er = avfilter_graph_config(filter_graph_, nullptr) < 0) {
        char buffer[AV_ERROR_MAX_STRING_SIZE] = { 0 };
        av_make_error_string(buffer, AV_ERROR_MAX_STRING_SIZE, er);

        LOG(ERROR) << "avfilter_graph_config(filter_graph_, nullptr)" << buffer;
        return -1;
    }
    
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

AVFilterContext* Dispatcher::find_sink(Consumer<AVFrame>* ptr, enum AVMediaType type)
{
    for (const auto& [ctx, coder, _] : o_streams_) {
        if (coder == ptr && av_buffersink_get_type(ctx) == type) {
            return ctx;
        }
    }
    return nullptr;
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

    for (auto& coder : video_producers_) {
        coder->run();
    }

    for (auto& coder : audio_producers_) {
        coder->run();
    }

    consumer_->run();

    running_ = true;
    dispatch_thread_ = std::thread([this]() { dispatch_thread_f(); });

    return 0;
}

int Dispatcher::dispatch_thread_f()
{
    AVFrame* frame_ = av_frame_alloc();
    defer(av_frame_free(&frame_));
    AVFrame* filtered_frame = av_frame_alloc();
    defer(av_frame_free(&filtered_frame));

    LOG(INFO) << "[DISPATCHER] STARTED";
    defer(LOG(INFO) << "[DISPATCHER] EXITED");

    int ret = 0;
    while (running_) {
        bool need_dispatch = false;
        // input streams
        for (auto& [src_ctx, producer, eof] : i_streams_) {
            if (!eof) {
                ret = producer->produce(frame_, avfilter_pad_get_type(src_ctx->filter->outputs, 0));
                if (ret == AVERROR_EOF) {
                    eof = true;
                    LOG(INFO) << fmt::format("[DISPATCHER] {} EOF", av_get_media_type_string(avfilter_pad_get_type(src_ctx->filter->outputs, 0)));
                }
                else if (ret < 0) {
                    continue;
                }

                ret = av_buffersrc_add_frame_flags(src_ctx, (ret == AVERROR_EOF) ? nullptr : frame_, AV_BUFFERSRC_FLAG_PUSH);
                if (ret < 0) {
                    LOG(ERROR) << "av_buffersrc_add_frame_flags";
                    running_ = false;
                    break;
                }

                need_dispatch = true;
            }
        }

        // pause @{
        if (paused_) {
            if (paused_pts_ != AV_NOPTS_VALUE) {
                offset_pts_ += av_gettime_relative() - paused_pts_;
            }

            paused_pts_ = av_gettime_relative();
        }
        else if (paused_pts_ != AV_NOPTS_VALUE) {
            offset_pts_ += av_gettime_relative() - paused_pts_;
            paused_pts_ = AV_NOPTS_VALUE;
        }
        // @}

        if (!need_dispatch) {
            std::this_thread::sleep_for(10ms);
            continue;
        }

        first_pts_ = (first_pts_ == AV_NOPTS_VALUE) ? av_gettime_relative() : first_pts_;

        // output streams
        for (auto& [sink_ctx, consumer, stream_eof] : o_streams_) {
            enum AVMediaType media_type = av_buffersink_get_type(sink_ctx);
            ret = 0;
            while (ret >= 0 && !stream_eof && !consumer->full(media_type)) {
                av_frame_unref(filtered_frame);
                ret = av_buffersink_get_frame_flags(sink_ctx, filtered_frame, AV_BUFFERSINK_FLAG_NO_REQUEST);

                if (ret == AVERROR(EAGAIN)) {
                    break;
                }
                else if (ret == AVERROR_EOF) {
                    LOG(INFO) << "[DISPATCHER] " << av_get_media_type_string(media_type) <<" EOF";
                    stream_eof = true;
                    // through and send null packet
                    av_frame_unref(filtered_frame);
                }
                else if (ret < 0) {
                    LOG(ERROR) << "av_buffersink_get_frame_flags()";
                    running_ = false;
                    break;
                }

                while (consumer->full(media_type) && running_) {
                    LOG(WARNING) << "[DISPATCHER] better not be here";
                    std::this_thread::sleep_for(20ms);
                }
                ret = consumer->consume(filtered_frame, media_type);
            }
        }
    }

    return ret;
}

void Dispatcher::pause()
{
    for (auto& coder : video_producers_) {
        coder->pause();
    }

    for (auto& coder : audio_producers_) {
        coder->pause();
    }

    consumer_->pause();

    paused_ = true;
}

void Dispatcher::resume()
{
    paused_ = false;

    consumer_->resume();

    for (auto& coder : video_producers_) {
        coder->time_offset(offset_pts_);
        coder->resume();
    }

    for (auto& coder : audio_producers_) {
        coder->time_offset(offset_pts_);
        coder->resume();
    }
}

int Dispatcher::reset()
{
    paused_ = false;
    ready_ = false;

    for (auto& coder : video_producers_) {
        coder->reset();
    }

    for (auto& coder : audio_producers_) {
        coder->reset();
    }

    // wait 0~3s
    if (consumer_) {
        for (int i = 0; i < 100; i++) {
            if (!consumer_->ready() || consumer_->eof()) break;

            std::this_thread::sleep_for(100ms);
        }
        consumer_->reset();
    }

    running_ = false;

    if (dispatch_thread_.joinable()) {
        dispatch_thread_.join();
    }

    video_producers_.clear();
    audio_producers_.clear();
    consumer_ = nullptr;
    i_streams_.clear();
    o_streams_.clear();

    video_graph_desc_ = {};
    audio_graph_desc_ = {};

    first_pts_ = AV_NOPTS_VALUE;
    paused_pts_ = AV_NOPTS_VALUE;
    offset_pts_ = 0;

    avfilter_graph_free(&filter_graph_);

    LOG(INFO) << "[DISPATCHER] RESETED";
    return 0;
}

int64_t Dispatcher::escaped_us() const {
    return (first_pts_ == AV_NOPTS_VALUE) ? 0 : std::max<int64_t>(0, av_gettime_relative() - first_pts_ - offset_pts_);
}
