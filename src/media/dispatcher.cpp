#include <chrono>
#include "dispatcher.h"
#include "logging.h"
#include "fmt/format.h"

using namespace std::chrono_literals;

int Dispatcher::create_filter_graph(const std::string_view& filters)
{
    filters_ = filters;

    // filters
    if (!filter_graph_) {
        LOG(ERROR) << "avfilter_graph_alloc";
        return -1;
    }

    if ((decoders_.size() > 1 || encoders_.size() > 1) && filters_.length() == 0) {
        LOG(ERROR) << "parameters error";
        return -1;
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
            if (avfilter_link(ptr->filter_ctx, ptr->pad_idx, encoders_[index].second, 0) != 0) {
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
        if (avfilter_link(decoders_[0].second, 0, encoders_[0].second, 0) != 0) {
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
                ret = av_buffersrc_add_frame_flags(buffersrc_ctx, frame_, AV_BUFFERSRC_FLAG_PUSH);

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
            if (paused_pts_ != AV_NOPTS_VALUE)
                offset_pts_ += av_gettime_relative() - paused_pts_;

            paused_pts_ = av_gettime_relative();

            std::this_thread::sleep_for(20ms);
            continue;
        }

        if (paused_pts_ != AV_NOPTS_VALUE) {
            offset_pts_ += av_gettime_relative() - paused_pts_;
            paused_pts_ = AV_NOPTS_VALUE;
        }
        // @}

        for (auto& [encoder, buffersink_ctx] : encoders_) {
            while (ret >= 0) {
                av_frame_unref(filtered_frame);
                ret = av_buffersink_get_frame_flags(buffersink_ctx, filtered_frame, AV_BUFFERSINK_FLAG_NO_REQUEST);

                if (ret == AVERROR(EAGAIN)) {
                    continue;
                }
                else if (ret == AVERROR_EOF) {
                    LOG(INFO) << "[FILTER THREAD] EOF";

                    // flush and exit
                    ret = encoder->consume(nullptr, AVMEDIA_TYPE_VIDEO);
                    break;
                }
                if (ret < 0) {
                    LOG(ERROR) << "av_buffersink_get_frame_flags()";
                    running_ = false;
                    break;
                }

                ret = encoder->consume(filtered_frame, AVMEDIA_TYPE_VIDEO);
            }
        }
    }

    return ret;
}