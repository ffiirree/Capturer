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
    explicit Dispatcher(const std::string_view& graph_desc) : graph_desc_(graph_desc) { }

    ~Dispatcher() { reset(); }

    decltype(auto) append(Producer<AVFrame>* decoder)
    {
        return decoders_.emplace_back(decoder);
    }

    decltype(auto) append(Consumer<AVFrame>* encoder)
    {
        return encoders_.emplace_back(encoder);
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

    bool has_audio() const { return has_audio_in_; }

    AVFilterContext* find_sink(Consumer<AVFrame>*, enum AVMediaType);

private:
    int create_filter_for_input(const Producer<AVFrame>*, AVFilterContext**, enum AVMediaType);
    int create_filter_for_output(const Consumer<AVFrame>*, AVFilterContext**, enum AVMediaType);

    int create_filter_for_video_input(const Producer<AVFrame>*, AVFilterContext**);
    int create_filter_for_video_output(const Consumer<AVFrame>*, AVFilterContext**);
    
    int create_filter_for_audio_input(const Producer<AVFrame>*, AVFilterContext**);
    int create_filter_for_audio_output(const Consumer<AVFrame>*, AVFilterContext**);
    int dispatch_thread_f();

    int64_t first_pts_{ AV_NOPTS_VALUE };
    int64_t paused_pts_{ AV_NOPTS_VALUE };
    int64_t offset_pts_{ 0 };    // AV_TIME_BASE_Q unit

    std::atomic<bool> running_{ false };
    std::atomic<bool> paused_{ false };
    std::atomic<bool> ready_{ false };
    std::atomic<bool> has_audio_in_{ false };

    std::vector<Producer<AVFrame>*> decoders_;
    std::vector<Consumer<AVFrame>*> encoders_;

    std::vector<std::tuple<AVFilterContext*, Producer<AVFrame>*, bool>> i_streams_;
    std::vector<std::tuple<AVFilterContext*, Consumer<AVFrame>*, bool>> o_streams_;

    std::string graph_desc_{ };

    AVFilterGraph* filter_graph_{ nullptr };

    std::thread dispatch_thread_;
};

#endif // !CAPTURER_DISPATCHER_H