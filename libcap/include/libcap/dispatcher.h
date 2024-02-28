#ifndef CAPTURER_DISPATCHER_H
#define CAPTURER_DISPATCHER_H

#include "consumer.h"
#include "ffmpeg-wrapper.h"
#include "hwaccel.h"
#include "media.h"
#include "producer.h"
#include "queue.h"

#include <mutex>
#include <set>
#include <thread>

extern "C" {
#include <libavfilter/avfilter.h>
}

struct DispatchContext
{
    std::unordered_map<Producer<av::frame> *, AVFilterContext *> srcs{};
    AVFilterContext                                             *sink{};

    safe_queue<std::pair<av::frame, Producer<av::frame> *>> queue{ 60 };

    AVFilterGraph    *graph{};
    AVHWDeviceType    hwaccel{ AV_HWDEVICE_TYPE_NONE };
    std::string       graph_desc{};
    std::atomic<bool> dirty{};

    std::atomic<bool> enabled{};
    std::atomic<bool> running{};
    std::atomic<bool> seeking{};

    std::jthread thread;
};

class Dispatcher
{
public:
    explicit Dispatcher() = default;

    ~Dispatcher();

    int add_input(Producer<av::frame> *decoder);

    void set_output(Consumer<av::frame> *encoder);

    void set_hwaccel(AVHWDeviceType);

    int initialize(const std::string_view& video_filters, const std::string_view& audio_filters);

    int                      start();
    void                     pause();
    void                     resume();
    std::chrono::nanoseconds paused_time();

    void seek(const std::chrono::nanoseconds& ts, const std::chrono::nanoseconds& rel);

    void stop();

    [[nodiscard]] bool running() const { return vctx_.running || actx_.running; }

    [[nodiscard]] std::chrono::nanoseconds escaped();

    void set_video_filters(std::string_view vf);

    void set_audio_filters(std::string_view af);

private:
    int create_video_sink(AVFilterContext **ctx, const av::vformat_t& args) const;
    int create_audio_sink(AVFilterContext **ctx, const av::aformat_t& args) const;

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

    std::set<Producer<av::frame> *> producers_{};
    Consumer<av::frame>            *consumer_{};

    std::atomic<bool> ready_{};

    // check we have enough frames for each media type
    std::mutex              notenough_mtx_{};
    std::condition_variable notenough_{};

    DispatchContext vctx_{};
    DispatchContext actx_{};
};

#endif //! CAPTURER_DISPATCHER_H