#ifndef CAPTURER_DISPATCHER_H
#define CAPTURER_DISPATCHER_H

#include "consumer.h"
#include "hwaccel.h"
#include "media.h"
#include "producer.h"

#include <mutex>
#include <set>
#include <thread>

extern "C" {
#include <libavfilter/avfilter.h>
}

class Dispatcher
{
public:
    explicit Dispatcher() = default;

    ~Dispatcher() { reset(); }

    void append(Producer<av::frame> *decoder);

    void set_encoder(Consumer<av::frame> *encoder);

    int initialize(const std::string_view& video_filters, const std::string_view& audio_filters);

    int send_command(AVMediaType mt, const std::string& target, const std::string& cmd,
                     const std::string& arg);

    int                      start();
    void                     pause();
    void                     resume();
    std::chrono::nanoseconds paused_time();
    int                      reset();

    // AV_TIME_BESE
    void seek(const std::chrono::nanoseconds& ts, std::chrono::nanoseconds = 0ns,
              std::chrono::nanoseconds = 0ns);

    void stop() { reset(); }

    [[nodiscard]] bool running() const { return vrunning_ || arunning_; }

    [[nodiscard]] std::chrono::nanoseconds escaped();

    void                      set_clock(const av::clock_t t) { clock_ = t; }
    [[nodiscard]] av::clock_t clock() const { return clock_; }

    void set_video_filters(std::string_view vf);

    void set_audio_filters(std::string_view af);

    av::vformat_t vfmt{};
    av::aformat_t afmt{};

private:
    struct ProducerContext
    {
        AVFilterContext     *src_ctx;
        Producer<av::frame> *producer;
    };

    struct ConsumerContext
    {
        AVFilterContext     *asink_ctx;
        AVFilterContext     *vsink_ctx;
        Consumer<av::frame> *consumer;
    };

    int create_video_sink(const Consumer<av::frame> *, AVFilterContext **);
    int create_audio_sink(const Consumer<av::frame> *, AVFilterContext **);

    int create_filter_graph(AVMediaType);

    int set_encoder_format_by_sinks();

    int dispatch_fn(AVMediaType mt);

    bool is_valid_pts(const std::chrono::nanoseconds&);

    // clock @{
    std::mutex pause_mtx_;

    std::chrono::nanoseconds paused_pts_{ av::clock::nopts }; //
    std::chrono::nanoseconds resumed_pts_{ av::clock::nopts };
    // do not include the time being paused, please use paused_time()
    std::chrono::nanoseconds paused_time_{};
    std::chrono::nanoseconds start_time_{ av::clock::nopts };
    //@}

    // filter graphs @{
    bool video_enabled_{};
    bool audio_enabled_{};

    std::set<Producer<av::frame> *> producers_{};

    std::vector<ProducerContext> aproducer_ctxs_{};
    std::vector<ProducerContext> vproducer_ctxs_{};

    ConsumerContext consumer_ctx_{};

    std::string vfilters_{};
    std::string afilters_{};

    AVFilterGraph    *agraph_{};
    std::atomic<bool> af_dirty_{};
    AVFilterGraph    *vgraph_{};
    std::atomic<bool> vf_dirty_{};
    // @}

    std::atomic<bool> vrunning_{ false };
    std::atomic<bool> arunning_{ false };
    std::atomic<bool> draining_{ false };
    std::atomic<bool> ready_{ false };

    std::jthread video_thread_;
    std::jthread audio_thread_;

    std::atomic<bool>        vseeking_{ false };
    std::atomic<bool>        aseeking_{ false };
    std::atomic<av::clock_t> clock_{ av::clock_t::system };
};

#endif //! CAPTURER_DISPATCHER_H