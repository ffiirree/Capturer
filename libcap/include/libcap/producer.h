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
    Producer()                           = default;
    Producer(const Producer&)            = delete;
    Producer& operator=(const Producer&) = delete;

    virtual ~Producer()
    {
        std::lock_guard lock(mtx_);

        running_ = false;
        eof_     = 0x00;
        ready_   = false;
        enabled_.clear();

        if (thread_.joinable()) {
            thread_.join();
        }
    }

    // name
    // options
    [[nodiscard]] virtual int open(const std::string&, std::map<std::string, std::string>) = 0;

    virtual void reset() = 0;

    virtual int run() = 0;

    [[nodiscard]] virtual int produce(T *, int) = 0;
    virtual bool empty(int)                     = 0;

    [[nodiscard]] virtual bool has(int) const               = 0;
    [[nodiscard]] virtual std::string format_str(int) const = 0;
    [[nodiscard]] virtual AVRational time_base(int) const   = 0;

    [[nodiscard]] virtual bool enabled(int t) { return (enabled_.count(t) > 0) && enabled_[t]; }

    virtual void enable(int t) { enabled_[t] = true; }

    virtual int64_t duration() const { return AV_NOPTS_VALUE; }

    virtual void seek(const std::chrono::microseconds&) {}

    virtual void stop() { running_ = false; }

    [[nodiscard]] virtual bool eof() { return eof_ != 0; }

    void mute(bool v) { muted_ = v; }

    virtual int wait()
    {
        if (thread_.joinable()) {
            thread_.join();
        }
        return 0;
    }

    [[nodiscard]] bool ready() const { return ready_; }

    [[nodiscard]] bool running() const { return running_; }

    // supported formats
    virtual std::vector<av::vformat_t> vformats() const = 0;
    virtual std::vector<av::aformat_t> aformats() const = 0;

    // current format
    av::aformat_t afmt{};
    av::vformat_t vfmt{};

protected:
    std::atomic<bool> running_{ false };
    std::atomic<int64_t> seeking_{ AV_NOPTS_VALUE };
    std::atomic<uint8_t> eof_{ 0x00 };
    std::atomic<bool> ready_{ false };
    std::thread thread_;
    std::mutex mtx_;
    std::map<int, bool> enabled_{};

    std::atomic<bool> muted_{ false };
};

#endif //! CAPTURER_PRODUCER_H
