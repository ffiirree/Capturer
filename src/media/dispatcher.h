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
    explicit Dispatcher(const std::string_view& video_graph_desc, const std::string_view& audio_graph_desc) 
        : video_graph_desc_(video_graph_desc), audio_graph_desc_(audio_graph_desc) { }

    ~Dispatcher() { reset(); }

    void append(Producer<AVFrame>* decoder)
    {
        if (decoder && decoder->has(AVMEDIA_TYPE_AUDIO)) {
            a_producer_ctxs_.push_back({ nullptr, decoder, false });
        }

        if (decoder && decoder->has(AVMEDIA_TYPE_VIDEO)) {
            v_producer_ctxs_.push_back({ nullptr, decoder, false });
        }
    }

    void set_encoder(Consumer<AVFrame>* encoder)
    {
        consumer_ctx_.consumer = encoder;
    }

    int create_filter_graph(const std::string_view& video_filters, const std::string_view& audio_filters);

    int start();
    void pause();
    void resume();
    int64_t paused_time();
    int reset();
    void stop() { reset(); }

    [[nodiscard]] bool running() const { return running_; }

    [[nodiscard]] int64_t escaped_us();

    [[nodiscard]] int64_t escaped_ms() { return escaped_us() / 1000; }

private:
    struct ProducerContext {
        AVFilterContext* ctx;
        Producer<AVFrame>* producer;
        bool eof;
    };

    struct ConsumerContext {
        AVFilterContext* actx;
        AVFilterContext* vctx;
        Consumer<AVFrame>* consumer;
        bool a_eof;
        bool v_eof;
    };

    int create_filter_for_input(const Producer<AVFrame>*, AVFilterContext**, enum AVMediaType);
    int create_filter_for_output(const Consumer<AVFrame>*, AVFilterContext**, enum AVMediaType);

    int create_filter_for_video_input(const Producer<AVFrame>*, AVFilterContext**);
    int create_filter_for_video_output(const Consumer<AVFrame>*, AVFilterContext**);
    
    int create_filter_for_audio_input(const Producer<AVFrame>*, AVFilterContext**);
    int create_filter_for_audio_output(const Consumer<AVFrame>*, AVFilterContext**);

    [[nodiscard]] int create_filter_graph_for(std::vector<ProducerContext>&, const std::string&, enum AVMediaType);

    int set_encoder_format_by_sinks();

    int dispatch_fn(enum AVMediaType mt);

    // pts must in OS_TIME_BASE_Q
    bool is_valid_pts(int64_t);

    // clock @{
    std::mutex pause_mtx_;

    int64_t paused_pts_{ AV_NOPTS_VALUE };  // 
    int64_t resumed_pts_{ AV_NOPTS_VALUE };
    int64_t paused_time_{ 0 };              // do not include the time being paused, please use paused_time()
    std::atomic<int64_t> start_time_{ 0 };
    //@}

    std::atomic<bool> running_{ false };
    std::atomic<bool> ready_{ false };

    std::vector<ProducerContext> a_producer_ctxs_{};
    std::vector<ProducerContext> v_producer_ctxs_{};

    ConsumerContext consumer_ctx_{};

    std::string video_graph_desc_{ };
    std::string audio_graph_desc_{ };

    AVFilterGraph* filter_graph_{ nullptr };

    std::thread video_thread_;
    std::thread auido_thread_;
};

#endif // !CAPTURER_DISPATCHER_H