#ifndef CAPTURER_CONSUMER_H
#define CAPTURER_CONSUMER_H

#include <thread>
#include <mutex>
#include <atomic>
#include "clock.h"
extern "C" {
#include <libavutil/rational.h>
#include <libavutil/pixfmt.h>
#include <libavutil/samplefmt.h>
}

template<class T>
class Consumer {
public:
    Consumer() = default;
    Consumer(const Consumer&) = delete;
    Consumer& operator=(const Consumer&) = delete;
    virtual ~Consumer() {
        std::lock_guard lock(mtx_);

        running_ = false;
        eof_ = 0x00;
        ready_ = false;

        if (thread_.joinable()) {
            thread_.join();
        }
    }

    virtual int run() = 0;
    virtual int consume(T*, int) = 0;

    virtual void stop() { running_ = false; reset(); }
    virtual void reset() = 0;
    virtual int wait()
    {
        if (thread_.joinable()) {
            thread_.join();
        }
        return 0;
    }

    [[nodiscard]] virtual bool full(int) const = 0;
    [[nodiscard]] virtual int format(int) const = 0;
    [[nodiscard]] virtual bool accepts(int) const = 0;
    virtual void enable(int, bool = true) = 0;

    [[nodiscard]] virtual bool ready() const { return ready_; }
    [[nodiscard]] bool running() const { return running_; }
    [[nodiscard]] virtual bool eof() const { return eof_; }

    struct VideoFormat {
        int width{ 0 };
        int height{ 0 };

        bool is_cfr{ false };
        enum AVPixelFormat format { AV_PIX_FMT_YUV420P };
        AVRational framerate{ 24, 1 };
        AVRational sample_aspect_ratio{ 1,1 };
        AVRational time_base{ 1, OS_TIME_BASE };
    } vfmt_;

    struct AudioFormat {
        int sample_rate{ 48000 };
        int channels{ 2 };
        enum AVSampleFormat format { AV_SAMPLE_FMT_FLTP };
        uint64_t channel_layout{ 0 };

        AVRational time_base{ 1, OS_TIME_BASE };
    } afmt_;

protected:
    std::atomic<bool> running_{ false };
    std::atomic<uint8_t> eof_{ 0x00 };
    std::atomic<bool> ready_{ false };
    std::thread thread_;
    std::mutex mtx_;
};

#endif // !CAPTURER_CONSUMER_H
