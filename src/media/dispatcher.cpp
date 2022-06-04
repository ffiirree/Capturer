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

int Dispatcher::create_filter_for_input(const Producer<AVFrame>* decoder, AVFilterContext** ctx)
{
    if (!decoder->ready()) {
        LOG(ERROR) << "[DISPATCHER] decoder is not ready";
        return -1;
    }
    LOG(INFO) << "[DISPATCHER] " << "buffersrc: " << decoder->format_str();

    const AVFilter* buffersrc = avfilter_get_by_name("buffer");
    if (!buffersrc) {
        LOG(ERROR) << "avfilter_get_by_name";
        return -1;
    }

    if (avfilter_graph_create_filter(ctx, buffersrc, "in", decoder->format_str().c_str(), nullptr, filter_graph_) < 0) {
        LOG(ERROR) << "avfilter_graph_create_filter";
        return -1;
    }

    return 0;
}

int Dispatcher::create_filter_for_output(const Consumer<AVFrame>* encoder, AVFilterContext** ctx)
{
    const AVFilter* buffersink = avfilter_get_by_name("buffersink");
    if (!buffersink) {
        LOG(ERROR) << "avfilter_get_by_name";
        return -1;
    }

    if (avfilter_graph_create_filter(ctx, buffersink, "out", nullptr, nullptr, filter_graph_) < 0) {
        LOG(ERROR) << "avfilter_graph_create_filter";
        return -1;
    }

    enum AVPixelFormat pix_fmt = ((AVPixelFormat)encoder->format() == AV_PIX_FMT_NONE) ? AV_PIX_FMT_YUV420P : (AVPixelFormat)encoder->format();
    enum AVPixelFormat pix_fmts[] = { pix_fmt, AV_PIX_FMT_NONE };
    if (av_opt_set_int_list(*ctx, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN) < 0) {
        LOG(ERROR) << "av_opt_set_int_list";
        return -1;
    }

    return 0;
}

int Dispatcher::create_filter_graph(const std::string_view& filters)
{
    filters_ = filters;

    if (filter_graph_) {
        avfilter_graph_free(&filter_graph_);
    }

    // filters
    if (!(filter_graph_ = avfilter_graph_alloc())) {
        LOG(ERROR) << "avfilter_graph_alloc";
        return -1;
    }

    if ((decoders_.size() > 1 || encoders_.size() > 1) && filters_.length() == 0) {
        LOG(ERROR) << "parameters error";
        return -1;
    }

    // buffersrc
    for (auto& [coder, ctx] : decoders_) {
        if (create_filter_for_input(coder, &ctx) < 0) {
            LOG(INFO) << "[DISPATCHER] create_filter_for_input";
            return -1;
        }
    }

    // buffersink
    for (auto& [coder, ctx, _] : encoders_) {
        if (create_filter_for_output(coder, &ctx) < 0) {
            LOG(INFO) << "[DISPATCHER] create_filter_for_output";
            return -1;
        }
    }

    if (filters_.length() > 0) {
        AVFilterInOut* inputs = nullptr;
        AVFilterInOut* outputs = nullptr;
        defer(avfilter_inout_free(&inputs));
        defer(avfilter_inout_free(&outputs));

        if (avfilter_graph_parse2(filter_graph_, filters_.c_str(), &inputs, &outputs) < 0) {
            LOG(ERROR) << "avfilter_graph_parse2";
            return -1;
        }

        auto index = 0;
        for (auto ptr = inputs; ptr; ptr = ptr->next, index++) {
            if (index >= decoders_.size()) {
                LOG(ERROR) << "error filters";
                return -1;
            }
            if (avfilter_link(decoders_[index].second, 0, ptr->filter_ctx, ptr->pad_idx) != 0) {
                LOG(ERROR) << "avfilter_link";
                return -1;
            }
        }
        if (index != decoders_.size()) {
            LOG(ERROR) << "error filters";
            return -1;
        }

        index = 0;
        for (auto ptr = outputs; ptr; ptr = ptr->next, index++) {
            if (index >= encoders_.size()) {
                LOG(ERROR) << "error filters";
                return -1;
            }
            if (avfilter_link(ptr->filter_ctx, ptr->pad_idx, std::get<1>(encoders_[index]), 0) != 0) {
                LOG(ERROR) << "avfilter_link";
                return -1;
            }
        }
        if (index != encoders_.size()) {
            LOG(ERROR) << "error filters";
            return -1;
        }
    }
    else {
        if (avfilter_link(decoders_[0].second, 0, std::get<1>(encoders_[0]), 0) != 0) {
            LOG(ERROR) << "avfilter_link";
            return -1;
        }
    }

    if (avfilter_graph_config(filter_graph_, nullptr) < 0) {
        LOG(ERROR) << "avfilter_graph_config(filter_graph_, nullptr)";
        return -1;
    }

    ready_ = true;
    LOG(INFO) << "[ENCODER] " << "filter graph @{";
    LOG(INFO) << "\n" << avfilter_graph_dump(filter_graph_, nullptr);
    LOG(INFO) << "[ENCODER] " << "@}";
    return 0;
}

int Dispatcher::start()
{
    if (!ready_ || running_) {
        LOG(INFO) << "[DISPATCHER] already running or not ready!";
        return -1;
    }

    for (auto& [decoder, _] : decoders_) {
        if (!decoder->ready()) {
            LOG(ERROR) << "deocoder not ready!";
            return -1;
        }
    }

    for (auto& [encoder, _, __] : encoders_) {
        if (!encoder->ready()) {
            LOG(ERROR) << "encoder not ready!";
            return -1;
        }
    }

    for (auto& [coder, _] : decoders_) {
        coder->run();
    }

    for (auto& [coder, _, __] : encoders_) {
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
    while (running_) {
        for (auto& [decoder, buffersrc_ctx] : decoders_) {
            if (decoder->produce(frame_, AVMEDIA_TYPE_VIDEO) >= 0) {
                ret = av_buffersrc_add_frame_flags(
                    buffersrc_ctx, 
                    (!frame_->width && !frame_->height) ? nullptr : frame_,
                    AV_BUFFERSRC_FLAG_PUSH
                );
                if (ret < 0) {
                    LOG(ERROR) << "av_buffersrc_add_frame_flags";
                    running_ = false;
                    break;
                }
            }
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

        for (auto& [encoder, buffersink_ctx, sink_eof] : encoders_) {
            while (ret >= 0 && !sink_eof) {
                av_frame_unref(filtered_frame);
                ret = av_buffersink_get_frame_flags(buffersink_ctx, filtered_frame, AV_BUFFERSINK_FLAG_NO_REQUEST);

                if (ret == AVERROR(EAGAIN)) {
                    continue;
                }
                else if (ret == AVERROR_EOF) {
                    LOG(INFO) << "[DISPATCHER] EOF";
                    sink_eof = true;
                    // through and send null packet
                    av_frame_unref(filtered_frame);
                }
                else if (ret < 0) {
                    LOG(ERROR) << "av_buffersink_get_frame_flags()";
                    running_ = false;
                    break;
                }

                while (encoder->full(AVMEDIA_TYPE_VIDEO) && running_) {
                    std::this_thread::sleep_for(20ms);
                }
                ret = encoder->consume(filtered_frame, AVMEDIA_TYPE_VIDEO);
            }
        }
    }

    return ret;
}

void Dispatcher::pause()
{
    for (auto& [coder, _] : decoders_) {
        coder->pause();
    }

    for (auto& [coder, _, __] : encoders_) {
        coder->pause();
    }

    paused_ = true;
}

void Dispatcher::resume()
{
    paused_ = false;

    for (auto& [coder, _, __] : encoders_) {
        coder->resume();
    }

    for (auto& [coder, _] : decoders_) {
        coder->time_offset(offset_pts_);
        coder->resume();
    }
}

int Dispatcher::reset()
{
    paused_ = false;
    ready_ = false;

    for (auto& [coder, _] : decoders_) {
        coder->reset();
    }

    for (auto& [coder, _, __] : encoders_) {
        // wait 0~3s
        for (int i = 0; i < 30; i++) {
            if (coder->eof()) break;

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

    first_pts_ = AV_NOPTS_VALUE;
    paused_pts_ = AV_NOPTS_VALUE;
    offset_pts_ = 0;

    avfilter_graph_free(&filter_graph_);

    LOG(INFO) << "[DISPATCHER] RESETED";
    return 0;
}