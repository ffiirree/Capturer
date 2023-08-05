#ifndef CAPTURER_AUDIO_RENDER_H
#define CAPTURER_AUDIO_RENDER_H

#include "media.h"

#include <functional>
#include <string>

struct AudioRender
{
    enum RenderFlags
    {
        RENDER_NONE                = 0x00,
        RENDER_ALLOW_STREAM_SWITCH = 0x01,
    };

    virtual ~AudioRender() = default;

    [[nodiscard]] virtual int open(const std::string& name, RenderFlags flags) = 0;
    [[nodiscard]] virtual bool ready() const                                   = 0;

    // callback: buffer address, samples (per channels), timestamp -> written samples
    [[nodiscard]] virtual int start(const std::function<uint32_t(uint8_t **, uint32_t, int64_t)>&) = 0;
    // resetting the stream flushes all pending data and resets the audio clock stream position to 0.
    virtual int reset()                                                                            = 0;
    virtual int stop()                                                                             = 0;

    virtual int mute(bool)                   = 0;
    [[nodiscard]] virtual bool muted() const = 0;

    // 0.0 ~ 1.0
    virtual int setVolume(float)               = 0;
    [[nodiscard]] virtual float volume() const = 0;

    virtual int pause()                       = 0;
    virtual int resume()                      = 0;
    [[nodiscard]] virtual bool paused() const = 0;

    [[nodiscard]] virtual av::aformat_t format() const = 0;

    // sample number
    [[nodiscard]] virtual uint32_t bufferSize() const = 0;
};

#endif //! CAPTURER_AUDIO_RENDER_H