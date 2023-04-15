#ifndef CAPTURER_PRODUCER_H
#define CAPTURER_PRODUCER_H

#include <thread>
#include <mutex>
#include <atomic>
#include <map>
#include "media.h"

struct AVRational;

template<class T>
class Producer {
public:
    Producer() = default;
    Producer(const Producer&) = delete;
    Producer& operator=(const Producer&) = delete;
    virtual ~Producer()
    {
        std::lock_guard lock(mtx_);

        running_ = false;
        eof_ = 0x00;
        ready_ = false;
        enabled_.clear();

        if (thread_.joinable()) {
            thread_.join();
        }
    }

    virtual void reset() = 0;

    virtual int run() = 0;

    virtual int produce(T*, int) = 0;
    virtual bool empty(int) = 0;

    [[nodiscard]] virtual bool has(int) const = 0;
    [[nodiscard]] virtual std::string format_str(int) const = 0;
    [[nodiscard]] virtual AVRational time_base(int) const = 0;
    virtual bool enabled(int t) { return (enabled_.count(t) > 0) && enabled_[t]; }

    virtual void enable(int t) { enabled_[t] = true; }
    virtual void stop() { running_ = false; }
    virtual bool eof() { return eof_ != 0; }

    virtual int wait()
    {
        if (thread_.joinable()) {
            thread_.join();
        }
        return 0;
    }
    
    [[nodiscard]] bool ready() const { return ready_; }
    [[nodiscard]] bool running() const { return running_; }
    
    aformat_t afmt{};
    vformat_t vfmt{};

protected:
    std::atomic<bool> running_{ false };
    std::atomic<uint8_t> eof_{ 0x00 };
    std::atomic<bool> ready_{ false };
    std::thread thread_;
    std::mutex mtx_;
    std::map<int, bool> enabled_;
};

#endif // !CAPTURER_PRODUCER_H
