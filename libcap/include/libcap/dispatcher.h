#ifndef CAPTURER_DISPATCHER_H
#define CAPTURER_DISPATCHER_H

#include "consumer.h"
#include "ffmpeg-wrapper.h"
#include "hwaccel.h"
#include "media.h"
#include "producer.h"
#include "queue.h"
#include "timeline.h"

#include <set>
#include <thread>

extern "C" {
#include <libavfilter/avfilter.h>
}

struct DispatchContext
{
    std::unordered_map<Producer<av::frame> *, AVFilterContext *> srcs{};
    AVFilterContext                                             *sink{};

    safe_queue<std::pair<av::frame, Producer<av::frame> *>> queue{ 4 };

    AVFilterGraph    *graph{};
    AVHWDeviceType    hwaccel{ AV_HWDEVICE_TYPE_NONE };
    std::string       graph_desc{};
    std::atomic<bool> dirty{};

    std::atomic<bool> enabled{};
    std::atomic<bool> running{};

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

    int start();

    void pause();
    void resume();

    void stop();

    [[nodiscard]] bool running() const { return vctx_.running || actx_.running; }

    [[nodiscard]] std::chrono::nanoseconds escaped() const;

private:
    int create_filter_graph(AVMediaType);

    int update_encoder_format_by_sinks();

    int dispatch_fn(AVMediaType mt);

    // clock @{
    std::chrono::nanoseconds start_time_{ av::clock::nopts };
    av::timeline_t           timeline_{};
    //@}

    std::set<Producer<av::frame> *> producers_{};
    Consumer<av::frame>            *consumer_{};

    std::atomic<bool> ready_{};

    DispatchContext vctx_{};
    DispatchContext actx_{};
};

#endif //! CAPTURER_DISPATCHER_H