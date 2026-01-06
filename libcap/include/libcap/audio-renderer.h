#ifndef CAPTURER_AUDIO_RENDER_H
#define CAPTURER_AUDIO_RENDER_H

#include "media.h"

#include <chrono>
#include <functional>
#include <string>

struct AudioRenderer
{
    enum RenderFlags
    {
        RENDER_NONE                = 0x00,
        RENDER_ALLOW_STREAM_SWITCH = 0x01,
    };

    AudioRenderer()                                = default;
    AudioRenderer(const AudioRenderer&)            = delete;
    AudioRenderer(AudioRenderer&&)                 = delete;
    AudioRenderer& operator=(const AudioRenderer&) = delete;
    AudioRenderer& operator=(AudioRenderer&&)      = delete;
    virtual ~AudioRenderer()                       = default;

    [[nodiscard]] virtual int  open(const std::string& name, RenderFlags flags) = 0;
    [[nodiscard]] virtual bool ready() const                                    = 0;

    [[nodiscard]] virtual int start() = 0;
    // resetting the stream flushes all pending data and resets the audio clock stream position to 0.
    virtual int               reset() = 0;
    virtual int               stop()  = 0;

    virtual int                mute(bool)    = 0;
    [[nodiscard]] virtual bool muted() const = 0;

    // 0.0 ~ 1.0
    virtual int                 set_volume(float) = 0;
    [[nodiscard]] virtual float volume() const    = 0;

    virtual int                pause()        = 0;
    virtual int                resume()       = 0;
    [[nodiscard]] virtual bool paused() const = 0;

    [[nodiscard]] virtual av::aformat_t format() const = 0;

    // sample number
    [[nodiscard]] virtual uint32_t buffer_size() const = 0;

    std::function<void(float, bool)> on_volume_changed = [](auto, auto) {};

    std::function<uint32_t(uint8_t **data, uint32_t samples, std::chrono::nanoseconds)> callback =
        [](auto, auto, auto) -> int32_t { return 0; };
};

#endif //! CAPTURER_AUDIO_RENDER_H