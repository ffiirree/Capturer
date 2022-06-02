#ifndef CAPTURER_DISPATCHER_H
#define CAPTURER_DISPATCHER_H

#include <thread>
#include <future>
#include <tuple>
#include "utils.h"
#include "producer.h"
#include "consumer.h"

extern "C" {
#include <libavutil/time.h>
#include <libavfilter/avfilter.h>
}

class Dispatcher {
public:
    explicit Dispatcher() = default;
    explicit Dispatcher(const std::string_view& filter_complex) : filters_(filter_complex) { }

    ~Dispatcher() { reset(); }

    decltype(auto) append(Producer<AVFrame>* decoder)
    {
        return decoders_.emplace_back(std::pair{ decoder, nullptr });
    }

    decltype(auto) append(Consumer<AVFrame>* encoder)
    {
        return encoders_.emplace_back(std::pair{ encoder, nullptr });
    }

    int create_filter_graph(const std::string_view& filters);

    int start();
    void pause();
    void resume();
    int reset();
    void stop() { reset(); }

    bool running() const { return running_; }

    int64_t escaped_us() const 
    {
        return (first_pts_ == AV_NOPTS_VALUE) ? 0 : std::max<int64_t>(0, av_gettime_relative() - first_pts_ - offset_pts_);
    }

    int64_t escaped_ms() const { return escaped_us() / 1000; }

private:
    int create_filter_for_input(const Producer<AVFrame>* decoder, AVFilterContext** ctx);
    int create_filter_for_output(const Consumer<AVFrame>* encoder, AVFilterContext** ctx);
    int dispatch_thread_f();

    int64_t first_pts_{ AV_NOPTS_VALUE };
    int64_t paused_pts_{ AV_NOPTS_VALUE };
    int64_t offset_pts_{ 0 };    // AV_TIME_BASE_Q unit

    std::atomic<bool> running_{ false };
    std::atomic<bool> paused_{ false };
    std::atomic<bool> ready_{ false };

    std::vector<std::pair<Producer<AVFrame>*, AVFilterContext*>> decoders_;
    std::vector<std::pair<Consumer<AVFrame>*, AVFilterContext*>> encoders_;

    std::string filters_{ };

    AVFilterGraph* filter_graph_{ nullptr };

    std::thread dispatch_thread_;
};

#endif // !CAPTURER_DISPATCHER_H