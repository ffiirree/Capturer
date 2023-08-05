#ifndef CAPTURER_CONSUMER_H
#define CAPTURER_CONSUMER_H

#include "clock.h"
#include "ffmpeg-wrapper.h"
#include "media.h"

#include <atomic>
#include <map>
#include <mutex>
#include <thread>

template<class T> class Consumer
{
public:
    Consumer()                           = default;
    Consumer(const Consumer&)            = delete;
    Consumer& operator=(const Consumer&) = delete;

    virtual ~Consumer()
    {
        std::lock_guard lock(mtx_);

        running_ = false;
        eof_     = 0x00;
        ready_   = false;

        if (thread_.joinable()) {
            thread_.join();
        }
    }

    [[nodiscard]] virtual int open(const std::string&, std::map<std::string, std::string>) = 0;

    virtual int run()                     = 0;
    virtual int consume(T *, AVMediaType) = 0;

    virtual void pause() { paused_ = true; }
    virtual void resume() { paused_ = false; }

    virtual void stop()
    {
        running_ = false;
        reset();
    }

    virtual void reset() = 0;

    virtual int wait()
    {
        if (thread_.joinable()) {
            thread_.join();
        }
        return 0;
    }

    [[nodiscard]] virtual bool full(AVMediaType) const    = 0;
    [[nodiscard]] virtual bool accepts(AVMediaType) const = 0;
    virtual void enable(AVMediaType, bool = true)         = 0;

    [[nodiscard]] virtual bool ready() const { return ready_; }

    [[nodiscard]] bool running() const { return running_; }

    [[nodiscard]] virtual bool eof() const { return eof_; }

    av::vformat_t vfmt{};
    av::aformat_t afmt{};
    AVRational sink_framerate{ 24, 1 };

protected:
    std::atomic<bool> running_{ false };
    std::atomic<uint8_t> eof_{ 0x00 };
    std::atomic<bool> ready_{ false };
    std::thread thread_;
    std::mutex mtx_;
    std::atomic<bool> paused_{ false };

    av::vsync_t vsync_{ av::vsync_t::cfr };
};

#endif //! CAPTURER_CONSUMER_H
