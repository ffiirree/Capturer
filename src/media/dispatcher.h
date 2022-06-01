#ifndef CAPTURER_DISPATCHER_H
#define CAPTURER_DISPATCHER_H

#include <thread>
#include <future>
#include <tuple>
#include "utils.h"
#include "producer.h"
#include "consumer.h"
#include "logging.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
#include <libavutil/time.h>
#include <libavutil/imgutils.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
}

class Dispatcher {
public:
    explicit Dispatcher() { filter_graph_ = avfilter_graph_alloc(); };
    explicit Dispatcher(const std::string_view& filter_complex) : filters_(filter_complex) { filter_graph_ = avfilter_graph_alloc(); }

    ~Dispatcher() { avfilter_graph_free(&filter_graph_); }

    int append( Producer<AVFrame>* decoder) 
    { 
        if (!decoder->ready()) {
            LOG(INFO) << "[DISPATCHER] producer is not ready!";
            return -1;
        }

        AVFilterContext* ctx = nullptr;
        if (create_filter_for_input(decoder, &ctx) < 0) {
            LOG(INFO) << "[DISPATCHER] create_filter_for_input";
            return -1;
        }

        decoders_.emplace_back(std::pair{ decoder, ctx });

        return 0;
    }

    AVFilterContext* append(Consumer<AVFrame>* encoder)
    { 
        AVFilterContext* ctx = nullptr;
        if (create_filter_for_output(encoder, &ctx) < 0) {
            LOG(INFO) << "[DISPATCHER] create_filter_for_output";
            return ctx;
        }

        encoders_.emplace_back(std::pair{  encoder, ctx });

        return ctx;
    }

    int create_filter_graph(const std::string_view& filters);

    int start()
    {
        if (!ready_) {
            LOG(INFO) << "[DISPATCHER] dispatcher is not ready!";
            return -1;
        }

        for (auto& [decoder, _] : decoders_) {
            if (!decoder->ready()) {
                LOG(ERROR) << "deocoder not ready!";
                return -1;
            }
        }

        for (auto& [encoder, _] : encoders_) {
            if (!encoder->ready()) {
                LOG(ERROR) << "encoder not ready!";
                return -1;
            }
        }

        for (auto& [coder, _] : decoders_) {
            coder->run();
        }

        for (auto& [coder, _] : encoders_) {
            coder->run();
        }

        running_ = true;
        dispatch_thread_ = std::thread([this]() { dispatch_thread_f(); });

        return 0;
    }

    int dispatch_thread_f();

    void stop() { reset(); }

    void pause()
    {
        for (auto& [coder, _] : encoders_) {
            coder->pause();
        }

        for (auto& [coder, _] : decoders_) {
            coder->pause();
        }

        paused_ = true;
    }

    void resume()
    {
        paused_ = false;

        for (auto& [coder, _] : encoders_) {
            coder->resume();
        }

        for (auto& [coder, _] : decoders_) {
            coder->time_offset(offset_pts_);
            coder->resume();
        }
    }

    bool running() const { return running_; }

    int reset()
    {
        running_ = false;
        paused_ = false;
        ready_ = false;

        for (auto& [coder, _] : decoders_) {
            coder->stop();
            coder->wait();
        }
        for (auto& [coder, _] : encoders_) {
            coder->stop();
            coder->wait();
        }

        if (dispatch_thread_.joinable()) {
            dispatch_thread_.join();
        }

        decoders_.clear();
        encoders_.clear();

        first_pts_ = AV_NOPTS_VALUE;
        paused_pts_ = AV_NOPTS_VALUE;
        offset_pts_ = 0;


        LOG(INFO) << "[DISPATCHER] RESETED";
        return 0;
    }

    int64_t escaped_us()
    {
        return (first_pts_ == AV_NOPTS_VALUE) ? 0 : av_gettime_relative() - first_pts_ - offset_pts_;
    }

    int64_t escaped_ms()
    {
        return escaped_us() / 1000;
    }

private:
    int create_filter_for_input(const Producer<AVFrame>* decoder, AVFilterContext ** ctx)
    {
        LOG(INFO) << "[DECODER] " << "create filter for decoder: " << decoder->format_str();

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

    int create_filter_for_output(const Consumer<AVFrame>* encoder, AVFilterContext ** ctx)
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

        enum AVPixelFormat pix_fmts[] = { (AVPixelFormat)encoder->format(), AV_PIX_FMT_NONE };
        if (av_opt_set_int_list(*ctx, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN) < 0) {
            LOG(ERROR) << "av_opt_set_int_list";
            return -1;
        }

        return 0;
    }

    int64_t first_pts_{ AV_NOPTS_VALUE };
    int64_t paused_pts_{ AV_NOPTS_VALUE };
    int64_t offset_pts_{ 0 };    // AV_TIME_BASE_Q unit

    std::atomic<bool> running_;
    std::atomic<bool> paused_;
    std::atomic<bool> ready_;

    std::vector<std::pair<Producer<AVFrame>*, AVFilterContext*>> decoders_;
    std::vector<std::pair<Consumer<AVFrame>*, AVFilterContext*>> encoders_;

    std::string filters_{ };

    AVFilterGraph* filter_graph_{ nullptr };

    std::thread dispatch_thread_;
};

#endif // !CAPTURER_DISPATCHER_H