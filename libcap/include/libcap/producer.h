#ifndef CAPTURER_PRODUCER_H
#define CAPTURER_PRODUCER_H

#include "media.h"

#include <atomic>
#include <chrono>

template<class T> class Producer
{
public:
    Producer() = default;

    Producer(const Producer&)            = delete;
    Producer(Producer&&)                 = delete;
    Producer& operator=(const Producer&) = delete;
    Producer& operator=(Producer&&)      = delete;

    virtual ~Producer() = default;

    virtual std::string name() const { return "Unknown"; };

    [[nodiscard]] virtual int open(const std::string& name, std::map<std::string, std::string> options) = 0;

    [[nodiscard]] virtual bool ready() const { return ready_; }

    [[nodiscard]] virtual int start() = 0;

    [[nodiscard]] virtual bool running() const { return running_; }

    virtual void stop() { running_ = false; }

    [[nodiscard]] virtual bool eof() { return eof_ != 0; }

    //
    virtual void enable(AVMediaType, bool) {}

    [[nodiscard]] virtual bool has(AVMediaType) const = 0;

    // system / relative clock
    [[nodiscard]] virtual bool is_realtime() const = 0;

    [[nodiscard]] virtual std::chrono::nanoseconds start_time() const { return start_time_; }

    [[nodiscard]] virtual std::chrono::nanoseconds duration() const { return av::clock::nopts; }

    // audio only
    virtual void mute(const bool v) { muted_ = v; }

    // audio only
    [[nodiscard]] virtual bool muted() const { return muted_; }

    // supported formats
    [[nodiscard]] virtual std::vector<av::vformat_t> video_formats() const { return {}; }
    [[nodiscard]] virtual std::vector<av::aformat_t> audio_formats() const { return {}; }

    // current format
    av::aformat_t afmt{};
    av::vformat_t vfmt{};

    // callback
    std::function<void(const T&, AVMediaType)> onarrived = [](auto, auto) {};

protected:
    std::atomic<bool>    ready_{};
    std::atomic<bool>    running_{};
    std::atomic<bool>    muted_{};
    std::atomic<uint8_t> eof_{};

    std::chrono::nanoseconds start_time_{ av::clock::nopts };
};

#endif //! CAPTURER_PRODUCER_H
