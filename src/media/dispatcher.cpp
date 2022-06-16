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
    LOG(INFO) << "[DISPATCHER] " << "create  buffersrc: " << decoder->format_str(AVMEDIA_TYPE_VIDEO);

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

    LOG(INFO) << "[DISPATCHER] " << "create  buffersink";

    return 0;
}

int Dispatcher::create_filter_for_audio_input(const Producer<AVFrame>* decoder, AVFilterContext** ctx)
{
    if (!decoder->ready()) {
        LOG(ERROR) << "[DISPATCHER] decoder is not ready";
        return -1;
    }
    LOG(INFO) << "[DISPATCHER] " << "create abuffersrc: " << decoder->format_str(AVMEDIA_TYPE_AUDIO);

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

    LOG(INFO) << "[DISPATCHER] " << "create abuffersink";

    return 0;
}

int Dispatcher::create_filter_graph(const std::string_view& graph_desc)
{
    graph_desc_ = graph_desc;

    LOG(INFO) << "[DISPATCHER] create filter graph: " << graph_desc_;

    if (encoders_.size() < 1 || decoders_.size() < 1) {
        LOG(INFO) << "At least on input and one output";
        return -1;
    }

    // filter graph
    if (filter_graph_) avfilter_graph_free(&filter_graph_);
    if (!(filter_graph_ = avfilter_graph_alloc())) {
        LOG(ERROR) << "avfilter_graph_alloc";
        return -1;
    }

    AVFilterInOut* inputs = nullptr, * outputs = nullptr;
    defer(avfilter_inout_free(&inputs); avfilter_inout_free(&outputs));

    if (graph_desc_.empty()) {
        for (const auto& mt : { AVMEDIA_TYPE_VIDEO, AVMEDIA_TYPE_AUDIO }) {
            if (encoders_[0]->accepts(mt)) {
                bool ok = false;
                for (const auto& decoder : decoders_) {
                    if (decoder->has(mt)) {
                        AVFilterContext* src_ctx = nullptr, * sink_ctx = nullptr;

                        if (create_filter_for_input(decoder, &src_ctx, mt) < 0) {
                            return -1;
                        }

                        if (create_filter_for_output(encoders_[0], &sink_ctx, mt) < 0) {
                            return -1;
                        }

                        if (avfilter_link(src_ctx, 0, sink_ctx, 0) < 0) {
                            return -1;
                        }
                        decoder->enable(mt);
                        i_streams_.push_back({ src_ctx, decoder, false });
                        o_streams_.push_back({ sink_ctx, encoders_[0], false });
                        ok = true;
                        break;
                    }
                }
                if (!ok) {
                    LOG(INFO) << "video: [input] x [output]";
                    return -1;
                }
            }
        }
    }
    else {  // complex filter graph
        if (avfilter_graph_parse2(filter_graph_, graph_desc_.c_str(), &inputs, &outputs) < 0) {
            LOG(ERROR) << "avfilter_graph_parse2";
            return -1;
        }

        for (auto in = inputs; in; in = in->next) {
            enum AVMediaType mt = avfilter_pad_get_type(in->filter_ctx->input_pads, in->pad_idx);
            if (mt != AVMEDIA_TYPE_AUDIO && mt != AVMEDIA_TYPE_VIDEO) {
                LOG(ERROR) << "Only video and audio filters supported currently";
                return -1;
            }

            Producer<AVFrame>* producer = nullptr;
            if (in->name) {
                LOG(INFO) << fmt::format("[FILTER GRAPH] I : {}, type = {}, idx = {}", in->name, av_get_media_type_string(mt), in->pad_idx);

                char* end_ptr = nullptr;
                int idx = std::strtol(in->name, &end_ptr, 0);
                if (idx < 0 || idx > decoders_.size()) {
                    LOG(ERROR) << "Invalid file index " << idx << " in filtergraph description " << graph_desc_;
                    return -1;
                }

                if (decoders_[idx]->has(mt)) {
                    producer = decoders_[idx];
                }
            }
            else {
                // find the first unused stream of corresponding type
                for (const auto& coder : decoders_) {
                    if (coder->has(mt) && !coder->enabled(mt)) {
                        producer = coder;
                    }
                }
            }

            if (!producer) {
                LOG(INFO) << "not found input for " << in->filter_ctx->name;
                return -1;
            }

            AVFilterContext* ctx = nullptr;
            if (create_filter_for_input(producer, &ctx, mt) < 0) {
                return -1;
            }
            if (avfilter_link(ctx, 0, in->filter_ctx, in->pad_idx) < 0) {
                LOG(ERROR) << "link failed";
                return -1;
            }

            producer->enable(mt);
            i_streams_.push_back({ ctx, producer, false });
        }

        for (auto out = outputs; out; out = out->next) {
            enum AVMediaType mt = avfilter_pad_get_type(out->filter_ctx->input_pads, out->pad_idx);
            if (mt != AVMEDIA_TYPE_AUDIO && mt != AVMEDIA_TYPE_VIDEO) {
                LOG(ERROR) << "Only video and audio filters supported currently";
                return -1;
            }

            Consumer<AVFrame>* consumer = nullptr;
            for (const auto& encoder : encoders_) {
                if (encoder->accepts(mt)) {
                    consumer = encoder;
                }
            }

            if (!consumer) {
                LOG(ERROR) << out->filter_ctx->name << "has a unconnected output";
                return -1;
            }

            AVFilterContext* sink_ctx = nullptr;
            if (create_filter_for_output(consumer, &sink_ctx, mt) < 0) {
                return -1;
            }

            if (avfilter_link(out->filter_ctx, out->pad_idx, sink_ctx, 0) < 0) {
                LOG(ERROR) << "link failed";
                return -1;
            }

            o_streams_.push_back({ sink_ctx, consumer, false });
        }
    }

    if (avfilter_graph_config(filter_graph_, nullptr) < 0) {
        LOG(ERROR) << "avfilter_graph_config(filter_graph_, nullptr)";
        return -1;
    }

    ready_ = true;
    LOG(INFO) << "[DISPATCHER] " << "filter graph @{\n" << avfilter_graph_dump(filter_graph_, nullptr);
    LOG(INFO) << "[DISPATCHER] " << "@}";
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

    for (auto& coder : decoders_) {
        if (!coder->ready()) {
            LOG(ERROR) << "deocoder not ready!";
            return -1;
        }
    }

    for (auto& coder : encoders_) {
        if (!coder->ready()) {
            LOG(ERROR) << "encoder not ready!";
            return -1;
        }
    }

    for (auto& coder : decoders_) {
        coder->run();
    }

    for (auto& coder : encoders_) {
        coder->run();
    }

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

    LOG(INFO) << "[DISPATCHER@" << std::this_thread::get_id() << "] STARTED";
    defer(LOG(INFO) << "[DISPATCHER@" << std::this_thread::get_id() << "] EXITED");

    int ret = 0;
    bool need_dispatch = false;
    while (running_) {
        for (auto& [src_ctx, producer, eof] : i_streams_) {
            if (producer->produce(frame_, avfilter_pad_get_type(src_ctx->filter->outputs, 0)) >= 0) {

                ret = av_buffersrc_add_frame_flags(
                    src_ctx,
                    (!frame_->width && !frame_->nb_samples) ? nullptr : frame_,
                    AV_BUFFERSRC_FLAG_PUSH
                );
                if (ret < 0) {
                    LOG(ERROR) << "av_buffersrc_add_frame_flags";
                    running_ = false;
                    break;
                }

                need_dispatch = true;
            }
        }

        if (!need_dispatch) {
            std::this_thread::sleep_for(20ms);
            need_dispatch = true;
            LOG(INFO) << "[DISPATCHER] sleep 20ms";
            continue;
        }

        first_pts_ = (first_pts_ == AV_NOPTS_VALUE) ? av_gettime_relative() : first_pts_;

        // pause @{
        if (paused_) {
            if (paused_pts_ != AV_NOPTS_VALUE) {
                offset_pts_ += av_gettime_relative() - paused_pts_;
            }

            paused_pts_ = av_gettime_relative();

            std::this_thread::sleep_for(20ms);
            continue;
        }

        if (paused_pts_ != AV_NOPTS_VALUE) {
            offset_pts_ += av_gettime_relative() - paused_pts_;
            paused_pts_ = AV_NOPTS_VALUE;
        }
        // @}

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
    for (auto& coder : decoders_) {
        coder->pause();
    }

    for (auto& coder : encoders_) {
        coder->pause();
    }

    paused_ = true;
}

void Dispatcher::resume()
{
    paused_ = false;

    for (auto& coder : encoders_) {
        coder->resume();
    }

    for (auto& coder : decoders_) {
        coder->time_offset(offset_pts_);
        coder->resume();
    }
}

int Dispatcher::reset()
{
    paused_ = false;
    ready_ = false;

    for (auto& coder : decoders_) {
        coder->reset();
    }

    for (auto& coder : encoders_) {
        // wait 0~3s
        for (int i = 0; i < 100; i++) {
            if (!coder->ready() || coder->eof()) break;

            std::this_thread::sleep_for(100ms);
        }
        coder->reset();
    }

    running_ = false;

    if (dispatch_thread_.joinable()) {
        dispatch_thread_.join();
    }

    decoders_.clear();
    encoders_.clear();
    i_streams_.clear();
    o_streams_.clear();

    graph_desc_ = "";

    first_pts_ = AV_NOPTS_VALUE;
    paused_pts_ = AV_NOPTS_VALUE;
    offset_pts_ = 0;

    avfilter_graph_free(&filter_graph_);

    LOG(INFO) << "[DISPATCHER] RESETED";
    return 0;
}