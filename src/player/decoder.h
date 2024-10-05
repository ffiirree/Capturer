#ifndef CAPTURER_DECODER_H
#define CAPTURER_DECODER_H

#include "libcap/ffmpeg-wrapper.h"
#include "libcap/producer.h"
#include "libcap/queue.h"
#include "subtitle.h"

#include <ass/ass.h>
#include <list>
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

    std::pair<int, std::list<Subtitle>> subtitle(const std::chrono::milliseconds& now);

    std::vector<AVStream *> streams(AVMediaType type);

    AVStream *stream(AVMediaType type);

    void set_ass_render_size(int w, int h);

    int select(AVMediaType type, int index);

    int index(AVMediaType type) const;

    int open_external_subtitle(const std::string& filename);

    std::string external_subtitle() const { return ass_external_; }

    int set_hwaccel(AVHWDeviceType, AVPixelFormat);

private:
    int open_video_stream(int index);
    int open_audio_stream(int index);
    int open_subtitle_stream(int index);

    int ass_init();
    int ass_open_internal();

    int ass_open_external(const std::string& filename);
    int ff_open_external(const std::string& filename);

    int create_audio_graph();
    int create_video_graph();
    int filter_frame(DecodingContext& ctx, const av::frame& frame, AVMediaType type);

    void readpkt_thread_fn();
    void vdecode_thread_fn();
    void adecode_thread_fn();
    void sdecode_thread_fn();

    AVFormatContext  *fmt_ctx_{};
    std::atomic<bool> ready_{};
    std::atomic<bool> running_{};

    // read thread @{
    std::jthread            rthread_{};
    std::atomic<bool>       eof_{};
    std::mutex              notenough_mtx_{};
    std::condition_variable notenough_{};
    //@}

    DecodingContext actx_{};
    DecodingContext vctx_{};
    DecodingContext sctx_{};

    // subtitles
    std::atomic<int> sub_type_{}; // text or bitmap based

    // text: libass
    mutable std::shared_mutex ass_mtx_{};
    ASS_Library              *ass_library_{};
    ASS_Renderer             *ass_renderer_{};
    ASS_Track                *ass_track_{};
    std::atomic<int>          ass_width_{};
    std::atomic<int>          ass_height_{};
    std::atomic<int>          ass_cached_chunks_{};
    std::string               ass_external_{};

    // bitmap
    std::mutex          subtitle_mtx_{};
    std::atomic<int>    subtitle_changed_{};
    std::list<Subtitle> bitmaps_{}; // buffered subtitles
    std::list<Subtitle> displayed_{};

    // switch stream
    mutable std::shared_mutex selected_mtx_{};
    AVMediaType               selected_type_{ AVMEDIA_TYPE_UNKNOWN };
    int                       selected_index_{ -1 };

    // seek, AV_TIME_BASE @{
    mutable std::shared_mutex seek_mtx_{};
    int64_t                   seek_min_{ std::numeric_limits<int64_t>::min() };
    std::atomic<int64_t>      seek_pts_{ AV_NOPTS_VALUE };
    int64_t                   seek_max_{ std::numeric_limits<int64_t>::max() };
    std::atomic<int64_t>      trim_pts_{ 0 };
    // @}
};

#endif //! CAPTURER_DECODER_H
