#ifndef CAPTURER_DISPATCHER_H
#define CAPTURER_DISPATCHER_H

#include "consumer.h"
#include "hwaccel.h"
#include "media.h"
#include "producer.h"

#include <future>
#include <thread>

extern "C" {
#include <libavfilter/avfilter.h>
#include <libavutil/time.h>
}

class Dispatcher
{
public:
    explicit Dispatcher() = default;

    ~Dispatcher() { reset(); }

    void append(Producer<AVFrame> *decoder)
    {
        if (!locked_) {
            if (decoder && decoder->has(AVMEDIA_TYPE_AUDIO)) {
                aproducer_ctxs_.push_back({ nullptr, decoder, false });
                audio_enabled_ = true;
            }

            if (decoder && decoder->has(AVMEDIA_TYPE_VIDEO)) {
                vproducer_ctxs_.push_back({ nullptr, decoder, false });
                video_enabled_ = true;
            }
        }
    }

    void set_encoder(Consumer<AVFrame> *encoder)
    {
        if (!locked_) {
            consumer_ctx_.consumer = encoder;
        }
    }

    int initialize(const std::string_view& video_filters, const std::string_view& audio_filters);

    int send_command(AVMediaType mt, const std::string& target, const std::string& cmd,
                     const std::string& arg);

    int start();
    void pause();
    void resume();
    int64_t paused_time();
    int reset();

    // AV_TIME_BESE
    void seek(const std::chrono::nanoseconds& ts, std::chrono::nanoseconds = 0ns,
              std::chrono::nanoseconds = 0ns);

    void stop() { reset(); }

    [[nodiscard]] bool running() const { return vrunning_ || arunning_; }

    [[nodiscard]] int64_t escaped_us();

    [[nodiscard]] int64_t escaped_ms() { return escaped_us() / 1'000; }

    void set_timing(av::timing_t t) { timing_ = t; }
    [[nodiscard]] av::timing_t timing() const { return timing_; }

    av::vformat_t vfmt{};
    av::aformat_t afmt{};

    std::string vfilters{};
    std::string afilters{};

private:
    struct ProducerContext
    {
        AVFilterContext *src_ctx;
        Producer<AVFrame> *producer;
        bool eof;
    };

    struct ConsumerContext
    {
        AVFilterContext *asink_ctx;
        AVFilterContext *vsink_ctx;
        Consumer<AVFrame> *consumer;
        bool aeof;
        bool veof;
    };

    int create_src_filter(const Producer<AVFrame> *, AVFilterContext **, AVMediaType);
    int create_sink_filter(const Consumer<AVFrame> *, AVFilterContext **, AVMediaType);

    int create_video_src(const Producer<AVFrame> *, AVFilterContext **);
    int create_video_sink(const Consumer<AVFrame> *, AVFilterContext **);

    int create_audio_src(const Producer<AVFrame> *, AVFilterContext **);
    int create_audio_sink(const Consumer<AVFrame> *, AVFilterContext **);

    int create_filter_graph(AVMediaType);

    int set_encoder_format_by_sinks();

    int dispatch_fn(AVMediaType mt);

    // pts must in OS_TIME_BASE_Q
    bool is_valid_pts(int64_t);

    // clock @{
    std::mutex pause_mtx_;

    int64_t paused_pts_{ AV_NOPTS_VALUE }; //
    int64_t resumed_pts_{ AV_NOPTS_VALUE };
    int64_t paused_time_{ 0 };             // do not include the time being paused, please use paused_time()
    int64_t start_time_{ AV_NOPTS_VALUE };
    //@}

    // filter graphs @{
    std::atomic<bool> locked_{}; // lock filter graph sources

    bool video_enabled_{};
    bool audio_enabled_{};

    std::vector<ProducerContext> aproducer_ctxs_{};
    std::vector<ProducerContext> vproducer_ctxs_{};

    ConsumerContext consumer_ctx_{};

    AVFilterGraph *agraph_{ nullptr };
    AVFilterGraph *vgraph_{ nullptr };
    // @}

    std::atomic<bool> vrunning_{ false };
    std::atomic<bool> arunning_{ false };
    std::atomic<bool> draining_{ false };
    std::atomic<bool> ready_{ false };

    std::thread video_thread_;
    std::thread audio_thread_;

    std::atomic<bool> vseeking_{ false };
    std::atomic<bool> aseeking_{ false };
    std::atomic<av::timing_t> timing_{ av::timing_t::system };
};

#endif //! CAPTURER_DISPATCHER_H