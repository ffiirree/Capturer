#ifndef CAPTURER_CONSUMER_H
#define CAPTURER_CONSUMER_H

#include "media.h"

#include <atomic>

template<class T> class Consumer
{
public:
    Consumer() = default;

    Consumer(const Consumer&)            = delete;
    Consumer(Consumer&&)                 = delete;
    Consumer& operator=(const Consumer&) = delete;
    Consumer& operator=(Consumer&&)      = delete;

    virtual ~Consumer() = default;

    [[nodiscard]] virtual int open(const std::string&, std::map<std::string, std::string>) = 0;

    [[nodiscard]] virtual bool ready() const { return ready_; }

    [[nodiscard]] virtual int start() = 0;

    [[nodiscard]] bool running() const { return running_; }

    virtual void               pause() { paused_ = true; }
    [[nodiscard]] virtual bool paused() const { return paused_; }

    virtual void resume() { paused_ = false; }

    virtual void stop() { running_ = false; }

    [[nodiscard]] virtual bool eof() const { return eof_; }

    virtual int consume(const T&, AVMediaType) = 0;

    [[nodiscard]] virtual bool accepts(AVMediaType) const = 0;
    virtual void               enable(AVMediaType, bool)  = 0;

    av::vformat_t vfmt{};
    av::aformat_t afmt{};
    AVRational    input_framerate{ 24, 1 };

protected:
    std::atomic<bool>    ready_{ false };
    std::atomic<bool>    running_{ false };
    std::atomic<bool>    paused_{ false };
    std::atomic<uint8_t> eof_{ 0x00 };
};

#endif //! CAPTURER_CONSUMER_H
