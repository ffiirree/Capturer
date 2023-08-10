#ifndef CAPTURER_PRODUCER_H
#define CAPTURER_PRODUCER_H

#include "ffmpeg-wrapper.h"
#include "media.h"

#include <atomic>
#include <chrono>
#include <limits>
#include <map>
#include <mutex>
#include <thread>

struct AVRational;

template<class T> class Producer
{
public:
    Producer() = default;

    Producer(const Producer&)            = delete;
    Producer(Producer&&)                 = delete;
    Producer& operator=(const Producer&) = delete;
    Producer& operator=(Producer&&)      = delete;

    virtual ~Producer()
    {
        std::lock_guard lock(mtx_);

        running_ = false;
        eof_     = 0x00;
        ready_   = false;
        enabled_.clear();

        if (thread_.joinable()) thread_.join();
    }

    // name
    // options
    [[nodiscard]] virtual int open(const std::string&, std::map<std::string, std::string>) = 0;

    virtual void reset() = 0;

    virtual int run() = 0;

    [[nodiscard]] virtual int produce(T *, AVMediaType) = 0;
    virtual bool empty(AVMediaType)                     = 0;

    [[nodiscard]] virtual bool has(AVMediaType) const               = 0;
    [[nodiscard]] virtual std::string format_str(AVMediaType) const = 0;
    [[nodiscard]] virtual AVRational time_base(AVMediaType) const   = 0;

    [[nodiscard]] virtual bool enabled(AVMediaType t) { return (enabled_.count(t) > 0) && enabled_[t]; }

    virtual void enable(AVMediaType t) { enabled_[t] = true; }

    [[nodiscard]] virtual int64_t duration() const { return AV_NOPTS_VALUE; }

    virtual void seek(const std::chrono::microseconds& ts, int64_t lts, int64_t rts)
    {
        seek_.ts  = ts.count();
        seek_.min = lts;
        seek_.max = rts;
    }

    [[nodiscard]] virtual bool seeking() const { return seek_.ts != AV_NOPTS_VALUE; }

    virtual void stop() { running_ = false; }

    [[nodiscard]] virtual bool eof() { return eof_ != 0; }

    void mute(bool v) { muted_ = v; }

    [[nodiscard]] bool ready() const { return ready_; }

    [[nodiscard]] bool running() const { return running_; }

    // supported formats
    [[nodiscard]] virtual std::vector<av::vformat_t> vformats() const { return {}; }
    [[nodiscard]] virtual std::vector<av::aformat_t> aformats() const { return {}; }

    virtual void set_timing(av::timing_t t) { timing_ = t; }
    [[nodiscard]] virtual av::timing_t timing() const { return timing_; }

    virtual std::vector<std::vector<std::pair<std::string, std::string>>> properties(AVMediaType) const
    {
        return {};
    }

    // current format
    av::aformat_t afmt{};
    av::vformat_t vfmt{};

protected:
    std::atomic<bool> running_{ false };
    std::atomic<uint8_t> eof_{ 0x00 };
    std::atomic<bool> ready_{ false };
    std::thread thread_;
    std::mutex mtx_;
    std::map<int, bool> enabled_{};

    std::atomic<bool> muted_{ false };

    struct seek_t
    {
        std::atomic<int64_t> ts{ AV_NOPTS_VALUE };
        std::atomic<int64_t> min{ std::numeric_limits<int64_t>::min() };
        std::atomic<int64_t> max{ std::numeric_limits<int64_t>::max() };
    } seek_;

    std::atomic<av::timing_t> timing_{ av::timing_t::system };
};

#endif //! CAPTURER_PRODUCER_H
