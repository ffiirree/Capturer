#ifndef CAPTURER_PRODUCER_H
#define CAPTURER_PRODUCER_H

#include "media.h"

#include <atomic>
#include <chrono>
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

    [[nodiscard]] virtual bool enabled(AVMediaType t) { return enabled_.contains(t) && enabled_[t]; }

    virtual void enable(const AVMediaType t) { enabled_[t] = true; }

    [[nodiscard]] virtual int64_t duration() const { return AV_NOPTS_VALUE; }

    virtual void seek(const std::chrono::nanoseconds& ts, std::chrono::nanoseconds lts,
                      std::chrono::nanoseconds rts)
    {
        seek_.ts  = ts;
        seek_.min = lts;
        seek_.max = rts;
    }

    [[nodiscard]] virtual bool seeking() const { return seek_.ts.load() != av::clock::nopts; }

    virtual void stop() { running_ = false; }

    [[nodiscard]] virtual bool eof() { return eof_ != 0; }

    void mute(const bool v) { muted_ = v; }

    [[nodiscard]] bool ready() const { return ready_; }

    [[nodiscard]] bool running() const { return running_; }

    // supported formats
    [[nodiscard]] virtual std::vector<av::vformat_t> vformats() const { return {}; }
    [[nodiscard]] virtual std::vector<av::aformat_t> aformats() const { return {}; }

    virtual void set_timing(const av::timing_t t) { timing_ = t; }
    [[nodiscard]] virtual av::timing_t timing() const { return timing_; }

    virtual std::vector<std::map<std::string, std::string>> properties(AVMediaType) const { return {}; }

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
        std::atomic<std::chrono::nanoseconds> ts{ av::clock::nopts };
        std::atomic<std::chrono::nanoseconds> min{ av::clock::min };
        std::atomic<std::chrono::nanoseconds> max{ av::clock::max };
    } seek_;

    std::atomic<av::timing_t> timing_{ av::timing_t::system };
};

#endif //! CAPTURER_PRODUCER_H
