#ifndef CAPTURER_DECODER_H
#define CAPTURER_DECODER_H

#include "libcap/ffmpeg-wrapper.h"
#include "libcap/producer.h"
#include "libcap/queue.h"

#include <shared_mutex>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
}

struct DecodingContext
{
    std::atomic<int> index{ -1 };
    AVStream        *stream{};

    std::jthread thread{};

    AVCodecContext *codec{};

    AVFilterGraph    *graph{};
    std::string       graph_desc{};
    std::atomic<bool> dirty{ true };
    AVFilterContext  *src{};
    AVFilterContext  *sink{};
    av::frame         frame{};

    int64_t next_pts{ AV_NOPTS_VALUE };

    std::atomic<bool> done{};

    std::atomic<bool>      synced{ true }; // after seeking
    safe_queue<av::packet> queue{ 240 };
};

class Decoder
{
public:
    ~Decoder();

    // open input
    int open(const std::string& name);

    bool ready() const { return ready_; }

    // start thread
    int start();

    // reset and stop the thread
    void stop();

    void seek(const std::chrono::nanoseconds& ts, const std::chrono::nanoseconds& rel);

    bool seeking(AVMediaType) const;

    bool has(AVMediaType) const;

    std::chrono::nanoseconds start_time() const;

    std::chrono::nanoseconds duration() const;

    bool eof(AVMediaType) const;

    bool eof() const;

    std::vector<std::map<std::string, std::string>> properties(AVMediaType) const;

    av::aformat_t afi{}; // audio input format
    av::aformat_t afo{}; // audio output format

    av::vformat_t vfi{}; // video input format
    av::vformat_t vfo{}; // video output format

    std::function<void(const av::frame&, AVMediaType)> onarrived = [](auto, auto) {};

private:
    int create_audio_graph();
    int create_video_graph();
    int filter_frame(DecodingContext& ctx, const av::frame& frame, AVMediaType type);

    void readpkt_thread_fn();
    void vdecode_thread_fn();
    void adecode_thread_fn();

    AVFormatContext  *fmt_ctx_{};
    std::atomic<bool> ready_{};
    std::atomic<bool> running_{};
    std::atomic<bool> eof_{};     // end of file
    std::jthread      rthread_{}; // read thread

    // read @{
    std::mutex              notenough_mtx_{};
    std::condition_variable notenough_{};
    //@}

    DecodingContext actx{}, vctx{}, sctx{};

    // seek, AV_TIME_BASE @{
    mutable std::shared_mutex seek_mtx_{};
    int64_t                   seek_min_{ std::numeric_limits<int64_t>::min() };
    std::atomic<int64_t>      seek_pts_{ AV_NOPTS_VALUE };
    int64_t                   seek_max_{ std::numeric_limits<int64_t>::max() };
    // @}
    std::atomic<int64_t>      trim_pts_{ 0 };
};

#endif //! CAPTURER_DECODER_H
