#ifndef CAPTURER_PRODUCER_H
#define CAPTURER_PRODUCER_H

#include <thread>
#include <mutex>
#include <atomic>

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
        paused_ = false;
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

    virtual bool has(int) const = 0;
    virtual std::string format_str(int) const = 0;
    virtual bool enabled(int t) { return (enabled_.count(t) > 0) && enabled_[t]; }

    virtual void enable(int t) { enabled_[t] = true; }
    virtual void pause() { paused_ = true; }
    virtual void resume() { paused_ = false; }
    virtual void stop() { running_ = false; }
    virtual bool eof() { return eof_ != 0; }

    virtual int wait()
    {
        if (thread_.joinable()) {
            thread_.join();
        }
        return 0;
    }
    
    bool ready() const { return ready_; }
    bool running() const { return running_; }
    bool paused() const { return paused_; }

    void time_offset(int64_t offset) { time_offset_ = offset; }
    
protected:
    std::atomic<bool> running_{ false };
    std::atomic<bool> paused_{ false };
    std::atomic<uint8_t> eof_{ 0x00 };
    std::atomic<bool> ready_{ false };
    std::thread thread_;
    std::mutex mtx_;
    std::atomic<int64_t> time_offset_{ 0 };
    std::map<int, bool> enabled_;
};

#endif // !CAPTURER_PRODUCER_H
